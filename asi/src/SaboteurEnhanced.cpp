#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <cmath>
#include <string>

#pragma intrinsic(_ReturnAddress)

static HMODULE g_self = nullptr;
static uintptr_t g_moduleBase = 0;
static INIT_ONCE g_initOnce = INIT_ONCE_STATIC_INIT;
static FILE* g_log = nullptr;
static SRWLOCK g_logLock = SRWLOCK_INIT;

static float g_shadowCasterMinLodDistance = 180.0f;
static uintptr_t g_shadowCasterReturn = 0;
static float g_particleLodMinDistance = 100.0f;
static volatile LONG g_redAuditMode = 0; // 0=native, 1=suppress, 2=verbose native
static volatile LONG g_redAuditThreadRun = 0;
static volatile LONG g_redAuditEventCount = 0;
static volatile LONG g_redAuditLastEbx = 0;
static volatile LONG g_redAuditLastOwner = 0;
static volatile LONG g_redAuditLastTemplate = 0;
static uintptr_t g_redFallbackContinue = 0;
static uintptr_t g_redFallbackExit = 0;


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



static bool WriteRel32Call6(void* target, const uint8_t expected[6], void* hook, const char* label)
{
    auto* at = reinterpret_cast<uint8_t*>(target);
    if (std::memcmp(at, expected, 6) != 0)
    {
        Log("[SKIP] %s instruction bytes mismatch.", label);
        return false;
    }

    const intptr_t delta =
        reinterpret_cast<intptr_t>(hook) - (reinterpret_cast<intptr_t>(at) + 5);
    if (delta < INT32_MIN || delta > INT32_MAX)
    {
        Log("[FAIL] %s hook is outside rel32 range.", label);
        return false;
    }

    uint8_t patch[6] = {0xE8,0,0,0,0,0x90};
    const int32_t rel = static_cast<int32_t>(delta);
    std::memcpy(patch + 1, &rel, sizeof(rel));
    if (!WriteBytes(at, patch, sizeof(patch)))
    {
        Log("[FAIL] %s CALL patch write failed.", label);
        return false;
    }

    Log("[OK] %s CALL hook installed.", label);
    return true;
}

static bool WriteRel32Jmp7(void* target, const uint8_t expected[7], void* hook, const char* label)
{
    auto* at = reinterpret_cast<uint8_t*>(target);
    if (std::memcmp(at, expected, 7) != 0)
    {
        Log("[SKIP] %s instruction bytes mismatch.", label);
        return false;
    }

    const intptr_t delta =
        reinterpret_cast<intptr_t>(hook) - (reinterpret_cast<intptr_t>(at) + 5);
    if (delta < INT32_MIN || delta > INT32_MAX)
    {
        Log("[FAIL] %s hook is outside rel32 range.", label);
        return false;
    }

    uint8_t patch[7] = {0xE9,0,0,0,0,0x90,0x90};
    const int32_t rel = static_cast<int32_t>(delta);
    std::memcpy(patch + 1, &rel, sizeof(rel));
    if (!WriteBytes(at, patch, sizeof(patch)))
    {
        Log("[FAIL] %s JMP patch write failed.", label);
        return false;
    }

    Log("[OK] %s JMP hook installed.", label);
    return true;
}

__declspec(naked) static void ShadowCasterMinLodHook()
{
    __asm
    {
        test ecx, ecx
        jz original_mask

        push eax
        mov eax, dword ptr [edi+0A4h]
        cmp eax, dword ptr [g_shadowCasterMinLodDistance]
        jae shadow_keep
        mov eax, dword ptr [g_shadowCasterMinLodDistance]
        mov dword ptr [edi+0A4h], eax
shadow_keep:
        pop eax

original_mask:
        mov ebx, 1
        shl ebx, cl
        jmp dword ptr [g_shadowCasterReturn]
    }
}

__declspec(naked) static void ParticleLodMinHook()
{
    __asm
    {
        pushfd
        push ecx
        mov ecx, dword ptr [eax+1D8h]
        cmp ecx, dword ptr [g_particleLodMinDistance]
        jae particle_native

        pop ecx
        popfd
        fld dword ptr [g_particleLodMinDistance]
        ret

particle_native:
        pop ecx
        popfd
        fld dword ptr [eax+1D8h]
        ret
    }
}


static uintptr_t g_wsPhGridInitOriginal = 0;

__declspec(naked) static void WSPhGridInitWrapper()
{
    __asm
    {
        // Preserve the caller's original three cdecl arguments while calling
        // the retail generic pool initializer ourselves.
        mov eax, dword ptr [esp+4]
        mov ecx, dword ptr [esp+8]
        mov edx, dword ptr [esp+0Ch]
        push edx
        push ecx
        push eax
        call dword ptr [g_wsPhGridInitOriginal]
        add esp, 0Ch

        // Historical V264 invariant: only WSPhGridObject receives 2000.
        // The adjacent WSHKCreationDataContainer must continue to see EDI=1000.
        mov edi, 03E8h
        ret
    }
}

static float ReadIniFloat(const std::wstring& path, const wchar_t* section, const wchar_t* key, float fallback)
{
    wchar_t fallbackText[64] = {};
    wchar_t valueText[64] = {};
    swprintf_s(fallbackText, L"%.9g", static_cast<double>(fallback));
    GetPrivateProfileStringW(section, key, fallbackText, valueText, static_cast<DWORD>(_countof(valueText)), path.c_str());

    wchar_t* end = nullptr;
    const double value = std::wcstod(valueText, &end);
    if (end == valueText || !std::isfinite(value))
        return fallback;
    return static_cast<float>(value);
}

static bool VerifyScalarBytes(const void* address, const void* expected, size_t size)
{
    return std::memcmp(address, expected, size) == 0;
}

static bool ApplyFloatGroup(HMODULE exe, const char* label, const uintptr_t* rvas,
                            const float* expected, size_t count, const float* values)
{
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    for (size_t i = 0; i < count; ++i)
    {
        const void* address = reinterpret_cast<const void*>(base + rvas[i]);
        if (!VerifyScalarBytes(address, &expected[i], sizeof(float)))
        {
            Log("[SKIP] %s native value mismatch at RVA 0x%08X.", label, static_cast<unsigned>(rvas[i]));
            return false;
        }
    }

    for (size_t i = 0; i < count; ++i)
    {
        void* address = reinterpret_cast<void*>(base + rvas[i]);
        if (!WriteBytes(address, &values[i], sizeof(float)))
        {
            Log("[FAIL] %s write failed at RVA 0x%08X.", label, static_cast<unsigned>(rvas[i]));
            return false;
        }
    }

    Log("[OK] %s applied.", label);
    return true;
}

struct ExactBytePatch
{
    uintptr_t rva;
    uint8_t expected;
    uint8_t patch;
};

static bool ApplyExactBytePatchSet(HMODULE exe, const char* label,
                                   const ExactBytePatch* patches, size_t count)
{
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    for (size_t i = 0; i < count; ++i)
    {
        const auto* address = reinterpret_cast<const uint8_t*>(base + patches[i].rva);
        if (*address != patches[i].expected)
        {
            Log("[SKIP] %s byte mismatch at RVA 0x%08X: expected %02X, got %02X.",
                label, static_cast<unsigned>(patches[i].rva),
                static_cast<unsigned>(patches[i].expected),
                static_cast<unsigned>(*address));
            return false;
        }
    }

    for (size_t i = 0; i < count; ++i)
    {
        auto* address = reinterpret_cast<uint8_t*>(base + patches[i].rva);
        if (!WriteBytes(address, &patches[i].patch, 1))
        {
            Log("[FAIL] %s write failed at RVA 0x%08X.", label, static_cast<unsigned>(patches[i].rva));
            return false;
        }
    }

    Log("[OK] %s applied.", label);
    return true;
}

static bool ApplyEnvironmentMapResolution(HMODULE exe, int resolution)
{
    if (resolution < 128 || resolution > 4096)
    {
        Log("[FAIL] EnvironmentMapResolution=%d outside 128..4096.", resolution);
        return false;
    }
    const uintptr_t rvas[] = {0x00DB6EA0, 0x00DB6EA4};
    const float expected[] = {128.0f, 128.0f};
    const float value = static_cast<float>(resolution);
    const float values[] = {value, value};
    return ApplyFloatGroup(exe, "Environment maps", rvas, expected, 2, values);
}

static bool ApplyShadowMapResolution(HMODULE exe, int resolution)
{
    if (resolution < 1024 || resolution > 8192)
    {
        Log("[FAIL] ShadowMapResolution=%d outside 1024..8192.", resolution);
        return false;
    }

    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    const uintptr_t sizeRvas[] = {0x00DD61B4, 0x00DD61B8};
    const float nativeSize[] = {1024.0f, 1024.0f};

    // Embedded PCF sampling offsets are authored for a 1024 shadow map.
    // Keep their screen-space sampling radius coherent with the selected
    // resolution instead of treating them as a PCF-kernel selector.
    const uintptr_t offsetRvas[] = {
        0x00D7C278,0x00D7C280,
        0x00D7CA0C,0x00D7CA14,
        0x00D7D308,0x00D7D310,0x00D7D31C,0x00D7D320,
        0x00D7D334,0x00D7D338,0x00D7D364,0x00D7D368,
        0x00D7D37C,0x00D7D380,
        0x00D7DF0C,0x00D7DF14,0x00D7DF20,0x00D7DF24,
        0x00D7DF38,0x00D7DF3C,0x00D7DF68,0x00D7DF6C,
        0x00D7DF80,0x00D7DF84
    };
    const float nativeOffsets[] = {
        -0.00048828125f,  0.00048828125f,
        -0.00048828125f,  0.00048828125f,
        -0.00146484375f, -0.00048828125f,  0.00048828125f, -0.00146484375f,
         0.00146484375f, -0.00146484375f,  0.00048828125f, -0.00048828125f,
         0.00146484375f, -0.00048828125f,
        -0.00146484375f, -0.00048828125f,  0.00048828125f, -0.00146484375f,
         0.00146484375f, -0.00146484375f,  0.00048828125f, -0.00048828125f,
         0.00146484375f, -0.00048828125f
    };
    static_assert(_countof(offsetRvas) == _countof(nativeOffsets), "shadow offset table mismatch");

    // Atomic verification: do not touch the map size if any embedded shader
    // constant is not the exact retail value.
    for (size_t i = 0; i < _countof(sizeRvas); ++i)
    {
        if (!VerifyScalarBytes(reinterpret_cast<void*>(base + sizeRvas[i]),
                               &nativeSize[i], sizeof(float)))
        {
            Log("[SKIP] Shadow-map size retail signature mismatch; nothing written.");
            return false;
        }
    }
    for (size_t i = 0; i < _countof(offsetRvas); ++i)
    {
        if (!VerifyScalarBytes(reinterpret_cast<void*>(base + offsetRvas[i]),
                               &nativeOffsets[i], sizeof(float)))
        {
            Log("[SKIP] Shadow PCF texel-offset retail signature mismatch at RVA 0x%08X; nothing written.",
                static_cast<unsigned>(offsetRvas[i]));
            return false;
        }
    }

    const float size = static_cast<float>(resolution);
    const float texelScale = 1024.0f / size;

    for (uintptr_t rva : sizeRvas)
        if (!WriteBytes(reinterpret_cast<void*>(base + rva), &size, sizeof(size)))
            return false;

    for (size_t i = 0; i < _countof(offsetRvas); ++i)
    {
        const float value = nativeOffsets[i] * texelScale;
        if (!WriteBytes(reinterpret_cast<void*>(base + offsetRvas[i]), &value, sizeof(value)))
            return false;
    }

    Log("[OK] ShadowMapResolution=%d with PCF texel offsets scaled by %.6f.", resolution, texelScale);
    return true;
}

static bool ApplyAnisotropicFiltering(HMODULE exe, int level)
{
    if (level < 1 || level > 16)
    {
        Log("[FAIL] AnisotropicFiltering=%d outside 1..16.", level);
        return false;
    }

    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    const uintptr_t filterRvas[] = {0x00DD624C, 0x00DD629C};
    const uintptr_t anisoRvas[] = {0x00DD626C, 0x00DD62BC};

    for (uintptr_t rva : filterRvas)
    {
        const uint8_t native = *reinterpret_cast<const uint8_t*>(base + rva);
        if (native != 0x02)
        {
            Log("[SKIP] Anisotropic filter-mode mismatch at RVA 0x%08X.", static_cast<unsigned>(rva));
            return false;
        }
    }
    for (uintptr_t rva : anisoRvas)
    {
        const uint8_t native = *reinterpret_cast<const uint8_t*>(base + rva);
        if (native != 0x04)
        {
            Log("[SKIP] Max-anisotropy mismatch at RVA 0x%08X.", static_cast<unsigned>(rva));
            return false;
        }
    }

    if (level > 4)
    {
        const uint8_t anisotropicMode = 0x03;
        const uint8_t maxAniso = static_cast<uint8_t>(level);
        for (uintptr_t rva : filterRvas)
            if (!WriteBytes(reinterpret_cast<void*>(base + rva), &anisotropicMode, 1)) return false;
        for (uintptr_t rva : anisoRvas)
            if (!WriteBytes(reinterpret_cast<void*>(base + rva), &maxAniso, 1)) return false;
    }

    Log("[OK] AnisotropicFiltering=%d applied.", level);
    return true;
}

static bool ApplyMipLodBias(HMODULE exe, float bias)
{
    if (bias < -2.0f || bias > 2.0f)
    {
        Log("[FAIL] MipLODBias=%.3f outside -2..2.", bias);
        return false;
    }
    const uintptr_t rvas[] = {0x00DD625C, 0x00DD62AC};
    const float expected[] = {0.0f, 0.0f};
    const float values[] = {bias, bias};
    return ApplyFloatGroup(exe, "MIP LOD bias", rvas, expected, 2, values);
}

static bool ApplyToneMap(HMODULE exe, float value)
{
    if (value < 0.01f || value > 2.0f)
    {
        Log("[FAIL] ToneMap=%.4f outside safe range.", value);
        return false;
    }
    const uintptr_t rvas[] = {0x00D6487C};
    const float expected[] = {0.25f};
    const float values[] = {value};
    return ApplyFloatGroup(exe, "ToneMap", rvas, expected, 1, values);
}

static bool ApplyCsmLambda(HMODULE exe, float value)
{
    if (value < 0.0f || value > 1.0f)
    {
        Log("[FAIL] CSMLambda=%.4f outside 0..1.", value);
        return false;
    }
    const uintptr_t rvas[] = {0x00D20C04};
    const float expected[] = {0.5f};
    const float values[] = {value};
    return ApplyFloatGroup(exe, "CSM lambda", rvas, expected, 1, values);
}

static bool ApplyCsmFarDistance(HMODULE exe, float farDistance)
{
    if (farDistance < 10.0f || farDistance > 1000.0f)
    {
        Log("[FAIL] CSMFarDistance=%.3f outside 10..1000.", farDistance);
        return false;
    }

    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    auto* address = reinterpret_cast<double*>(base + 0x00BC1FF0);
    uint64_t nativeBits = 0;
    std::memcpy(&nativeBits, address, sizeof(nativeBits));
    constexpr uint64_t expectedBits = 0x4058F99999980000ULL; // historical private ~99.9
    if (nativeBits != expectedBits)
    {
        Log("[SKIP] CSM far private constant mismatch at RVA 0x00BC1FF0.");
        return false;
    }

    const double value = static_cast<double>(farDistance) - 0.1;
    if (!WriteBytes(address, &value, sizeof(value)))
        return false;
    Log("[OK] CSMFarDistance=%.3f applied through private far-minus-0.1 constant.", farDistance);
    return true;
}

static bool ApplyFullResolutionAo(HMODULE exe)
{
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);

    // Retail AmbientOcclusionBB is created at half resolution.
    // V200's coherent full-resolution path removes BOTH CPU /2 shifts,
    // changes PsAmbientOcclusion scale 2.0 -> 1.0, and rewrites the linked
    // PsDepthConv instruction operands. All five owners are verified before
    // the first byte is written.
    auto* widthShift  = reinterpret_cast<uint8_t*>(base + 0x003D0E8C);
    auto* heightShift = reinterpret_cast<uint8_t*>(base + 0x003D0E90);
    auto* aoScale     = reinterpret_cast<float*>(base + 0x00D5FE30);
    auto* depthOp     = reinterpret_cast<uint8_t*>(base + 0x00D60F6C);
    auto* depthSrc    = reinterpret_cast<uint8_t*>(base + 0x00D60F78);
    auto* depthSwz    = reinterpret_cast<uint8_t*>(base + 0x00D60F7A);

    const uint8_t expectedWidth[3]  = {0x66,0xD1,0xE9}; // shr cx,1
    const uint8_t expectedHeight[3] = {0x66,0xD1,0xED}; // shr bp,1
    const float expectedScale = 2.0f;
    const uint8_t expectedDepthOp = 0x02;
    const uint8_t expectedDepthSrc = 0x00;
    const uint8_t expectedDepthSwz[2] = {0xE4,0x80};

    if (std::memcmp(widthShift, expectedWidth, sizeof(expectedWidth)) != 0 ||
        std::memcmp(heightShift, expectedHeight, sizeof(expectedHeight)) != 0 ||
        !VerifyScalarBytes(aoScale, &expectedScale, sizeof(expectedScale)) ||
        *depthOp != expectedDepthOp ||
        *depthSrc != expectedDepthSrc ||
        std::memcmp(depthSwz, expectedDepthSwz, sizeof(expectedDepthSwz)) != 0)
    {
        Log("[SKIP] Full-resolution AO atomic retail signature mismatch; nothing written.");
        return false;
    }

    const uint8_t nops[3] = {0x90,0x90,0x90};
    const float fullResScale = 1.0f;
    const uint8_t newDepthOp = 0x05;
    const uint8_t newDepthSrc = 0x01;
    const uint8_t newDepthSwz[2] = {0xFF,0xA0};

    if (!WriteBytes(widthShift, nops, sizeof(nops)) ||
        !WriteBytes(heightShift, nops, sizeof(nops)) ||
        !WriteBytes(aoScale, &fullResScale, sizeof(fullResScale)) ||
        !WriteBytes(depthOp, &newDepthOp, 1) ||
        !WriteBytes(depthSrc, &newDepthSrc, 1) ||
        !WriteBytes(depthSwz, newDepthSwz, sizeof(newDepthSwz)))
    {
        Log("[FAIL] Full-resolution AO coherent write failed.");
        return false;
    }

    Log("[OK] Full-resolution AO applied coherently: RT /2 removed + PsAmbientOcclusion/PsDepthConv matched.");
    return true;
}

static bool ApplyAoBlurScale(HMODULE exe, float value)
{
    if (value < 0.5f || value > 3.0f) return false;
    const uintptr_t rvas[] = {0x00D640C4};
    const float expected[] = {2.0f};
    const float values[] = {value};
    return ApplyFloatGroup(exe, "AO blur scale", rvas, expected, 1, values);
}

static bool ApplyAoErodeScale(HMODULE exe, float value)
{
    if (value < 0.5f || value > 3.0f) return false;
    const uintptr_t rvas[] = {0x00D644E8};
    const float expected[] = {2.0f};
    const float values[] = {value};
    return ApplyFloatGroup(exe, "AO erode scale", rvas, expected, 1, values);
}

static bool ApplyShadowPcf5x5(HMODULE exe)
{
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);

    // Retail selector branch:
    //   [esi+edi*4+0xF4] -> SampleShadowMapPCF3x3 family A
    //   [esi+edi*4+0xE8] -> SampleShadowMapPCF3x3 family B
    // V200 redirects those two consumers to the verified 5x5 families:
    //   +0x10C and +0x100 respectively.
    auto* selectorA = reinterpret_cast<uint8_t*>(base + 0x003C308D);
    auto* selectorB = reinterpret_cast<uint8_t*>(base + 0x003C3096);

    const uint8_t expectedA[7] = {0x8B,0x84,0xBE,0xF4,0x00,0x00,0x00};
    const uint8_t expectedB[7] = {0x8B,0x84,0xBE,0xE8,0x00,0x00,0x00};
    const uint8_t patchA[7]    = {0x8B,0x84,0xBE,0x0C,0x01,0x00,0x00};
    const uint8_t patchB[7]    = {0x8B,0x84,0xBE,0x00,0x01,0x00,0x00};

    if (std::memcmp(selectorA, expectedA, sizeof(expectedA)) != 0 ||
        std::memcmp(selectorB, expectedB, sizeof(expectedB)) != 0)
    {
        Log("[SKIP] True PCF 3x3 selector signatures mismatch; nothing written.");
        return false;
    }

    if (!WriteBytes(selectorA, patchA, sizeof(patchA)) ||
        !WriteBytes(selectorB, patchB, sizeof(patchB)))
    {
        Log("[FAIL] PCF 5x5 selector write failed.");
        return false;
    }

    Log("[OK] Shadow PCF selector redirected from verified 3x3 families to 5x5 families.");
    return true;
}

static bool ApplyShadowBiasScales(HMODULE exe, float depthScale, float slopeScale)
{
    if (depthScale < 0.1f || depthScale > 2.0f ||
        slopeScale < 0.1f || slopeScale > 2.0f)
    {
        Log("[FAIL] Shadow bias scales outside 0.1..2.");
        return false;
    }

    const uintptr_t depthRvas[] = {
        0x00DD99AC,0x00DD99B0,0x00DD99B4,0x00DD99B8,0x00DD99BC
    };
    const float depthExpected[] = {-500.0f,-3500.0f,-4000.0f,-5000.0f,-5000.0f};
    float depthValues[_countof(depthRvas)] = {};
    for (size_t i = 0; i < _countof(depthRvas); ++i)
        depthValues[i] = depthExpected[i] * depthScale;

    const uintptr_t slopeRvas[] = {
        0x00DD99C0,0x00DD99C4,0x00DD99C8,0x00DD99CC,0x00DD99D0
    };
    const float slopeExpected[] = {-1.8f,-1.8f,-2.2f,-3.0f,-13.0f};
    float slopeValues[_countof(slopeRvas)] = {};
    for (size_t i = 0; i < _countof(slopeRvas); ++i)
        slopeValues[i] = slopeExpected[i] * slopeScale;

    if (!ApplyFloatGroup(exe, "Shadow depth-bias table", depthRvas, depthExpected,
                         _countof(depthRvas), depthValues))
        return false;
    if (!ApplyFloatGroup(exe, "Shadow slope-bias table", slopeRvas, slopeExpected,
                         _countof(slopeRvas), slopeValues))
        return false;

    Log("[OK] Shadow bias scales depth=%.3f slope=%.3f.", depthScale, slopeScale);
    return true;
}

