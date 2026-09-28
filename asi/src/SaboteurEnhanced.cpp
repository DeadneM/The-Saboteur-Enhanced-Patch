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
static INIT_ONCE g_initOnce = INIT_ONCE_STATIC_INIT;
static FILE* g_log = nullptr;
static SRWLOCK g_logLock = SRWLOCK_INIT;

static float g_shadowCasterMinLodDistance = 180.0f;
static uintptr_t g_shadowCasterReturn = 0;
static float g_particleLodMinDistance = 100.0f;

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
    const uintptr_t rvas[] = {0x00DD61B4, 0x00DD61B8};
    const float expected[] = {1024.0f, 1024.0f};
    const float value = static_cast<float>(resolution);
    const float values[] = {value, value};
    return ApplyFloatGroup(exe, "Shadow map resolution", rvas, expected, 2, values);
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

static bool ApplyCsmQuality(HMODULE exe, int value)
{
    if (value < 2 || value > 8)
    {
        Log("[FAIL] CSMQuality=%d outside 2..8.", value);
        return false;
    }
    const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
    auto* address = reinterpret_cast<uint8_t*>(base + 0x00D60F6C);
    const uint8_t expected = 0x02;
    if (*address != expected)
    {
        Log("[SKIP] CSM quality native byte mismatch at RVA 0x00D60F6C.");
        return false;
    }
    const uint8_t patch = static_cast<uint8_t>(value);
    if (!WriteBytes(address, &patch, 1))
        return false;
    Log("[OK] CSM internal quality selector 2 -> %d applied.", value);
    return true;
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
    const float expectedScale = 2.0f;
    const float fullResScale = 1.0f;
    auto* scale = reinterpret_cast<float*>(base + 0x00D5FE30);
    auto* dimPatchA = reinterpret_cast<uint8_t*>(base + 0x00D69362);
    auto* dimPatchB = reinterpret_cast<uint8_t*>(base + 0x00D694FC);

    if (!VerifyScalarBytes(scale, &expectedScale, sizeof(expectedScale)) ||
        *dimPatchA != 0x80 || *dimPatchB != 0x04)
    {
        Log("[SKIP] Full-resolution AO native signature mismatch.");
        return false;
    }

    const uint8_t zero = 0;
    if (!WriteBytes(scale, &fullResScale, sizeof(fullResScale)) ||
        !WriteBytes(dimPatchA, &zero, 1) ||
        !WriteBytes(dimPatchB, &zero, 1))
    {
        Log("[FAIL] Full-resolution AO write failed.");
        return false;
    }

    Log("[OK] Full-resolution AO buffers/sample scale restored.");
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
    static const ExactBytePatch patches[] = {
        {0x00D7C27B,0xBA,0xB9},{0x00D7C283,0x3A,0x39},
        {0x00D7CA0F,0xBA,0xB9},{0x00D7CA17,0x3A,0x39},
        {0x00D7D30B,0xBA,0xB9},{0x00D7D313,0xBA,0xB9},
        {0x00D7D31F,0x3A,0x39},{0x00D7D323,0xBA,0xB9},
        {0x00D7D337,0x3A,0x39},{0x00D7D33B,0xBA,0xB9},
        {0x00D7D367,0x3A,0x39},{0x00D7D36B,0xBA,0xB9},
        {0x00D7D37F,0x3A,0x39},{0x00D7D383,0xBA,0xB9},
        {0x00D7DF0F,0xBA,0xB9},{0x00D7DF17,0xBA,0xB9},
        {0x00D7DF23,0x3A,0x39},{0x00D7DF27,0xBA,0xB9},
        {0x00D7DF3B,0x3A,0x39},{0x00D7DF3F,0xBA,0xB9},
        {0x00D7DF6B,0x3A,0x39},{0x00D7DF6F,0xBA,0xB9},
        {0x00D7DF83,0x3A,0x39},{0x00D7DF87,0xBA,0xB9}
    };
    return ApplyExactBytePatchSet(exe, "Shadow PCF 5x5 redirects", patches, _countof(patches));
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
    static double storage = 0.5;
    storage = scale;
    const uintptr_t sites[] = {
        0x00026054, 0x0002609B, 0x00026151
    };

    for (uintptr_t rva : sites)
    {
        if (!PatchAbsoluteOperand32(
                exe, rva, 0xDC, 0x0D, 0x00B7AC88,
                &storage, "Spot-shadow Z-buffer resolution scale"))
            return false;
    }

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

    // WSDecal pool initialization, VA 0x0098D7C9:
    //   push 0x190
    if (!PatchPushImm32(exe, 0x0058D7C9, 400u,
                        static_cast<uint32_t>(capacity),
                        "WSDecal pool"))
        return false;

    // WSDecal active-list ceiling, VA 0x0098E997:
    //   cmp ecx,0x190
    auto* cap = reinterpret_cast<uint8_t*>(base + 0x0058E997);
    const uint8_t expectedPrefix[2] = {0x81,0xF9};
    if (std::memcmp(cap, expectedPrefix, sizeof(expectedPrefix)) != 0)
    {
        Log("[SKIP] WSDecal active-ceiling opcode mismatch.");
        return false;
    }

    uint32_t current = 0;
    std::memcpy(&current, cap + 2, sizeof(current));
    if (current != 400u)
    {
        Log("[SKIP] WSDecal active-ceiling value mismatch: expected 400 got %u.", current);
        return false;
    }

    const uint32_t value = static_cast<uint32_t>(capacity);
    if (!WriteBytes(cap + 2, &value, sizeof(value)))
    {
        Log("[FAIL] WSDecal active-ceiling write failed.");
        return false;
    }

    Log("[OK] WSDecal pool + active ceiling 400 -> %d.", capacity);
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


static bool ApplyWSDamageableVariantSelectorBypass(const SectionRange& text, uintptr_t moduleBase)
{
    // WSDamageablePart child visibility selector.
    //
    // Native path at VA 0x0066797B..0x006679B2 selects one of two child groups
    // from:
    //   parent state this+0x20 == 0/1
    //   child resource flag +0x28 bit 0
    //
    // It then ANDs the selector result with the incoming visibility boolean:
    //   and al, byte ptr [esp+24h]
    //
    // A/B: preserve the incoming visibility directly, bypassing only this
    // 0/1 variant-group selector.
    static const uint8_t sig[] = {
        0xB8,0x01,0x00,0x00,0x00,
        0xEB,0x02,
        0x33,0xC0,
        0x22,0x44,0x24,0x24,
        0x8A,0x4E,0x4C,
        0x8A,0xD8
    };

    uint8_t* hit = FindExact(text, sig, sizeof(sig));
    if (!hit)
    {
        Log("[SKIP] WSDamageablePart variant-selector signature not found.");
        return false;
    }

    uint8_t* target = hit + 9;
    static const uint8_t expected[] = {0x22,0x44,0x24,0x24};
    static const uint8_t patch[] = {0x8A,0x44,0x24,0x24};

    if (std::memcmp(target, expected, sizeof(expected)) != 0)
    {
        Log("[SKIP] WSDamageablePart selector target bytes mismatch.");
        return false;
    }

    if (!WriteBytes(target, patch, sizeof(patch)))
    {
        Log("[FAIL] WSDamageablePart selector write failed.");
        return false;
    }

    Log("[OK] WSDamageablePart variant selector bypassed at RVA 0x%08X.",
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

    Log("SaboteurEnhanced ASI 0.10 RETAIL EXE PARAMETER AUDIT");
    Log("Architecture: validated Core 1 + complete retail EXE parameter audit");
    Log("Module base: 0x%08X", static_cast<unsigned>(g_moduleBase));

    const std::wstring iniPath = dir + L"\\SaboteurEnhanced.ini";
    const bool enableV310 = GetPrivateProfileIntW(L"Fixes", L"WSModelFullRenderMask", 1, iniPath.c_str()) != 0;
    const bool enableV311 = GetPrivateProfileIntW(L"Fixes", L"ModelInfoFullRenderSlice", 1, iniPath.c_str()) != 0;
    const bool enableOdinChildVisibility = GetPrivateProfileIntW(L"Fixes", L"OdinChildVisibilityGate", 0, iniPath.c_str()) != 0;
    const bool enableWSDamageableVariant = GetPrivateProfileIntW(L"Fixes", L"WSDamageableVariantSelector", 0, iniPath.c_str()) != 0;
    const int wsDynamicPartPriorityRadius = GetPrivateProfileIntW(L"Fixes", L"WSDynamicPartPriorityRadius", 25, iniPath.c_str());
    const int environmentMapResolution = GetPrivateProfileIntW(L"Graphics", L"EnvironmentMapResolution", 2048, iniPath.c_str());
    const int anisotropicFiltering = GetPrivateProfileIntW(L"Graphics", L"AnisotropicFiltering", 16, iniPath.c_str());
    const float mipLodBias = ReadIniFloat(iniPath, L"Graphics", L"MipLODBias", -0.25f);
    const float toneMap = ReadIniFloat(iniPath, L"Graphics", L"ToneMap", 0.15f);

    const int shadowMapResolution = GetPrivateProfileIntW(L"Shadows", L"ShadowMapResolution", 4096, iniPath.c_str());
    const bool shadowPcf5x5 = GetPrivateProfileIntW(L"Shadows", L"ShadowPCF5x5", 1, iniPath.c_str()) != 0;
    const int csmQuality = GetPrivateProfileIntW(L"Shadows", L"CSMQuality", 5, iniPath.c_str());
    const float csmLambda = ReadIniFloat(iniPath, L"Shadows", L"CSMLambda", 0.60f);
    const float csmFarDistance = ReadIniFloat(iniPath, L"Shadows", L"CSMFarDistance", 150.0f);
    const float shadowDepthBiasScale = ReadIniFloat(iniPath, L"Shadows", L"DepthBiasScale", 0.75f);
    const float shadowSlopeBiasScale = ReadIniFloat(iniPath, L"Shadows", L"SlopeBiasScale", 0.90f);
    const float spotShadowResolutionScale = ReadIniFloat(iniPath, L"Shadows", L"SpotShadowResolutionScale", 1.0f);

    const bool fullResolutionAo = GetPrivateProfileIntW(L"AmbientOcclusion", L"FullResolution", 1, iniPath.c_str()) != 0;
    const float aoBlurScale = ReadIniFloat(iniPath, L"AmbientOcclusion", L"BlurScale", 1.25f);
    const float aoErodeScale = ReadIniFloat(iniPath, L"AmbientOcclusion", L"ErodeScale", 1.25f);

    const float streamCoverageLow = ReadIniFloat(iniPath, L"Streaming", L"CoverageLow", 16000.0f);
    const float streamCoverageMedium = ReadIniFloat(iniPath, L"Streaming", L"CoverageMedium", 3200.0f);
    const float streamCoverageHigh = ReadIniFloat(iniPath, L"Streaming", L"CoverageHigh", 2500.0f);

    const float farSceneDistance = ReadIniFloat(iniPath, L"Distances", L"FarScene", 320.0f);
    const float decalVisibilityDistance = ReadIniFloat(iniPath, L"Distances", L"DecalVisibility", 160.0f);

    const float renderSlice3HighFar = ReadIniFloat(iniPath, L"Distances", L"RenderSlice3HighFar", 300.0f);
    const float renderSliceHighOuter = ReadIniFloat(iniPath, L"Distances", L"RenderSliceHighOuter", 1500.0f);
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

    const bool scaledTextureFullResolution = GetPrivateProfileIntW(L"ExperimentalPostFX", L"ScaledTextureFullResolution", 0, iniPath.c_str()) != 0;
    const float depthBlurMaskResolutionScale = ReadIniFloat(iniPath, L"ExperimentalPostFX", L"DepthBlurMaskResolutionScale", 0.5f);
    const int willToFightGridResolution = GetPrivateProfileIntW(L"WillToFight", L"GridResolution", 256, iniPath.c_str());
    const float veryFarSceneProfile0Threshold = ReadIniFloat(iniPath, L"ExperimentalDistances", L"VeryFarSceneProfile0Threshold", 22.0f);
    const float veryFarSceneProfile1Threshold = ReadIniFloat(iniPath, L"ExperimentalDistances", L"VeryFarSceneProfile1Threshold", 49.0f);

    const float depthBlurColorPyramidFactor = ReadIniFloat(iniPath, L"ExperimentalPostFX", L"DepthBlurColorPyramidFactor", 0.75f);
    const int skyDomeResolutionMultiplier = GetPrivateProfileIntW(L"Sky", L"ResolutionMultiplier", 1, iniPath.c_str());

    const float damageBlurResolutionScale = ReadIniFloat(iniPath, L"ExperimentalPostFX", L"DamageBlurResolutionScale", 0.5f);
    const int rainDensityPercentOverride = GetPrivateProfileIntW(L"Rain", L"DensityPercentOverride", 0, iniPath.c_str());
    const int particleRenderTargetResolutionMultiplier = GetPrivateProfileIntW(L"Particles", L"RenderTargetResolutionMultiplier", 1, iniPath.c_str());
    const int lightVolumeResolutionMultiplier = GetPrivateProfileIntW(L"Lighting", L"LightVolumeResolutionMultiplier", 1, iniPath.c_str());
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
    const int streamingJobCapacity = GetPrivateProfileIntW(L"EngineLimits", L"StreamingJobCapacity", 1200, iniPath.c_str());
    const int pblCrcTreeNodeCapacity = GetPrivateProfileIntW(L"EngineLimits", L"PblCRCTreeNodeCapacity", 40000, iniPath.c_str());

    const int wsDamageSphereCapacity = GetPrivateProfileIntW(L"EngineLimits", L"WSDamageSphereCapacity", 512, iniPath.c_str());
    const int wsInventoryStateStowCapacity = GetPrivateProfileIntW(L"EngineLimits", L"WSInventoryStateStowCapacity", 32, iniPath.c_str());
    const int wsDecalCapacity = GetPrivateProfileIntW(L"EngineLimits", L"WSDecalCapacity", 400, iniPath.c_str());

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
    Log("WSDamageableVariantSelector=%d", enableWSDamageableVariant ? 1 : 0);
    Log("WSDynamicPartPriorityRadius=%d", wsDynamicPartPriorityRadius);
    Log("[Graphics] EnvironmentMapResolution=%d AF=%d MipLODBias=%.3f ToneMap=%.3f",
        environmentMapResolution, anisotropicFiltering, mipLodBias, toneMap);
    Log("[Shadows] ShadowMapResolution=%d PCF5x5=%d CSMQuality=%d Lambda=%.3f Far=%.3f Bias=%.3f/%.3f SpotScale=%.3f",
        shadowMapResolution, shadowPcf5x5 ? 1 : 0, csmQuality, csmLambda, csmFarDistance,
        shadowDepthBiasScale, shadowSlopeBiasScale, spotShadowResolutionScale);
    Log("[AO] FullResolution=%d Blur=%.3f Erode=%.3f", fullResolutionAo ? 1 : 0, aoBlurScale, aoErodeScale);
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
    Log("[ExperimentalPostFX] ScaledTextureFullResolution=%d DepthBlurMaskResolutionScale=%.3f",
        scaledTextureFullResolution ? 1 : 0, depthBlurMaskResolutionScale);
    Log("[WillToFight] GridResolution=%d", willToFightGridResolution);
    Log("[ExperimentalDistances] VeryFarSceneProfileThresholds=%.3f/%.3f",
        veryFarSceneProfile0Threshold, veryFarSceneProfile1Threshold);
    Log("[ExperimentalPostFX] DepthBlurColorPyramidFactor=%.3f", depthBlurColorPyramidFactor);
    Log("[Sky] ResolutionMultiplier=%d", skyDomeResolutionMultiplier);
    Log("[ExperimentalPostFX] DamageBlurResolutionScale=%.3f", damageBlurResolutionScale);
    Log("[Rain] DensityPercentOverride=%d", rainDensityPercentOverride);
    Log("[Particles] RenderTargetResolutionMultiplier=%d", particleRenderTargetResolutionMultiplier);
    Log("[Lighting] LightVolumeResolutionMultiplier=%d", lightVolumeResolutionMultiplier);
    Log("[WillToFight] TransitionRingResolution=%d", wtfTransitionRingResolution);
    Log("[EngineLimits] WSLuaCall=%d Parking=%d ParticleInfo=%d ActivateSphere=%d WallGraph=%d",
        wsLuaCallCapacity, wsParkingSpaceCapacity, wsParticleInfoDataCapacity,
        wsActivateSphereCapacity, wallGraphCapacity);
    Log("[EngineLimits] WSPhysicsParticle=%d ParticleRender=%d/%d/%d",
        wsPhysicsParticleCapacity,
        wsParticleRenderMainCapacity,
        wsParticleRenderMediumCapacity,
        wsParticleRenderSmallCapacity);
    Log("[EngineLimits] HavokTOI=%d StreamingJobs=%d PblCRC=%d",
        havokToiEventQueue, streamingJobCapacity, pblCrcTreeNodeCapacity);
    Log("[EngineLimits] DamageSphere=%d InventoryStow=%d",
        wsDamageSphereCapacity, wsInventoryStateStowCapacity);
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

    if (csmQuality != 2) ApplyCsmQuality(exe, csmQuality);
    else Log("[OFF] CSM internal quality selector left at native 2.");

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

    if (std::fabs(streamCoverageLow - 1500.0f) > 0.01f ||
        std::fabs(streamCoverageMedium - 300.0f) > 0.01f ||
        std::fabs(streamCoverageHigh - 250.0f) > 0.01f)
        ApplyStreamCoverage(exe, streamCoverageLow, streamCoverageMedium, streamCoverageHigh);
    else
        Log("[OFF] Streaming coverage left native.");

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

    if (scaledTextureFullResolution)
        ApplyScaledTextureFullResolution(exe);
    else
        Log("[OFF] ScaledTexture left native half-resolution.");

    if (std::fabs(depthBlurMaskResolutionScale - 0.5f) > 0.0001f)
        ApplyDepthBlurMaskResolutionScale(exe, static_cast<double>(depthBlurMaskResolutionScale));
    else
        Log("[OFF] DepthBlur mask resolution scale left native 0.5.");

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

    if (std::fabs(depthBlurColorPyramidFactor - 0.75f) > 0.0001f)
        ApplyDepthBlurColorPyramidFactor(exe, static_cast<double>(depthBlurColorPyramidFactor));
    else
        Log("[OFF] DepthBlur color pyramid left at native factor 0.75.");

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
        ApplyParticleRenderTargetResolutionMultiplier(exe, particleRenderTargetResolutionMultiplier);
    else
        Log("[OFF] WSParticleRender target hierarchy left native (/2 and /16).");

    if (lightVolumeResolutionMultiplier != 1)
        ApplyLightVolumeResolutionMultiplier(exe, lightVolumeResolutionMultiplier);
    else
        Log("[OFF] LightVolumeRT left native half-resolution.");

    if (wtfTransitionRingResolution != 128)
        ApplyWtfTransitionRingResolution(exe, wtfTransitionRingResolution);
    else
        Log("[OFF] WTF transition-ring RT family left native 128x128.");

    if (wsLuaCallCapacity != 20 ||
        wsParkingSpaceCapacity != 32 ||
        wsParticleInfoDataCapacity != 1400 ||
        wsActivateSphereCapacity != 256 ||
        wallGraphCapacity != 50)
        ApplySimpleEngineLimits(
            exe,
            wsLuaCallCapacity,
            wsParkingSpaceCapacity,
            wsParticleInfoDataCapacity,
            wsActivateSphereCapacity,
            wallGraphCapacity);
    else
        Log("[OFF] Simple engine limits left at retail capacities.");

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

    if (havokToiEventQueue != 250)
        ApplyHavokToiEventQueue(exe, havokToiEventQueue);
    else
        Log("[OFF] Havok TOI event queue left at retail 250.");

    if (streamingJobCapacity != 1200)
        ApplyStreamingJobCapacity(exe, streamingJobCapacity);
    else
        Log("[OFF] WSReadJob / WSUncompressJob left at retail 1200.");

    if (pblCrcTreeNodeCapacity != 40000)
        ApplyPblCrcTreeNodeCapacity(exe, pblCrcTreeNodeCapacity);
    else
        Log("[OFF] PblCRCTreeNode left at retail 40000.");

    if (wsDamageSphereCapacity != 512 ||
        wsInventoryStateStowCapacity != 32)
        ApplyClassOwnedPoolConstants(
            exe,
            wsDamageSphereCapacity,
            wsInventoryStateStowCapacity);
    else
        Log("[OFF] WSDamageSphere and WSInventoryStateStow left at retail capacities.");

    if (enableV310) ApplyV310(text, g_moduleBase);
    else Log("[OFF] V310 WSModel fix disabled by INI.");

    if (enableV311) ApplyV311(text, g_moduleBase);
    else Log("[OFF] V311 ModelInfo fix disabled by INI.");

    if (enableOdinChildVisibility) ApplyOdinChildVisibilityGate(text, g_moduleBase);
    else Log("[OFF] Odin child-visibility A/B disabled by INI.");

    if (enableWSDamageableVariant) ApplyWSDamageableVariantSelectorBypass(text, g_moduleBase);
    else Log("[OFF] WSDamageablePart variant-selector A/B disabled by INI.");

    if (wsDynamicPartPriorityRadius != 25)
        ApplyWSDynamicPartPriorityRadius(exe, static_cast<float>(wsDynamicPartPriorityRadius));
    else
        Log("[OFF] WSDynamicPart priority radius left at native 25.");

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
