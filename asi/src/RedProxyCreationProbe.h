// 0.50: passive capture at the native proxy-creation CALL (x86 only).
// No red-proxy suppression, model changes, renderer changes or hot patches outside
// the single callsite. A mismatch leaves the original CALL untouched.
static uintptr_t g_proxyNative = 0;
static uintptr_t g_proxyContinue = 0;
static volatile LONG g_proxyEvents = 0;
static volatile LONG g_proxyBudget = 0;
static SRWLOCK g_proxyLastLock = SRWLOCK_INIT;
struct ProxyCapture
{
    DWORD id, tid, regs[8], args[8], result, object[48], ecx[16], arg0[16];
    bool objectOK, ecxOK, arg0OK, inspect;
};
static ProxyCapture g_proxyLast = {};
static volatile LONG g_proxyHasLast = 0;
// Per-thread stack pairs pre- and post-creation even if the native creator nests.
static __declspec(thread) ProxyCapture g_proxyPending[8] = {};
static __declspec(thread) unsigned g_proxyDepth = 0;

static bool ProxyCopy(uintptr_t address, DWORD* out, size_t words)
{
    std::memset(out, 0, words * sizeof(DWORD));
    if (!address || !IsReadableAddress(reinterpret_cast<const void*>(address),
                                       words * sizeof(DWORD))) return false;
    __try
    {
        std::memcpy(out, reinterpret_cast<const void*>(address),
                    words * sizeof(DWORD));
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static void ProxyLogWords(const char* name, const DWORD* data, size_t words)
{
    Log("[PROXY50] %s", name);
    for (size_t i = 0; i + 3 < words; i += 4)
        Log("[PROXY50]   +%03X: %08X %08X %08X %08X",
            static_cast<unsigned>(i * 4), data[i], data[i+1], data[i+2], data[i+3]);
}
static void ProxyLogCapture(const ProxyCapture& f)
{
    Log("[PROXY50] completed #%u thread=%u native return EAX=%08X (candidate pointer, type unverified)",
        f.id, f.tid, f.result);
    Log("[PROXY50] input EDI=%08X ESI=%08X EBP=%08X EBX=%08X EDX=%08X ECX=%08X EAX=%08X",
        f.regs[0],f.regs[1],f.regs[2],f.regs[4],f.regs[5],f.regs[6],f.regs[7]);
    ProxyLogWords("eight raw caller stack arguments", f.args, 8);
    if (f.objectOK) ProxyLogWords("returned EAX object candidate (192 bytes)", f.object, 48);
    else Log("[PROXY50] returned EAX does not have 192 readable bytes.");
    if (f.ecxOK) ProxyLogWords("entry ECX pointee (64 bytes)", f.ecx, 16);
    if (f.arg0OK) ProxyLogWords("entry first-stack-argument pointee (64 bytes)", f.arg0, 16);
}
static void __cdecl ProxyBefore(const DWORD* regs)
{
    LONG id=InterlockedIncrement(&g_proxyEvents);
    unsigned depth=g_proxyDepth++;
    if (depth >= _countof(g_proxyPending)) return;
    ProxyCapture& f=g_proxyPending[depth];
    f.id=static_cast<DWORD>(id);
    f.tid=GetCurrentThreadId();
    f.inspect=(id<=8 || id%256==0 ||
        InterlockedCompareExchange(&g_proxyBudget,0,0)>0);
    if (!f.inspect) return;
    std::memcpy(f.regs,regs,sizeof(f.regs));
    // PUSHAD saved ESP is after PUSHFD; first CALL stack arg is ESP+8.
    ProxyCopy(static_cast<uintptr_t>(regs[3])+8, f.args, 8);
}
static void __cdecl ProxyAfter(const DWORD* regs)
{
    if (!g_proxyDepth) return;
    unsigned depth=--g_proxyDepth;
    if (depth >= _countof(g_proxyPending)) return;
    ProxyCapture& f=g_proxyPending[depth];
    if (!f.inspect) return;
    f.result=regs[7]; // PUSHAD saved EAX: genuine native return value.
    f.objectOK=ProxyCopy(f.result,f.object,48);
    f.ecxOK=ProxyCopy(f.regs[6],f.ecx,16);
    f.arg0OK=ProxyCopy(f.args[0],f.arg0,16);
    AcquireSRWLockExclusive(&g_proxyLastLock);
    g_proxyLast=f;
    InterlockedExchange(&g_proxyHasLast,1);
    ReleaseSRWLockExclusive(&g_proxyLastLock);
    if (InterlockedCompareExchange(&g_proxyBudget,0,0)>0)
    {
        InterlockedDecrement(&g_proxyBudget);
        ProxyLogCapture(f);
    }
    else if (f.id<=8 || f.id%256==0)
        Log("[PROXY50] native completed #%u EAX=%08X readable=%u (F7/F8 detailed sampling)",
            f.id,f.result,f.objectOK?1u:0u);
}
__declspec(naked) static void ProxyAfterThunk()
{
    __asm
    {
        pushfd
        pushad
        push esp
        call ProxyAfter
        add esp,4
        popad
        popfd
        jmp dword ptr [g_proxyContinue]
    }
}
__declspec(naked) static void ProxyCallShim()
{
    __asm
    {
        pushfd
        pushad
        push esp
        call ProxyBefore
        add esp,4
        popad
        popfd
        // Original direct CALL pushed a known continuation; replace its return
        // with our thunk, preserving argument layout and input EFLAGS.
        lea esp,[esp+4]
        push offset ProxyAfterThunk
        jmp dword ptr [g_proxyNative]
    }
}
static bool InstallProxyNativeCreationProbe(HMODULE exe)
{
    const uintptr_t base=reinterpret_cast<uintptr_t>(exe);
    constexpr uintptr_t kCall=0x00092F38, kCallee=0x00588B30;
    SectionRange code=GetSectionRange(exe,".text");
    const uintptr_t begin=reinterpret_cast<uintptr_t>(code.begin);
    auto* callsite=reinterpret_cast<uint8_t*>(base+kCall);
    if (!code.begin || base+kCall<begin || base+kCall+5>begin+code.size)
    {
        Log("[SKIP] PROXY50 callsite not inside .text.");
        return false;
    }
    if (callsite[0]!=0xE8)
    {
        Log("[SKIP] PROXY50 RVA 00092F38 not a direct CALL (opcode %02X).",callsite[0]);
        return false;
    }
    int32_t displacement=0;
    std::memcpy(&displacement,callsite+1,sizeof(displacement));
    const uintptr_t destination=reinterpret_cast<uintptr_t>(callsite+5)+
        static_cast<intptr_t>(displacement);
    if (destination!=base+kCallee)
    {
        Log("[SKIP] PROXY50 target %08X instead of verified expected %08X.",
            static_cast<unsigned>(destination), static_cast<unsigned>(base+kCallee));
        return false;
    }
    // WSCivilianProp valid-result path: log read-only bytes, no guessed hook.
    DWORD realEntry[4]={};
    if (ProxyCopy(base+0x00092DD5,realEntry,4))
        Log("[PROXY50] real WSCivilianProp branch +92DD5 raw bytes: %08X %08X %08X %08X",
            realEntry[0],realEntry[1],realEntry[2],realEntry[3]);
    const intptr_t diff=reinterpret_cast<intptr_t>(&ProxyCallShim)-
        reinterpret_cast<intptr_t>(callsite+5);
    if (diff<INT32_MIN || diff>INT32_MAX)
    {
        Log("[SKIP] PROXY50 hook not rel32-reachable.");
        return false;
    }
    uint8_t patch[5]={0xE8,0,0,0,0};
    const int32_t rel=static_cast<int32_t>(diff);
    std::memcpy(patch+1,&rel,4);
    g_proxyNative=destination;
    g_proxyContinue=reinterpret_cast<uintptr_t>(callsite+5);
    if (!WriteBytes(callsite,patch,sizeof(patch)))
    {
        Log("[FAIL] PROXY50 native CALL write failed.");
        return false;
    }
    Log("[OK] PROXY50 hooked native creator CALL RVA 00092F38 -> 00588B30; post-return EAX capture.");
    Log("[PROXY50] read-only diagnostics; no fallback suppression and no material edits.");
    return true;
}
static void ProxyDumpLatest()
{
    if (!InterlockedCompareExchange(&g_proxyHasLast,0,0))
    {
        Log("[PROXY50] F9: no native create return observed.");
        return;
    }
    ProxyCapture snap={};
    AcquireSRWLockShared(&g_proxyLastLock);
    snap=g_proxyLast;
    ReleaseSRWLockShared(&g_proxyLastLock);
    Log("[PROXY50] F9: copied snapshot (no stale live pointer dereference).");
    ProxyLogCapture(snap);
}
static DWORD WINAPI ProxyHotkeyThread(LPVOID)
{
    InterlockedExchange(&g_redAuditThreadRun,1);
    Log("[PROXY50] F6 quiet; F7 capture next 16; F8 capture next 64; F9 latest snapshot. All keep native props.");
    while (InterlockedCompareExchange(&g_redAuditThreadRun,0,0))
    {
        if (GetAsyncKeyState(VK_F6)&1) { InterlockedExchange(&g_proxyBudget,0); Log("[PROXY50] F6 quiet."); }
        if (GetAsyncKeyState(VK_F7)&1) { InterlockedExchange(&g_proxyBudget,16); Log("[PROXY50] F7 16-event capture."); }
        if (GetAsyncKeyState(VK_F8)&1) { InterlockedExchange(&g_proxyBudget,64); Log("[PROXY50] F8 64-event capture."); }
        if (GetAsyncKeyState(VK_F9)&1) ProxyDumpLatest();
        Sleep(50);
    }
    return 0;
}
static bool StartProxyHotkeys()
{
    HANDLE t=CreateThread(nullptr,0,ProxyHotkeyThread,nullptr,0,nullptr);
    if (!t) { Log("[FAIL] PROXY50 hotkey thread creation."); return false; }
    CloseHandle(t);
    return true;
}