static bool ApplyStreamCoverage(HMODULE exe, float low, float medium, float high)
{
    if (low < 1000.0f || medium < 100.0f || high < 100.0f)
    {
        Log("[FAIL] StreamCoverage values too small.");
        return false;
    }
    const uintptr_t rvas[] = {0x00C4B044,0x00C4B048,0x00C4B04C};
    const float expected[] = {1500.0f,300.0f,250.0f};
    const float values[] = {low,medium,high};
    return ApplyFloatGroup(exe, "Streaming coverage Low/Medium/High", rvas, expected, 3, values);
}

static bool ApplyFarSceneDistance(HMODULE exe, float value)
{
    if (value < 50.0f || value > 5000.0f)
    {
        Log("[FAIL] FarSceneDistance=%.3f outside 50..5000.", value);
        return false;
    }
    const uintptr_t rvas[] = {0x00D14DD0,0x00D14DD4,0x00D14DD8};
    const float expected[] = {200.0f,200.0f,200.0f};
    const float values[] = {value,value,value};
    return ApplyFloatGroup(exe, "FarScene x3", rvas, expected, 3, values);
}

static bool ApplyDecalVisibilityDistance(HMODULE exe, float distance)
{
    if (distance < 20.0f || distance > 1000.0f)
    {
        Log("[FAIL] DecalVisibilityDistance=%.3f outside 20..1000.", distance);
        return false;
    }
    const uintptr_t rvas[] = {0x00BF1A50};
    const float expected[] = {14400.0f};
    const float values[] = {distance * distance};
    return ApplyFloatGroup(exe, "Decal visibility squared distance", rvas, expected, 1, values);
}



static bool PatchAbsoluteOperand32(HMODULE exe, uintptr_t instructionRva,
                                   uint8_t opcode0, uint8_t opcode1,
                                   uintptr_t expectedTargetRva,
                                   const void* newTarget,
                                   const char* label)
{
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    auto* instruction = reinterpret_cast<uint8_t*>(base + instructionRva);
    if (instruction[0] != opcode0 || instruction[1] != opcode1)
    {
        Log("[SKIP] %s opcode mismatch at RVA 0x%08X.", label,
            static_cast<unsigned>(instructionRva));
        return false;
    }

    uint32_t currentTarget = 0;
    std::memcpy(&currentTarget, instruction + 2, sizeof(currentTarget));
    const uint32_t expectedTarget =
        static_cast<uint32_t>(base + expectedTargetRva);
    if (currentTarget != expectedTarget)
    {
        Log("[SKIP] %s source target mismatch at RVA 0x%08X: expected 0x%08X, got 0x%08X.",
            label, static_cast<unsigned>(instructionRva),
            static_cast<unsigned>(expectedTarget),
            static_cast<unsigned>(currentTarget));
        return false;
    }

    const uintptr_t replacementPtr = reinterpret_cast<uintptr_t>(newTarget);
    if (replacementPtr > 0xFFFFFFFFu)
    {
        Log("[FAIL] %s replacement pointer outside x86 range.", label);
        return false;
    }
    const uint32_t replacement = static_cast<uint32_t>(replacementPtr);
    if (!WriteBytes(instruction + 2, &replacement, sizeof(replacement)))
    {
        Log("[FAIL] %s operand write failed at RVA 0x%08X.", label,
            static_cast<unsigned>(instructionRva));
        return false;
    }

    Log("[OK] %s operand redirected at RVA 0x%08X.", label,
        static_cast<unsigned>(instructionRva));
    return true;
}

static bool ApplyRenderSliceHighDistances(HMODULE exe, float slice3Far, float outerFar)
{
    if (slice3Far < 50.0f || slice3Far > 10000.0f ||
        outerFar < 100.0f || outerFar > 50000.0f ||
        outerFar < slice3Far)
    {
        Log("[FAIL] RenderSlice High distances invalid: Slice3Far=%.3f Outer=%.3f.",
            slice3Far, outerFar);
        return false;
    }

    // Native High profile:
    // record 3 far = 100, final/outer far = 500.
    // ShadowSlice shares these slice classes.
    const uintptr_t rvas[] = {0x00D20B00, 0x00D20B0C};
    const float expected[] = {100.0f, 500.0f};
    const float values[] = {slice3Far, outerFar};
    return ApplyFloatGroup(exe, "RenderSlice/ShadowSlice High distance bounds",
                           rvas, expected, _countof(rvas), values);
}

static bool ApplyModelInfoDefaultLodDistance(HMODULE exe, float distance)
{
    if (distance < 100.0f || distance > 50000.0f)
    {
        Log("[FAIL] ModelInfoDefaultLODDistance=%.3f outside 100..50000.", distance);
        return false;
    }

    // VA 0x00638F2D:
    //   fld dword ptr [0x00F7D630] ; native shared 1000.0
    // Redirect only this ModelInfo initializer to ASI-owned storage.
    static float storage = 1000.0f;
    storage = distance;
    const bool ok = PatchAbsoluteOperand32(
        exe, 0x00238F2D, 0xD9, 0x05, 0x00B7D630,
        &storage, "ModelInfo default LODDIST");
    if (ok) Log("[OK] ModelInfo default LODDIST 1000 -> %.3f.", distance);
    return ok;
}

static bool ApplyVeryFarSceneTerrainDistance(HMODULE exe, float distance)
{
    if (distance < 500.0f || distance > 100000.0f)
    {
        Log("[FAIL] VeryFarSceneTerrain=%.3f outside 500..100000.", distance);
        return false;
    }

    // Four terrain-only fld instructions read native 5000.0 at VA 0x00FC8A5C.
    // Redirect them to one ASI-owned value instead of changing the shared constant.
    static float storage = 5000.0f;
    storage = distance;
    const uintptr_t sites[] = {
        0x004014CB, 0x004014F7, 0x00401526, 0x00401A8B
    };

    for (uintptr_t rva : sites)
    {
        if (!PatchAbsoluteOperand32(
                exe, rva, 0xD9, 0x05, 0x00BC8A5C,
                &storage, "VeryFarSceneTerrain distance"))
            return false;
    }

    Log("[OK] VeryFarSceneTerrain 5000 -> %.3f.", distance);
    return true;
}

static bool ApplyHighPaletteThreshold(HMODULE exe, double threshold)
{
    if (!std::isfinite(threshold) || threshold < 1.0 || threshold > 1000000.0)
    {
        Log("[FAIL] HighPaletteThreshold=%.3f outside 1..1000000.", threshold);
        return false;
    }

    // VA 0x009EE461:
    //   fcomp qword ptr [0x00FB54B0] ; native threshold 80.0
    // Historical validation reached 1600 without instability, but the setting
    // remains experimental because no isolated visual benefit was proven.
    static double storage = 80.0;
    storage = threshold;
    const bool ok = PatchAbsoluteOperand32(
        exe, 0x005EE461, 0xDC, 0x1D, 0x00BB54B0,
        &storage, "SS_HighPalette threshold");
    if (ok) Log("[OK] SS_HighPalette threshold 80 -> %.3f.", threshold);
    return ok;
}



static bool ApplyClipRangeHigh(HMODULE exe, float distance)
{
    if (!std::isfinite(distance) || distance < 100.0f || distance > 50000.0f)
    {
        Log("[FAIL] ClipRangeHigh=%.3f outside 100..50000.", distance);
        return false;
    }

    // Retail graphics profile ClipRange=3 reaches VA 0x00642F8E:
    //   fld dword ptr [0x00F7D630] ; 1000.0f
    // Redirect only the High/default case. Lower ClipRange profiles remain native.
    static float storage = 1000.0f;
    storage = distance;
    const bool ok = PatchAbsoluteOperand32(
        exe, 0x00242F8E, 0xD9, 0x05, 0x00B7D630,
        &storage, "ClipRange High");
    if (ok) Log("[OK] ClipRange High 1000 -> %.3f.", distance);
    return ok;
}

static bool ApplyDetailSystemDistance(HMODULE exe, float distance)
{
    if (!std::isfinite(distance) || distance < 10.0f || distance > 50000.0f)
    {
        Log("[FAIL] DetailSystemDistance=%.3f outside 10..50000.", distance);
        return false;
    }

    // Retail WSDetailSystem +0x218:
    //   VA 0x007ECD20 initial = 50.0f
    //   VA 0x007ECDC3 maximum compare = 100.0
    //   VA 0x007ECDD0 clamp replacement = 100.0f
    // Lower minimum 10.0 remains untouched.
    static float initialAndClamp = 50.0f;
    static double maximum = 100.0;
    initialAndClamp = distance;
    maximum = static_cast<double>(distance);

    if (!PatchAbsoluteOperand32(
            exe, 0x003ECD20, 0xD9, 0x05, 0x00B7D3A8,
            &initialAndClamp, "WSDetailSystem initial distance"))
        return false;

    if (!PatchAbsoluteOperand32(
            exe, 0x003ECDC3, 0xDC, 0x1D, 0x00B7BF80,
            &maximum, "WSDetailSystem maximum distance"))
        return false;

    if (!PatchAbsoluteOperand32(
            exe, 0x003ECDD0, 0xD9, 0x05, 0x00B7D640,
            &initialAndClamp, "WSDetailSystem clamp distance"))
        return false;

    Log("[OK] WSDetailSystem distance initial 50 / max 100 -> %.3f.", distance);
    return true;
}

static bool ApplySpotShadowResolutionScale(HMODULE exe, double scale)
{
    if (!std::isfinite(scale) || scale < 0.25 || scale > 4.0)
    {
        Log("[FAIL] SpotShadowResolutionScale=%.3f outside 0.25..4.0.", scale);
        return false;
    }

    // WSSpotShadowZBuffer%d resource creation multiplies dimensions by the
    // shared native 0.5 double at three exact sites. A fourth 0.5 consumer at
    // VA 0x004268B7 is projection/midpoint math and MUST remain native.
    //
    // 0.33 makes this owner atomic: all three opcodes and source operands are
    // verified first. Only after the whole family matches do we redirect any
    // operand. If a write fails, earlier writes are restored to the original
    // retail/Core1 target.
    static double storage = 0.5;
    storage = scale;

    const uintptr_t sites[] = {
        0x00026054, 0x0002609B, 0x00026151
    };

    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    const uint32_t expectedTarget = static_cast<uint32_t>(base + 0x00B7AC88);
    const uintptr_t replacementPtr = reinterpret_cast<uintptr_t>(&storage);
    if (replacementPtr > 0xFFFFFFFFu)
    {
        Log("[FAIL] Spot-shadow replacement pointer outside x86 range.");
        return false;
    }
    const uint32_t replacement = static_cast<uint32_t>(replacementPtr);

    // Phase 1: verify the complete three-site family before writing.
    for (uintptr_t rva : sites)
    {
        auto* instruction = reinterpret_cast<uint8_t*>(base + rva);
        if (instruction[0] != 0xDC || instruction[1] != 0x0D)
        {
            Log("[SKIP] Spot-shadow Z-buffer opcode mismatch at RVA 0x%08X.",
                static_cast<unsigned>(rva));
            return false;
        }

        uint32_t currentTarget = 0;
        std::memcpy(&currentTarget, instruction + 2, sizeof(currentTarget));
        if (currentTarget != expectedTarget)
        {
            Log("[SKIP] Spot-shadow Z-buffer source target mismatch at RVA 0x%08X: expected 0x%08X, got 0x%08X.",
                static_cast<unsigned>(rva),
                static_cast<unsigned>(expectedTarget),
                static_cast<unsigned>(currentTarget));
            return false;
        }
    }

    // Phase 2: redirect all three operands. Roll back on any write failure.
    size_t written = 0;
    for (; written < _countof(sites); ++written)
    {
        auto* operand = reinterpret_cast<uint8_t*>(base + sites[written] + 2);
        if (!WriteBytes(operand, &replacement, sizeof(replacement)))
        {
            for (size_t restore = 0; restore < written; ++restore)
            {
                auto* previousOperand = reinterpret_cast<uint8_t*>(base + sites[restore] + 2);
                WriteBytes(previousOperand, &expectedTarget, sizeof(expectedTarget));
            }
            Log("[FAIL] Spot-shadow Z-buffer operand write failed at RVA 0x%08X; prior sites restored.",
                static_cast<unsigned>(sites[written]));
            return false;
        }
    }

    Log("[OK] WSSpotShadowZBuffer family atomically verified and redirected.");
    Log("[OK] WSSpotShadowZBuffer resolution scale 0.500 -> %.3f.", scale);
    return true;
}

static bool ApplyMotionBlurActivationThreshold(HMODULE exe, float threshold)
{
    if (!std::isfinite(threshold) || threshold < 0.0f || threshold > 10.0f)
    {
        Log("[FAIL] MotionBlurActivationThreshold=%.4f outside 0..10.", threshold);
        return false;
    }

    // WSMotionBlurFilter compares absolute motion components against the
    // class-owned retail threshold 0.12 at VA 0x0113AB74.
    // This is an activation threshold, not a blur-strength scalar.
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    constexpr uintptr_t kThresholdRva = 0x00D3AB74;
    auto* target = reinterpret_cast<float*>(base + kThresholdRva);
    const float expected = 0.12f;
    if (std::memcmp(target, &expected, sizeof(expected)) != 0)
    {
        Log("[SKIP] MotionBlur activation threshold mismatch at RVA 0x%08X.",
            static_cast<unsigned>(kThresholdRva));
        return false;
    }

    if (!WriteBytes(target, &threshold, sizeof(threshold)))
    {
        Log("[FAIL] MotionBlur activation threshold write failed.");
        return false;
    }

    Log("[OK] MotionBlur activation threshold 0.1200 -> %.4f.", threshold);
    return true;
}



static bool ApplyHumanObjectQualityScale(HMODULE exe, float scale)
{
    if (!std::isfinite(scale) || scale < 0.25f || scale > 20.0f)
    {
        Log("[FAIL] HumanObjectQualityScale=%.3f outside 0.25..20.", scale);
        return false;
    }

    // WSHuman ObjectQuality owns seven retail distance constants:
    //   5, 10, 14, 20, 30, 40, 60
    // The historical V200 branch used x4 and V257 used x5, producing
    //   25, 50, 70, 100, 150, 200, 300.
    //
    // Redirect every profile-update load and every constructor-time load to
    // ASI-owned values. This avoids modifying shared retail constants.
    static float values[7] = {};
    const float native[7] = {5.0f, 10.0f, 14.0f, 20.0f, 30.0f, 40.0f, 60.0f};
    for (size_t i = 0; i < 7; ++i)
        values[i] = native[i] * scale;

    struct Site
    {
        uintptr_t rva;
        uintptr_t expectedSourceRva;
        uint8_t valueIndex;
    };

    static const Site sites[] = {
        // WSHuman ObjectQuality update family A.
        {0x000F176E, 0x00B9EB7C, 2}, // 14
        {0x000F177A, 0x00B7B134, 4}, // 30
        {0x000F1787, 0x00B7D3BC, 1}, // 10
        {0x000F1793, 0x00B7B138, 3}, // 20
        {0x000F17A0, 0x00B7B148, 0}, // 5
        {0x000F17AC, 0x00B7D3BC, 1}, // 10

        // WSHuman ObjectQuality update family B.
        {0x000F17CE, 0x00B7B134, 4}, // 30
        {0x000F17DA, 0x00B9EB84, 6}, // 60
        {0x000F17E7, 0x00B7B138, 3}, // 20
        {0x000F17F3, 0x00B9EB80, 5}, // 40
        {0x000F1800, 0x00B7D3BC, 1}, // 10
        {0x000F180C, 0x00B7B138, 3}, // 20

        // WSHuman constructor/profile initialization.
        {0x0010B1B3, 0x00B7D3BC, 1}, // 10
        {0x0010B1C4, 0x00B9EB7C, 2}, // 14
        {0x0010B1D0, 0x00B7B134, 4}, // 30
        {0x0010B1DE, 0x00B7B138, 3}, // 20
        {0x0010B1E6, 0x00B7B148, 0}, // 5
        {0x0010B211, 0x00B7B134, 4}, // 30
        {0x0010B21D, 0x00B9EB84, 6}, // 60
        {0x0010B225, 0x00B7B138, 3}, // 20
        {0x0010B231, 0x00B9EB80, 5}, // 40
        {0x0010B239, 0x00B7D3BC, 1}, // 10
        {0x0010B245, 0x00B7B138, 3}, // 20
    };

    for (const Site& site : sites)
    {
        if (!PatchAbsoluteOperand32(
                exe, site.rva, 0xD9, 0x05, site.expectedSourceRva,
                &values[site.valueIndex], "WSHuman ObjectQuality distance"))
            return false;
    }

    Log("[OK] WSHuman ObjectQuality scale %.3fx => %.1f/%.1f/%.1f/%.1f/%.1f/%.1f/%.1f.",
        scale, values[0], values[1], values[2], values[3], values[4], values[5], values[6]);
    return true;
}



static bool ApplyFoliageModelLodDistance(HMODULE exe, float distance)
{
    if (!std::isfinite(distance) || distance < 10.0f || distance > 5000.0f)
    {
        Log("[FAIL] FoliageModelLODDistance=%.3f outside 10..5000.", distance);
        return false;
    }

    // ModelInfo parser tag FOLIAGE sets field +0x08 = 1.
    // WSModel then overrides its +0xA4 LODDIST from the retail 50.0 source at
    // VA 0x0063960D. Redirect only that FOLIAGE-specific load.
    static float storage = 50.0f;
    storage = distance;
    const bool ok = PatchAbsoluteOperand32(
        exe, 0x0023960D, 0xD9, 0x05, 0x00B7D3A8,
        &storage, "FOLIAGE ModelInfo LODDIST");
    if (ok) Log("[OK] FOLIAGE ModelInfo LODDIST 50 -> %.3f.", distance);
    return ok;
}

static bool ApplyShadowCasterMinLodDistance(HMODULE exe, float distance)
{
    if (!std::isfinite(distance) || distance < 1.0f || distance > 5000.0f)
    {
        Log("[FAIL] ShadowCasterMinLODDistance=%.3f outside 1..5000.", distance);
        return false;
    }

    // Retail explicit ModelInfo path:
    //   ecx = ModelInfo ShadowSlice byte
    //   WSModel+0xA4 = per-model LODDIST
    // Historical V200/V257 inserted a minimum only when ShadowSlice != 0:
    //   max(LODDIST, 180) -> max(LODDIST, 240).
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    auto* target = reinterpret_cast<void*>(base + 0x002395D2);
    static const uint8_t expected[7] = {
        0xBB,0x01,0x00,0x00,0x00,0xD3,0xE3
    };

    g_shadowCasterMinLodDistance = distance;
    g_shadowCasterReturn = base + 0x002395D9;
    if (!WriteRel32Jmp7(target, expected,
                       reinterpret_cast<void*>(&ShadowCasterMinLodHook),
                       "Shadow-caster minimum LODDIST"))
        return false;

    Log("[OK] Shadow-caster minimum LODDIST 180 historical baseline -> %.3f.", distance);
    return true;
}

static bool ApplyParticleLodMinDistance(HMODULE exe, float distance)
{
    if (!std::isfinite(distance) || distance < 1.0f || distance > 5000.0f)
    {
        Log("[FAIL] ParticleLODMinDistance=%.3f outside 1..5000.", distance);
        return false;
    }

    // Two WSParticleObject render/query paths originally perform:
    //   fld dword ptr [eax+1D8h]
    // Historical V200/V257 replaced each load with max(value,100/150).
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    static const uint8_t expected[6] = {0xD9,0x80,0xD8,0x01,0x00,0x00};

    g_particleLodMinDistance = distance;

    if (!WriteRel32Call6(reinterpret_cast<void*>(base + 0x002E2D32),
                         expected, reinterpret_cast<void*>(&ParticleLodMinHook),
                         "Particle LOD minimum A"))
        return false;

    if (!WriteRel32Call6(reinterpret_cast<void*>(base + 0x002E3643),
                         expected, reinterpret_cast<void*>(&ParticleLodMinHook),
                         "Particle LOD minimum B"))
        return false;

    Log("[OK] Particle LOD minimum 100 historical baseline -> %.3f.", distance);
    return true;
}



static bool ApplyWaterReflectionResolution(HMODULE exe, float width, float height)
{
    if (!std::isfinite(width) || !std::isfinite(height) ||
        width < 64.0f || width > 8192.0f ||
        height < 32.0f || height > 8192.0f)
    {
        Log("[FAIL] Water reflection resolution %.1fx%.1f outside safe range.", width, height);
        return false;
    }

    // WSWater::WaterReflection owns a shared width/height pair:
    //   VA 0x011C13D8 = 512.0
    //   VA 0x011C13DC = 128.0
    // The same pair is consumed by creation and backend/surface paths,
    // so changing the owner globals keeps all water-reflection consumers coherent.
    const uintptr_t rvas[] = {0x00DC13D8, 0x00DC13DC};
    const float expected[] = {512.0f, 128.0f};
    const float values[] = {width, height};
    return ApplyFloatGroup(exe, "Water reflection resolution", rvas, expected, 2, values);
}

static bool ApplyWaterNormalsResolution(HMODULE exe, int resolution)
{
    if (resolution < 64 || resolution > 2048)
    {
        Log("[FAIL] WaterNormalsResolution=%d outside 64..2048.", resolution);
        return false;
    }

    // WSWaterNormals creates WaterNormals%d and WaterNormalsTemp%d.
    // Both resources are 128x128 in retail, expressed as four exact
    // "push 0x80" immediates at the call sites below.
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    const uintptr_t sites[] = {
        0x0053A599, 0x0053A5BE, 0x0053A629, 0x0053A634
    };

    for (uintptr_t rva : sites)
    {
        const auto* at = reinterpret_cast<const uint8_t*>(base + rva);
        if (at[0] != 0x68)
        {
            Log("[SKIP] Water normals push opcode mismatch at RVA 0x%08X.",
                static_cast<unsigned>(rva));
            return false;
        }
        uint32_t current = 0;
        std::memcpy(&current, at + 1, sizeof(current));
        if (current != 128u)
        {
            Log("[SKIP] Water normals native dimension mismatch at RVA 0x%08X.",
                static_cast<unsigned>(rva));
            return false;
        }
    }

    const uint32_t value = static_cast<uint32_t>(resolution);
    for (uintptr_t rva : sites)
    {
        auto* at = reinterpret_cast<uint8_t*>(base + rva);
        if (!WriteBytes(at + 1, &value, sizeof(value)))
        {
            Log("[FAIL] Water normals dimension write failed at RVA 0x%08X.",
                static_cast<unsigned>(rva));
            return false;
        }
    }

    Log("[OK] WaterNormals/WaterNormalsTemp resolution 128 -> %d.", resolution);
    return true;
}

