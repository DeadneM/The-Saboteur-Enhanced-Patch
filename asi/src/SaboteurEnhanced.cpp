#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <string>

#pragma intrinsic(_ReturnAddress)

static HMODULE g_self = nullptr;
static INIT_ONCE g_initOnce = INIT_ONCE_STATIC_INIT;
static FILE* g_log = nullptr;
static SRWLOCK g_logLock = SRWLOCK_INIT;

static std::wstring ModuleDirectory(HMODULE module)
{
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(module, path, MAX_PATH);
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash) *slash = L'\0';
    return path;
}

static void Log(const char* fmt, ...)
{
    if (!g_log) return;
    AcquireSRWLockExclusive(&g_logLock);
    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::fprintf(g_log, "[%02u:%02u:%02u.%03u] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(g_log, fmt, ap);
    va_end(ap);
    std::fprintf(g_log, "\n");
    std::fflush(g_log);
    ReleaseSRWLockExclusive(&g_logLock);
}

struct SectionRange
{
    uint8_t* begin{};
    size_t size{};
};

static SectionRange GetSectionRange(HMODULE module, const char* wanted)
{
    auto* base = reinterpret_cast<uint8_t*>(module);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE) return {};
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return {};

    IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec)
    {
        char name[9] = {};
        std::memcpy(name, sec->Name, 8);
        if (std::strcmp(name, wanted) == 0)
        {
            size_t size = sec->Misc.VirtualSize ? sec->Misc.VirtualSize : sec->SizeOfRawData;
            return {base + sec->VirtualAddress, size};
        }
    }
    return {};
}

static uint8_t* FindExact(const SectionRange& range, const uint8_t* pattern, size_t patternSize)
{
    if (!range.begin || !pattern || !patternSize || range.size < patternSize) return nullptr;
    for (size_t i = 0; i + patternSize <= range.size; ++i)
    {
        if (std::memcmp(range.begin + i, pattern, patternSize) == 0)
            return range.begin + i;
    }
    return nullptr;
}

