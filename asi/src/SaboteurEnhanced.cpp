#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <string>

static HMODULE g_self = nullptr;
static INIT_ONCE g_initOnce = INIT_ONCE_STATIC_INIT;
static FILE* g_log = nullptr;

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
    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::fprintf(g_log, "[%02u:%02u:%02u.%03u] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(g_log, fmt, ap);
    va_end(ap);
    std::fprintf(g_log, "\n");
    std::fflush(g_log);
}

struct TextRange
{
    uint8_t* begin{};
    size_t size{};
};

static TextRange GetTextRange(HMODULE module)
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
        if (std::strcmp(name, ".text") == 0)
        {
            size_t size = sec->Misc.VirtualSize ? sec->Misc.VirtualSize : sec->SizeOfRawData;
            return {base + sec->VirtualAddress, size};
        }
    }
    return {};
}

static uint8_t* FindExact(const TextRange& range, const uint8_t* pattern, size_t patternSize)
{
    if (!range.begin || !pattern || !patternSize || range.size < patternSize) return nullptr;
    for (size_t i = 0; i + patternSize <= range.size; ++i)
    {
        if (std::memcmp(range.begin + i, pattern, patternSize) == 0)
            return range.begin + i;
    }
    return nullptr;
}

static bool WriteBytes(void* address, const uint8_t* bytes, size_t size)
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

static bool ApplyV310(const TextRange& text, uintptr_t moduleBase)
{
    // Core1 / retail context around RAW 0x00238813.
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

static bool ApplyV311(const TextRange& text, uintptr_t moduleBase)
{
    // Core1 / retail context around RAW 0x002387AA.
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

static BOOL CALLBACK InitializeOnce(PINIT_ONCE, PVOID, PVOID*)
{
    const std::wstring dir = ModuleDirectory(GetModuleHandleW(nullptr));
    const std::wstring logPath = dir + L"\\SaboteurEnhanced.log";
    _wfopen_s(&g_log, logPath.c_str(), L"w");

    Log("SaboteurEnhanced ASI 0.1");
    Log("Architecture: clean Core EXE + runtime ASI fixes");
    Log("Module base: 0x%08X", static_cast<unsigned>(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr))));

    const std::wstring iniPath = dir + L"\\SaboteurEnhanced.ini";
    const bool enableV310 = GetPrivateProfileIntW(L"Fixes", L"WSModelFullRenderMask", 1, iniPath.c_str()) != 0;
    const bool enableV311 = GetPrivateProfileIntW(L"Fixes", L"ModelInfoFullRenderSlice", 1, iniPath.c_str()) != 0;

    Log("INI: %ls", iniPath.c_str());
    Log("WSModelFullRenderMask=%d", enableV310 ? 1 : 0);
    Log("ModelInfoFullRenderSlice=%d", enableV311 ? 1 : 0);

    HMODULE exe = GetModuleHandleW(nullptr);
    TextRange text = GetTextRange(exe);
    if (!text.begin)
    {
        Log("[FAIL] Could not locate executable .text section.");
        return TRUE;
    }

    Log(".text range: RVA 0x%08X, size 0x%08X",
        static_cast<unsigned>(reinterpret_cast<uintptr_t>(text.begin) - reinterpret_cast<uintptr_t>(exe)),
        static_cast<unsigned>(text.size));

    if (enableV310) ApplyV310(text, reinterpret_cast<uintptr_t>(exe));
    else Log("[OFF] V310 WSModel fix disabled by INI.");

    if (enableV311) ApplyV311(text, reinterpret_cast<uintptr_t>(exe));
    else Log("[OFF] V311 ModelInfo fix disabled by INI.");

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
            Log("ASI unload.");
            std::fclose(g_log);
            g_log = nullptr;
        }
    }
    return TRUE;
}