static bool ApplyRainCubeResolution(HMODULE exe, int resolution)
{
    if (resolution < 32 || resolution > 2048)
    {
        Log("[FAIL] RainCubeResolution=%d outside 32..2048.", resolution);
        return false;
    }

    // RainCubeRT is created from one exact push-immediate dimension.
    // Keep this separate from HardwareRainDepthTexture, which shares the
    // WSShadowZBuffer dimensions and is already owned by ShadowMapResolution.
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    constexpr uintptr_t kRva = 0x00402785;
    auto* at = reinterpret_cast<uint8_t*>(base + kRva);
    if (at[0] != 0x68)
    {
        Log("[SKIP] RainCubeRT push opcode mismatch at RVA 0x%08X.",
            static_cast<unsigned>(kRva));
        return false;
    }
    uint32_t current = 0;
    std::memcpy(&current, at + 1, sizeof(current));
    if (current != 128u)
    {
        Log("[SKIP] RainCubeRT native dimension mismatch at RVA 0x%08X.",
            static_cast<unsigned>(kRva));
        return false;
    }

    const uint32_t value = static_cast<uint32_t>(resolution);
    if (!WriteBytes(at + 1, &value, sizeof(value)))
    {
        Log("[FAIL] RainCubeRT dimension write failed.");
        return false;
    }

    Log("[OK] RainCubeRT resolution 128 -> %d.", resolution);
    return true;
}

static bool ApplyDepthBlurAutoTransition(HMODULE exe, float start, float range)
{
    if (!std::isfinite(start) || !std::isfinite(range) ||
        start < 0.0f || start > 10000.0f ||
        range <= 0.0f || range > 10000.0f)
    {
        Log("[FAIL] DepthBlur auto transition invalid: start=%.3f range=%.3f.", start, range);
        return false;
    }

    // WSDepthBlurFilter primary path computes approximately:
    //   clamp(max(sourceValue - 200, 0) / 50, 0, 1)
    // at VA 0x007CE967 / 0x007CE97D.
    // The exact semantic meaning of sourceValue is intentionally not guessed;
    // these are exposed as the class-owned automatic transition start/range.
    const uintptr_t rvas[] = {0x00D3A3C0, 0x00D3A3BC};
    const float expected[] = {200.0f, 50.0f};
    const float values[] = {start, range};
    return ApplyFloatGroup(exe, "DepthBlur automatic transition", rvas, expected, 2, values);
}


static bool ApplyMotionBlurFullResolution(HMODULE exe)
{
    // MotionBlurDownsampledBackBuffer is created at half backbuffer resolution.
    // Retail performs two exact 16-bit shifts:
    //   VA 0x007D5648 : shr ax,1
    //   VA 0x007D5679 : shr ax,1
    // Full-resolution mode removes only those two downsample operations.
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    const uintptr_t sites[] = {0x003D5648, 0x003D5679};
    const uint8_t expected[] = {0x66,0xD1,0xE8};
    const uint8_t nops[] = {0x90,0x90,0x90};

    for (uintptr_t rva : sites)
    {
        auto* at = reinterpret_cast<uint8_t*>(base + rva);
        if (std::memcmp(at, expected, sizeof(expected)) != 0)
        {
            Log("[SKIP] MotionBlur half-resolution shift mismatch at RVA 0x%08X.",
                static_cast<unsigned>(rva));
            return false;
        }
    }
    for (uintptr_t rva : sites)
    {
        if (!WriteBytes(reinterpret_cast<void*>(base + rva), nops, sizeof(nops)))
        {
            Log("[FAIL] MotionBlur full-resolution patch failed at RVA 0x%08X.",
                static_cast<unsigned>(rva));
            return false;
        }
    }

    Log("[OK] MotionBlurDownsampledBackBuffer half-resolution -> full-resolution.");
    return true;
}

static bool ApplyBloomResolutionMultiplier(HMODULE exe, int multiplier)
{
    // Native bloom/god-rays pyramid is derived from the backbuffer as:
    //   GodRays / Bloom base : /4
    //   BloomTexture2x2      : /8
    //   BloomTexture4x4      : /16
    //   BloomTexture8x8      : /32
    //
    // Multiplier 2 raises every level coherently:
    //   /2, /4, /8, /16
    //
    // ScaledTexture remains the native /2 source, so multiplier 2 never
    // asks the base bloom level to exceed its source resolution.
    if (multiplier != 1 && multiplier != 2)
    {
        Log("[FAIL] BloomResolutionMultiplier=%d; supported values are 1 or 2.", multiplier);
        return false;
    }
    if (multiplier == 1)
        return true;

    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    struct ShiftSite
    {
        uintptr_t rva;
        uint8_t op2;
        uint8_t nativeShift;
    };
    static const ShiftSite sites[] = {
        {0x003C9F34, 0xE8, 2}, // ax /4
        {0x003C9F39, 0xED, 2}, // bp /4
        {0x003CA17D, 0xE8, 3}, // ax /8
        {0x003CA181, 0xE9, 3}, // cx /8
        {0x003CA32C, 0xE8, 4}, // ax /16
        {0x003CA330, 0xE9, 4}, // cx /16
        {0x003CA3FD, 0xE8, 5}, // ax /32
        {0x003CA414, 0xE8, 5}, // ax /32
    };

    for (const ShiftSite& site : sites)
    {
        const auto* at = reinterpret_cast<const uint8_t*>(base + site.rva);
        if (at[0] != 0x66 || at[1] != 0xC1 ||
            at[2] != site.op2 || at[3] != site.nativeShift)
        {
            Log("[SKIP] Bloom pyramid shift mismatch at RVA 0x%08X.",
                static_cast<unsigned>(site.rva));
            return false;
        }
    }

    for (const ShiftSite& site : sites)
    {
        auto* at = reinterpret_cast<uint8_t*>(base + site.rva);
        const uint8_t newShift = static_cast<uint8_t>(site.nativeShift - 1);
        if (!WriteBytes(at + 3, &newShift, 1))
        {
            Log("[FAIL] Bloom pyramid shift write failed at RVA 0x%08X.",
                static_cast<unsigned>(site.rva));
            return false;
        }
    }

    Log("[OK] Bloom/GodRays render-target pyramid resolution multiplier 1x -> 2x.");
    return true;
}


static bool ApplyBloomPrefilterGain(HMODULE exe, float value)
{
    if (!std::isfinite(value) || value < 0.25f || value > 4.0f)
    {
        Log("[FAIL] BloomPrefilterGain=%.3f outside 0.25..4.0.", value);
        return false;
    }

    // WSBloomFilterHDR.hlsl / PsBloom:
    // retail DEF c1 = { 1.0, 4.0, 0.0, -1/3 }.
    //
    // The shader performs:
    //   sampledColor.rgb *= c1.y;
    //   pixelLum = dot(lumaWeights, sampledColor.rgb);
    //   bloomMask = max(pixelLum - runtimeThreshold, 0);
    //
    // Therefore c1.y is the prefilter/extraction gain, upstream from the blur
    // and final composite. This is the correct place to normalize energy after
    // increasing the Bloom/GodRays RT pyramid resolution without changing the
    // final sampler, ToneMap or blur composition.
    //
    // Raw retail file offset 0x00D68568, PE runtime RVA +0x1600:
    // RVA 0x00D69B68, float 4.0.
    const uintptr_t rvas[] = {0x00D69B68};
    const float expected[] = {4.0f};
    const float values[] = {value};

    return ApplyFloatGroup(exe, "PsBloom prefilter gain", rvas, expected, 1, values);
}


static bool ApplyBloomFinalContribution(HMODULE exe, float value)
{
    if (!std::isfinite(value) || value < 0.5f || value > 4.0f)
    {
        Log("[FAIL] BloomFinalContribution=%.3f outside 0.5..4.0.", value);
        return false;
    }

    // PsBloomFinal retail constant c0.w = 4.0 at RVA 0x00D69360.
    // Historical V200 changed only this scalar 4.0 -> 2.0 AND separately
    // changed a sampler selector. For brightness control we expose ONLY the
    // scalar and deliberately leave the retail SkyBloomTextureSampler intact.
    //
    // This reduces final bloom energy without changing ToneMap, exposure,
    // render-target resolution or sampler routing.
    const uintptr_t rvas[] = {0x00D69360};
    const float expected[] = {4.0f};
    const float values[] = {value};
    return ApplyFloatGroup(exe, "PsBloomFinal contribution", rvas, expected, 1, values);
}


static bool ApplyBloomFinalDownsampledBackBufferSampler(HMODULE exe)
{
    // Historical V28/V29 anti-blur path:
    // PsBloomFinal samples DownsampledBackBufferSampler at s2 in one texld.
    // Redirect ONLY that sampler operand to BackBufferSampler s0.
    //
    // Exact shader operand raw offset in the retail file: 0x00D67E3C.
    // PE runtime RVA = raw + 0x1600 = 0x00D6943C.
    //
    // This is distinct from the later V200 SkyBloomTextureSampler s4 -> s0
    // experiment at RVA 0x00D694FC, which is intentionally left untouched.
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    auto* sampler = reinterpret_cast<uint8_t*>(base + 0x00D6943C);
    const uint8_t expected = 0x02;
    const uint8_t value = 0x00;

    if (*sampler != expected)
    {
        Log("[SKIP] PsBloomFinal DownsampledBackBuffer sampler mismatch at RVA 0x00D6943C.");
        return false;
    }

    if (!WriteBytes(sampler, &value, 1))
        return false;

    Log("[OK] PsBloomFinal sampler DownsampledBackBuffer s2 -> BackBuffer s0.");
    return true;
}


static bool ApplyBloomFinalBackBufferSampler(HMODULE exe)
{
    // PsBloomFinal retail samples SkyBloomTextureSampler at s4 here.
    // Historical V200/V28 anti-blur path redirects only this sampler operand
    // to BackBufferSampler s0. Exact manifest delta:
    //   RVA 0x00D694FC: 0x04 -> 0x00
    //
    // Keep this independently configurable from the 4.0 -> 2.0 contribution
    // scalar so resolution, energy and source sharpness remain separate knobs.
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    auto* sampler = reinterpret_cast<uint8_t*>(base + 0x00D694FC);
    const uint8_t expected = 0x04;
    const uint8_t value = 0x00;

    if (*sampler != expected)
    {
        Log("[SKIP] PsBloomFinal sampler retail byte mismatch at RVA 0x00D694FC.");
        return false;
    }

    if (!WriteBytes(sampler, &value, 1))
        return false;

    Log("[OK] PsBloomFinal sampler SkyBloom s4 -> BackBuffer s0.");
    return true;
}


static bool ApplyScaledTextureFullResolution(HMODULE exe)
{
    // PostFX ScaledTexture is created from backbuffer dimensions divided by 2.
    // Retail exact sites:
    //   VA 0x007C9EAF: shr cx,1
    //   VA 0x007C9EBE: shr cx,1
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    const uintptr_t sites[] = {0x003C9EAF, 0x003C9EBE};
    const uint8_t expected[] = {0x66,0xD1,0xE9};
    const uint8_t nops[] = {0x90,0x90,0x90};

    for (uintptr_t rva : sites)
    {
        const auto* at = reinterpret_cast<const uint8_t*>(base + rva);
        if (std::memcmp(at, expected, sizeof(expected)) != 0)
        {
            Log("[SKIP] ScaledTexture half-resolution shift mismatch at RVA 0x%08X.",
                static_cast<unsigned>(rva));
            return false;
        }
    }

    for (uintptr_t rva : sites)
        if (!WriteBytes(reinterpret_cast<void*>(base + rva), nops, sizeof(nops)))
            return false;

    Log("[OK] ScaledTexture half-resolution -> full-resolution.");
    return true;
}

static bool ApplyDepthBlurMaskResolutionScale(HMODULE exe, double scale)
{
    if (!std::isfinite(scale) || scale < 0.25 || scale > 2.0)
    {
        Log("[FAIL] DepthBlurMaskResolutionScale=%.3f outside 0.25..2.0.", scale);
        return false;
    }

    // DepthBlurMask2x2 / DepthBlurMask2x2Temp derive their dimensions from
    // one local 0.5 double at VA 0x007CFE4A. Redirect only that consumer.
    static double storage = 0.5;
    storage = scale;
    const bool ok = PatchAbsoluteOperand32(
        exe, 0x003CFE4A, 0xDD, 0x05, 0x00B7AC88,
        &storage, "DepthBlur mask resolution scale");
    if (ok)
        Log("[OK] DepthBlur mask resolution scale 0.500 -> %.3f.", scale);
    return ok;
}

static bool ApplyDepthBlurMaskTapOffsetScale(HMODULE exe, float scale)
{
    if (!std::isfinite(scale) || scale < 0.25f || scale > 2.0f)
    {
        Log("[FAIL] DepthBlurMaskTapOffsetScale=%.6f outside 0.25..2.0.", scale);
        return false;
    }

    // The original 0.14 audit recovered these literals from RAW file offsets.
    // 0.28 used a fixed RAW->RVA displacement, but the current canonical Core1
    // runtime proves that fixed-address verification is still fragile.
    //
    // Resolve the complete two-shader retail signature instead. Both shader
    // families must match together, exactly once, inside the narrow embedded
    // shader window before any write is allowed.
    static const uintptr_t rels[] = {
        0x000,
        0x018,0x01C,0x020,0x024,
        0x2D8,0x2DC,
        0x2F0,0x2F4,0x2F8,0x2FC
    };
    static const float expected[] = {
        7.5f,
        2.0f,4.0f,6.0f,8.0f,
        6.0f,7.5f,
        2.0f,4.0f,5.0f,8.0f
    };

    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    constexpr uintptr_t searchStart = 0x00D66000;
    constexpr uintptr_t searchEnd   = 0x00D69000;

    uintptr_t foundRva = 0;
    unsigned matches = 0;
    for (uintptr_t candidate = searchStart;
         candidate + rels[_countof(rels) - 1] + sizeof(float) <= searchEnd;
         candidate += 4)
    {
        bool match = true;
        for (size_t i = 0; i < _countof(rels); ++i)
        {
            float current = 0.0f;
            std::memcpy(&current,
                        reinterpret_cast<const void*>(base + candidate + rels[i]),
                        sizeof(current));
            if (std::memcmp(&current, &expected[i], sizeof(current)) != 0)
            {
                match = false;
                break;
            }
        }
        if (match)
        {
            foundRva = candidate;
            ++matches;
        }
    }

    if (matches != 1)
    {
        Log("[SKIP] DepthBlur mask retail signature match count=%u in RVA 0x00D66000..0x00D69000.",
            matches);
        return false;
    }

    uintptr_t rvas[_countof(rels)] = {};
    float values[_countof(rels)] = {};
    for (size_t i = 0; i < _countof(rels); ++i)
    {
        rvas[i] = foundRva + rels[i];
        values[i] = expected[i] * scale;
    }

    Log("[OK] DepthBlur mask retail signature resolved at RVA 0x%08X.",
        static_cast<unsigned>(foundRva));
    return ApplyFloatGroup(exe, "DepthBlur mask shader tap offsets",
                           rvas, expected, _countof(rvas), values);
}

static bool ApplyDepthBlurColorTexelOffsetScale(HMODULE exe, float scale)
{
    if (!std::isfinite(scale) || scale < 0.25f || scale > 2.0f)
    {
        Log("[FAIL] DepthBlurColorTexelOffsetScale=%.6f outside 0.25..2.0.", scale);
        return false;
    }

    // Resolve both retail -1/+1 texel-offset pairs as one signature.
    // The second pair is exactly +0x210 from the first in the retail shader
    // layout. Requiring the combined signature to be unique prevents a broad
    // -1/+1 float scan from patching unrelated shader constants.
    static const uintptr_t rels[] = {0x000,0x004,0x210,0x214};
    static const float expected[] = {-1.0f,1.0f,-1.0f,1.0f};

    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    constexpr uintptr_t searchStart = 0x00D65800;
    constexpr uintptr_t searchEnd   = 0x00D68800;

    uintptr_t foundRva = 0;
    unsigned matches = 0;
    for (uintptr_t candidate = searchStart;
         candidate + rels[_countof(rels) - 1] + sizeof(float) <= searchEnd;
         candidate += 4)
    {
        bool match = true;
        for (size_t i = 0; i < _countof(rels); ++i)
        {
            float current = 0.0f;
            std::memcpy(&current,
                        reinterpret_cast<const void*>(base + candidate + rels[i]),
                        sizeof(current));
            if (std::memcmp(&current, &expected[i], sizeof(current)) != 0)
            {
                match = false;
                break;
            }
        }
        if (match)
        {
            foundRva = candidate;
            ++matches;
        }
    }

    if (matches != 1)
    {
        Log("[SKIP] DepthBlur color retail signature match count=%u in RVA 0x00D65800..0x00D68800.",
            matches);
        return false;
    }

    uintptr_t rvas[_countof(rels)] = {};
    float values[_countof(rels)] = {};
    for (size_t i = 0; i < _countof(rels); ++i)
    {
        rvas[i] = foundRva + rels[i];
        values[i] = expected[i] * scale;
    }

    Log("[OK] DepthBlur color retail signature resolved at RVA 0x%08X.",
        static_cast<unsigned>(foundRva));
    return ApplyFloatGroup(exe, "DepthBlur color shader texel offsets",
                           rvas, expected, _countof(rvas), values);
}


static bool ApplyWillToFightGridResolution(HMODULE exe, int resolution)
{
    if (resolution < 64 || resolution > 2048)
    {
        Log("[FAIL] WillToFight GridResolution=%d outside 64..2048.", resolution);
        return false;
    }

    // WSWillToFightGrid owns:
    // - CPU grid dimension loaded at VA 0x0097690C from native float 256
    // - LowResWorldWTF 256x256
    // - LowResWorldWTFVertex 256x256
    // Keep all five dimension consumers coherent.
    static float cpuDimension = 256.0f;
    cpuDimension = static_cast<float>(resolution);

    if (!PatchAbsoluteOperand32(
            exe, 0x0057690C, 0xD9, 0x05, 0x00C27748,
            &cpuDimension, "WSWillToFightGrid CPU dimension"))
        return false;

    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    const uintptr_t sites[] = {
        0x0057603B, 0x00576040, 0x00576085, 0x0057608A
    };

    for (uintptr_t rva : sites)
    {
        const auto* at = reinterpret_cast<const uint8_t*>(base + rva);
        if (at[0] != 0x68)
        {
            Log("[SKIP] WTF render-target push opcode mismatch at RVA 0x%08X.",
                static_cast<unsigned>(rva));
            return false;
        }
        uint32_t current = 0;
        std::memcpy(&current, at + 1, sizeof(current));
        if (current != 256u)
        {
            Log("[SKIP] WTF render-target native dimension mismatch at RVA 0x%08X.",
                static_cast<unsigned>(rva));
            return false;
        }
    }

    const uint32_t value = static_cast<uint32_t>(resolution);
    for (uintptr_t rva : sites)
    {
        auto* at = reinterpret_cast<uint8_t*>(base + rva);
        if (!WriteBytes(at + 1, &value, sizeof(value)))
        {
            Log("[FAIL] WTF render-target dimension write failed at RVA 0x%08X.",
                static_cast<unsigned>(rva));
            return false;
        }
    }

    Log("[OK] WSWillToFightGrid CPU/GPU resolution 256 -> %d.", resolution);
    return true;
}

static bool ApplyWSSphereActivatorMaxRadius(HMODULE exe, float radius)
{
    if (!std::isfinite(radius) || radius < 0.5f || radius > 64.0f)
    {
        Log("[FAIL] WSSphereActivatorMaxRadius=%.3f outside 0.5..64.0.", radius);
        return false;
    }

    // Exact clean-retail clamp scalar used by WSSphereActivator sphere-create:
    //   VA 0x00FCD834 / RVA 0x00BCD834 = 2.06f
    // Native routine at VA 0x0068EF80 computes:
    //   effective_radius = min(requested_radius * 1.05, 2.06)
    //
    // This is an experimental activation/query diagnostic, NOT a proven
    // static-render distance owner. Fail closed if the exact retail scalar
    // does not match.
    const uintptr_t rvas[] = {0x00BCD834};
    const float expected[] = {2.06f};
    const float values[] = {radius};
    const bool ok = ApplyFloatGroup(
        exe, "WSSphereActivator max-radius clamp", rvas, expected, 1, values);
    if (ok)
        Log("[OK] WSSphereActivator max-radius clamp 2.060 -> %.3f.", radius);
    return ok;
}


static bool ApplyVeryFarSceneProfileThresholds(HMODULE exe, float profile0, float profile1)
{
    if (!std::isfinite(profile0) || !std::isfinite(profile1) ||
        profile0 < 1.0f || profile0 > 10000.0f ||
        profile1 < 1.0f || profile1 > 10000.0f)
    {
        Log("[FAIL] VeryFarScene profile thresholds invalid %.3f/%.3f.", profile0, profile1);
        return false;
    }

    // VeryFarScene profile record +0x08:
    // profile 0 = 22, profile 1 = 49.
    // Redirect ONLY the six VeryFarScene loads. The shared 49.0 constant has
    // an unrelated consumer at VA 0x008C04F5 and must remain untouched.
    static float p0 = 22.0f;
    static float p1 = 49.0f;
    p0 = profile0;
    p1 = profile1;

    const uintptr_t p0Sites[] = {0x004014EE, 0x0040153A, 0x00401A9B};
    const uintptr_t p1Sites[] = {0x004014C2, 0x0040151E, 0x00401A7B};

    for (uintptr_t rva : p0Sites)
        if (!PatchAbsoluteOperand32(
                exe, rva, 0xD9, 0x05, 0x00BFCFE4,
                &p0, "VeryFarScene profile-0 threshold"))
            return false;

    for (uintptr_t rva : p1Sites)
        if (!PatchAbsoluteOperand32(
                exe, rva, 0xD9, 0x05, 0x00BFCFE8,
                &p1, "VeryFarScene profile-1 threshold"))
            return false;

    Log("[OK] VeryFarScene profile thresholds 22/49 -> %.3f/%.3f.", profile0, profile1);
    return true;
}