static bool WriteBytes(void* address, const void* bytes, size_t size)
{
    DWORD oldProtect = 0;
    if (!VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    std::memcpy(address, bytes, size);
    FlushInstructionCache(GetCurrentProcess(), address, size);

    DWORD ignored = 0;
    VirtualProtect(address, size, oldProtect, &ignored);
    return true;
}

static bool ApplyV310(const SectionRange& text, uintptr_t moduleBase)
{
    static const uint8_t sig[] = {
        0xBE,0x1F,0x00,0x00,0x00,0xD9,0x9F,0xA4,0x00,0x00,
        0x00,0x8B,0xEE,0xEB,0x51,0xD9,0x47,0x58,0xD9,0x05
    };
    uint8_t* hit = FindExact(text, sig, sizeof(sig));
    if (!hit)
    {
        Log("[SKIP] V310 WSModel auto RenderSlice signature not found.");
        return false;
    }

    uint8_t* target = hit + 15;
    static const uint8_t expected[] = {0xD9,0x47};
    static const uint8_t patch[] = {0xEB,0x4F};
    if (std::memcmp(target, expected, sizeof(expected)) != 0)
    {
        Log("[SKIP] V310 target bytes mismatch.");
        return false;
    }
    if (!WriteBytes(target, patch, sizeof(patch)))
    {
        Log("[FAIL] V310 VirtualProtect/write failed.");
        return false;
    }

    Log("[OK] V310 WSModel auto RenderSlice full-mask fix applied at RVA 0x%08X.",
        static_cast<unsigned>(reinterpret_cast<uintptr_t>(target) - moduleBase));
    return true;
}

static bool ApplyV311(const SectionRange& text, uintptr_t moduleBase)
{
    static const uint8_t sig[] = {
        0xD9,0xE8,0xD9,0x5F,0x74,0x0F,0xB6,0x4A,0x02,0xD9,
        0x42,0x04,0xBE,0x01,0x00,0x00,0x00,0xD3,0xE6
    };
    uint8_t* hit = FindExact(text, sig, sizeof(sig));
    if (!hit)
    {
        Log("[SKIP] V311 ModelInfo RenderSlice signature not found.");
        return false;
    }

    uint8_t* target = hit + 5;
    static const uint8_t expected[] = {0x0F,0xB6,0x4A,0x02};
    static const uint8_t patch[] = {0xB1,0x05,0x90,0x90};
    if (std::memcmp(target, expected, sizeof(expected)) != 0)
    {
        Log("[SKIP] V311 target bytes mismatch.");
        return false;
    }
    if (!WriteBytes(target, patch, sizeof(patch)))
    {
        Log("[FAIL] V311 VirtualProtect/write failed.");
        return false;
    }

    Log("[OK] V311 explicit ModelInfo RenderSlice full-mask fix applied at RVA 0x%08X.",
        static_cast<unsigned>(reinterpret_cast<uintptr_t>(target) - moduleBase));
    return true;
}


static bool ApplyOdinChildVisibilityGate(const SectionRange& text, uintptr_t moduleBase)
{
    // Hierarchical repeat/instanced-mesh visibility gate.
    //
    // Original logic around VA 0x00667CBB:
    //   cmp byte ptr [esp+20h],0
    //   je  invisible
    //   cmp byte ptr [esp+24h],0
    //   jne invisible        <-- target
    //   mov al,1
    // invisible:
    //   xor al,al
    //
    // [esp+24h] is derived from a virtual float result compared against the
    // exact 0.0f constant at VA 0x010A45B0. This A/B ignores only that
    // zero-result rejection while preserving the parent-visible gate.
    static const uint8_t sig[] = {
        0x80,0x7C,0x24,0x20,0x00,
        0x74,0x0B,
        0x80,0x7C,0x24,0x24,0x00,
        0x75,0x04,
        0xB0,0x01,
        0xEB,0x02,
        0x32,0xC0
    };

    uint8_t* hit = FindExact(text, sig, sizeof(sig));
    if (!hit)
    {
        Log("[SKIP] Odin child-visibility gate signature not found.");
        return false;
    }

    uint8_t* target = hit + 12;
    static const uint8_t expected[] = {0x75,0x04};
    static const uint8_t patch[] = {0x90,0x90};

    if (std::memcmp(target, expected, sizeof(expected)) != 0)
    {
        Log("[SKIP] Odin child-visibility gate target bytes mismatch.");
        return false;
    }

    if (!WriteBytes(target, patch, sizeof(patch)))
    {
        Log("[FAIL] Odin child-visibility gate write failed.");
        return false;
    }

    Log("[OK] Odin child-visibility zero-result rejection bypassed at RVA 0x%08X.",
        static_cast<unsigned>(reinterpret_cast<uintptr_t>(target) - moduleBase));
    return true;
}

// -----------------------------------------------------------------------------
// ASI 0.2 OdinMeshInstance diagnostic
// -----------------------------------------------------------------------------
//
// Recovered Core1/retail vtable:
//   VA 0x01081898 / RVA 0x00C81898
//   slot 1 = 0x00E14BF0 OdinMeshInstance::ReInstance
//   slot 6 = 0x00E14BA0 recovered as RemoveHighResSegments
//            implementation is only "mov al,[ecx+33h] ; ret"
//   slot 8 = 0x00E143D0 OdinMeshInstance::PreRelease
//   slot 9 = 0x00E14BD0 OdinMeshInstance::IsFullyLoaded
//
// 0.2 does not alter any Odin decision. It replaces selected vtable entries
// only with pass-through tracing wrappers, preserving original return values.

using OdinVoidMethod = void (__thiscall*)(void*);
using OdinBoolMethod = bool (__thiscall*)(void*);
using OdinGetRootMethod = void* (__thiscall*)(void*);
using OdinSyncMethod = void (__thiscall*)(void*, void*, int);

static OdinVoidMethod g_odinReInstance = nullptr;
static OdinBoolMethod g_odinRemoveHighRes = nullptr;
static OdinVoidMethod g_odinPreRelease = nullptr;
static OdinBoolMethod g_odinIsFullyLoaded = nullptr;

static uintptr_t g_moduleBase = 0;
static LONG g_odinEventLimit = 5000;
static bool g_odinTraceAllQueries = false;
static volatile LONG g_odinLoggedEvents = 0;
static volatile LONG g_reInstanceCalls = 0;
static volatile LONG g_removeQueries = 0;
static volatile LONG g_preReleaseCalls = 0;
static volatile LONG g_loadedQueries = 0;
static OdinSyncMethod g_odinSyncOriginal = nullptr;
static volatile LONG g_odinSyncCalls = 0;
static volatile LONG g_odinSyncMismatches = 0;
static volatile LONG g_markerCount = 0;
static volatile LONG g_markerThreadRun = 0;
static LONG g_fingerprintWindowMs = 1200;
static LONG g_fingerprintMaxRoots = 16;

struct RecentMismatch
{
    ULONGLONG tick{};
    void* self{};
    void* root{};
    unsigned caller{};
    int entriesCount{};
    uint16_t segmentCount{};
};

static constexpr size_t kRecentMismatchSlots = 256;
static RecentMismatch g_recentMismatches[kRecentMismatchSlots]{};
static volatile LONG g_recentMismatchWrite = 0;
static SRWLOCK g_recentMismatchLock = SRWLOCK_INIT;

struct OdinSnapshot
{
    uintptr_t segments{};
    uint16_t segmentCount{};
    uint8_t flag33{};
    uint8_t state35{};
    uint8_t state2A{};
    uintptr_t field38{};
    uintptr_t field3C{};
    uintptr_t field40{};
    uint8_t fullyLoaded44{};
};

static OdinSnapshot SnapshotOdin(void* self)
{
    auto* p = reinterpret_cast<uint8_t*>(self);
    OdinSnapshot s{};
    s.segments = *reinterpret_cast<uintptr_t*>(p + 0x14);
    s.segmentCount = *reinterpret_cast<uint16_t*>(p + 0x28);
    s.flag33 = *(p + 0x33);
    s.state35 = *(p + 0x35);
    s.state2A = *(p + 0x2A);
    s.field38 = *reinterpret_cast<uintptr_t*>(p + 0x38);
    s.field3C = *reinterpret_cast<uintptr_t*>(p + 0x3C);
    s.field40 = *reinterpret_cast<uintptr_t*>(p + 0x40);
    s.fullyLoaded44 = *(p + 0x44);
    return s;
}

static unsigned CallerRva(void* returnAddress)
{
    const uintptr_t caller = reinterpret_cast<uintptr_t>(returnAddress);
    if (caller >= g_moduleBase)
        return static_cast<unsigned>(caller - g_moduleBase);
    return 0;
}

static bool AllowOdinLog()
{
    const LONG n = InterlockedIncrement(&g_odinLoggedEvents);
    return n <= g_odinEventLimit;
}

struct OdinTrackedState
{
    void* key{};
    uint8_t removeValue{0xFF};
    uint8_t loadedValue{0xFF};
    uint32_t removeCalls{};
    uint32_t loadedCalls{};
};

static constexpr size_t kOdinStateSlots = 4096;
static OdinTrackedState g_odinStates[kOdinStateSlots]{};
static SRWLOCK g_odinStateLock = SRWLOCK_INIT;

static OdinTrackedState* FindOdinStateLocked(void* self, bool create)
{
    const uintptr_t key = reinterpret_cast<uintptr_t>(self);
    size_t index = ((key >> 4) ^ (key >> 13)) & (kOdinStateSlots - 1);
    OdinTrackedState* firstEmpty = nullptr;

    for (size_t probe = 0; probe < 32; ++probe)
    {
        OdinTrackedState& state = g_odinStates[(index + probe) & (kOdinStateSlots - 1)];
        if (state.key == self)
            return &state;
        if (!state.key && !firstEmpty)
            firstEmpty = &state;
    }

    if (create && firstEmpty)
    {
        firstEmpty->key = self;
        firstEmpty->removeValue = 0xFF;
        firstEmpty->loadedValue = 0xFF;
        firstEmpty->removeCalls = 0;
        firstEmpty->loadedCalls = 0;
        return firstEmpty;
    }
    return nullptr;
}

static void ForgetOdinState(void* self)
{
    AcquireSRWLockExclusive(&g_odinStateLock);
    OdinTrackedState* state = FindOdinStateLocked(self, false);
    if (state)
        *state = {};
    ReleaseSRWLockExclusive(&g_odinStateLock);
}

static void LogOdinSnapshot(const char* eventName, void* self, const OdinSnapshot& s, unsigned callerRva)
{
    if (!AllowOdinLog()) return;
    Log("[ODIN] %-14s this=%p callerRVA=0x%08X seg=%p count=%u state2A=%u state33=0x%02X state35=%u f38=%08X f3C=%08X f40=%08X loaded44=%u",
        eventName,
        self,
        callerRva,
        reinterpret_cast<void*>(s.segments),
        static_cast<unsigned>(s.segmentCount),
        static_cast<unsigned>(s.state2A),
        static_cast<unsigned>(s.flag33),
        static_cast<unsigned>(s.state35),
        static_cast<unsigned>(s.field38),
        static_cast<unsigned>(s.field3C),
        static_cast<unsigned>(s.field40),
        static_cast<unsigned>(s.fullyLoaded44));
}

static void __fastcall HookOdinReInstance(void* self, void*)
{
    InterlockedIncrement(&g_reInstanceCalls);
    const OdinSnapshot before = SnapshotOdin(self);
    const unsigned caller = CallerRva(_ReturnAddress());
    LogOdinSnapshot("ReInstance-pre", self, before, caller);

    g_odinReInstance(self);

    const OdinSnapshot after = SnapshotOdin(self);
    LogOdinSnapshot("ReInstance-post", self, after, caller);
}

static bool __fastcall HookOdinRemoveHighRes(void* self, void*)
{
    InterlockedIncrement(&g_removeQueries);
    const bool result = g_odinRemoveHighRes(self);
    const OdinSnapshot snap = SnapshotOdin(self);

    bool shouldLog = g_odinTraceAllQueries;
    uint32_t calls = 0;
    AcquireSRWLockExclusive(&g_odinStateLock);
    OdinTrackedState* state = FindOdinStateLocked(self, true);
    if (state)
    {
        ++state->removeCalls;
        calls = state->removeCalls;
        const uint8_t now = result ? 1u : 0u;
        if (state->removeValue == 0xFF || state->removeValue != now)
            shouldLog = true;
        state->removeValue = now;
    }
    ReleaseSRWLockExclusive(&g_odinStateLock);

    if (shouldLog && AllowOdinLog())
    {
        Log("[ODIN] RemoveHighRes? this=%p callerRVA=0x%08X result=%u queries=%u flag33=%u loaded44=%u seg=%p count=%u",
            self, CallerRva(_ReturnAddress()), result ? 1u : 0u, calls,
            static_cast<unsigned>(snap.flag33),
            static_cast<unsigned>(snap.fullyLoaded44),
            reinterpret_cast<void*>(snap.segments),
            static_cast<unsigned>(snap.segmentCount));
    }
    return result;
}

static bool __fastcall HookOdinIsFullyLoaded(void* self, void*)
{
    InterlockedIncrement(&g_loadedQueries);
    const bool result = g_odinIsFullyLoaded(self);
    const OdinSnapshot snap = SnapshotOdin(self);

    bool shouldLog = g_odinTraceAllQueries;
    uint32_t calls = 0;
    AcquireSRWLockExclusive(&g_odinStateLock);
    OdinTrackedState* state = FindOdinStateLocked(self, true);
    if (state)
    {
        ++state->loadedCalls;
        calls = state->loadedCalls;
        const uint8_t now = result ? 1u : 0u;
        if (state->loadedValue == 0xFF || state->loadedValue != now)
            shouldLog = true;
        state->loadedValue = now;
    }
    ReleaseSRWLockExclusive(&g_odinStateLock);

    if (shouldLog && AllowOdinLog())
    {
        Log("[ODIN] FullyLoaded?   this=%p callerRVA=0x%08X result=%u queries=%u flag33=%u loaded44=%u seg=%p count=%u",
            self, CallerRva(_ReturnAddress()), result ? 1u : 0u, calls,
            static_cast<unsigned>(snap.flag33),
            static_cast<unsigned>(snap.fullyLoaded44),
            reinterpret_cast<void*>(snap.segments),
            static_cast<unsigned>(snap.segmentCount));
    }
    return result;
}

static void __fastcall HookOdinPreRelease(void* self, void*)
{
    InterlockedIncrement(&g_preReleaseCalls);
    const OdinSnapshot before = SnapshotOdin(self);
    const unsigned caller = CallerRva(_ReturnAddress());
    LogOdinSnapshot("PreRelease", self, before, caller);
    ForgetOdinState(self);
    g_odinPreRelease(self);
}

static bool PatchVtableSlot(uintptr_t* slot, uintptr_t expected, void* hook, void** original, const char* label)
{
    if (*slot != expected)
    {
        Log("[FAIL] Odin %s vtable mismatch: expected=%p actual=%p.",
            label, reinterpret_cast<void*>(expected), reinterpret_cast<void*>(*slot));
        return false;
    }

    *original = reinterpret_cast<void*>(*slot);
    const uintptr_t replacement = reinterpret_cast<uintptr_t>(hook);
    if (!WriteBytes(slot, &replacement, sizeof(replacement)))
    {
        Log("[FAIL] Odin %s vtable write failed.", label);
        return false;
    }

    Log("[OK] Odin %s diagnostic hook installed.", label);
    return true;
}


static bool InstallAbsoluteDetour9(void* target, const uint8_t expected[9], void* hook, void** original)
{
    auto* at = reinterpret_cast<uint8_t*>(target);
    if (std::memcmp(at, expected, 9) != 0)
    {
        Log("[FAIL] Odin sync prologue mismatch.");
        return false;
    }

    auto* tramp = reinterpret_cast<uint8_t*>(
        VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!tramp)
    {
        Log("[FAIL] Odin sync trampoline allocation failed.");
        return false;
    }

    std::memcpy(tramp, at, 9);
    tramp[9] = 0x68;
    *reinterpret_cast<uint32_t*>(tramp + 10) =
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(at + 9));
    tramp[14] = 0xC3;
    FlushInstructionCache(GetCurrentProcess(), tramp, 15);

    uint8_t patch[9] = {0x68,0,0,0,0,0xC3,0x90,0x90,0x90};
    *reinterpret_cast<uint32_t*>(patch + 1) =
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(hook));

    if (!WriteBytes(at, patch, sizeof(patch)))
    {
        VirtualFree(tramp, 0, MEM_RELEASE);
        Log("[FAIL] Odin sync detour write failed.");
        return false;
    }

    *original = tramp;
    return true;
}

