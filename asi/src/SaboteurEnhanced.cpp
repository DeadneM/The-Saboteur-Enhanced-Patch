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

struct OdinSnapshot
{
    uintptr_t segments{};
    uint16_t segmentCount{};
    uint8_t flag33{};
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
    Log("[ODIN] %-14s this=%p callerRVA=0x%08X seg=%p count=%u flag33=%u f38=%08X f3C=%08X f40=%08X loaded44=%u",
        eventName,
        self,
        callerRva,
        reinterpret_cast<void*>(s.segments),
        static_cast<unsigned>(s.segmentCount),
        static_cast<unsigned>(s.flag33),
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

static bool InstallOdinDiagnostics(HMODULE exe)
{
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    constexpr uintptr_t kVtableRva = 0x00C81898;
    constexpr uintptr_t kReInstanceRva = 0x00A14BF0;
    constexpr uintptr_t kRemoveHighResRva = 0x00A14BA0;
    constexpr uintptr_t kPreReleaseRva = 0x00A143D0;
    constexpr uintptr_t kIsFullyLoadedRva = 0x00A14BD0;

    auto* vtable = reinterpret_cast<uintptr_t*>(base + kVtableRva);

    // Verify the complete recovered 10-slot table before touching it.
    const uintptr_t expectedTable[10] = {
        base + 0x00A14C10,
        base + kReInstanceRva,
        base + 0x00A0C460,
        base + 0x00A14BE0,
        base + 0x00A14BB0,
        base + 0x00A14BC0,
        base + kRemoveHighResRva,
        base + 0x00A14420,
        base + kPreReleaseRva,
        base + kIsFullyLoadedRva
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

    bool ok = true;
    ok &= PatchVtableSlot(&vtable[1], expectedTable[1], reinterpret_cast<void*>(&HookOdinReInstance),
                          reinterpret_cast<void**>(&g_odinReInstance), "ReInstance");
    ok &= PatchVtableSlot(&vtable[6], expectedTable[6], reinterpret_cast<void*>(&HookOdinRemoveHighRes),
                          reinterpret_cast<void**>(&g_odinRemoveHighRes), "RemoveHighResSegments/query");
    ok &= PatchVtableSlot(&vtable[8], expectedTable[8], reinterpret_cast<void*>(&HookOdinPreRelease),
                          reinterpret_cast<void**>(&g_odinPreRelease), "PreRelease");
    ok &= PatchVtableSlot(&vtable[9], expectedTable[9], reinterpret_cast<void*>(&HookOdinIsFullyLoaded),
                          reinterpret_cast<void**>(&g_odinIsFullyLoaded), "IsFullyLoaded");

    return ok;
}

static BOOL CALLBACK InitializeOnce(PINIT_ONCE, PVOID, PVOID*)
{
    HMODULE exe = GetModuleHandleW(nullptr);
    g_moduleBase = reinterpret_cast<uintptr_t>(exe);

    const std::wstring dir = ModuleDirectory(exe);
    const std::wstring logPath = dir + L"\\SaboteurEnhanced.log";
    _wfopen_s(&g_log, logPath.c_str(), L"w");

    Log("SaboteurEnhanced ASI 0.2 ODIN DIAGNOSTIC");
    Log("Architecture: validated Core 1 + runtime ASI fixes + pass-through Odin tracing");
    Log("Module base: 0x%08X", static_cast<unsigned>(g_moduleBase));

    const std::wstring iniPath = dir + L"\\SaboteurEnhanced.ini";
    const bool enableV310 = GetPrivateProfileIntW(L"Fixes", L"WSModelFullRenderMask", 1, iniPath.c_str()) != 0;
    const bool enableV311 = GetPrivateProfileIntW(L"Fixes", L"ModelInfoFullRenderSlice", 1, iniPath.c_str()) != 0;
    const bool enableOdin = GetPrivateProfileIntW(L"Diagnostics", L"OdinInstancing", 1, iniPath.c_str()) != 0;
    g_odinTraceAllQueries = GetPrivateProfileIntW(L"Diagnostics", L"OdinTraceAllQueries", 0, iniPath.c_str()) != 0;
    g_odinEventLimit = GetPrivateProfileIntW(L"Diagnostics", L"OdinEventLimit", 5000, iniPath.c_str());
    if (g_odinEventLimit < 100) g_odinEventLimit = 100;
    if (g_odinEventLimit > 100000) g_odinEventLimit = 100000;

    Log("INI: %ls", iniPath.c_str());
    Log("WSModelFullRenderMask=%d", enableV310 ? 1 : 0);
    Log("ModelInfoFullRenderSlice=%d", enableV311 ? 1 : 0);
    Log("OdinInstancing=%d", enableOdin ? 1 : 0);
    Log("OdinTraceAllQueries=%d", g_odinTraceAllQueries ? 1 : 0);
    Log("OdinEventLimit=%ld", g_odinEventLimit);

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

    if (enableOdin)
    {
        if (InstallOdinDiagnostics(exe))
            Log("[OK] Odin pass-through diagnostics active. No Odin rendering decision is modified.");
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
            Log("Odin summary: ReInstance=%ld RemoveHighResQueries=%ld PreRelease=%ld FullyLoadedQueries=%ld LoggedEvents=%ld",
                g_reInstanceCalls, g_removeQueries, g_preReleaseCalls, g_loadedQueries, g_odinLoggedEvents);
            Log("ASI unload.");
            AcquireSRWLockExclusive(&g_logLock);
            std::fclose(g_log);
            g_log = nullptr;
            ReleaseSRWLockExclusive(&g_logLock);
        }
    }
    return TRUE;
}