static bool ApplyDepthBlurColorPyramidFactor(HMODULE exe, double factor)
{
    if (!std::isfinite(factor) || factor < 0.25 || factor > 2.0)
    {
        Log("[FAIL] DepthBlurColorPyramidFactor=%.3f outside 0.25..2.0.", factor);
        return false;
    }

    // DepthBlurColor%dx%d builds four levels using:
    //   divisor = factor * 2^(level+1)
    // Retail factor = 0.75 => divisors 1.5 / 3 / 6 / 12.
    // Only the local multiplier at VA 0x007CFA3B is redirected.
    // The same shared 0.75 constant has unrelated consumers, including a
    // DepthBlur arithmetic path at VA 0x007CF84C, which must remain untouched.
    static double storage = 0.75;
    storage = factor;
    const bool ok = PatchAbsoluteOperand32(
        exe, 0x003CFA3B, 0xDD, 0x05, 0x00B8A368,
        &storage, "DepthBlur color-pyramid factor");
    if (ok)
        Log("[OK] DepthBlur color-pyramid factor 0.750 -> %.3f.", factor);
    return ok;
}

static bool ApplySkyDomeResolutionMultiplier(HMODULE exe, int multiplier)
{
    if (multiplier != 1 && multiplier != 2)
    {
        Log("[FAIL] SkyDomeResolutionMultiplier=%d; supported values are 1 or 2.", multiplier);
        return false;
    }
    if (multiplier == 1)
        return true;

    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);

    // Main SkyDomeBlendTexture family: retail backbuffer /4 -> /2.
    struct Shift4
    {
        uintptr_t rva;
        uint8_t regOpcode;
    };
    const Shift4 baseSites[] = {
        {0x00615C32, 0xE8}, // shr ax,2
        {0x00615C37, 0xED}, // shr bp,2
    };
    for (const auto& site : baseSites)
    {
        const auto* at = reinterpret_cast<const uint8_t*>(base + site.rva);
        if (at[0] != 0x66 || at[1] != 0xC1 || at[2] != site.regOpcode || at[3] != 0x02)
        {
            Log("[SKIP] SkyDome /4 shift mismatch at RVA 0x%08X.",
                static_cast<unsigned>(site.rva));
            return false;
        }
    }

    // SkyDomeBlendTexture3x3 family uses positive screen dimensions divided
    // by 12: magic multiply by ~1/6 followed by sar edx,1.
    // Removing only the final /2 yields /6, preserving a coherent 2x increase.
    const uintptr_t div12Sites[] = {0x00615D92, 0x00615DA8};
    const uint8_t div12Expected[] = {0xD1,0xFA};
    for (uintptr_t rva : div12Sites)
    {
        const auto* at = reinterpret_cast<const uint8_t*>(base + rva);
        if (std::memcmp(at, div12Expected, sizeof(div12Expected)) != 0)
        {
            Log("[SKIP] SkyDome /12 tail mismatch at RVA 0x%08X.",
                static_cast<unsigned>(rva));
            return false;
        }
    }

    // SkyDomeDistortionTexture: retail /8 -> /4.
    const uintptr_t distortionSites[] = {0x00615E2C, 0x00615E4E};
    for (uintptr_t rva : distortionSites)
    {
        const auto* at = reinterpret_cast<const uint8_t*>(base + rva);
        if (at[0] != 0x66 || at[1] != 0xC1 || at[2] != 0xE9 || at[3] != 0x03)
        {
            Log("[SKIP] SkyDome distortion /8 shift mismatch at RVA 0x%08X.",
                static_cast<unsigned>(rva));
            return false;
        }
    }

    const uint8_t one = 0x01;
    for (const auto& site : baseSites)
        if (!WriteBytes(reinterpret_cast<void*>(base + site.rva + 3), &one, 1))
            return false;

    const uint8_t nops2[] = {0x90,0x90};
    for (uintptr_t rva : div12Sites)
        if (!WriteBytes(reinterpret_cast<void*>(base + rva), nops2, sizeof(nops2)))
            return false;

    const uint8_t two = 0x02;
    for (uintptr_t rva : distortionSites)
        if (!WriteBytes(reinterpret_cast<void*>(base + rva + 3), &two, 1))
            return false;

    Log("[OK] SkyDome render-target family resolution multiplier 1x -> 2x.");
    return true;
}


static bool ApplyDamageBlurResolutionScale(HMODULE exe, double scale)
{
    if (!std::isfinite(scale) || scale < 0.25 || scale > 1.0)
    {
        Log("[FAIL] DamageBlurResolutionScale=%.3f outside 0.25..1.0.", scale);
        return false;
    }

    // BackBufferLDRPostFiltersDamageBlur is created from one local 0.5 scale
    // used coherently for width and height at VA 0x007D72D3.
    static double storage = 0.5;
    storage = scale;
    const bool ok = PatchAbsoluteOperand32(
        exe, 0x003D72D3, 0xDD, 0x05, 0x00B7AC88,
        &storage, "DamageBlur render-target resolution scale");
    if (ok)
        Log("[OK] DamageBlur render-target resolution scale 0.500 -> %.3f.", scale);
    return ok;
}

static bool ApplyRainDensityOverride(HMODULE exe, int percent)
{
    if (percent < 25 || percent > 200)
    {
        Log("[FAIL] RainDensityPercentOverride=%d outside native effective range 25..200.", percent);
        return false;
    }

    // Renderer initialization calls the hidden RainDensity integer setting at
    // VA 0x00801EFC. Retail default registration is 100, then the consumer
    // divides by 100 and clamps the normalized value to 0.25..2.0.
    //
    // Replace only this five-byte CALL with mov eax,imm32. The following native
    // store/divide/clamp sequence remains intact.
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    constexpr uintptr_t kRva = 0x00401EFC;
    auto* at = reinterpret_cast<uint8_t*>(base + kRva);
    const uint8_t expected[5] = {0xE8,0x1F,0xAB,0xFB,0xFF};
    if (std::memcmp(at, expected, sizeof(expected)) != 0)
    {
        Log("[SKIP] RainDensity consumer CALL mismatch at RVA 0x%08X.",
            static_cast<unsigned>(kRva));
        return false;
    }

    uint8_t patch[5] = {0xB8,0,0,0,0};
    const uint32_t value = static_cast<uint32_t>(percent);
    std::memcpy(patch + 1, &value, sizeof(value));
    if (!WriteBytes(at, patch, sizeof(patch)))
    {
        Log("[FAIL] RainDensity override patch failed.");
        return false;
    }

    Log("[OK] RainDensity hidden setting overridden to %d%%; native 25..200%% clamp retained.", percent);
    return true;
}


static bool ApplyParticleRenderTargetResolutionMultiplier(HMODULE exe, int multiplier)
{
    if (multiplier != 1 && multiplier != 2)
    {
        Log("[FAIL] ParticleRenderTargetResolutionMultiplier=%d; supported values are 1 or 2.", multiplier);
        return false;
    }
    if (multiplier == 1)
        return true;

    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);

    // WSParticleRender uses one half-resolution width/height pair for the
    // ParticleBB0 / AfterParticleLightVolume / distortion family, then a
    // separate /16 pair for the ParticleBB3 family.
    //
    // 2x quality keeps the hierarchy coherent:
    //   /2  -> full
    //   /16 -> /8
    static const uint8_t halfCx[] = {0x66,0xD1,0xE9};
    static const uint8_t halfBp[] = {0x66,0xD1,0xED};
    static const uint8_t sixteenthCx[] = {0x66,0xC1,0xE9,0x04};
    static const uint8_t sixteenthBp[] = {0x66,0xC1,0xED,0x04};

    auto* halfCxAt = reinterpret_cast<uint8_t*>(base + 0x002E7DFA);
    auto* halfBpAt = reinterpret_cast<uint8_t*>(base + 0x002E7DFE);
    auto* sixteenthCxAt = reinterpret_cast<uint8_t*>(base + 0x002E7FAF);
    auto* sixteenthBpAt = reinterpret_cast<uint8_t*>(base + 0x002E7FB4);

    if (std::memcmp(halfCxAt, halfCx, sizeof(halfCx)) != 0 ||
        std::memcmp(halfBpAt, halfBp, sizeof(halfBp)) != 0 ||
        std::memcmp(sixteenthCxAt, sixteenthCx, sizeof(sixteenthCx)) != 0 ||
        std::memcmp(sixteenthBpAt, sixteenthBp, sizeof(sixteenthBp)) != 0)
    {
        Log("[SKIP] WSParticleRender target-resolution signatures do not match retail.");
        return false;
    }

    const uint8_t nops3[] = {0x90,0x90,0x90};
    const uint8_t shift3 = 0x03;

    if (!WriteBytes(halfCxAt, nops3, sizeof(nops3)) ||
        !WriteBytes(halfBpAt, nops3, sizeof(nops3)) ||
        !WriteBytes(sixteenthCxAt + 3, &shift3, 1) ||
        !WriteBytes(sixteenthBpAt + 3, &shift3, 1))
    {
        Log("[FAIL] WSParticleRender target-resolution writes failed.");
        return false;
    }

    Log("[OK] WSParticleRender target family 2x quality applied (/2->full, /16->/8).");
    return true;
}


static bool VerifyParticleFullResolutionDepthRestore(HMODULE exe)
{
    // WildStar/Particles/ApplyPS.hlsl, RestoreDepthBuffer variant.
    //
    // Retail ParticleBB0 is half-resolution and this shader reconstructs the
    // packed half-column layout with four fixed constants:
    //   1/80, 2, -80, +80.
    //
    // When ParticleBB0 / AfterParticleLightVolume are promoted from /2 to
    // full resolution, that packed-column reconstruction must no longer be
    // applied. Verify all four embedded constants before changing the RT
    // dimensions so the feature fails closed as one coherent invariant.
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    const uintptr_t rvas[] = {
        0x00D40DDC, // 1/80 half-column parity scale
        0x00D40DE8, // 2x coordinate expansion
        0x00D40DF8, // -80 packed-column offset
        0x00D40DFC  // +80 packed-column offset
    };
    const float expected[] = {0.0125f, 2.0f, -80.0f, 80.0f};

    for (size_t i = 0; i < _countof(rvas); ++i)
    {
        if (!VerifyScalarBytes(reinterpret_cast<void*>(base + rvas[i]),
                               &expected[i], sizeof(float)))
        {
            Log("[SKIP] Particle RestoreDepthBuffer retail constant mismatch at RVA 0x%08X.",
                static_cast<unsigned>(rvas[i]));
            return false;
        }
    }

    return true;
}

static bool ApplyParticleFullResolutionDepthRestore(HMODULE exe)
{
    // Full-resolution identity mapping for RestoreDepthBuffer:
    //   1/80 -> 0 removes half-column parity classification,
    //   2    -> 1 removes the half-resolution x expansion,
    //   -80/+80 -> 0 remove the packed-column offsets.
    //
    // The resulting coordinate reconstruction reduces to the ordinary
    // full-resolution screen position multiplied by g_Resolution.
    const uintptr_t rvas[] = {
        0x00D40DDC, 0x00D40DE8, 0x00D40DF8, 0x00D40DFC
    };
    const float expected[] = {0.0125f, 2.0f, -80.0f, 80.0f};
    const float values[]   = {0.0f,    1.0f,   0.0f,  0.0f};

    return ApplyFloatGroup(
        exe,
        "WSParticleRender RestoreDepthBuffer full-resolution coordinates",
        rvas, expected, _countof(rvas), values);
}


static bool ApplyLightVolumeCoordinateResolutionScale(HMODULE exe, double scale)
{
    if (!std::isfinite(scale) || scale < 0.25 || scale > 2.0)
    {
        Log("[FAIL] LightVolumeCoordinateResolutionScale=%.3f outside 0.25..2.0.", scale);
        return false;
    }

    // WSLightVolumeManager::refresh/render-state path (vtable method 0x7FE560
    // -> helper 0x7FE060) derives a second internal resolution profile from
    // the physical backbuffer dimensions.
    //
    // Exact retail flow:
    //   VA 0x007FE07F..0x007FE09D : cache full screen width
    //   VA 0x007FE0A5             : fld qword ptr [0x00F7AC88] = 0.5
    //   VA 0x007FE0AD..0x007FE0CD : width * 0.5
    //   VA 0x007FE0D7..0x007FE121 : height * 0.5 using the same loaded factor
    //   VA 0x007FE139 onward       : derives reciprocal/coordinate values
    //                                from those cached dimensions.
    //
    // LightVolumeRT itself is /2 in retail. When the RT is raised to full
    // resolution, leaving this independent coordinate profile at 0.5 keeps
    // the LightVolume renderer mathematically half-resolution and can expose
    // the raw light-volume geometry during composition.
    //
    // Redirect ONLY this local FLD operand. The shared retail 0.5 constant is
    // not modified because it has many unrelated consumers.
    static double storage = 0.5;
    storage = scale;
    const bool ok = PatchAbsoluteOperand32(
        exe, 0x003FE0A5, 0xDD, 0x05, 0x00B7AC88,
        &storage, "LightVolume coordinate-resolution scale");
    if (ok)
        Log("[OK] WSLightVolume coordinate-resolution scale 0.500 -> %.3f.", scale);
    return ok;
}


static bool ApplyLightVolumeResolutionMultiplier(HMODULE exe, int multiplier)
{
    if (multiplier != 1 && multiplier != 2)
    {
        Log("[FAIL] LightVolumeResolutionMultiplier=%d; supported values are 1 or 2.", multiplier);
        return false;
    }
    if (multiplier == 1)
        return true;

    // LightVolumeRT derives its width/height directly from the backbuffer,
    // then halves both dimensions at the two local instructions below:
    //   VA 0x0080001F : shr cx,1
    //   VA 0x00800022 : shr dx,1
    // 2x quality removes only those two shifts => full resolution.
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    auto* widthShift  = reinterpret_cast<uint8_t*>(base + 0x0040001F);
    auto* heightShift = reinterpret_cast<uint8_t*>(base + 0x00400022);
    static const uint8_t expectedW[] = {0x66,0xD1,0xE9};
    static const uint8_t expectedH[] = {0x66,0xD1,0xEA};
    static const uint8_t nops[] = {0x90,0x90,0x90};

    if (std::memcmp(widthShift, expectedW, sizeof(expectedW)) != 0 ||
        std::memcmp(heightShift, expectedH, sizeof(expectedH)) != 0)
    {
        Log("[SKIP] LightVolumeRT half-resolution signatures do not match retail.");
        return false;
    }

    if (!WriteBytes(widthShift, nops, sizeof(nops)) ||
        !WriteBytes(heightShift, nops, sizeof(nops)))
    {
        Log("[FAIL] LightVolumeRT full-resolution writes failed.");
        return false;
    }

    Log("[OK] LightVolumeRT resolution multiplier 1x -> 2x (/2 -> full).");
    return true;
}

static bool ApplyWtfTransitionRingResolution(HMODULE exe, int resolution)
{
    if (resolution < 32 || resolution > 2048)
    {
        Log("[FAIL] WTF TransitionRingResolution=%d outside 32..2048.", resolution);
        return false;
    }

    // Three related transition render targets are all 128x128 in retail:
    //   WTFTransitionRingRT
    //   WTFTransitionRingRTTemp
    //   PreviousWTFTransitionRingRT
    //
    // Keep all six dimensions coherent.
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    const uintptr_t sites[] = {
        0x0057C4F0, 0x0057C4F5,
        0x0057C571, 0x0057C576,
        0x0057C5B3, 0x0057C5D8
    };

    for (uintptr_t rva : sites)
    {
        const auto* at = reinterpret_cast<const uint8_t*>(base + rva);
        if (at[0] != 0x68)
        {
            Log("[SKIP] WTF transition-ring push opcode mismatch at RVA 0x%08X.",
                static_cast<unsigned>(rva));
            return false;
        }

        uint32_t current = 0;
        std::memcpy(&current, at + 1, sizeof(current));
        if (current != 128u)
        {
            Log("[SKIP] WTF transition-ring native dimension mismatch at RVA 0x%08X.",
                static_cast<unsigned>(rva));
            return false;
        }
    }

    const uint32_t value = static_cast<uint32_t>(resolution);
    for (uintptr_t rva : sites)
    {
        auto* at = reinterpret_cast<uint8_t*>(base + rva);
        if (!WriteBytes(at + 1, &value, sizeof(value)))
        {
            Log("[FAIL] WTF transition-ring dimension write failed at RVA 0x%08X.",
                static_cast<unsigned>(rva));
            return false;
        }
    }

    Log("[OK] WTF transition-ring RT family 128x128 -> %dx%d.", resolution, resolution);
    return true;
}


static bool PatchMovEaxImm32(HMODULE exe, uintptr_t rva, uint32_t expected,
                             uint32_t value, const char* label)
{
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    auto* at = reinterpret_cast<uint8_t*>(base + rva);

    if (at[0] != 0xB8)
    {
        Log("[SKIP] %s opcode mismatch at RVA 0x%08X.", label,
            static_cast<unsigned>(rva));
        return false;
    }

    uint32_t current = 0;
    std::memcpy(&current, at + 1, sizeof(current));
    if (current != expected)
    {
        Log("[SKIP] %s native value mismatch at RVA 0x%08X: expected %u got %u.",
            label, static_cast<unsigned>(rva), expected, current);
        return false;
    }

    if (!WriteBytes(at + 1, &value, sizeof(value)))
    {
        Log("[FAIL] %s write failed at RVA 0x%08X.", label,
            static_cast<unsigned>(rva));
        return false;
    }

    Log("[OK] %s capacity %u -> %u.", label, expected, value);
    return true;
}

static bool ApplySimpleEngineLimits(HMODULE exe,
                                    int wsLuaCall,
                                    int wsParkingSpace,
                                    int wsParticleInfoData,
                                    int wsActivateSphere,
                                    int wallGraph)
{
    if (wsLuaCall < 1 || wsLuaCall > 4096 ||
        wsParkingSpace < 1 || wsParkingSpace > 4096 ||
        wsParticleInfoData < 100 || wsParticleInfoData > 20000 ||
        wsActivateSphere < 16 || wsActivateSphere > 8192 ||
        wallGraph < 1 || wallGraph > 4096)
    {
        Log("[FAIL] One or more simple engine-limit values are outside guarded ranges.");
        return false;
    }

    if (wsLuaCall != 20 &&
        !PatchMovEaxImm32(exe, 0x005F6C74, 20,
                          static_cast<uint32_t>(wsLuaCall), "WSLuaCall"))
        return false;

    if (wsParkingSpace != 32 &&
        !PatchMovEaxImm32(exe, 0x0050706E, 32,
                          static_cast<uint32_t>(wsParkingSpace), "WSParkingSpace"))
        return false;

    if (wsParticleInfoData != 1400 &&
        !PatchMovEaxImm32(exe, 0x005D3173, 1400,
                          static_cast<uint32_t>(wsParticleInfoData), "WSParticleInfoData"))
        return false;

    if (wsActivateSphere != 256 &&
        !PatchMovEaxImm32(exe, 0x005AE7E6, 256,
                          static_cast<uint32_t>(wsActivateSphere), "WSActivateSphere pool"))
        return false;

    if (wallGraph != 50)
    {
        if (!PatchMovEaxImm32(exe, 0x005F6B91, 50,
                              static_cast<uint32_t>(wallGraph), "WallPoint"))
            return false;
        if (!PatchMovEaxImm32(exe, 0x005F6BDE, 50,
                              static_cast<uint32_t>(wallGraph), "WallSegment"))
            return false;
    }

    return true;
}


static bool PatchPushImm32(HMODULE exe, uintptr_t rva, uint32_t expected,
                           uint32_t value, const char* label)
{
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    auto* at = reinterpret_cast<uint8_t*>(base + rva);
    if (at[0] != 0x68)
    {
        Log("[SKIP] %s push opcode mismatch at RVA 0x%08X.",
            label, static_cast<unsigned>(rva));
        return false;
    }

    uint32_t current = 0;
    std::memcpy(&current, at + 1, sizeof(current));
    if (current != expected)
    {
        Log("[SKIP] %s push immediate mismatch at RVA 0x%08X: expected %u got %u.",
            label, static_cast<unsigned>(rva), expected, current);
        return false;
    }

    if (!WriteBytes(at + 1, &value, sizeof(value)))
    {
        Log("[FAIL] %s push immediate write failed at RVA 0x%08X.",
            label, static_cast<unsigned>(rva));
        return false;
    }

    return true;
}

static bool PatchCmpImm32AtPlus6(HMODULE exe, uintptr_t rva, uint32_t expected,
                                uint32_t value, const char* label)
{
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    auto* at = reinterpret_cast<uint8_t*>(base + rva);

    if (at[0] != 0x81)
    {
        Log("[SKIP] %s cmp opcode mismatch at RVA 0x%08X.",
            label, static_cast<unsigned>(rva));
        return false;
    }

    uint32_t current = 0;
    std::memcpy(&current, at + 6, sizeof(current));
    if (current != expected)
    {
        Log("[SKIP] %s cmp immediate mismatch at RVA 0x%08X: expected %u got %u.",
            label, static_cast<unsigned>(rva), expected, current);
        return false;
    }

    if (!WriteBytes(at + 6, &value, sizeof(value)))
    {
        Log("[FAIL] %s cmp immediate write failed at RVA 0x%08X.",
            label, static_cast<unsigned>(rva));
        return false;
    }

    return true;
}

static bool ApplyWSPhysicsParticleCapacity(HMODULE exe, int capacity)
{
    if (capacity < 100 || capacity > 20000)
    {
        Log("[FAIL] WSPhysicsParticleCapacity=%d outside 100..20000.", capacity);
        return false;
    }
    if (capacity == 1000)
        return true;

    // Retail owns the same 1000 limit at:
    // - pool allocation/init push, VA 0x009DB5D2
    // - runtime active ceiling,   VA 0x009DB66B
    if (!PatchPushImm32(exe, 0x005DB5D2, 1000,
                        static_cast<uint32_t>(capacity),
                        "WSPhysicsParticle allocation"))
        return false;

    if (!PatchCmpImm32AtPlus6(exe, 0x005DB66B, 1000,
                              static_cast<uint32_t>(capacity),
                              "WSPhysicsParticle runtime ceiling"))
        return false;

    Log("[OK] WSPhysicsParticle capacity 1000 -> %d (allocation + runtime ceiling).", capacity);
    return true;
}