static const char* OdinSyncCallerLabel(unsigned rva)
{
    switch (rva)
    {
        case 0x0026BA4F: return "batch-A";
        case 0x0026BACD: return "batch-B";
        case 0x0026C374: return "batch-C";
        default: return "unknown";
    }
}


static bool IsReadablePointer(const void* p, size_t bytes = 1)
{
    if (!p || bytes == 0) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    const DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                           PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    if (!(mbi.Protect & readable)) return false;
    const uintptr_t begin = reinterpret_cast<uintptr_t>(p);
    const uintptr_t end = begin + bytes;
    const uintptr_t regionEnd =
        reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    return end >= begin && end <= regionEnd;
}

static bool SafeCopy(const void* src, void* dst, size_t size)
{
    if (!IsReadablePointer(src, size)) return false;
    __try
    {
        std::memcpy(dst, src, size);
        return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool TryReadAscii(uintptr_t address, char* out, size_t outSize)
{
    if (!address || !out || outSize < 5) return false;
    size_t n = 0;
    __try
    {
        while (n + 1 < outSize)
        {
            const unsigned char ch = *reinterpret_cast<const unsigned char*>(address + n);
            if (ch == 0) break;
            if (ch < 0x20 || ch >= 0x7F) return false;
            out[n++] = static_cast<char>(ch);
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    if (n < 4) return false;
    out[n] = '\0';
    return true;
}

static uint32_t Fnv1a32(const void* data, size_t size)
{
    const auto* p = reinterpret_cast<const uint8_t*>(data);
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < size; ++i)
    {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

static uint32_t StableRootFingerprint(const uint32_t* dwords, size_t count)
{
    uint32_t normalized[48]{};
    if (count > 48) count = 48;
    for (size_t i = 0; i < count; ++i)
    {
        const uintptr_t value = dwords[i];
        normalized[i] = IsReadablePointer(reinterpret_cast<void*>(value))
            ? 0x50545200u | static_cast<uint32_t>(i)
            : static_cast<uint32_t>(value);
    }
    return Fnv1a32(normalized, count * sizeof(uint32_t));
}

static void RecordRecentMismatch(void* self, void* root, unsigned caller, int count, uint16_t segCount)
{
    AcquireSRWLockExclusive(&g_recentMismatchLock);
    const LONG index = g_recentMismatchWrite++;
    RecentMismatch& slot = g_recentMismatches[
        static_cast<size_t>(index) & (kRecentMismatchSlots - 1)];
    slot.tick = GetTickCount64();
    slot.self = self;
    slot.root = root;
    slot.caller = caller;
    slot.entriesCount = count;
    slot.segmentCount = segCount;
    ReleaseSRWLockExclusive(&g_recentMismatchLock);
}

static void DumpRootFingerprint(void* root, void* self, unsigned caller, int entriesCount, uint16_t segCount)
{
    if (!root)
        return;

    uint32_t dw[48]{};
    if (!SafeCopy(root, dw, sizeof(dw)))
    {
        Log("[FPRINT] root=%p unreadable", root);
        return;
    }

    const uint32_t rawHash = Fnv1a32(dw, sizeof(dw));
    const uint32_t stableHash = StableRootFingerprint(dw, 48);
    const uintptr_t rootVtable = dw[0];

    Log("[FPRINT] root=%p rootVT=%p stable=%08X raw=%08X lastThis=%p caller=0x%08X(%s) n=%d seg=%u",
        root, reinterpret_cast<void*>(rootVtable), stableHash, rawHash, self,
        caller, OdinSyncCallerLabel(caller), entriesCount, static_cast<unsigned>(segCount));

    for (int row = 0; row < 6; ++row)
    {
        const int i = row * 8;
        Log("[FPRINT-DW] root=%p +%02X: %08X %08X %08X %08X %08X %08X %08X %08X",
            root, i * 4,
            dw[i+0],dw[i+1],dw[i+2],dw[i+3],dw[i+4],dw[i+5],dw[i+6],dw[i+7]);
    }

    // Probe direct pointers in the first 0xC0 bytes. Mesh/resource objects often
    // keep an asset name either directly or near the pointed object's header.
    static const int probeOffsets[] = {0,4,8,0x10,0x14,0x20};
    char ascii[96]{};
    int stringHits = 0;
    for (int i = 0; i < 48 && stringHits < 12; ++i)
    {
        const uintptr_t ptr = dw[i];
        if (!IsReadablePointer(reinterpret_cast<void*>(ptr), 4))
            continue;

        uint32_t magic = 0;
        SafeCopy(reinterpret_cast<void*>(ptr), &magic, sizeof(magic));
        if (magic == 0x4D534841u) // "AHSM" little-endian bytes
        {
            if (TryReadAscii(ptr + 0x14, ascii, sizeof(ascii)))
            {
                Log("[FPRINT-NAME] root=%p field=+0x%02X AHSM name=\"%s\"",
                    root, i * 4, ascii);
                ++stringHits;
            }
        }

        for (int off : probeOffsets)
        {
            if (TryReadAscii(ptr + static_cast<uintptr_t>(off), ascii, sizeof(ascii)))
            {
                Log("[FPRINT-STR] root=%p field=+0x%02X ptr=%p probe=+0x%02X \"%s\"",
                    root, i * 4, reinterpret_cast<void*>(ptr), off, ascii);
                ++stringHits;
                break;
            }
        }
    }
}

static void DumpRecentFingerprints(LONG markerNumber)
{
    RecentMismatch copy[kRecentMismatchSlots]{};
    LONG writeSnapshot = 0;

    AcquireSRWLockShared(&g_recentMismatchLock);
    std::memcpy(copy, g_recentMismatches, sizeof(copy));
    writeSnapshot = g_recentMismatchWrite;
    ReleaseSRWLockShared(&g_recentMismatchLock);

    const ULONGLONG now = GetTickCount64();
    void* seen[32]{};
    int seenCount = 0;

    Log("[FPRINT] BEGIN marker=%ld windowMs=%ld", markerNumber, g_fingerprintWindowMs);

    const LONG available = writeSnapshot < static_cast<LONG>(kRecentMismatchSlots)
        ? writeSnapshot : static_cast<LONG>(kRecentMismatchSlots);

    for (LONG back = 1; back <= available && seenCount < g_fingerprintMaxRoots; ++back)
    {
        const LONG logical = writeSnapshot - back;
        const RecentMismatch& e = copy[
            static_cast<size_t>(logical) & (kRecentMismatchSlots - 1)];

        if (!e.tick || now < e.tick) continue;
        if (now - e.tick > static_cast<ULONGLONG>(g_fingerprintWindowMs))
            break;

        bool duplicate = false;
        for (int i = 0; i < seenCount; ++i)
            if (seen[i] == e.root) { duplicate = true; break; }
        if (duplicate) continue;

        seen[seenCount++] = e.root;
        DumpRootFingerprint(e.root, e.self, e.caller, e.entriesCount, e.segmentCount);
    }

    Log("[FPRINT] END marker=%ld roots=%d", markerNumber, seenCount);
}

__declspec(noinline) static void __fastcall HookOdinSync(void* self, void*, void* entries, int count)
{
    InterlockedIncrement(&g_odinSyncCalls);

    const unsigned caller = CallerRva(_ReturnAddress());
    auto* vtable = *reinterpret_cast<uintptr_t**>(self);
    auto getRoot = reinterpret_cast<OdinGetRootMethod>(vtable[3]);
    void* root = getRoot ? getRoot(self) : nullptr;

    bool rootLoaded = false;
    uintptr_t rootVtable = 0;
    if (root)
    {
        auto* rootTable = *reinterpret_cast<uintptr_t**>(root);
        rootVtable = reinterpret_cast<uintptr_t>(rootTable);
        auto rootLoadedFn = reinterpret_cast<OdinBoolMethod>(rootTable[2]);
        if (rootLoadedFn)
            rootLoaded = rootLoadedFn(root);
    }

    auto* p = reinterpret_cast<uint8_t*>(self);
    const bool selfLoadedBefore = *(p + 0x44) != 0;
    const uint8_t state35Before = *(p + 0x35);
    const uint8_t state2ABefore = *(p + 0x2A);
    const uint16_t segmentCountBefore = *reinterpret_cast<uint16_t*>(p + 0x28);
    const bool mismatch = root && (rootLoaded != selfLoadedBefore);

    if (mismatch)
    {
        InterlockedIncrement(&g_odinSyncMismatches);
        RecordRecentMismatch(self, root, caller, count, segmentCountBefore);
        if (AllowOdinLog())
        {
            Log("[ODIN-SYNC] MISMATCH this=%p callerRVA=0x%08X(%s) entries=%p n=%d root=%p rootVT=%p rootLoaded=%u selfLoaded=%u state2A=%u state35=%u segCount=%u",
                self, caller, OdinSyncCallerLabel(caller), entries, count, root,
                reinterpret_cast<void*>(rootVtable), rootLoaded ? 1u : 0u,
                selfLoadedBefore ? 1u : 0u, static_cast<unsigned>(state2ABefore),
                static_cast<unsigned>(state35Before), static_cast<unsigned>(segmentCountBefore));
        }
    }

    g_odinSyncOriginal(self, entries, count);

    const bool selfLoadedAfter = *(p + 0x44) != 0;
    const uint8_t state35After = *(p + 0x35);
    const uint8_t state2AAfter = *(p + 0x2A);
    const uint16_t segmentCountAfter = *reinterpret_cast<uint16_t*>(p + 0x28);

    if (mismatch && AllowOdinLog())
    {
        Log("[ODIN-SYNC] AFTER    this=%p selfLoaded=%u state2A=%u state35=%u segCount=%u",
            self, selfLoadedAfter ? 1u : 0u, static_cast<unsigned>(state2AAfter),
            static_cast<unsigned>(state35After), static_cast<unsigned>(segmentCountAfter));
    }
}

static DWORD WINAPI DiagnosticMarkerThread(LPVOID)
{
    bool wasDown = false;
    while (InterlockedCompareExchange(&g_markerThreadRun, 0, 0) != 0)
    {
        const bool down = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
        if (down && !wasDown)
        {
            const LONG n = InterlockedIncrement(&g_markerCount);
            Log("[MARK] F9 #%ld", n);
            DumpRecentFingerprints(n);
        }
        wasDown = down;
        Sleep(20);
    }
    return 0;
}

static bool StartDiagnosticMarker()
{
    InterlockedExchange(&g_markerThreadRun, 1);
    HANDLE thread = CreateThread(nullptr, 0, DiagnosticMarkerThread, nullptr, 0, nullptr);
    if (!thread)
    {
        InterlockedExchange(&g_markerThreadRun, 0);
        Log("[FAIL] Could not start F9 diagnostic marker thread.");
        return false;
    }
    CloseHandle(thread);
    Log("[OK] F9 diagnostic marker active.");
    return true;
}

static bool InstallOdinDiagnostics(HMODULE exe)
{
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    constexpr uintptr_t kVtableRva = 0x00C81898;
    constexpr uintptr_t kSyncRva = 0x00A14610;

    auto* vtable = reinterpret_cast<uintptr_t*>(base + kVtableRva);
    const uintptr_t expectedTable[10] = {
        base + 0x00A14C10,
        base + 0x00A14BF0,
        base + 0x00A0C460,
        base + 0x00A14BE0,
        base + 0x00A14BB0,
        base + 0x00A14BC0,
        base + 0x00A14BA0,
        base + 0x00A14420,
        base + 0x00A143D0,
        base + 0x00A14BD0
    };

    for (size_t i = 0; i < 10; ++i)
    {
        if (vtable[i] != expectedTable[i])
        {
            Log("[FAIL] OdinMeshInstance vtable verification failed at slot %u: expected=%p actual=%p.",
                static_cast<unsigned>(i),
                reinterpret_cast<void*>(expectedTable[i]),
                reinterpret_cast<void*>(vtable[i]));
            return false;
        }
    }

    Log("[OK] OdinMeshInstance vtable verified at RVA 0x%08X.", static_cast<unsigned>(kVtableRva));

    static const uint8_t expectedSyncPrologue[9] = {
        0x83,0xEC,0x0C,0x56,0x57,0x8B,0x7C,0x24,0x1C
    };

    if (!InstallAbsoluteDetour9(
            reinterpret_cast<void*>(base + kSyncRva),
            expectedSyncPrologue,
            reinterpret_cast<void*>(&HookOdinSync),
            reinterpret_cast<void**>(&g_odinSyncOriginal)))
    {
        return false;
    }

    Log("[OK] Odin sync diagnostic detour installed at RVA 0x%08X.", static_cast<unsigned>(kSyncRva));
    return true;
}

static BOOL CALLBACK InitializeOnce(PINIT_ONCE, PVOID, PVOID*)
{
    HMODULE exe = GetModuleHandleW(nullptr);
    g_moduleBase = reinterpret_cast<uintptr_t>(exe);

    const std::wstring dir = ModuleDirectory(exe);
    const std::wstring logPath = dir + L"\\SaboteurEnhanced.log";
    _wfopen_s(&g_log, logPath.c_str(), L"w");

    Log("SaboteurEnhanced ASI 0.5 ODIN CHILD VISIBILITY A/B");
    Log("Architecture: validated Core 1 + targeted Odin child-visibility A/B");
    Log("Module base: 0x%08X", static_cast<unsigned>(g_moduleBase));

    const std::wstring iniPath = dir + L"\\SaboteurEnhanced.ini";
    const bool enableV310 = GetPrivateProfileIntW(L"Fixes", L"WSModelFullRenderMask", 1, iniPath.c_str()) != 0;
    const bool enableV311 = GetPrivateProfileIntW(L"Fixes", L"ModelInfoFullRenderSlice", 1, iniPath.c_str()) != 0;
    const bool enableOdinChildVisibility = GetPrivateProfileIntW(L"Fixes", L"OdinChildVisibilityGate", 1, iniPath.c_str()) != 0;
    const bool enableOdin = GetPrivateProfileIntW(L"Diagnostics", L"OdinInstancing", 0, iniPath.c_str()) != 0;
    g_odinTraceAllQueries = GetPrivateProfileIntW(L"Diagnostics", L"OdinTraceAllQueries", 0, iniPath.c_str()) != 0;
    g_odinEventLimit = GetPrivateProfileIntW(L"Diagnostics", L"OdinEventLimit", 5000, iniPath.c_str());
    if (g_odinEventLimit < 100) g_odinEventLimit = 100;
    if (g_odinEventLimit > 100000) g_odinEventLimit = 100000;
    g_fingerprintWindowMs = GetPrivateProfileIntW(L"Diagnostics", L"FingerprintWindowMs", 1200, iniPath.c_str());
    if (g_fingerprintWindowMs < 200) g_fingerprintWindowMs = 200;
    if (g_fingerprintWindowMs > 5000) g_fingerprintWindowMs = 5000;
    g_fingerprintMaxRoots = GetPrivateProfileIntW(L"Diagnostics", L"FingerprintMaxRoots", 16, iniPath.c_str());
    if (g_fingerprintMaxRoots < 1) g_fingerprintMaxRoots = 1;
    if (g_fingerprintMaxRoots > 32) g_fingerprintMaxRoots = 32;

    Log("INI: %ls", iniPath.c_str());
    Log("WSModelFullRenderMask=%d", enableV310 ? 1 : 0);
    Log("ModelInfoFullRenderSlice=%d", enableV311 ? 1 : 0);
    Log("OdinChildVisibilityGate=%d", enableOdinChildVisibility ? 1 : 0);
    Log("OdinInstancing=%d", enableOdin ? 1 : 0);
    Log("OdinTraceAllQueries=%d", g_odinTraceAllQueries ? 1 : 0);
    Log("OdinEventLimit=%ld", g_odinEventLimit);
    Log("FingerprintWindowMs=%ld", g_fingerprintWindowMs);
    Log("FingerprintMaxRoots=%ld", g_fingerprintMaxRoots);

    SectionRange text = GetSectionRange(exe, ".text");
    if (!text.begin)
    {
        Log("[FAIL] Could not locate executable .text section.");
        return TRUE;
    }

    Log(".text range: RVA 0x%08X, size 0x%08X",
        static_cast<unsigned>(reinterpret_cast<uintptr_t>(text.begin) - g_moduleBase),
        static_cast<unsigned>(text.size));

    if (enableV310) ApplyV310(text, g_moduleBase);
    else Log("[OFF] V310 WSModel fix disabled by INI.");

    if (enableV311) ApplyV311(text, g_moduleBase);
    else Log("[OFF] V311 ModelInfo fix disabled by INI.");

    if (enableOdinChildVisibility) ApplyOdinChildVisibilityGate(text, g_moduleBase);
    else Log("[OFF] Odin child-visibility A/B disabled by INI.");

    if (enableOdin)
    {
        if (InstallOdinDiagnostics(exe))
        {
            Log("[OK] Odin sync diagnostics active. No Odin rendering decision is modified.");
            StartDiagnosticMarker();
        }
        else
            Log("[FAIL] Odin diagnostics not installed completely.");
    }
    else
    {
        Log("[OFF] Odin diagnostics disabled by INI.");
    }

    Log("ASI initialization complete.");
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL __cdecl SaboteurEnhanced_Initialize()
{
    return InitOnceExecuteOnce(&g_initOnce, InitializeOnce, nullptr, nullptr);
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_self = module;
        DisableThreadLibraryCalls(module);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        if (g_log)
        {
            InterlockedExchange(&g_markerThreadRun, 0);
            Log("Odin summary: SyncCalls=%ld Mismatches=%ld F9Markers=%ld LoggedEvents=%ld",
                g_odinSyncCalls, g_odinSyncMismatches, g_markerCount, g_odinLoggedEvents);
            Log("ASI unload.");
            AcquireSRWLockExclusive(&g_logLock);
            std::fclose(g_log);
            g_log = nullptr;
            ReleaseSRWLockExclusive(&g_logLock);
        }
    }
    return TRUE;
}
