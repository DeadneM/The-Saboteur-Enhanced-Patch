#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Unknwn.h>
#include <cwchar>
#include <string>

using DirectInput8CreateFn = HRESULT (WINAPI*)(HINSTANCE,DWORD,REFIID,LPVOID*,LPUNKNOWN);
using DllCanUnloadNowFn = HRESULT (WINAPI*)();
using DllGetClassObjectFn = HRESULT (WINAPI*)(REFCLSID,REFIID,LPVOID*);
using DllRegisterServerFn = HRESULT (WINAPI*)();
using DllUnregisterServerFn = HRESULT (WINAPI*)();
using GetdfDIJoystickFn = const void* (WINAPI*)();

static INIT_ONCE g_once = INIT_ONCE_STATIC_INIT;
static HMODULE g_real = nullptr;
static DirectInput8CreateFn g_DirectInput8Create = nullptr;
static DllCanUnloadNowFn g_DllCanUnloadNow = nullptr;
static DllGetClassObjectFn g_DllGetClassObject = nullptr;
static DllRegisterServerFn g_DllRegisterServer = nullptr;
static DllUnregisterServerFn g_DllUnregisterServer = nullptr;
static GetdfDIJoystickFn g_GetdfDIJoystick = nullptr;

using AsiInitFn = BOOL (__cdecl*)();

static std::wstring ExeDirectory()
{
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash) *slash = L'\0';
    return path;
}

static BOOL CALLBACK InitializeProxy(PINIT_ONCE, PVOID, PVOID*)
{
    wchar_t sys[MAX_PATH] = {};
    if (!GetSystemDirectoryW(sys, MAX_PATH))
        return TRUE;

    wcscat_s(sys, L"\\dinput8.dll");
    g_real = LoadLibraryW(sys);
    if (g_real)
    {
        g_DirectInput8Create = reinterpret_cast<DirectInput8CreateFn>(GetProcAddress(g_real, "DirectInput8Create"));
        g_DllCanUnloadNow = reinterpret_cast<DllCanUnloadNowFn>(GetProcAddress(g_real, "DllCanUnloadNow"));
        g_DllGetClassObject = reinterpret_cast<DllGetClassObjectFn>(GetProcAddress(g_real, "DllGetClassObject"));
        g_DllRegisterServer = reinterpret_cast<DllRegisterServerFn>(GetProcAddress(g_real, "DllRegisterServer"));
        g_DllUnregisterServer = reinterpret_cast<DllUnregisterServerFn>(GetProcAddress(g_real, "DllUnregisterServer"));
        g_GetdfDIJoystick = reinterpret_cast<GetdfDIJoystickFn>(GetProcAddress(g_real, "GetdfDIJoystick"));
    }

    const std::wstring asiPath = ExeDirectory() + L"\\SaboteurEnhanced.asi";
    HMODULE asi = LoadLibraryW(asiPath.c_str());
    if (asi)
    {
        AsiInitFn init = reinterpret_cast<AsiInitFn>(GetProcAddress(asi, "SaboteurEnhanced_Initialize"));
        if (!init)
            init = reinterpret_cast<AsiInitFn>(GetProcAddress(asi, "_SaboteurEnhanced_Initialize"));
        if (init)
            init();
    }
    return TRUE;
}

static void EnsureInitialized()
{
    InitOnceExecuteOnce(&g_once, InitializeProxy, nullptr, nullptr);
}

extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE a,DWORD b,REFIID c,LPVOID* d,LPUNKNOWN e)
{
    EnsureInitialized();
    return g_DirectInput8Create ? g_DirectInput8Create(a,b,c,d,e) : E_FAIL;
}
extern "C" HRESULT WINAPI DllCanUnloadNow()
{
    EnsureInitialized();
    return g_DllCanUnloadNow ? g_DllCanUnloadNow() : S_FALSE;
}
extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID a,REFIID b,LPVOID* c)
{
    EnsureInitialized();
    return g_DllGetClassObject ? g_DllGetClassObject(a,b,c) : CLASS_E_CLASSNOTAVAILABLE;
}
extern "C" HRESULT WINAPI DllRegisterServer()
{
    EnsureInitialized();
    return g_DllRegisterServer ? g_DllRegisterServer() : E_NOTIMPL;
}
extern "C" HRESULT WINAPI DllUnregisterServer()
{
    EnsureInitialized();
    return g_DllUnregisterServer ? g_DllUnregisterServer() : E_NOTIMPL;
}
extern "C" const void* WINAPI GetdfDIJoystick()
{
    EnsureInitialized();
    return g_GetdfDIJoystick ? g_GetdfDIJoystick() : nullptr;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(module);
    return TRUE;
}