static bool ApplyWSParticleRenderCapacities(HMODULE exe,
                                            int mainCapacity,
                                            int mediumCapacity,
                                            int smallCapacity)
{
    if (mainCapacity < 1000 || mainCapacity > 50000 ||
        mediumCapacity < 250 || mediumCapacity > 20000 ||
        smallCapacity < 100 || smallCapacity > 10000)
    {
        Log("[FAIL] WSParticleRender capacities outside guarded ranges.");
        return false;
    }

    if (mainCapacity == 4500 && mediumCapacity == 1000 && smallCapacity == 500)
        return true;

    const uint64_t mainBytes64 = static_cast<uint64_t>(mainCapacity) * 68ull;
    const uint64_t mediumBytes64 = static_cast<uint64_t>(mediumCapacity) * 68ull;
    const uint64_t smallBytes64 = static_cast<uint64_t>(smallCapacity) * 68ull;
    const uint64_t mainScratch64 = static_cast<uint64_t>(mainCapacity) * 8ull;
    const uint64_t mediumScratch64 = static_cast<uint64_t>(mediumCapacity) * 8ull;
    if (mainBytes64 > 0xFFFFFFFFull || mediumBytes64 > 0xFFFFFFFFull ||
        smallBytes64 > 0xFFFFFFFFull || mainScratch64 > 0xFFFFFFFFull ||
        mediumScratch64 > 0xFFFFFFFFull)
    {
        Log("[FAIL] WSParticleRender derived allocation size overflow.");
        return false;
    }

    const uint32_t mainBytes = static_cast<uint32_t>(mainBytes64);
    const uint32_t mediumBytes = static_cast<uint32_t>(mediumBytes64);
    const uint32_t smallBytes = static_cast<uint32_t>(smallBytes64);
    const uint32_t mainScratch = static_cast<uint32_t>(mainScratch64);
    const uint32_t mediumScratch = static_cast<uint32_t>(mediumScratch64);

    // Allocation arenas: count * 68 bytes.
    if (!PatchPushImm32(exe, 0x002E7B83, 4500u * 68u, mainBytes,
                        "WSParticleRender main arena"))
        return false;
    if (!PatchPushImm32(exe, 0x002E7BAD, 1000u * 68u, mediumBytes,
                        "WSParticleRender medium arena"))
        return false;
    if (!PatchPushImm32(exe, 0x002E7B9C, 500u * 68u, smallBytes,
                        "WSParticleRender small arena"))
        return false;

    // Sort scratch follows main and medium counts at 8 bytes/entry.
    if (!PatchPushImm32(exe, 0x002E7BBE, 4500u * 8u, mainScratch,
                        "WSParticleRender main sort scratch"))
        return false;
    if (!PatchPushImm32(exe, 0x002E7BCF, 1000u * 8u, mediumScratch,
                        "WSParticleRender medium sort scratch"))
        return false;

    // Main class runtime caps.
    const uintptr_t mainCaps[] = {0x002E3F72, 0x002E4066, 0x002E40CD};
    for (uintptr_t rva : mainCaps)
        if (!PatchCmpImm32AtPlus6(exe, rva, 4500,
                                  static_cast<uint32_t>(mainCapacity),
                                  "WSParticleRender main runtime cap"))
            return false;

    // Medium runtime cap.
    if (!PatchCmpImm32AtPlus6(exe, 0x002E4097, 1000,
                              static_cast<uint32_t>(mediumCapacity),
                              "WSParticleRender medium runtime cap"))
        return false;

    // Small class uses both N and N-1 comparisons in retail.
    const uint32_t smallMinusOne = static_cast<uint32_t>(smallCapacity - 1);
    if (!PatchCmpImm32AtPlus6(exe, 0x002E40FA, 499, smallMinusOne,
                              "WSParticleRender small runtime cap A"))
        return false;
    if (!PatchCmpImm32AtPlus6(exe, 0x002E412E, 500,
                              static_cast<uint32_t>(smallCapacity),
                              "WSParticleRender small runtime cap B"))
        return false;
    if (!PatchCmpImm32AtPlus6(exe, 0x002E416A, 499, smallMinusOne,
                              "WSParticleRender small runtime cap C"))
        return false;

    Log("[OK] WSParticleRender capacities main=%d medium=%d small=%d with matched arenas/caps/scratch.",
        mainCapacity, mediumCapacity, smallCapacity);
    return true;
}


static bool ApplyValidatedParticleCapacityPack037(HMODULE exe)
{
    // 0.37 combines the historically validated particle/physics capacity
    // lineage into one fail-closed transaction.
    //
    // WSPhysicsParticle:
    //   allocation 1000 -> 2000
    //   runtime cap 1000 -> 2000
    //
    // WSParticleRender:
    //   arenas 4500/1000/500 -> 9000/2000/1000
    //   sort scratch follows 9000/2000
    //   every matching runtime cap follows the same capacities.
    //
    // All 14 owners are verified before the first write.
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);

    struct Site
    {
        uintptr_t rva;
        uint8_t opcode;
        uint8_t immOffset;
        uint32_t expected;
        uint32_t value;
        const char* label;
    };

    const Site sites[] = {
        {0x005DB5D2, 0x68, 1, 1000u, 2000u, "WSPhysicsParticle allocation"},
        {0x005DB66B, 0x81, 6, 1000u, 2000u, "WSPhysicsParticle runtime ceiling"},

        {0x002E7B83, 0x68, 1, 4500u * 68u, 9000u * 68u, "WSParticleRender main arena"},
        {0x002E7BAD, 0x68, 1, 1000u * 68u, 2000u * 68u, "WSParticleRender medium arena"},
        {0x002E7B9C, 0x68, 1,  500u * 68u, 1000u * 68u, "WSParticleRender small arena"},
        {0x002E7BBE, 0x68, 1, 4500u * 8u, 9000u * 8u, "WSParticleRender main sort scratch"},
        {0x002E7BCF, 0x68, 1, 1000u * 8u, 2000u * 8u, "WSParticleRender medium sort scratch"},

        {0x002E3F72, 0x81, 6, 4500u, 9000u, "WSParticleRender main cap A"},
        {0x002E4066, 0x81, 6, 4500u, 9000u, "WSParticleRender main cap B"},
        {0x002E40CD, 0x81, 6, 4500u, 9000u, "WSParticleRender main cap C"},
        {0x002E4097, 0x81, 6, 1000u, 2000u, "WSParticleRender medium cap"},
        {0x002E40FA, 0x81, 6,  499u,  999u, "WSParticleRender small cap A"},
        {0x002E412E, 0x81, 6,  500u, 1000u, "WSParticleRender small cap B"},
        {0x002E416A, 0x81, 6,  499u,  999u, "WSParticleRender small cap C"}
    };

    for (const Site& site : sites)
    {
        auto* at = reinterpret_cast<uint8_t*>(base + site.rva);
        if (at[0] != site.opcode)
        {
            Log("[SKIP] 0.37 %s opcode mismatch at RVA 0x%08X.",
                site.label, static_cast<unsigned>(site.rva));
            return false;
        }

        uint32_t current = 0;
        std::memcpy(&current, at + site.immOffset, sizeof(current));
        if (current != site.expected)
        {
            Log("[SKIP] 0.37 %s value mismatch: expected %u got %u.",
                site.label, site.expected, current);
            return false;
        }
    }

    size_t written = 0;
    for (; written < (sizeof(sites) / sizeof(sites[0])); ++written)
    {
        const Site& site = sites[written];
        auto* at = reinterpret_cast<uint8_t*>(base + site.rva);
        if (!WriteBytes(at + site.immOffset, &site.value, sizeof(site.value)))
            break;
    }

    if (written != (sizeof(sites) / sizeof(sites[0])))
    {
        for (size_t i = written; i > 0; --i)
        {
            const Site& site = sites[i - 1];
            auto* at = reinterpret_cast<uint8_t*>(base + site.rva);
            WriteBytes(at + site.immOffset, &site.expected, sizeof(site.expected));
        }

        Log("[FAIL] 0.37 particle-capacity pack failed at %s; prior sites restored.",
            sites[written].label);
        return false;
    }

    Log("[OK] 0.37 particle-capacity pack applied atomically across 14 owners.");
    Log("[OK] WSPhysicsParticle 1000->2000; WSParticleRender 4500/1000/500->9000/2000/1000.");
    return true;
}


static bool ApplyWSPhGridObjectCapacity(HMODULE exe, int capacity)
{
    if (capacity != 1000 && capacity != 2000)
    {
        Log("[FAIL] WSPhGridObjectCapacity=%d; supported values are 1000 or 2000.", capacity);
        return false;
    }
    if (capacity == 1000)
        return true;

    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);

    // Exact retail initializer:
    //   VA 0x009AE665 / RVA 0x005AE665: mov edi,1000
    // WSPhGridObject then stores EDI into both capacity fields
    //   0x0132AF7C / 0x0132AF78,
    // sets stride 0x38, and calls the generic pool initializer at 0x00E44D00.
    //
    // The immediately following WSHKCreationDataContainer shares EDI and MUST
    // remain at 1000. We therefore redirect only the first generic initializer
    // CALL through an ASI wrapper that restores EDI=1000 before returning.
    //
    // Three active-count guards compare [0x0132AF80] against 1000 and must
    // move together to 2000:
    //   VA 0x006CC64C / RVA 0x002CC64C
    //   VA 0x006CCB30 / RVA 0x002CCB30
    //   VA 0x006CCDD2 / RVA 0x002CCDD2
    auto* loadAt = reinterpret_cast<uint8_t*>(base + 0x005AE665);
    auto* callAt = reinterpret_cast<uint8_t*>(base + 0x005AE6A6);
    const uintptr_t capRvas[] = {0x002CC64C, 0x002CCB30, 0x002CCDD2};

    const uint8_t expectedLoad[5] = {0xBF,0xE8,0x03,0x00,0x00};
    const uint8_t expectedCall[5] = {0xE8,0x55,0x66,0x49,0x00};
    const uint8_t expectedCmpPrefix[6] = {0x81,0x3D,0x80,0xAF,0x32,0x01};

    if (std::memcmp(loadAt, expectedLoad, sizeof(expectedLoad)) != 0)
    {
        Log("[SKIP] WSPhGridObject shared EDI initializer bytes mismatch.");
        return false;
    }
    if (std::memcmp(callAt, expectedCall, sizeof(expectedCall)) != 0)
    {
        Log("[SKIP] WSPhGridObject generic initializer CALL mismatch.");
        return false;
    }

    for (uintptr_t rva : capRvas)
    {
        auto* at = reinterpret_cast<uint8_t*>(base + rva);
        if (std::memcmp(at, expectedCmpPrefix, sizeof(expectedCmpPrefix)) != 0)
        {
            Log("[SKIP] WSPhGridObject active-cap prefix mismatch at RVA 0x%08X.",
                static_cast<unsigned>(rva));
            return false;
        }
        uint32_t current = 0;
        std::memcpy(&current, at + 6, sizeof(current));
        if (current != 1000u)
        {
            Log("[SKIP] WSPhGridObject active-cap value mismatch at RVA 0x%08X: %u.",
                static_cast<unsigned>(rva), current);
            return false;
        }
    }

    g_wsPhGridInitOriginal = base + 0x00A44D00;

    // Prepare all replacement bytes before touching retail code.
    uint8_t loadPatch[5] = {0xBF,0,0,0,0};
    const uint32_t value = 2000u;
    std::memcpy(loadPatch + 1, &value, sizeof(value));

    const intptr_t delta =
        reinterpret_cast<intptr_t>(&WSPhGridInitWrapper) -
        (reinterpret_cast<intptr_t>(callAt) + 5);
    if (delta < INT32_MIN || delta > INT32_MAX)
    {
        Log("[FAIL] WSPhGridObject wrapper is outside rel32 range.");
        return false;
    }
    uint8_t callPatch[5] = {0xE8,0,0,0,0};
    const int32_t rel = static_cast<int32_t>(delta);
    std::memcpy(callPatch + 1, &rel, sizeof(rel));

    // Fail-closed transaction with explicit rollback.
    bool loadWritten = false;
    bool callWritten = false;
    size_t capsWritten = 0;

    if (!WriteBytes(loadAt, loadPatch, sizeof(loadPatch)))
        goto rollback;
    loadWritten = true;

    if (!WriteBytes(callAt, callPatch, sizeof(callPatch)))
        goto rollback;
    callWritten = true;

    for (; capsWritten < 3; ++capsWritten)
    {
        auto* at = reinterpret_cast<uint8_t*>(base + capRvas[capsWritten]);
        if (!WriteBytes(at + 6, &value, sizeof(value)))
            goto rollback;
    }

    Log("[OK] WSPhGridObject capacity 1000 -> 2000 with matched three active caps.");
    Log("[OK] WSHKCreationDataContainer preserved at 1000 via post-init EDI restore.");
    return true;

rollback:
    for (size_t i = capsWritten; i > 0; --i)
    {
        const uint32_t retail = 1000u;
        auto* at = reinterpret_cast<uint8_t*>(base + capRvas[i - 1]);
        WriteBytes(at + 6, &retail, sizeof(retail));
    }
    if (callWritten)
        WriteBytes(callAt, expectedCall, sizeof(expectedCall));
    if (loadWritten)
        WriteBytes(loadAt, expectedLoad, sizeof(expectedLoad));

    Log("[FAIL] WSPhGridObject 0.43 transaction failed; prior writes restored.");
    return false;
}


static bool ApplyHavokBroadPhaseQuerySize(HMODULE exe, int querySize)
{
    if (querySize < 256 || querySize > 16384)
    {
        Log("[FAIL] HavokBroadPhaseQuerySize=%d outside 256..16384.", querySize);
        return false;
    }
    if (querySize == 1024)
        return true;

    // Recovered from the exact retail -> V200 byte manifest:
    //   RAW 0x006C4560: 04 -> 08
    // This is byte +1 of the little-endian dword 1024 -> 2048.
    // Runtime mapping for this .text area is RAW + 0xE00:
    //   RVA 0x006C535F = 00 04 00 00 (1024).
    //
    // Adjacent historical TOI owner cross-check:
    //   RAW 0x006C45B0: FA 00 -> 00 02
    //   matching the independently audited TOI field in the same Havok block.
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    auto* at = reinterpret_cast<uint8_t*>(base + 0x006C535F);

    uint32_t current = 0;
    std::memcpy(&current, at, sizeof(current));
    if (current != 1024u)
    {
        Log("[SKIP] Havok broad-phase retail value mismatch: expected 1024 got %u.", current);
        return false;
    }

    const uint32_t value = static_cast<uint32_t>(querySize);
    if (!WriteBytes(at, &value, sizeof(value)))
    {
        Log("[FAIL] Havok broad-phase query-size write failed.");
        return false;
    }

    Log("[OK] Havok broad-phase query size 1024 -> %d.", querySize);
    return true;
}


static bool ApplyHavokToiEventQueue(HMODULE exe, int capacity)
{
    if (capacity < 64 || capacity > 8192)
    {
        Log("[FAIL] HavokTOIEventQueue=%d outside 64..8192.", capacity);
        return false;
    }
    if (capacity == 250)
        return true;

    // Exact retail field initialization at VA 0x00AC53AD:
    //   C7 46 64 FA 00 00 00
    // => m_sizeOfToiEventQueue-like field = 250.
    // The old cumulative V262 note 512 -> 1024 was therefore not the retail
    // baseline; an earlier cumulative stage had already raised 250 -> 512.
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    auto* at = reinterpret_cast<uint8_t*>(base + 0x006C53AD);
    const uint8_t expectedPrefix[3] = {0xC7,0x46,0x64};
    if (std::memcmp(at, expectedPrefix, sizeof(expectedPrefix)) != 0)
    {
        Log("[SKIP] Havok TOI field signature mismatch.");
        return false;
    }

    uint32_t current = 0;
    std::memcpy(&current, at + 3, sizeof(current));
    if (current != 250u)
    {
        Log("[SKIP] Havok TOI retail value mismatch: expected 250 got %u.", current);
        return false;
    }

    const uint32_t value = static_cast<uint32_t>(capacity);
    if (!WriteBytes(at + 3, &value, sizeof(value)))
        return false;

    Log("[OK] Havok TOI event queue 250 -> %d.", capacity);
    return true;
}

static bool ApplyStreamingJobCapacity(HMODULE exe, int capacity)
{
    if (capacity < 128 || capacity > 16384)
    {
        Log("[FAIL] StreamingJobCapacity=%d outside 128..16384.", capacity);
        return false;
    }
    if (capacity == 1200)
        return true;

    // .secu VA 0x0162F336:
    //   BF B0 04 00 00   mov edi,1200
    // EDI is then stored into BOTH WSReadJob and WSUncompressJob pool
    // capacity pairs before any later overwrite.
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    auto* at = reinterpret_cast<uint8_t*>(base + 0x0122F336);
    if (at[0] != 0xBF)
    {
        Log("[SKIP] Streaming job capacity opcode mismatch.");
        return false;
    }

    uint32_t current = 0;
    std::memcpy(&current, at + 1, sizeof(current));
    if (current != 1200u)
    {
        Log("[SKIP] Streaming job retail capacity mismatch: expected 1200 got %u.", current);
        return false;
    }

    const uint32_t value = static_cast<uint32_t>(capacity);
    if (!WriteBytes(at + 1, &value, sizeof(value)))
        return false;

    Log("[OK] WSReadJob / WSUncompressJob capacity 1200/1200 -> %d/%d.",
        capacity, capacity);
    return true;
}

static bool ApplyPblCrcTreeNodeCapacity(HMODULE exe, int capacity)
{
    if (capacity < 1000 || capacity > 65534)
    {
        Log("[FAIL] PblCRCTreeNodeCapacity=%d outside 1000..65534.", capacity);
        return false;
    }
    if (capacity == 40000)
        return true;

    // .secu VA 0x016055F2: push 40000.
    // Tree links are WORD-sized and use 0xFFFF as sentinel, so 65534 is the
    // hard structural ceiling.
    if (!PatchPushImm32(exe, 0x012055F2, 40000u,
                        static_cast<uint32_t>(capacity),
                        "PblCRCTreeNode"))
        return false;

    Log("[OK] PblCRCTreeNode capacity 40000 -> %d.", capacity);
    return true;
}


static bool PatchDwordScalar(HMODULE exe, uintptr_t rva, uint32_t expected,
                             uint32_t value, const char* label)
{
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    auto* at = reinterpret_cast<uint32_t*>(base + rva);
    uint32_t current = 0;
    std::memcpy(&current, at, sizeof(current));
    if (current != expected)
    {
        Log("[SKIP] %s scalar mismatch at RVA 0x%08X: expected %u got %u.",
            label, static_cast<unsigned>(rva), expected, current);
        return false;
    }

    if (!WriteBytes(at, &value, sizeof(value)))
    {
        Log("[FAIL] %s scalar write failed at RVA 0x%08X.",
            label, static_cast<unsigned>(rva));
        return false;
    }

    Log("[OK] %s %u -> %u.", label, expected, value);
    return true;
}

static bool ApplyClassOwnedPoolConstants(HMODULE exe,
                                         int wsDamageSphere,
                                         int wsInventoryStateStow)
{
    if (wsDamageSphere < 64 || wsDamageSphere > 8192 ||
        wsInventoryStateStow < 8 || wsInventoryStateStow > 1024)
    {
        Log("[FAIL] Class-owned pool constant outside guarded range.");
        return false;
    }

    // Retail .rdata constants, each with one proven pool-initializer consumer:
    // WSDamageSphere    VA 0x00F87564 = 512
    // InventoryStow     VA 0x00FD0AF4 = 32
    if (wsDamageSphere != 512 &&
        !PatchDwordScalar(exe, 0x00B87564, 512,
                          static_cast<uint32_t>(wsDamageSphere),
                          "WSDamageSphere capacity"))
        return false;

    if (wsInventoryStateStow != 32 &&
        !PatchDwordScalar(exe, 0x00BD0AF4, 32,
                          static_cast<uint32_t>(wsInventoryStateStow),
                          "WSInventoryStateStow capacity"))
        return false;

    return true;
}


static bool ApplyValidatedEngineLimitsPack035(HMODULE exe)
{
    // 0.35 bundles four historically validated, structurally independent
    // engine-capacity owners into one fail-closed transaction:
    //   WSLuaCall            20 -> 40
    //   WSDamageSphere      512 -> 1024
    //   WSInventoryStateStow 32 -> 64
    //   PblCRCTreeNode    40000 -> 60000
    //
    // Every retail/Core1 site is verified before the first write. If any
    // later write fails, every earlier site is restored to its retail value.
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);

    auto* lua = reinterpret_cast<uint8_t*>(base + 0x005F6C74);
    auto* damage = reinterpret_cast<uint32_t*>(base + 0x00B87564);
    auto* inventory = reinterpret_cast<uint32_t*>(base + 0x00BD0AF4);
    auto* crc = reinterpret_cast<uint8_t*>(base + 0x012055F2);

    if (lua[0] != 0xB8)
    {
        Log("[SKIP] 0.35 WSLuaCall opcode mismatch.");
        return false;
    }
    uint32_t luaCurrent = 0;
    std::memcpy(&luaCurrent, lua + 1, sizeof(luaCurrent));
    if (luaCurrent != 20u)
    {
        Log("[SKIP] 0.35 WSLuaCall value mismatch: expected 20 got %u.", luaCurrent);
        return false;
    }

    uint32_t damageCurrent = 0;
    std::memcpy(&damageCurrent, damage, sizeof(damageCurrent));
    if (damageCurrent != 512u)
    {
        Log("[SKIP] 0.35 WSDamageSphere value mismatch: expected 512 got %u.", damageCurrent);
        return false;
    }

    uint32_t inventoryCurrent = 0;
    std::memcpy(&inventoryCurrent, inventory, sizeof(inventoryCurrent));
    if (inventoryCurrent != 32u)
    {
        Log("[SKIP] 0.35 InventoryStow value mismatch: expected 32 got %u.", inventoryCurrent);
        return false;
    }

    if (crc[0] != 0x68)
    {
        Log("[SKIP] 0.35 PblCRCTreeNode push opcode mismatch.");
        return false;
    }
    uint32_t crcCurrent = 0;
    std::memcpy(&crcCurrent, crc + 1, sizeof(crcCurrent));
    if (crcCurrent != 40000u)
    {
        Log("[SKIP] 0.35 PblCRCTreeNode value mismatch: expected 40000 got %u.", crcCurrent);
        return false;
    }

    const uint32_t luaNew = 40u;
    const uint32_t damageNew = 1024u;
    const uint32_t inventoryNew = 64u;
    const uint32_t crcNew = 60000u;

    if (!WriteBytes(lua + 1, &luaNew, sizeof(luaNew)))
    {
        Log("[FAIL] 0.35 WSLuaCall write failed.");
        return false;
    }

    if (!WriteBytes(damage, &damageNew, sizeof(damageNew)))
    {
        const uint32_t retail = 20u;
        WriteBytes(lua + 1, &retail, sizeof(retail));
        Log("[FAIL] 0.35 WSDamageSphere write failed; WSLuaCall restored.");
        return false;
    }

    if (!WriteBytes(inventory, &inventoryNew, sizeof(inventoryNew)))
    {
        const uint32_t luaRetail = 20u;
        const uint32_t damageRetail = 512u;
        WriteBytes(damage, &damageRetail, sizeof(damageRetail));
        WriteBytes(lua + 1, &luaRetail, sizeof(luaRetail));
        Log("[FAIL] 0.35 InventoryStow write failed; prior sites restored.");
        return false;
    }

    if (!WriteBytes(crc + 1, &crcNew, sizeof(crcNew)))
    {
        const uint32_t luaRetail = 20u;
        const uint32_t damageRetail = 512u;
        const uint32_t inventoryRetail = 32u;
        WriteBytes(inventory, &inventoryRetail, sizeof(inventoryRetail));
        WriteBytes(damage, &damageRetail, sizeof(damageRetail));
        WriteBytes(lua + 1, &luaRetail, sizeof(luaRetail));
        Log("[FAIL] 0.35 PblCRCTreeNode write failed; prior sites restored.");
        return false;
    }

    Log("[OK] 0.35 validated engine-limit pack applied atomically.");
    Log("[OK] WSLuaCall 20->40, WSDamageSphere 512->1024, InventoryStow 32->64, PblCRC 40000->60000.");
    return true;
}


static bool ApplyValidatedSimpleEngineLimitsPack036(HMODULE exe)
{
    // 0.36 bundles the remaining historically validated simple fixed-pool
    // owners into one fail-closed transaction:
    //   WSParkingSpace      32 -> 64
    //   WSParticleInfoData 1400 -> 2800
    //   WSActivateSphere   256 -> 512
    //   WallPoint           50 -> 100
    //   WallSegment         50 -> 100
    //
    // All five MOV EAX,imm32 owners are verified before the first write.
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);

    struct Site
    {
        uintptr_t rva;
        uint32_t expected;
        uint32_t value;
        const char* label;
    };

    const Site sites[] = {
        {0x0050706E,   32u,   64u, "WSParkingSpace"},
        {0x005D3173, 1400u, 2800u, "WSParticleInfoData"},
        {0x005AE7E6,  256u,  512u, "WSActivateSphere"},
        {0x005F6B91,   50u,  100u, "WallPoint"},
        {0x005F6BDE,   50u,  100u, "WallSegment"}
    };

    for (const Site& site : sites)
    {
        auto* at = reinterpret_cast<uint8_t*>(base + site.rva);
        if (at[0] != 0xB8)
        {
            Log("[SKIP] 0.36 %s opcode mismatch at RVA 0x%08X.",
                site.label, static_cast<unsigned>(site.rva));
            return false;
        }

        uint32_t current = 0;
        std::memcpy(&current, at + 1, sizeof(current));
        if (current != site.expected)
        {
            Log("[SKIP] 0.36 %s value mismatch: expected %u got %u.",
                site.label, site.expected, current);
            return false;
        }
    }

    size_t written = 0;
    for (; written < (sizeof(sites) / sizeof(sites[0])); ++written)
    {
        const Site& site = sites[written];
        auto* at = reinterpret_cast<uint8_t*>(base + site.rva);
        if (!WriteBytes(at + 1, &site.value, sizeof(site.value)))
            break;
    }

    if (written != (sizeof(sites) / sizeof(sites[0])))
    {
        for (size_t i = written; i > 0; --i)
        {
            const Site& site = sites[i - 1];
            auto* at = reinterpret_cast<uint8_t*>(base + site.rva);
            WriteBytes(at + 1, &site.expected, sizeof(site.expected));
        }

        Log("[FAIL] 0.36 simple engine-limit pack write failed at %s; prior sites restored.",
            sites[written].label);
        return false;
    }

    Log("[OK] 0.36 simple engine-limit pack applied atomically.");
    Log("[OK] Parking 32->64, ParticleInfo 1400->2800, ActivateSphere 256->512, WallPoint/WallSegment 50->100.");
    return true;
}


static bool ApplyWSDecalCapacity(HMODULE exe, int capacity)
{
    if (capacity < 100 || capacity > 5000)
    {
        Log("[FAIL] WSDecalCapacity=%d outside 100..5000.", capacity);
        return false;
    }
    if (capacity == 400)
        return true;

    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    auto* pool = reinterpret_cast<uint8_t*>(base + 0x0058D7C9);
    auto* cap  = reinterpret_cast<uint8_t*>(base + 0x0058E997);

    // 0.34 treats WSDecal as one atomic owner:
    // - pool initialization, VA 0x0098D7C9: push 0x190
    // - active-list ceiling, VA 0x0098E997: cmp ecx,0x190
    // Verify both sites before changing either one.
    if (pool[0] != 0x68)
    {
        Log("[SKIP] WSDecal pool push opcode mismatch.");
        return false;
    }

    uint32_t poolCurrent = 0;
    std::memcpy(&poolCurrent, pool + 1, sizeof(poolCurrent));
    if (poolCurrent != 400u)
    {
        Log("[SKIP] WSDecal pool value mismatch: expected 400 got %u.", poolCurrent);
        return false;
    }

    const uint8_t expectedPrefix[2] = {0x81,0xF9};
    if (std::memcmp(cap, expectedPrefix, sizeof(expectedPrefix)) != 0)
    {
        Log("[SKIP] WSDecal active-ceiling opcode mismatch.");
        return false;
    }

    uint32_t capCurrent = 0;
    std::memcpy(&capCurrent, cap + 2, sizeof(capCurrent));
    if (capCurrent != 400u)
    {
        Log("[SKIP] WSDecal active-ceiling value mismatch: expected 400 got %u.", capCurrent);
        return false;
    }

    const uint32_t value = static_cast<uint32_t>(capacity);
    if (!WriteBytes(pool + 1, &value, sizeof(value)))
    {
        Log("[FAIL] WSDecal pool write failed.");
        return false;
    }

    if (!WriteBytes(cap + 2, &value, sizeof(value)))
    {
        const uint32_t retail = 400u;
        WriteBytes(pool + 1, &retail, sizeof(retail));
        Log("[FAIL] WSDecal active-ceiling write failed; pool restored to 400.");
        return false;
    }

    Log("[OK] WSDecal atomic pool + active ceiling 400 -> %d.", capacity);
    return true;
}


static bool ApplyCoalescedReadBatchByteLimit(HMODULE exe, int byteLimit)
{
    if (byteLimit < 65536 || byteLimit > 268435456)
    {
        Log("[FAIL] CoalescedReadBatchByteLimit=%d outside 64KiB..256MiB.", byteLimit);
        return false;
    }
    if (byteLimit == 512000)
        return true;

    // Streaming/coalescing path at VA 0x00DB5CE9:
    //   accumulatedBytes + candidateBytes
    //   cmp eax,0x7D000
    // Candidate bytes are derived from sector span * 0x800 (2048 bytes).
    // Retail threshold = 0x7D000 = 512000 bytes.
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    auto* at = reinterpret_cast<uint8_t*>(base + 0x009B5CE9);
    if (at[0] != 0x3D)
    {
        Log("[SKIP] Coalesced-read batch cmp opcode mismatch.");
        return false;
    }

    uint32_t current = 0;
    std::memcpy(&current, at + 1, sizeof(current));
    if (current != 0x0007D000u)
    {
        Log("[SKIP] Coalesced-read batch retail limit mismatch: expected 512000 got %u.", current);
        return false;
    }

    const uint32_t value = static_cast<uint32_t>(byteLimit);
    if (!WriteBytes(at + 1, &value, sizeof(value)))
        return false;

    Log("[OK] Coalesced read batch byte limit 512000 -> %d.", byteLimit);
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


static bool ApplyWSDynamicPartPriorityRadius(HMODULE exe, float radius)
{
    // WSDynamicPart priority scoring function at VA 0x00669980 uses:
    //   proximity = max(625.0 - distance_like_value^2, 0)
    // 625 = 25^2.
    //
    // The two constants below are referenced ONLY by that function:
    //   VA 0x00FC76DC : float  625.0f
    //   VA 0x00FC77C8 : double 625.0
    //
    // Keep both values coherent when changing the radius.
    if (radius < 1.0f || radius > 500.0f)
    {
        Log("[FAIL] WSDynamicPartPriorityRadius %.3f out of safe A/B range.", radius);
        return false;
    }

    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    constexpr uintptr_t kFloatRva  = 0x00BC76DC;
    constexpr uintptr_t kDoubleRva = 0x00BC77C8;

    auto* f32 = reinterpret_cast<float*>(base + kFloatRva);
    auto* f64 = reinterpret_cast<double*>(base + kDoubleRva);

    const float expectedF = 625.0f;
    const double expectedD = 625.0;

    if (std::memcmp(f32, &expectedF, sizeof(expectedF)) != 0)
    {
        Log("[SKIP] WSDynamicPart 625.0f constant mismatch at RVA 0x%08X.",
            static_cast<unsigned>(kFloatRva));
        return false;
    }
    if (std::memcmp(f64, &expectedD, sizeof(expectedD)) != 0)
    {
        Log("[SKIP] WSDynamicPart 625.0 constant mismatch at RVA 0x%08X.",
            static_cast<unsigned>(kDoubleRva));
        return false;
    }

    const float radiusSqF = radius * radius;
    const double radiusSqD = static_cast<double>(radius) * static_cast<double>(radius);

    if (!WriteBytes(f32, &radiusSqF, sizeof(radiusSqF)))
    {
        Log("[FAIL] WSDynamicPart float radius constant write failed.");
        return false;
    }
    if (!WriteBytes(f64, &radiusSqD, sizeof(radiusSqD)))
    {
        // Fail closed as much as possible: restore first constant.
        WriteBytes(f32, &expectedF, sizeof(expectedF));
        Log("[FAIL] WSDynamicPart double radius constant write failed; float restored.");
        return false;
    }

    Log("[OK] WSDynamicPart priority radius %.3f -> %.3f (squared %.3f -> %.3f).",
        25.0f, radius, 625.0f, radiusSqF);
    return true;
}

static bool ApplyUiMeshCacheMiB(HMODULE exe, int mib)
{
    // Exact Scaleform _Mesh_Cache constructor owner.
    // Retail: 8 MiB. Historical V200: 16 MiB.
    if (mib != 8 && mib != 16)
    {
        Log("[FAIL] UI MeshCacheMiB=%d; supported audited values are 8 or 16.", mib);
        return false;
    }
    if (mib == 8) return true;

    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    auto* at = reinterpret_cast<uint8_t*>(base + 0x007B4667);
    static const uint8_t expected[7] = {0xC7,0x46,0x14,0x00,0x00,0x80,0x00};
    static const uint8_t patch[7]    = {0xC7,0x46,0x14,0x00,0x00,0x00,0x01};

    if (std::memcmp(at, expected, sizeof(expected)) != 0)
    {
        Log("[SKIP] Scaleform _Mesh_Cache constructor mismatch at RVA 0x007B4667.");
        return false;
    }
    if (!WriteBytes(at, patch, sizeof(patch)))
    {
        Log("[FAIL] Scaleform _Mesh_Cache write failed.");
        return false;
    }

    Log("[OK] Scaleform _Mesh_Cache 8 MiB -> 16 MiB.");
    return true;
}

static bool ApplyUiVectorGlyphCache(HMODULE exe, int capacity)
{
    // Exact _Font_Cache vector-glyph owner next to the native warning:
    // "Increase vector glyph cache capacity - SetMaxVectorCacheSize()."
    // Retail: 512. Historical V200: 1024.
    if (capacity != 512 && capacity != 1024)
    {
        Log("[FAIL] UI VectorGlyphCache=%d; supported audited values are 512 or 1024.", capacity);
        return false;
    }
    if (capacity == 512) return true;

    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    auto* at = reinterpret_cast<uint8_t*>(base + 0x007E6AD7);
    static const uint8_t expected[10] = {
        0xC7,0x86,0xC0,0x09,0x00,0x00,0x00,0x02,0x00,0x00
    };
    static const uint8_t patch[10] = {
        0xC7,0x86,0xC0,0x09,0x00,0x00,0x00,0x04,0x00,0x00
    };

    if (std::memcmp(at, expected, sizeof(expected)) != 0)
    {
        Log("[SKIP] Scaleform vector glyph-cache constructor mismatch at RVA 0x007E6AD7.");
        return false;
    }
    if (!WriteBytes(at, patch, sizeof(patch)))
    {
        Log("[FAIL] Scaleform vector glyph-cache write failed.");
        return false;
    }

    Log("[OK] Scaleform vector glyph cache 512 -> 1024.");
    return true;
}

static bool ApplyUiFontCacheTextures(HMODULE exe, int textures)
{
    // Two _Font_Cache constructors initialize the same +0x1C field.
    // Treat them atomically: retail 1, historical V200 2.
    if (textures != 1 && textures != 2)
    {
        Log("[FAIL] UI FontCacheTextures=%d; supported audited values are 1 or 2.", textures);
        return false;
    }
    if (textures == 1) return true;

    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    const uintptr_t rvas[] = {0x007E7D3E, 0x007E7E19};
    static const uint8_t expected[7] = {0xC7,0x46,0x1C,0x01,0x00,0x00,0x00};
    static const uint8_t patch[7]    = {0xC7,0x46,0x1C,0x02,0x00,0x00,0x00};

    for (uintptr_t rva : rvas)
    {
        const auto* at = reinterpret_cast<const uint8_t*>(base + rva);
        if (std::memcmp(at, expected, sizeof(expected)) != 0)
        {
            Log("[SKIP] Scaleform font-cache constructor mismatch at RVA 0x%08X; nothing written.",
                static_cast<unsigned>(rva));
            return false;
        }
    }

    for (uintptr_t rva : rvas)
    {
        auto* at = reinterpret_cast<uint8_t*>(base + rva);
        if (!WriteBytes(at, patch, sizeof(patch)))
        {
            Log("[FAIL] Scaleform font-cache texture-count write failed at RVA 0x%08X.",
                static_cast<unsigned>(rva));
            return false;
        }
    }

    Log("[OK] Scaleform font-cache texture count 1 -> 2 in both constructors.");
    return true;
}


static bool ApplyWsModelSmallObjectHardCullBypass(HMODULE exe)
{
    // Small unlisted models start with WSModel+0xA8 = 10000.0, then retail
    // rewrites A8 to a short size-derived hard-cull distance when metric < 1.5:
    // A8 = 20 + 60 * metric.
    //
    // VA 0x0063954E / runtime RVA 0x0023954E is the exact JP entering
    // that A8 rewrite block. JP -> JMP skips only the A8 rewrite and
    // continues into the existing AC/shadow-distance path.
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    auto* at = reinterpret_cast<uint8_t*>(base + 0x0023954E);
    static const uint8_t expected[2] = {0x7A, 0x1A};
    static const uint8_t patch[2]    = {0xEB, 0x1A};

    if (std::memcmp(at, expected, sizeof(expected)) != 0)
    {
        Log("[SKIP] WSModel small-object A8 hard-cull branch mismatch at RVA 0x0023954E.");
        return false;
    }

    if (!WriteBytes(at, patch, sizeof(patch)))
    {
        Log("[FAIL] WSModel small-object A8 hard-cull bypass write failed.");
        return false;
    }

    Log("[OK] WSModel small-object A8 hard-cull rewrite bypassed; constructor 10000 retained.");
    return true;
}

static bool ApplyWsModelShadowCullBypass(HMODULE exe)
{
    // 0.31 byte audit proved the independent WSModel+0xAC path exactly:
    //   RVA 0x00239577: 75 1A  jne 0x00239593
    //   RVA 0x00239579..0x00239590 computes/stores the size-derived AC value
    //   RVA 0x00239593: DD D8  fstp st(0)
    //
    // For metric < 5 retail derives approximately AC = 15 + 20*metric.
    // Forcing the existing branch to the native cleanup block skips only that
    // rewrite, retains constructor +0xAC = 10000, and preserves x87 balance.
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    auto* at = reinterpret_cast<uint8_t*>(base + 0x00239577);
    static const uint8_t expected[2] = {0x75, 0x1A};
    static const uint8_t patch[2]    = {0xEB, 0x1A};

    if (std::memcmp(at, expected, sizeof(expected)) != 0)
    {
        Log("[SKIP] WSModel +0xAC shadow-cull branch mismatch at RVA 0x00239577.");
        return false;
    }

    if (!WriteBytes(at, patch, sizeof(patch)))
    {
        Log("[FAIL] WSModel +0xAC shadow-cull bypass write failed.");
        return false;
    }

    Log("[OK] WSModel +0xAC size-derived shadow cutoff bypassed; constructor 10000 retained.");
    return true;
}


static bool IsReadableAddress(const void* p, size_t bytes = sizeof(uint32_t))
{
    if (!p || bytes == 0) return false;

    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if ((mbi.Protect & PAGE_GUARD) || (mbi.Protect & PAGE_NOACCESS)) return false;

    const DWORD readable =
        PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
        PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    if ((mbi.Protect & readable) == 0) return false;

    const uintptr_t start = reinterpret_cast<uintptr_t>(p);
    const uintptr_t end = start + bytes;
    const uintptr_t regionEnd =
        reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    return end >= start && end <= regionEnd;
}

static uint32_t ReadU32Safe(uintptr_t address)
{
    if (!IsReadableAddress(reinterpret_cast<const void*>(address), sizeof(uint32_t)))
        return 0;

    uint32_t value = 0;
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
    return value;
}

static void DumpDwords(const char* label, uintptr_t address, size_t count)
{
    if (!address)
    {
        Log("[RED-AUDIT] %s = NULL", label);
        return;
    }

    Log("[RED-AUDIT] %s @ 0x%08X", label, static_cast<unsigned>(address));

    for (size_t i = 0; i < count; i += 4)
    {
        uint32_t v[4] = {};
        for (size_t j = 0; j < 4 && i + j < count; ++j)
            v[j] = ReadU32Safe(address + (i + j) * sizeof(uint32_t));

        Log("[RED-AUDIT]   +%03X : %08X %08X %08X %08X",
            static_cast<unsigned>(i * 4),
            static_cast<unsigned>(v[0]), static_cast<unsigned>(v[1]),
            static_cast<unsigned>(v[2]), static_cast<unsigned>(v[3]));
    }
}

static void DumpLatestRedAuditContext()
{
    const uintptr_t ebx = static_cast<uintptr_t>(
        InterlockedCompareExchange(&g_redAuditLastEbx, 0, 0));
    const uintptr_t owner = static_cast<uintptr_t>(
        InterlockedCompareExchange(&g_redAuditLastOwner, 0, 0));
    const uintptr_t templ = static_cast<uintptr_t>(
        InterlockedCompareExchange(&g_redAuditLastTemplate, 0, 0));
    const LONG eventCount =
        InterlockedCompareExchange(&g_redAuditEventCount, 0, 0);

    Log("[RED-AUDIT] ===== MANUAL SNAPSHOT event=%ld mode=%ld =====",
        eventCount, InterlockedCompareExchange(&g_redAuditMode, 0, 0));
    Log("[RED-AUDIT] EBX=0x%08X owner[+1384]=0x%08X template[+2DC]=0x%08X",
        static_cast<unsigned>(ebx),
        static_cast<unsigned>(owner),
        static_cast<unsigned>(templ));

    DumpDwords("WSHumanSpore/actor", ebx, 32);
    DumpDwords("fallback owner", owner, 32);
    DumpDwords("fallback template", templ, 64);

    const uintptr_t tableCell = g_moduleBase + 0x00E129E0;
    Log("[RED-AUDIT] real-prop table cell VA=0x%08X value=0x%08X",
        static_cast<unsigned>(tableCell),
        static_cast<unsigned>(ReadU32Safe(tableCell)));
    Log("[RED-AUDIT] ===== END SNAPSHOT =====");
}

static void __cdecl RedAuditOnFallback(uintptr_t ebx)
{
    const LONG eventId = InterlockedIncrement(&g_redAuditEventCount);

    uintptr_t owner = 0;
    uintptr_t templ = 0;
    if (ebx)
    {
        owner = static_cast<uintptr_t>(ReadU32Safe(ebx + 0x1384));
        if (owner)
            templ = static_cast<uintptr_t>(ReadU32Safe(owner + 0x2DC));
    }

    InterlockedExchange(&g_redAuditLastEbx, static_cast<LONG>(ebx));
    InterlockedExchange(&g_redAuditLastOwner, static_cast<LONG>(owner));
    InterlockedExchange(&g_redAuditLastTemplate, static_cast<LONG>(templ));

    const LONG mode = InterlockedCompareExchange(&g_redAuditMode, 0, 0);

    if (eventId <= 20 || mode == 2 || (eventId % 100) == 0)
    {
        Log("[RED-AUDIT] fallback #%ld mode=%ld EBX=0x%08X owner=0x%08X template=0x%08X",
            eventId, mode,
            static_cast<unsigned>(ebx),
            static_cast<unsigned>(owner),
            static_cast<unsigned>(templ));
    }

    if (mode == 2)
    {
        DumpDwords("fallback owner", owner, 16);
        DumpDwords("fallback template", templ, 32);
    }
}


__declspec(naked) static void RedAuditFallbackHook()
{
    __asm
    {
        pushfd
        pushad

        push ebx
        call RedAuditOnFallback
        add esp, 4

        popad
        popfd

        cmp dword ptr [g_redAuditMode], 1
        je suppress_proxy

        test ebx, ebx
        jz suppress_proxy
        jmp dword ptr [g_redFallbackContinue]

suppress_proxy:
        jmp dword ptr [g_redFallbackExit]
    }
}

static bool InstallRedPropRuntimeAudit(HMODULE exe)
{
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    constexpr uintptr_t kRva = 0x00092E03;
    auto* at = reinterpret_cast<uint8_t*>(base + kRva);

    static const uint8_t expected[8] = {
        0x85,0xDB,0x0F,0x84,0x3B,0x01,0x00,0x00
    };

    if (std::memcmp(at, expected, sizeof(expected)) != 0)
    {
        Log("[SKIP] Red-prop audit signature mismatch at RVA 0x%08X.",
            static_cast<unsigned>(kRva));
        return false;
    }

    const intptr_t delta =
        reinterpret_cast<intptr_t>(&RedAuditFallbackHook) -
        (reinterpret_cast<intptr_t>(at) + 5);
    if (delta < INT32_MIN || delta > INT32_MAX)
    {
        Log("[FAIL] Red-prop audit hook outside rel32 range.");
        return false;
    }

    uint8_t patch[8] = {0xE9,0,0,0,0,0x90,0x90,0x90};
    const int32_t rel = static_cast<int32_t>(delta);
    std::memcpy(patch + 1, &rel, sizeof(rel));

    g_redFallbackContinue = base + 0x00092E0B;
    g_redFallbackExit = base + 0x00092F46;

    if (!WriteBytes(at, patch, sizeof(patch)))
    {
        Log("[FAIL] Red-prop runtime audit hook write failed.");
        return false;
    }

    Log("[OK] Red-prop runtime audit installed at RVA 0x00092E03.");
    return true;
}


static DWORD WINAPI RedAuditHotkeyThread(LPVOID)
{
    InterlockedExchange(&g_redAuditThreadRun, 1);

    Log("[RED-AUDIT] F6=native | F7=suppress proxy | F8=verbose native | F9=dump latest context");

    while (InterlockedCompareExchange(&g_redAuditThreadRun, 0, 0))
    {
        if (GetAsyncKeyState(VK_F6) & 1)
        {
            InterlockedExchange(&g_redAuditMode, 0);
            Log("[RED-AUDIT] F6: mode=NATIVE proxy.");
        }

        if (GetAsyncKeyState(VK_F7) & 1)
        {
            InterlockedExchange(&g_redAuditMode, 1);
            Log("[RED-AUDIT] F7: mode=SUPPRESS proxy.");
        }

        if (GetAsyncKeyState(VK_F8) & 1)
        {
            InterlockedExchange(&g_redAuditMode, 2);
            Log("[RED-AUDIT] F8: mode=VERBOSE NATIVE.");
        }

        if (GetAsyncKeyState(VK_F9) & 1)
            DumpLatestRedAuditContext();

        Sleep(50);
    }

    return 0;
}

static bool StartRedAuditHotkeys()
{
    HANDLE thread = CreateThread(
        nullptr, 0, RedAuditHotkeyThread, nullptr, 0, nullptr);

    if (!thread)
    {
        Log("[FAIL] Red-prop audit hotkey thread creation failed.");
        return false;
    }

    CloseHandle(thread);
    return true;
}


static BOOL CALLBACK InitializeOnce(PINIT_ONCE, PVOID, PVOID*)
{
    HMODULE exe = GetModuleHandleW(nullptr);
    g_moduleBase = reinterpret_cast<uintptr_t>(exe);

    const std::wstring dir = ModuleDirectory(exe);
    const std::wstring logPath = dir + L"\\SaboteurEnhanced.log";
    _wfopen_s(&g_log, logPath.c_str(), L"w");

    Log("SaboteurEnhanced ASI 0.49 RED PROP RUNTIME AUDIT");
    Log("Architecture: validated Core 1 + complete retail EXE parameter audit");
    Log("Module base: 0x%08X", static_cast<unsigned>(g_moduleBase));

    const std::wstring iniPath = dir + L"\\SaboteurEnhanced.ini";
    const bool enableV310 = GetPrivateProfileIntW(L"Fixes", L"WSModelFullRenderMask", 1, iniPath.c_str()) != 0;
    const bool enableV311 = GetPrivateProfileIntW(L"Fixes", L"ModelInfoFullRenderSlice", 1, iniPath.c_str()) != 0;
    const bool redPropRuntimeAudit = GetPrivateProfileIntW(L"Diagnostics", L"RedPropRuntimeAudit", 1, iniPath.c_str()) != 0;
    const int wsDynamicPartPriorityRadius = GetPrivateProfileIntW(L"Fixes", L"WSDynamicPartPriorityRadius", 25, iniPath.c_str());
    const bool wsModelSmallObjectHardCullBypass = GetPrivateProfileIntW(L"Fixes", L"WSModelSmallObjectHardCullBypass", 0, iniPath.c_str()) != 0;
    const bool wsModelShadowCullBypass = GetPrivateProfileIntW(L"Fixes", L"WSModelShadowCullBypass", 0, iniPath.c_str()) != 0;
    const int environmentMapResolution = GetPrivateProfileIntW(L"Graphics", L"EnvironmentMapResolution", 2048, iniPath.c_str());
    const int anisotropicFiltering = GetPrivateProfileIntW(L"Graphics", L"AnisotropicFiltering", 16, iniPath.c_str());
    const float mipLodBias = ReadIniFloat(iniPath, L"Graphics", L"MipLODBias", 0.0f);
    const float toneMap = ReadIniFloat(iniPath, L"Graphics", L"ToneMap", 0.25f);

    const int shadowMapResolution = GetPrivateProfileIntW(L"Shadows", L"ShadowMapResolution", 4096, iniPath.c_str());
    const bool shadowPcf5x5 = GetPrivateProfileIntW(L"Shadows", L"ShadowPCF5x5", 0, iniPath.c_str()) != 0;
    const float csmLambda = ReadIniFloat(iniPath, L"Shadows", L"CSMLambda", 0.50f);
    const float csmFarDistance = ReadIniFloat(iniPath, L"Shadows", L"CSMFarDistance", 100.0f);
    const float shadowDepthBiasScale = ReadIniFloat(iniPath, L"Shadows", L"DepthBiasScale", 1.0f);
    const float shadowSlopeBiasScale = ReadIniFloat(iniPath, L"Shadows", L"SlopeBiasScale", 1.0f);
    const float spotShadowResolutionScale = ReadIniFloat(iniPath, L"Shadows", L"SpotShadowResolutionScale", 0.5f);

    const bool fullResolutionAo = GetPrivateProfileIntW(L"AmbientOcclusion", L"FullResolution", 0, iniPath.c_str()) != 0;
    const float aoBlurScale = ReadIniFloat(iniPath, L"AmbientOcclusion", L"BlurScale", 2.0f);
    const float aoErodeScale = ReadIniFloat(iniPath, L"AmbientOcclusion", L"ErodeScale", 2.0f);

    const int uiMeshCacheMiB = GetPrivateProfileIntW(L"UI", L"MeshCacheMiB", 8, iniPath.c_str());
    const int uiVectorGlyphCache = GetPrivateProfileIntW(L"UI", L"VectorGlyphCache", 512, iniPath.c_str());
    const int uiFontCacheTextures = GetPrivateProfileIntW(L"UI", L"FontCacheTextures", 1, iniPath.c_str());

    const float streamCoverageLow = ReadIniFloat(iniPath, L"Streaming", L"CoverageLow", 16000.0f);
    const float streamCoverageMedium = ReadIniFloat(iniPath, L"Streaming", L"CoverageMedium", 3200.0f);
    const float streamCoverageHigh = ReadIniFloat(iniPath, L"Streaming", L"CoverageHigh", 2500.0f);

    const float farSceneDistance = ReadIniFloat(iniPath, L"Distances", L"FarScene", 320.0f);
    const float decalVisibilityDistance = ReadIniFloat(iniPath, L"Distances", L"DecalVisibility", 160.0f);

    const float renderSlice3HighFar = ReadIniFloat(iniPath, L"Distances", L"RenderSlice3HighFar", 100.0f);
    const float renderSliceHighOuter = ReadIniFloat(iniPath, L"Distances", L"RenderSliceHighOuter", 500.0f);
    const float modelInfoDefaultLodDistance = ReadIniFloat(iniPath, L"Distances", L"ModelInfoDefaultLODDistance", 1500.0f);
    const float veryFarSceneTerrainDistance = ReadIniFloat(iniPath, L"Distances", L"VeryFarSceneTerrain", 10000.0f);
    const float clipRangeHigh = ReadIniFloat(iniPath, L"Distances", L"ClipRangeHigh", 1000.0f);
    const float detailSystemDistance = ReadIniFloat(iniPath, L"Distances", L"DetailSystemDistance", 1000.0f);
    const float humanObjectQualityScale = ReadIniFloat(iniPath, L"Distances", L"HumanObjectQualityScale", 5.0f);
    const float foliageModelLodDistance = ReadIniFloat(iniPath, L"Distances", L"FoliageModelLODDistance", 250.0f);
    const float shadowCasterMinLodDistance = ReadIniFloat(iniPath, L"Distances", L"ShadowCasterMinLODDistance", 240.0f);
    const float particleLodMinDistance = ReadIniFloat(iniPath, L"Distances", L"ParticleLODMinDistance", 150.0f);

    const float highPaletteThreshold = ReadIniFloat(iniPath, L"Experimental", L"HighPaletteThreshold", 80.0f);
    const float motionBlurActivationThreshold = ReadIniFloat(iniPath, L"ExperimentalPostFX", L"MotionBlurActivationThreshold", 0.12f);

    const int waterNormalsResolution = GetPrivateProfileIntW(L"Water", L"NormalMapResolution", 128, iniPath.c_str());
    const float waterReflectionWidth = ReadIniFloat(iniPath, L"Water", L"ReflectionWidth", 512.0f);
    const float waterReflectionHeight = ReadIniFloat(iniPath, L"Water", L"ReflectionHeight", 128.0f);
    const int rainCubeResolution = GetPrivateProfileIntW(L"Rain", L"CubeResolution", 128, iniPath.c_str());
    const float depthBlurAutoStart = ReadIniFloat(iniPath, L"ExperimentalPostFX", L"DepthBlurAutoStart", 200.0f);
    const float depthBlurAutoRange = ReadIniFloat(iniPath, L"ExperimentalPostFX", L"DepthBlurAutoRange", 50.0f);

    const bool motionBlurFullResolution = GetPrivateProfileIntW(L"ExperimentalPostFX", L"MotionBlurFullResolution", 0, iniPath.c_str()) != 0;
    const int bloomResolutionMultiplier = GetPrivateProfileIntW(L"ExperimentalPostFX", L"BloomResolutionMultiplier", 1, iniPath.c_str());
    const float bloomPrefilterGain = ReadIniFloat(iniPath, L"ExperimentalPostFX", L"BloomPrefilterGain", 4.0f);
    const float bloomFinalContribution = ReadIniFloat(iniPath, L"ExperimentalPostFX", L"BloomFinalContribution", 4.0f);
    const bool bloomFinalBackBufferSampler = GetPrivateProfileIntW(L"ExperimentalPostFX", L"BloomFinalBackBufferSampler", 0, iniPath.c_str()) != 0;
    const bool bloomFinalDownsampledBackBufferSampler = GetPrivateProfileIntW(L"ExperimentalPostFX", L"BloomFinalDownsampledBackBufferSampler", 0, iniPath.c_str()) != 0;

    const bool scaledTextureFullResolution = GetPrivateProfileIntW(L"ExperimentalPostFX", L"ScaledTextureFullResolution", 0, iniPath.c_str()) != 0;
    const float depthBlurMaskResolutionScale = ReadIniFloat(iniPath, L"ExperimentalPostFX", L"DepthBlurMaskResolutionScale", 0.5f);
    const float depthBlurMaskTapOffsetScale = ReadIniFloat(iniPath, L"ExperimentalPostFX", L"DepthBlurMaskTapOffsetScale", 1.0f);
    const float depthBlurColorTexelOffsetScale = ReadIniFloat(iniPath, L"ExperimentalPostFX", L"DepthBlurColorTexelOffsetScale", 1.0f);
    const int willToFightGridResolution = GetPrivateProfileIntW(L"WillToFight", L"GridResolution", 256, iniPath.c_str());
    const float veryFarSceneProfile0Threshold = ReadIniFloat(iniPath, L"ExperimentalDistances", L"VeryFarSceneProfile0Threshold", 22.0f);
    const float veryFarSceneProfile1Threshold = ReadIniFloat(iniPath, L"ExperimentalDistances", L"VeryFarSceneProfile1Threshold", 49.0f);
    const float wsSphereActivatorMaxRadius = ReadIniFloat(
        iniPath, L"ExperimentalDistances", L"WSSphereActivatorMaxRadius", 2.06f);

    const float depthBlurColorPyramidFactor = ReadIniFloat(iniPath, L"ExperimentalPostFX", L"DepthBlurColorPyramidFactor", 0.75f);
    const int skyDomeResolutionMultiplier = GetPrivateProfileIntW(L"Sky", L"ResolutionMultiplier", 1, iniPath.c_str());

    const float damageBlurResolutionScale = ReadIniFloat(iniPath, L"ExperimentalPostFX", L"DamageBlurResolutionScale", 0.5f);
    const int rainDensityPercentOverride = GetPrivateProfileIntW(L"Rain", L"DensityPercentOverride", 0, iniPath.c_str());
    const int particleRenderTargetResolutionMultiplier = GetPrivateProfileIntW(L"Particles", L"RenderTargetResolutionMultiplier", 1, iniPath.c_str());
    const bool particleFullResolutionDepthRestore = GetPrivateProfileIntW(L"Particles", L"FullResolutionDepthRestore", 0, iniPath.c_str()) != 0;
    const int lightVolumeResolutionMultiplier = GetPrivateProfileIntW(L"Lighting", L"LightVolumeResolutionMultiplier", 1, iniPath.c_str());
    const float lightVolumeCoordinateResolutionScale = ReadIniFloat(iniPath, L"Lighting", L"LightVolumeCoordinateResolutionScale", 0.5f);
    const int wtfTransitionRingResolution = GetPrivateProfileIntW(L"WillToFight", L"TransitionRingResolution", 128, iniPath.c_str());

    const int wsLuaCallCapacity = GetPrivateProfileIntW(L"EngineLimits", L"WSLuaCallCapacity", 20, iniPath.c_str());
    const int wsParkingSpaceCapacity = GetPrivateProfileIntW(L"EngineLimits", L"WSParkingSpaceCapacity", 32, iniPath.c_str());
    const int wsParticleInfoDataCapacity = GetPrivateProfileIntW(L"EngineLimits", L"WSParticleInfoDataCapacity", 1400, iniPath.c_str());
    const int wsActivateSphereCapacity = GetPrivateProfileIntW(L"EngineLimits", L"WSActivateSphereCapacity", 256, iniPath.c_str());
    const int wallGraphCapacity = GetPrivateProfileIntW(L"EngineLimits", L"WallGraphCapacity", 50, iniPath.c_str());

    const int wsPhysicsParticleCapacity = GetPrivateProfileIntW(L"EngineLimits", L"WSPhysicsParticleCapacity", 1000, iniPath.c_str());
    const int wsParticleRenderMainCapacity = GetPrivateProfileIntW(L"EngineLimits", L"WSParticleRenderMainCapacity", 4500, iniPath.c_str());
    const int wsParticleRenderMediumCapacity = GetPrivateProfileIntW(L"EngineLimits", L"WSParticleRenderMediumCapacity", 1000, iniPath.c_str());
    const int wsParticleRenderSmallCapacity = GetPrivateProfileIntW(L"EngineLimits", L"WSParticleRenderSmallCapacity", 500, iniPath.c_str());

    const int havokToiEventQueue = GetPrivateProfileIntW(L"EngineLimits", L"HavokTOIEventQueue", 250, iniPath.c_str());
    const int havokBroadPhaseQuerySize = GetPrivateProfileIntW(
        L"EngineLimits", L"HavokBroadPhaseQuerySize", 1024, iniPath.c_str());
    const int wsPhGridObjectCapacity = GetPrivateProfileIntW(
        L"EngineLimits", L"WSPhGridObjectCapacity", 1000, iniPath.c_str());
    const int streamingJobCapacity = GetPrivateProfileIntW(L"EngineLimits", L"StreamingJobCapacity", 1200, iniPath.c_str());
    const int pblCrcTreeNodeCapacity = GetPrivateProfileIntW(L"EngineLimits", L"PblCRCTreeNodeCapacity", 40000, iniPath.c_str());

    const int wsDamageSphereCapacity = GetPrivateProfileIntW(L"EngineLimits", L"WSDamageSphereCapacity", 512, iniPath.c_str());
    const int wsInventoryStateStowCapacity = GetPrivateProfileIntW(L"EngineLimits", L"WSInventoryStateStowCapacity", 32, iniPath.c_str());
    const int wsDecalCapacity = GetPrivateProfileIntW(L"EngineLimits", L"WSDecalCapacity", 400, iniPath.c_str());
    const int coalescedReadBatchByteLimit = GetPrivateProfileIntW(L"Streaming", L"CoalescedReadBatchByteLimit", 512000, iniPath.c_str());
    const bool validatedEngineLimitsPack035 =
        wsLuaCallCapacity == 40 &&
        pblCrcTreeNodeCapacity == 60000 &&
        wsDamageSphereCapacity == 1024 &&
        wsInventoryStateStowCapacity == 64;
    const bool validatedSimpleEngineLimitsPack036 =
        wsParkingSpaceCapacity == 64 &&
        wsParticleInfoDataCapacity == 2800 &&
        wsActivateSphereCapacity == 512 &&
        wallGraphCapacity == 100;
    const bool validatedParticleCapacityPack037 =
        wsPhysicsParticleCapacity == 2000 &&
        wsParticleRenderMainCapacity == 9000 &&
        wsParticleRenderMediumCapacity == 2000 &&
        wsParticleRenderSmallCapacity == 1000;

    Log("INI: %ls", iniPath.c_str());
    Log("WSModelFullRenderMask=%d", enableV310 ? 1 : 0);
    Log("ModelInfoFullRenderSlice=%d", enableV311 ? 1 : 0);
    Log("RedPropRuntimeAudit=%d", redPropRuntimeAudit ? 1 : 0);
    Log("WSDynamicPartPriorityRadius=%d", wsDynamicPartPriorityRadius);
    Log("WSModelSmallObjectHardCullBypass=%d", wsModelSmallObjectHardCullBypass ? 1 : 0);
    Log("WSModelShadowCullBypass=%d", wsModelShadowCullBypass ? 1 : 0);
    Log("[Graphics] EnvironmentMapResolution=%d AF=%d MipLODBias=%.3f ToneMap=%.3f",
        environmentMapResolution, anisotropicFiltering, mipLodBias, toneMap);
    Log("[Shadows] ShadowMapResolution=%d PCF5x5=%d Lambda=%.3f Far=%.3f Bias=%.3f/%.3f SpotScale=%.3f",
        shadowMapResolution, shadowPcf5x5 ? 1 : 0, csmLambda, csmFarDistance,
        shadowDepthBiasScale, shadowSlopeBiasScale, spotShadowResolutionScale);
    Log("[AO] FullResolution=%d Blur=%.3f Erode=%.3f", fullResolutionAo ? 1 : 0, aoBlurScale, aoErodeScale);
    Log("[UI] MeshCacheMiB=%d VectorGlyphCache=%d FontCacheTextures=%d",
        uiMeshCacheMiB, uiVectorGlyphCache, uiFontCacheTextures);
    Log("[Streaming] Coverage=%.1f/%.1f/%.1f", streamCoverageLow, streamCoverageMedium, streamCoverageHigh);
    Log("[Distances] FarScene=%.1f DecalVisibility=%.1f RenderSlice3HighFar=%.1f Outer=%.1f ModelInfoLOD=%.1f VeryFarTerrain=%.1f ClipRangeHigh=%.1f DetailSystem=%.1f",
        farSceneDistance, decalVisibilityDistance, renderSlice3HighFar, renderSliceHighOuter,
        modelInfoDefaultLodDistance, veryFarSceneTerrainDistance, clipRangeHigh, detailSystemDistance);
    Log("[Distances] HumanObjectQualityScale=%.3f", humanObjectQualityScale);
    Log("[Distances] FoliageLOD=%.1f ShadowCasterMinLOD=%.1f ParticleLODMin=%.1f",
        foliageModelLodDistance, shadowCasterMinLodDistance, particleLodMinDistance);
    Log("[Experimental] HighPaletteThreshold=%.1f", highPaletteThreshold);
    Log("[ExperimentalPostFX] MotionBlurActivationThreshold=%.4f", motionBlurActivationThreshold);
    Log("[Water] Reflection=%.0fx%.0f Normals=%d", waterReflectionWidth, waterReflectionHeight, waterNormalsResolution);
    Log("[Rain] CubeResolution=%d", rainCubeResolution);
    Log("[ExperimentalPostFX] DepthBlurAutoStart=%.3f DepthBlurAutoRange=%.3f",
        depthBlurAutoStart, depthBlurAutoRange);
    Log("[ExperimentalPostFX] MotionBlurFullResolution=%d BloomResolutionMultiplier=%d",
        motionBlurFullResolution ? 1 : 0, bloomResolutionMultiplier);
    Log("[ExperimentalPostFX] BloomPrefilterGain=%.3f", bloomPrefilterGain);
    Log("[ExperimentalPostFX] BloomFinalContribution=%.3f SkyBloomBackBufferSampler=%d DownsampledBackBufferSampler=%d",
        bloomFinalContribution,
        bloomFinalBackBufferSampler ? 1 : 0,
        bloomFinalDownsampledBackBufferSampler ? 1 : 0);
    Log("[ExperimentalPostFX] ScaledTextureFullResolution=%d DepthBlurMaskResolutionScale=%.3f",
        scaledTextureFullResolution ? 1 : 0, depthBlurMaskResolutionScale);
    Log("[ExperimentalPostFX] DepthBlurMaskTapOffsetScale=%.6f DepthBlurColorTexelOffsetScale=%.6f",
        depthBlurMaskTapOffsetScale, depthBlurColorTexelOffsetScale);
    Log("[WillToFight] GridResolution=%d", willToFightGridResolution);
    Log("[ExperimentalDistances] VeryFarSceneProfileThresholds=%.3f/%.3f",
        veryFarSceneProfile0Threshold, veryFarSceneProfile1Threshold);
    Log("[ExperimentalDistances] WSSphereActivatorMaxRadius=%.3f",
        wsSphereActivatorMaxRadius);
    Log("[ExperimentalPostFX] DepthBlurColorPyramidFactor=%.3f", depthBlurColorPyramidFactor);
    Log("[Sky] ResolutionMultiplier=%d", skyDomeResolutionMultiplier);
    Log("[ExperimentalPostFX] DamageBlurResolutionScale=%.3f", damageBlurResolutionScale);
    Log("[Rain] DensityPercentOverride=%d", rainDensityPercentOverride);
    Log("[Particles] RenderTargetResolutionMultiplier=%d FullResolutionDepthRestore=%d",
        particleRenderTargetResolutionMultiplier, particleFullResolutionDepthRestore ? 1 : 0);
    Log("[Lighting] LightVolumeResolutionMultiplier=%d CoordinateResolutionScale=%.3f",
        lightVolumeResolutionMultiplier, lightVolumeCoordinateResolutionScale);
    Log("[WillToFight] TransitionRingResolution=%d", wtfTransitionRingResolution);
    Log("[EngineLimits] WSLuaCall=%d Parking=%d ParticleInfo=%d ActivateSphere=%d WallGraph=%d",
        wsLuaCallCapacity, wsParkingSpaceCapacity, wsParticleInfoDataCapacity,
        wsActivateSphereCapacity, wallGraphCapacity);
    Log("[EngineLimits] WSPhysicsParticle=%d ParticleRender=%d/%d/%d",
        wsPhysicsParticleCapacity,
        wsParticleRenderMainCapacity,
        wsParticleRenderMediumCapacity,
        wsParticleRenderSmallCapacity);
    Log("[EngineLimits] HavokTOI=%d HavokBroadPhase=%d StreamingJobs=%d PblCRC=%d",
        havokToiEventQueue, havokBroadPhaseQuerySize,
        streamingJobCapacity, pblCrcTreeNodeCapacity);
    Log("[EngineLimits] WSPhGridObject=%d", wsPhGridObjectCapacity);
    Log("[EngineLimits] DamageSphere=%d InventoryStow=%d",
        wsDamageSphereCapacity, wsInventoryStateStowCapacity);
    Log("[EngineLimits] WSDecal=%d", wsDecalCapacity);
    SectionRange text = GetSectionRange(exe, ".text");
    if (!text.begin)
    {
        Log("[FAIL] Could not locate executable .text section.");
        return TRUE;
    }

    Log(".text range: RVA 0x%08X, size 0x%08X",
        static_cast<unsigned>(reinterpret_cast<uintptr_t>(text.begin) - g_moduleBase),
        static_cast<unsigned>(text.size));

    if (wsModelSmallObjectHardCullBypass)
        ApplyWsModelSmallObjectHardCullBypass(exe);
    else
        Log("[OFF] WSModel small-object A8 hard-cull rewrite left retail.");

    if (wsModelShadowCullBypass)
        ApplyWsModelShadowCullBypass(exe);
    else
        Log("[OFF] WSModel +0xAC size-derived shadow cutoff left retail.");

    // Restored validated graphics/render findings. Every family is controlled
    // independently by SaboteurEnhanced.ini and verified against Core1/native bytes.
    if (environmentMapResolution != 128) ApplyEnvironmentMapResolution(exe, environmentMapResolution);
    else Log("[OFF] Environment maps left at native 128.");

    if (shadowMapResolution != 1024) ApplyShadowMapResolution(exe, shadowMapResolution);
    else Log("[OFF] Shadow maps left at native 1024.");

    if (anisotropicFiltering != 4) ApplyAnisotropicFiltering(exe, anisotropicFiltering);
    else Log("[OFF] Anisotropic filtering left at native level 4.");

    if (std::fabs(mipLodBias) > 0.0001f) ApplyMipLodBias(exe, mipLodBias);
    else Log("[OFF] MIP LOD bias left at native 0.");

    if (std::fabs(toneMap - 0.25f) > 0.0001f) ApplyToneMap(exe, toneMap);
    else Log("[OFF] ToneMap left at native 0.25.");

    if (shadowPcf5x5) ApplyShadowPcf5x5(exe);
    else Log("[OFF] Shadow PCF 5x5 disabled.");

    Log("[OFF] Deprecated CSMQuality patch path disabled after retail shader re-audit.");

    if (std::fabs(csmLambda - 0.5f) > 0.0001f) ApplyCsmLambda(exe, csmLambda);
    else Log("[OFF] CSM lambda left at native 0.5.");

    if (std::fabs(csmFarDistance - 100.0f) > 0.01f) ApplyCsmFarDistance(exe, csmFarDistance);
    else Log("[OFF] CSM far distance left at native ~100.");

    if (fullResolutionAo) ApplyFullResolutionAo(exe);
    else Log("[OFF] Full-resolution AO disabled.");

    if (std::fabs(aoBlurScale - 2.0f) > 0.0001f) ApplyAoBlurScale(exe, aoBlurScale);
    else Log("[OFF] AO blur scale left at native 2.0.");

    if (std::fabs(aoErodeScale - 2.0f) > 0.0001f) ApplyAoErodeScale(exe, aoErodeScale);
    else Log("[OFF] AO erode scale left at native 2.0.");

    if (std::fabs(shadowDepthBiasScale - 1.0f) > 0.0001f ||
        std::fabs(shadowSlopeBiasScale - 1.0f) > 0.0001f)
        ApplyShadowBiasScales(exe, shadowDepthBiasScale, shadowSlopeBiasScale);
    else
        Log("[OFF] Shadow bias tables left native.");

    if (std::fabs(spotShadowResolutionScale - 0.5f) > 0.0001f)
        ApplySpotShadowResolutionScale(exe, static_cast<double>(spotShadowResolutionScale));
    else
        Log("[OFF] Spot-shadow Z-buffer resolution scale left native 0.5.");

    if (uiMeshCacheMiB != 8)
        ApplyUiMeshCacheMiB(exe, uiMeshCacheMiB);
    else
        Log("[OFF] Scaleform _Mesh_Cache left retail 8 MiB.");

    if (uiVectorGlyphCache != 512)
        ApplyUiVectorGlyphCache(exe, uiVectorGlyphCache);
    else
        Log("[OFF] Scaleform vector glyph cache left retail 512.");

    if (uiFontCacheTextures != 1)
        ApplyUiFontCacheTextures(exe, uiFontCacheTextures);
    else
        Log("[OFF] Scaleform font-cache texture count left retail 1.");

    if (std::fabs(streamCoverageLow - 1500.0f) > 0.01f ||
        std::fabs(streamCoverageMedium - 300.0f) > 0.01f ||
        std::fabs(streamCoverageHigh - 250.0f) > 0.01f)
        ApplyStreamCoverage(exe, streamCoverageLow, streamCoverageMedium, streamCoverageHigh);
    else
        Log("[OFF] Streaming coverage left native.");

    if (coalescedReadBatchByteLimit != 512000)
        ApplyCoalescedReadBatchByteLimit(exe, coalescedReadBatchByteLimit);
    else
        Log("[OFF] Coalesced-read batch byte limit left retail 512000.");

    if (std::fabs(farSceneDistance - 200.0f) > 0.01f) ApplyFarSceneDistance(exe, farSceneDistance);
    else Log("[OFF] FarScene left at native 200.");

    if (std::fabs(decalVisibilityDistance - 120.0f) > 0.01f)
        ApplyDecalVisibilityDistance(exe, decalVisibilityDistance);
    else
        Log("[OFF] Decal visibility left at native 120.");

    if (std::fabs(renderSlice3HighFar - 100.0f) > 0.01f ||
        std::fabs(renderSliceHighOuter - 500.0f) > 0.01f)
        ApplyRenderSliceHighDistances(exe, renderSlice3HighFar, renderSliceHighOuter);
    else
        Log("[OFF] RenderSlice/ShadowSlice High bounds left native 100/500.");

    if (std::fabs(modelInfoDefaultLodDistance - 1000.0f) > 0.01f)
        ApplyModelInfoDefaultLodDistance(exe, modelInfoDefaultLodDistance);
    else
        Log("[OFF] ModelInfo default LODDIST left native 1000.");

    if (std::fabs(veryFarSceneTerrainDistance - 5000.0f) > 0.01f)
        ApplyVeryFarSceneTerrainDistance(exe, veryFarSceneTerrainDistance);
    else
        Log("[OFF] VeryFarSceneTerrain left native 5000.");

    if (std::fabs(clipRangeHigh - 1000.0f) > 0.01f)
        ApplyClipRangeHigh(exe, clipRangeHigh);
    else
        Log("[OFF] ClipRange High left native 1000.");

    if (detailSystemDistance > 0.0f)
        ApplyDetailSystemDistance(exe, detailSystemDistance);
    else
        Log("[OFF] WSDetailSystem left at native initial 50 / max 100 behavior.");

    if (std::fabs(humanObjectQualityScale - 1.0f) > 0.0001f)
        ApplyHumanObjectQualityScale(exe, humanObjectQualityScale);
    else
        Log("[OFF] WSHuman ObjectQuality distances left native.");

    if (std::fabs(foliageModelLodDistance - 50.0f) > 0.01f)
        ApplyFoliageModelLodDistance(exe, foliageModelLodDistance);
    else
        Log("[OFF] FOLIAGE ModelInfo LODDIST left native 50.");

    if (shadowCasterMinLodDistance > 0.0f)
        ApplyShadowCasterMinLodDistance(exe, shadowCasterMinLodDistance);
    else
        Log("[OFF] Shadow-caster minimum LODDIST hook disabled.");

    if (particleLodMinDistance > 0.0f)
        ApplyParticleLodMinDistance(exe, particleLodMinDistance);
    else
        Log("[OFF] Particle LOD minimum hooks disabled.");

    if (std::fabs(highPaletteThreshold - 80.0f) > 0.01f)
        ApplyHighPaletteThreshold(exe, static_cast<double>(highPaletteThreshold));
    else
        Log("[OFF] SS_HighPalette threshold left native 80.");

    if (std::fabs(motionBlurActivationThreshold - 0.12f) > 0.0001f)
        ApplyMotionBlurActivationThreshold(exe, motionBlurActivationThreshold);
    else
        Log("[OFF] MotionBlur activation threshold left native 0.12.");

    if (std::fabs(waterReflectionWidth - 512.0f) > 0.01f ||
        std::fabs(waterReflectionHeight - 128.0f) > 0.01f)
        ApplyWaterReflectionResolution(exe, waterReflectionWidth, waterReflectionHeight);
    else
        Log("[OFF] Water reflection resolution left native 512x128.");

    if (waterNormalsResolution != 128)
        ApplyWaterNormalsResolution(exe, waterNormalsResolution);
    else
        Log("[OFF] Water normals resolution left native 128.");

    if (rainCubeResolution != 128)
        ApplyRainCubeResolution(exe, rainCubeResolution);
    else
        Log("[OFF] RainCubeRT resolution left native 128.");

    if (std::fabs(depthBlurAutoStart - 200.0f) > 0.0001f ||
        std::fabs(depthBlurAutoRange - 50.0f) > 0.0001f)
        ApplyDepthBlurAutoTransition(exe, depthBlurAutoStart, depthBlurAutoRange);
    else
        Log("[OFF] DepthBlur automatic transition left native 200/50.");

    if (motionBlurFullResolution)
        ApplyMotionBlurFullResolution(exe);
    else
        Log("[OFF] MotionBlurDownsampledBackBuffer left native half-resolution.");

    if (bloomResolutionMultiplier != 1)
        ApplyBloomResolutionMultiplier(exe, bloomResolutionMultiplier);
    else
        Log("[OFF] Bloom/GodRays pyramid left at native resolution.");

    if (std::fabs(bloomPrefilterGain - 4.0f) > 0.0001f)
        ApplyBloomPrefilterGain(exe, bloomPrefilterGain);
    else
        Log("[OFF] PsBloom prefilter gain left native 4.0.");

    if (std::fabs(bloomFinalContribution - 4.0f) > 0.0001f)
        ApplyBloomFinalContribution(exe, bloomFinalContribution);
    else
        Log("[OFF] PsBloomFinal contribution left native 4.0.");

    if (bloomFinalBackBufferSampler)
        ApplyBloomFinalBackBufferSampler(exe);
    else
        Log("[OFF] PsBloomFinal source sampler left native SkyBloom s4.");

    if (bloomFinalDownsampledBackBufferSampler)
        ApplyBloomFinalDownsampledBackBufferSampler(exe);
    else
        Log("[OFF] PsBloomFinal DownsampledBackBuffer source left native s2.");

    if (scaledTextureFullResolution)
        ApplyScaledTextureFullResolution(exe);
    else
        Log("[OFF] ScaledTexture left native half-resolution.");

    if (std::fabs(depthBlurMaskResolutionScale - 0.5f) > 0.0001f)
        ApplyDepthBlurMaskResolutionScale(exe, static_cast<double>(depthBlurMaskResolutionScale));
    else
        Log("[OFF] DepthBlur mask resolution scale left native 0.5.");

    if (std::fabs(depthBlurMaskTapOffsetScale - 1.0f) > 0.0001f)
        ApplyDepthBlurMaskTapOffsetScale(exe, depthBlurMaskTapOffsetScale);
    else
        Log("[OFF] DepthBlur mask shader tap offsets left native 1.0x.");

    if (willToFightGridResolution != 256)
        ApplyWillToFightGridResolution(exe, willToFightGridResolution);
    else
        Log("[OFF] WSWillToFightGrid left native 256x256.");

    if (std::fabs(veryFarSceneProfile0Threshold - 22.0f) > 0.0001f ||
        std::fabs(veryFarSceneProfile1Threshold - 49.0f) > 0.0001f)
        ApplyVeryFarSceneProfileThresholds(
            exe, veryFarSceneProfile0Threshold, veryFarSceneProfile1Threshold);
    else
        Log("[OFF] VeryFarScene profile thresholds left native 22/49.");

    if (std::fabs(wsSphereActivatorMaxRadius - 2.06f) > 0.0001f)
        ApplyWSSphereActivatorMaxRadius(exe, wsSphereActivatorMaxRadius);
    else
        Log("[OFF] WSSphereActivator max-radius clamp left native 2.06.");

    if (std::fabs(depthBlurColorPyramidFactor - 0.75f) > 0.0001f)
        ApplyDepthBlurColorPyramidFactor(exe, static_cast<double>(depthBlurColorPyramidFactor));
    else
        Log("[OFF] DepthBlur color pyramid left at native factor 0.75.");

    if (std::fabs(depthBlurColorTexelOffsetScale - 1.0f) > 0.0001f)
        ApplyDepthBlurColorTexelOffsetScale(exe, depthBlurColorTexelOffsetScale);
    else
        Log("[OFF] DepthBlur color shader texel offsets left native 1.0x.");

    if (skyDomeResolutionMultiplier != 1)
        ApplySkyDomeResolutionMultiplier(exe, skyDomeResolutionMultiplier);
    else
        Log("[OFF] SkyDome render-target family left at native resolution.");

    if (std::fabs(damageBlurResolutionScale - 0.5f) > 0.0001f)
        ApplyDamageBlurResolutionScale(exe, static_cast<double>(damageBlurResolutionScale));
    else
        Log("[OFF] DamageBlur render target left native half-resolution.");

    if (rainDensityPercentOverride != 0)
        ApplyRainDensityOverride(exe, rainDensityPercentOverride);
    else
        Log("[OFF] RainDensity override disabled; native hidden setting is used.");

    if (particleRenderTargetResolutionMultiplier != 1)
    {
        if (particleFullResolutionDepthRestore &&
            particleRenderTargetResolutionMultiplier == 2)
        {
            // Verify the shader-side invariant BEFORE resizing the particle
            // targets. This avoids knowingly entering the old half-column
            // reconstruction with full-resolution buffers.
            if (VerifyParticleFullResolutionDepthRestore(exe))
            {
                if (ApplyParticleRenderTargetResolutionMultiplier(
                        exe, particleRenderTargetResolutionMultiplier))
                    ApplyParticleFullResolutionDepthRestore(exe);
            }
        }
        else
        {
            if (particleFullResolutionDepthRestore)
                Log("[SKIP] FullResolutionDepthRestore requires RenderTargetResolutionMultiplier=2.");
            ApplyParticleRenderTargetResolutionMultiplier(
                exe, particleRenderTargetResolutionMultiplier);
        }
    }
    else
    {
        if (particleFullResolutionDepthRestore)
            Log("[SKIP] FullResolutionDepthRestore requested while particle RTs remain native.");
        Log("[OFF] WSParticleRender target hierarchy left native (/2 and /16).");
    }

    if (std::fabs(lightVolumeCoordinateResolutionScale - 0.5f) > 0.0001f)
        ApplyLightVolumeCoordinateResolutionScale(
            exe, static_cast<double>(lightVolumeCoordinateResolutionScale));
    else
        Log("[OFF] WSLightVolume coordinate-resolution profile left native 0.5x.");

    if (lightVolumeResolutionMultiplier != 1)
        ApplyLightVolumeResolutionMultiplier(exe, lightVolumeResolutionMultiplier);
    else
        Log("[OFF] LightVolumeRT left native half-resolution.");

    if (wtfTransitionRingResolution != 128)
        ApplyWtfTransitionRingResolution(exe, wtfTransitionRingResolution);
    else
        Log("[OFF] WTF transition-ring RT family left native 128x128.");

    if (validatedEngineLimitsPack035)
        ApplyValidatedEngineLimitsPack035(exe);

    if (validatedSimpleEngineLimitsPack036)
        ApplyValidatedSimpleEngineLimitsPack036(exe);

    if ((!validatedEngineLimitsPack035 && wsLuaCallCapacity != 20) ||
        (!validatedSimpleEngineLimitsPack036 &&
         (wsParkingSpaceCapacity != 32 ||
          wsParticleInfoDataCapacity != 1400 ||
          wsActivateSphereCapacity != 256 ||
          wallGraphCapacity != 50)))
        ApplySimpleEngineLimits(
            exe,
            validatedEngineLimitsPack035 ? 20 : wsLuaCallCapacity,
            validatedSimpleEngineLimitsPack036 ? 32 : wsParkingSpaceCapacity,
            validatedSimpleEngineLimitsPack036 ? 1400 : wsParticleInfoDataCapacity,
            validatedSimpleEngineLimitsPack036 ? 256 : wsActivateSphereCapacity,
            validatedSimpleEngineLimitsPack036 ? 50 : wallGraphCapacity);
    else
        Log("[OFF] Additional simple engine limits left at retail capacities.");

    if (validatedParticleCapacityPack037)
        ApplyValidatedParticleCapacityPack037(exe);
    else
    {
        if (wsPhysicsParticleCapacity != 1000)
            ApplyWSPhysicsParticleCapacity(exe, wsPhysicsParticleCapacity);
        else
            Log("[OFF] WSPhysicsParticle left at retail capacity 1000.");

        if (wsParticleRenderMainCapacity != 4500 ||
            wsParticleRenderMediumCapacity != 1000 ||
            wsParticleRenderSmallCapacity != 500)
            ApplyWSParticleRenderCapacities(
                exe,
                wsParticleRenderMainCapacity,
                wsParticleRenderMediumCapacity,
                wsParticleRenderSmallCapacity);
        else
            Log("[OFF] WSParticleRender capacities left retail 4500/1000/500.");
    }

    if (havokToiEventQueue != 250)
        ApplyHavokToiEventQueue(exe, havokToiEventQueue);
    else
        Log("[OFF] Havok TOI event queue left at retail 250.");

    if (havokBroadPhaseQuerySize != 1024)
        ApplyHavokBroadPhaseQuerySize(exe, havokBroadPhaseQuerySize);
    else
        Log("[OFF] Havok broad-phase query size left at retail 1024.");

    if (wsPhGridObjectCapacity != 1000)
        ApplyWSPhGridObjectCapacity(exe, wsPhGridObjectCapacity);
    else
        Log("[OFF] WSPhGridObject left at retail capacity 1000.");

    if (streamingJobCapacity != 1200)
        ApplyStreamingJobCapacity(exe, streamingJobCapacity);
    else
        Log("[OFF] WSReadJob / WSUncompressJob left at retail 1200.");

    if (!validatedEngineLimitsPack035 && pblCrcTreeNodeCapacity != 40000)
        ApplyPblCrcTreeNodeCapacity(exe, pblCrcTreeNodeCapacity);
    else if (!validatedEngineLimitsPack035)
        Log("[OFF] PblCRCTreeNode left at retail 40000.");

    if (!validatedEngineLimitsPack035 &&
        (wsDamageSphereCapacity != 512 ||
         wsInventoryStateStowCapacity != 32))
        ApplyClassOwnedPoolConstants(
            exe,
            wsDamageSphereCapacity,
            wsInventoryStateStowCapacity);
    else if (!validatedEngineLimitsPack035)
        Log("[OFF] WSDamageSphere and WSInventoryStateStow left at retail capacities.");

    if (wsDecalCapacity != 400)
        ApplyWSDecalCapacity(exe, wsDecalCapacity);
    else
        Log("[OFF] WSDecal left at retail pool/active ceiling 400.");

    if (enableV310) ApplyV310(text, g_moduleBase);
    else Log("[OFF] V310 WSModel fix disabled by INI.");

    if (enableV311) ApplyV311(text, g_moduleBase);
    else Log("[OFF] V311 ModelInfo fix disabled by INI.");

    if (redPropRuntimeAudit)
    {
        if (InstallRedPropRuntimeAudit(exe))
            StartRedAuditHotkeys();
    }
    else
        Log("[OFF] Red-prop runtime audit disabled by INI.");

    if (wsDynamicPartPriorityRadius != 25)
        ApplyWSDynamicPartPriorityRadius(exe, static_cast<float>(wsDynamicPartPriorityRadius));
    else
        Log("[OFF] WSDynamicPart priority radius left at native 25.");

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
        InterlockedExchange(&g_redAuditThreadRun, 0);
        if (g_log)
        {
            Log("ASI unload.");
            AcquireSRWLockExclusive(&g_logLock);
            std::fclose(g_log);
            g_log = nullptr;
            ReleaseSRWLockExclusive(&g_logLock);
        }
    }
    return TRUE;
}
