// 0.55: correlate verified real-lookup availability with fallback gate and native calls.
// No red-proxy suppression, model changes, renderer changes or hot patches outside
// the single callsite. A mismatch leaves the original CALL untouched.
static uintptr_t g_proxyNative = 0;
static uintptr_t g_proxyContinue = 0;
static volatile LONG g_proxyEvents = 0;
static volatile LONG g_proxyBudget = 0;
static volatile LONG g_proxyAutoFirst = 1; // first 16 calls are captured even before hotkeys
static uintptr_t g_fbContinue=0, g_fbExit=0;
static uintptr_t g_lookupContinue=0, g_lookupMiss=0;
static volatile LONG g_lookupCount=0, g_lookupNull=0, g_lookupEmpty=0;
static volatile LONG g_lookupAvailable=0, g_lookupUnreadable=0;
static volatile LONG g_lookupLinkedFallback=0, g_lookupUnlinkedFallback=0;
struct NativeLookup
{
    DWORD seq,tid,ebx,ebp,first,field04,field08,tick,firstObject[16];
    bool nodeReadOK, firstReadable;
    unsigned state; // 0 null, 1 invalid, 2 empty, 3 populated
};
static __declspec(thread) NativeLookup g_lookupLatest={};
static __declspec(thread) DWORD g_lookupLastLinked=0;
static volatile LONG g_fbEvents=0, g_fbNonzero=0, g_fbNearCall=0;
static volatile LONG g_fbActorMatch=0, g_fbTemplateMatch=0, g_fbDoubleMatch=0, g_fbMismatch=0;
static __declspec(thread) DWORD g_fbSeenTemplates[8]={};
static __declspec(thread) unsigned g_fbSeenTemplateCount=0;
struct FallbackVisit
{
    DWORD seq, tid, tick, ebx, owner, templ;
    bool ownerFieldOK, templateFieldOK;
};
static __declspec(thread) FallbackVisit g_fbThreadLast = {};
static __declspec(thread) DWORD g_fbThreadLastCompared = 0;
static SRWLOCK g_proxyLastLock = SRWLOCK_INIT;
struct ProxyCapture
{
    DWORD id, tid, regs[8], args[8], result, object[48], ecx[16], arg0[16];
    // Four input candidates. Snapshot 256 bytes before and after the real call.
    // 0: ECX; 1: stack arg0 / observed ESI; 2: stack arg1 / EDX; 3: EDI.
    DWORD candidates[4], before[4][64], after[4][64];
    bool beforeOK[4], afterOK[4];
    // ECX+207C: this is a *candidate* deep field from the 0.51 follow-up.
    DWORD farBefore[16], farAfter[16];
    bool farBeforeOK, farAfterOK;
    // Fields +08 and +1C of ECX are runtime pointer-like values. They
    // may be changing object identities; never diff two different pointees.
    DWORD childBeforePtr[2], childAfterPtr[2];
    DWORD childBefore[2][16], childAfter[2][16];
    bool childBeforeOK[2], childAfterOK[2];
    // The discredited native-EBX owner-guess fields from 0.52/0.53
    // are deliberately removed; only verified gate ownership is sampled.
    FallbackVisit fallback;
    DWORD fallbackAgeMs;
    bool fallbackFresh, fallbackRecent, verifiedActor, verifiedTemplate, verifiedBoth, firstTemplate;
    DWORD verifiedOwnerBefore[64], verifiedOwnerAfter[64], verifiedTemplateBefore[64], verifiedTemplateAfter[64];
    bool verifiedOwnerReadBefore, verifiedOwnerReadAfter, verifiedTemplateReadBefore, verifiedTemplateReadAfter;
    DWORD verifiedOwnerAfterPtr, verifiedTemplateAfterPtr;
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
    Log("[PROXY55] %s", name);
    for (size_t i = 0; i + 3 < words; i += 4)
        Log("[PROXY55]   +%03X: %08X %08X %08X %08X",
            static_cast<unsigned>(i * 4), data[i], data[i+1], data[i+2], data[i+3]);
}

static bool ProxyReadU32(uintptr_t address, DWORD* out)
{
    DWORD v=0;
    if (!ProxyCopy(address,&v,1)) { if(out) *out=0; return false; }
    if(out) *out=v;
    return true;
}


static void __cdecl ProxyNormalLookupObserved(uintptr_t node,uintptr_t actor)
{
    NativeLookup n={};
    n.seq=static_cast<DWORD>(InterlockedIncrement(&g_lookupCount));
    n.tid=GetCurrentThreadId();
    n.tick=GetTickCount();
    n.ebx=static_cast<DWORD>(actor);
    n.ebp=static_cast<DWORD>(node);
    if(!node)
    {
        n.state=0;
        InterlockedIncrement(&g_lookupNull);
    }
    else
    {
        DWORD words[3]={};
        n.nodeReadOK=ProxyCopy(node,words,3);
        if(!n.nodeReadOK)
        {
            n.state=1;
            InterlockedIncrement(&g_lookupUnreadable);
        }
        else
        {
            n.first=words[0];n.field04=words[1];n.field08=words[2];
            if(!n.first)
            {
                n.state=2;
                InterlockedIncrement(&g_lookupEmpty);
            }
            else
            {
                n.state=3;
                InterlockedIncrement(&g_lookupAvailable);
                // A successful table lookup is *not* yet proven to be
                // a loaded in-game material; never mutate this pointer.
                n.firstReadable=ProxyCopy(n.first,n.firstObject,16);
            }
        }
    }
    g_lookupLatest=n;
    if(n.seq<=12 || (n.state==3 && n.seq<=64) || n.seq%256==0)
    {
        Log("[PROXY55] REAL_LOOKUP #%u thread=%u actor=%08X EBP=%08X class=%u entry=%08X fields=%08X/%08X entry_readable=%u",
            n.seq,n.tid,n.ebx,n.ebp,n.state,
            n.first,n.field04,n.field08,n.firstReadable?1u:0u);
        if(n.state==3 && n.firstReadable && n.seq<=24)
            ProxyLogWords("REAL_LOOKUP populated target 64-byte prefix",n.firstObject,16);
    }
}
__declspec(naked) static void ProxyNormalLookupShim()
{
    __asm
    {
        pushfd
        pushad
        // Original instructions: mov ecx,ebp / test ecx,ecx / je +28.
        push ebx
        push ebp
        call ProxyNormalLookupObserved
        add esp,8
        popad
        popfd
        mov ecx,ebp
        test ecx,ecx
        jz lookup_miss
        jmp dword ptr [g_lookupContinue]
lookup_miss:
        jmp dword ptr [g_lookupMiss]
    }
}
static bool InstallProxyNormalLookupProbe(HMODULE exe)
{
    const uintptr_t base=reinterpret_cast<uintptr_t>(exe);
    constexpr uintptr_t kRva=0x00092DD5;
    auto* p=reinterpret_cast<uint8_t*>(base+kRva);
    SectionRange code=GetSectionRange(exe,".text");
    const uintptr_t start=reinterpret_cast<uintptr_t>(code.begin);
    if(!code.begin || base+kRva<start || base+kRva+11>start+code.size)
    {
        Log("[SKIP] PROXY55 real-lookup site outside .text.");
        return false;
    }
    // Signature copied from 0.50-0.54 read-only runtime bytes.
    static const uint8_t expected[11]={
        0x8B,0xCD,0x85,0xC9,0x74,0x28,0x83,0x39,0x00,0x74,0x23};
    if(std::memcmp(p,expected,11)!=0)
    {
        Log("[SKIP] PROXY55 real-lookup opcode signature mismatch at 00092DD5.");
        return false;
    }
    const intptr_t d=reinterpret_cast<intptr_t>(&ProxyNormalLookupShim)-
        reinterpret_cast<intptr_t>(p+5);
    if(d<INT32_MIN || d>INT32_MAX)
    {
        Log("[SKIP] PROXY55 real-lookup trampoline not reachable.");
        return false;
    }
    uint8_t patch[6]={0xE9,0,0,0,0,0x90};
    const int32_t rel=static_cast<int32_t>(d);
    std::memcpy(patch+1,&rel,4);
    g_lookupContinue=base+0x00092DDB;
    g_lookupMiss=base+0x00092E03; // Original JE target, also fallback gate.
    if(!WriteBytes(p,patch,sizeof(patch)))
    {
        Log("[FAIL] PROXY55 real-lookup hook failed.");
        return false;
    }
    Log("[OK] PROXY55 REAL_LOOKUP hooked at RVA 00092DD5; original MOV/TEST/JE behavior replayed.");
    return true;
}

static void __cdecl ProxyFallbackObserved(uintptr_t actor)
{
    FallbackVisit v={};
    v.seq=static_cast<DWORD>(InterlockedIncrement(&g_fbEvents));
    v.tid=GetCurrentThreadId();
    v.tick=GetTickCount();
    v.ebx=static_cast<DWORD>(actor);
    if(actor) InterlockedIncrement(&g_fbNonzero);
    // The EBX value is only interpreted at the exact verified gate.
    // A zero/unreadable owner/template is recorded, never guessed.
    v.ownerFieldOK=actor!=0 &&
        ProxyReadU32(actor+0x1384,&v.owner);
    v.templateFieldOK=v.ownerFieldOK && v.owner!=0 &&
        ProxyReadU32(static_cast<uintptr_t>(v.owner)+0x2DC,&v.templ);
    g_fbThreadLast=v;
    // A same-thread, same-actor normal lookup is a much stronger
    // relationship than just a recent timestamp.
    const NativeLookup& l=g_lookupLatest;
    const bool linked=l.seq && l.ebx==v.ebx &&
        l.seq!=g_lookupLastLinked;
    if(linked) { g_lookupLastLinked=l.seq; InterlockedIncrement(&g_lookupLinkedFallback); }
    else InterlockedIncrement(&g_lookupUnlinkedFallback);
    if(v.seq<=12 && l.seq)
        Log("[PROXY55] FALLBACK_FROM_LOOKUP gate=%u lookup=%u actor_match=%u lookup_class=%u gate_EBX=%08X lookup_EBX=%08X",
            v.seq,l.seq,linked?1u:0u,l.state,v.ebx,l.ebx);
    if(v.seq<=12 || v.seq%256==0)
        Log("[PROXY55] FALLBACK_GATE #%u thread=%u EBX=%08X owner=%08X owner_read=%u template=%08X template_read=%u",
            v.seq,v.tid,v.ebx,v.owner,v.ownerFieldOK?1u:0u,
            v.templ,v.templateFieldOK?1u:0u);
}
__declspec(naked) static void ProxyFallbackCorrelationShim()
{
    __asm
    {
        pushfd
        pushad
        push ebx
        call ProxyFallbackObserved
        add esp,4
        popad
        popfd
        // Original 85 DB / 0F 84 ... semantics: test EBX then conditional skip.
        test ebx,ebx
        jz fb_skip
        jmp dword ptr [g_fbContinue]
fb_skip:
        jmp dword ptr [g_fbExit]
    }
}
static bool InstallProxyFallbackCorrelationProbe(HMODULE exe)
{
    const uintptr_t base=reinterpret_cast<uintptr_t>(exe);
    constexpr uintptr_t kRva=0x00092E03;
    auto* p=reinterpret_cast<uint8_t*>(base+kRva);
    const SectionRange code=GetSectionRange(exe,".text");
    if(!code.begin ||
       base+kRva<reinterpret_cast<uintptr_t>(code.begin) ||
       base+kRva+8>reinterpret_cast<uintptr_t>(code.begin)+code.size)
    {
        Log("[SKIP] PROXY55 fallback gate outside .text.");
        return false;
    }
    static const uint8_t expected[8]={0x85,0xDB,0x0F,0x84,0x3B,0x01,0x00,0x00};
    if(std::memcmp(p,expected,8)!=0)
    {
        Log("[SKIP] PROXY55 fallback gate RVA 00092E03 signature mismatch.");
        return false;
    }
    const intptr_t delta=reinterpret_cast<intptr_t>(&ProxyFallbackCorrelationShim)-
        reinterpret_cast<intptr_t>(p+5);
    if(delta<INT32_MIN || delta>INT32_MAX)
    {
        Log("[SKIP] PROXY55 fallback hook unreachable.");
        return false;
    }
    uint8_t patch[8]={0xE9,0,0,0,0,0x90,0x90,0x90};
    const int32_t rel=static_cast<int32_t>(delta);
    std::memcpy(patch+1,&rel,4);
    g_fbContinue=base+0x00092E0B;
    g_fbExit=base+0x00092F46;
    if(!WriteBytes(p,patch,8))
    {
        Log("[FAIL] PROXY55 fallback hook write failed.");
        return false;
    }
    Log("[OK] PROXY55 fallback gate 00092E03 pass-through hooked; test EBX/JE semantics preserved.");
    return true;
}

static void ProxyLogDiff64(const char* name, DWORD ptrBefore, DWORD ptrAfter,
                           const DWORD* before, bool beforeOK,
                           const DWORD* after, bool afterOK)
{
    if (!beforeOK || !afterOK)
    {
        Log("[PROXY55] %s read before=%u after=%u at %08X / %08X",
            name,beforeOK?1u:0u,afterOK?1u:0u,ptrBefore,ptrAfter);
        return;
    }
    if(ptrBefore!=ptrAfter)
    {
        Log("[PROXY55] %s pointer transition %08X -> %08X (separate objects; not a same-object diff)",
            name,ptrBefore,ptrAfter);
        ProxyLogWords("old object prefix",before,16);
        ProxyLogWords("new object prefix",after,16);
        return;
    }
    unsigned changes=0;
    for(unsigned i=0;i<16;++i)
    {
        if(before[i]==after[i]) continue;
        Log("[PROXY55] DELTA %s=%08X +%02X: %08X -> %08X",
            name,ptrBefore,i*4,before[i],after[i]);
        ++changes;
    }
    Log("[PROXY55] DELTA_SUM %s=%08X changed=%u/16 dwords",name,ptrBefore,changes);
}

static void ProxyLogCapture(const ProxyCapture& f)
{
    Log("[PROXY55] completed #%u thread=%u native return EAX=%08X (candidate pointer, type unverified)",
        f.id, f.tid, f.result);
    Log("[PROXY55] input EDI=%08X ESI=%08X EBP=%08X EBX=%08X EDX=%08X ECX=%08X EAX=%08X",
        f.regs[0],f.regs[1],f.regs[2],f.regs[4],f.regs[5],f.regs[6],f.regs[7]);
    ProxyLogWords("eight raw caller stack arguments", f.args, 8);
    if (f.objectOK) ProxyLogWords("returned EAX object candidate (192 bytes)", f.object, 48);
    else Log("[PROXY55] returned EAX does not have 192 readable bytes.");
    if (f.ecxOK) ProxyLogWords("entry ECX pointee (64 bytes)", f.ecx, 16);
    if (f.arg0OK) ProxyLogWords("entry first-stack-argument pointee (64 bytes)", f.arg0, 16);
    const char* const labels[4]={"ECX","ARG0/ESI","ARG1/EDX","EDI"};
    for (int c=0;c<4;++c)
    {
        if (!f.beforeOK[c] || !f.afterOK[c])
        {
            Log("[PROXY55] %s=%08X before readable=%u after readable=%u (cannot diff)",
                labels[c],f.candidates[c],f.beforeOK[c]?1u:0u,f.afterOK[c]?1u:0u);
            continue;
        }
        unsigned changed=0;
        for (unsigned w=0;w<64;++w)
        {
            if (f.before[c][w]==f.after[c][w]) continue;
            if (changed<20)
                Log("[PROXY55] DELTA %s=%08X +%03X: %08X -> %08X",
                    labels[c],f.candidates[c],w*4,f.before[c][w],f.after[c][w]);
            ++changed;
        }
        Log("[PROXY55] DELTA_SUM %s=%08X changed=%u/64 dwords (first 20 differences logged)",
            labels[c],f.candidates[c],changed);
    }
    const DWORD farBeforeAddress=f.regs[6]+0x207C;
    const DWORD farAfterAddress=f.regs[6]+0x207C;
    ProxyLogDiff64("ECX+207C",farBeforeAddress,farAfterAddress,
        f.farBefore,f.farBeforeOK,f.farAfter,f.farAfterOK);
    for(int i=0;i<2;++i)
    {
        const char* name=i==0?"ECX[+08] child":"ECX[+1C] child";
        Log("[PROXY55] %s pointer BEFORE=%08X AFTER=%08X",
            name,f.childBeforePtr[i],f.childAfterPtr[i]);
        ProxyLogDiff64(name,f.childBeforePtr[i],f.childAfterPtr[i],
            f.childBefore[i],f.childBeforeOK[i],f.childAfter[i],f.childAfterOK[i]);
    }
    Log("[PROXY55] native EBX=%08X; no guessed nativeEBX+1384 owner dereferences.",
        f.regs[4]);

    Log("[PROXY55] MATCH actor_plus_D78=%u ARG0_eq_template=%u FULL_OWNER_LINK=%u gateEBX=%08X nativeEBX=%08X owner=%08X template=%08X",
        f.verifiedActor?1u:0u,f.verifiedTemplate?1u:0u,f.verifiedBoth?1u:0u,
        f.fallback.ebx,f.regs[4],f.fallback.owner,f.fallback.templ);
    if(f.verifiedBoth)
    {
        const bool matchChain=f.verifiedOwnerAfterPtr==f.fallback.owner &&
                              f.verifiedTemplateAfterPtr==f.fallback.templ;
        Log("[PROXY55] VERIFIED_CHAIN_AFTER stable=%u ownerAfter=%08X templateAfter=%08X",
            matchChain?1u:0u,f.verifiedOwnerAfterPtr,f.verifiedTemplateAfterPtr);
        const char* names[2]={"OWNER","TEMPLATE"};
        const DWORD* oldBuf[2]={f.verifiedOwnerBefore,f.verifiedTemplateBefore};
        const DWORD* newBuf[2]={f.verifiedOwnerAfter,f.verifiedTemplateAfter};
        const bool oldOK[2]={f.verifiedOwnerReadBefore,f.verifiedTemplateReadBefore};
        const bool newOK[2]={f.verifiedOwnerReadAfter,f.verifiedTemplateReadAfter};
        for(int c=0;c<2;++c)
        {
            unsigned changes=0;
            if(oldOK[c] && newOK[c] && matchChain)
            {
                for(unsigned j=0;j<64;++j)
                {
                    if(oldBuf[c][j]==newBuf[c][j]) continue;
                    if(changes<12) Log("[PROXY55] VERIFIED_DELTA %s +%03X: %08X -> %08X",
                        names[c],j*4,oldBuf[c][j],newBuf[c][j]);
                    ++changes;
                }
                Log("[PROXY55] VERIFIED_DIFF_SUM %s changed=%u/64",names[c],changes);
            }
            else Log("[PROXY55] VERIFIED_DIFF_UNAVAILABLE %s before=%u after=%u stable=%u",
                names[c],oldOK[c]?1u:0u,newOK[c]?1u:0u,matchChain?1u:0u);
        }
        if(f.firstTemplate && f.verifiedTemplateReadBefore)
            ProxyLogWords("FIRST_SEEN VERIFIED TEMPLATE 256-byte BEFORE snapshot",
                f.verifiedTemplateBefore,64);
    }
    if(f.fallback.seq)
        Log("[PROXY55] FALLBACK_GATE last_same_thread seq=%u age_ms=%u fresh_since_previous_call=%u nearby_1sec=%u gate_EBX=%08X owner=%08X owner_field_readable=%u template=%08X template_field_readable=%u",
            f.fallback.seq,f.fallbackAgeMs,f.fallbackFresh?1u:0u,f.fallbackRecent?1u:0u,
            f.fallback.ebx,f.fallback.owner,f.fallback.ownerFieldOK?1u:0u,
            f.fallback.templ,f.fallback.templateFieldOK?1u:0u);
    else
        Log("[PROXY55] FALLBACK_GATE no gate entry recorded on this native-call thread.");
}
static void __cdecl ProxyBefore(const DWORD* regs)
{
    LONG id=InterlockedIncrement(&g_proxyEvents);
    unsigned depth=g_proxyDepth++;
    if (depth >= _countof(g_proxyPending)) return;
    ProxyCapture& f=g_proxyPending[depth];
    f.id=static_cast<DWORD>(id);
    f.tid=GetCurrentThreadId();
    f.fallback=g_fbThreadLast;
    f.fallbackAgeMs=f.fallback.seq ? GetTickCount()-f.fallback.tick : 0;
    f.fallbackFresh=f.fallback.seq && f.fallback.seq!=g_fbThreadLastCompared;
    f.fallbackRecent=f.fallbackFresh && f.fallbackAgeMs<=1000;
    if(f.fallbackFresh) g_fbThreadLastCompared=f.fallback.seq;
    if(f.fallbackRecent) InterlockedIncrement(&g_fbNearCall);
    f.inspect=((id<=16 && InterlockedCompareExchange(&g_proxyAutoFirst,0,0)>0)
        || id%256==0 || InterlockedCompareExchange(&g_proxyBudget,0,0)>0);
    if (!f.inspect) return;
    std::memcpy(f.regs,regs,sizeof(f.regs));
    // PUSHAD saved ESP is after PUSHFD; first CALL stack arg is ESP+8.
    const bool stackValid=ProxyCopy(static_cast<uintptr_t>(regs[3])+8,f.args,8);
    // Measured in the 0.53 user log: 16/16 exact matches of both
    // nativeEBX = gateEBX + 0xD78 and native ARG0 = gate template.
    f.verifiedActor=f.fallbackFresh && f.fallback.ebx!=0 &&
        f.regs[4]==static_cast<DWORD>(f.fallback.ebx+0xD78);
    f.verifiedTemplate=f.fallbackFresh && stackValid &&
        f.fallback.templateFieldOK && f.fallback.templ!=0 &&
        f.args[0]==f.fallback.templ;
    f.verifiedBoth=f.verifiedActor && f.verifiedTemplate &&
        f.fallback.ownerFieldOK && f.fallback.owner!=0;
    if(f.fallbackFresh && f.fallback.templateFieldOK && f.fallback.templ)
    {
        if(f.verifiedActor) InterlockedIncrement(&g_fbActorMatch);
        if(f.verifiedTemplate) InterlockedIncrement(&g_fbTemplateMatch);
        if(f.verifiedBoth) InterlockedIncrement(&g_fbDoubleMatch);
        else InterlockedIncrement(&g_fbMismatch);
    }
    f.firstTemplate=false;
    if(f.verifiedBoth)
    {
        f.verifiedOwnerReadBefore=ProxyCopy(f.fallback.owner,f.verifiedOwnerBefore,64);
        f.verifiedTemplateReadBefore=ProxyCopy(f.fallback.templ,f.verifiedTemplateBefore,64);
        if(f.verifiedTemplateReadBefore)
        {
            bool seen=false;
            for(unsigned i=0;i<g_fbSeenTemplateCount;++i)
                if(g_fbSeenTemplates[i]==f.fallback.templ) {seen=true;break;}
            if(!seen && g_fbSeenTemplateCount<_countof(g_fbSeenTemplates))
            {
                g_fbSeenTemplates[g_fbSeenTemplateCount++]=f.fallback.templ;
                f.firstTemplate=true;
            }
        }
    }
    f.candidates[0]=f.regs[6]; // ECX / this-like candidate
    f.candidates[1]=f.args[0]; // in 0.50 it matched ESI
    f.candidates[2]=f.args[1]; // in 0.50 it matched EDX
    f.candidates[3]=f.regs[0]; // EDI
    for (unsigned c=0;c<4;++c)
        f.beforeOK[c]=ProxyCopy(f.candidates[c],f.before[c],64);
    // Save all initial data while caller pointers still refer to this event.
    f.farBeforeOK=ProxyCopy(static_cast<uintptr_t>(f.regs[6])+0x207C,
                            f.farBefore,16);
    const DWORD ecx=f.regs[6];
    ProxyReadU32(static_cast<uintptr_t>(ecx)+0x08,&f.childBeforePtr[0]);
    ProxyReadU32(static_cast<uintptr_t>(ecx)+0x1C,&f.childBeforePtr[1]);
    for(unsigned i=0;i<2;++i)
        f.childBeforeOK[i]=ProxyCopy(f.childBeforePtr[i],f.childBefore[i],16);
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
    for (unsigned c=0;c<4;++c)
        f.afterOK[c]=ProxyCopy(f.candidates[c],f.after[c],64);
    f.farAfterOK=ProxyCopy(static_cast<uintptr_t>(f.regs[6])+0x207C,
                           f.farAfter,16);
    const DWORD ecx=f.regs[6];
    ProxyReadU32(static_cast<uintptr_t>(ecx)+0x08,&f.childAfterPtr[0]);
    ProxyReadU32(static_cast<uintptr_t>(ecx)+0x1C,&f.childAfterPtr[1]);
    for(unsigned i=0;i<2;++i)
        f.childAfterOK[i]=ProxyCopy(f.childAfterPtr[i],f.childAfter[i],16);
    if(f.verifiedBoth)
    {
        ProxyReadU32(static_cast<uintptr_t>(f.fallback.ebx)+0x1384,&f.verifiedOwnerAfterPtr);
        ProxyReadU32(static_cast<uintptr_t>(f.fallback.owner)+0x2DC,&f.verifiedTemplateAfterPtr);
        if(f.verifiedOwnerAfterPtr==f.fallback.owner && f.verifiedTemplateAfterPtr==f.fallback.templ)
        {
            f.verifiedOwnerReadAfter=ProxyCopy(f.fallback.owner,f.verifiedOwnerAfter,64);
            f.verifiedTemplateReadAfter=ProxyCopy(f.fallback.templ,f.verifiedTemplateAfter,64);
        }
    }
    AcquireSRWLockExclusive(&g_proxyLastLock);
    g_proxyLast=f;
    InterlockedExchange(&g_proxyHasLast,1);
    ReleaseSRWLockExclusive(&g_proxyLastLock);
    const bool budget=InterlockedCompareExchange(&g_proxyBudget,0,0)>0;
    if (budget) InterlockedDecrement(&g_proxyBudget);
    if (budget || (f.id<=16 && InterlockedCompareExchange(&g_proxyAutoFirst,0,0)>0))
        ProxyLogCapture(f);
    else if (f.id%256==0)
        Log("[PROXY55] native completed #%u EAX=%08X (F7/F8 to capture more)",f.id,f.result);
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
        Log("[SKIP] PROXY55 callsite not inside .text.");
        return false;
    }
    if (callsite[0]!=0xE8)
    {
        Log("[SKIP] PROXY55 RVA 00092F38 not a direct CALL (opcode %02X).",callsite[0]);
        return false;
    }
    int32_t displacement=0;
    std::memcpy(&displacement,callsite+1,sizeof(displacement));
    const uintptr_t destination=reinterpret_cast<uintptr_t>(callsite+5)+
        static_cast<intptr_t>(displacement);
    if (destination!=base+kCallee)
    {
        Log("[SKIP] PROXY55 target %08X instead of verified expected %08X.",
            static_cast<unsigned>(destination), static_cast<unsigned>(base+kCallee));
        return false;
    }
    // WSCivilianProp valid-result path: log read-only bytes, no guessed hook.
    DWORD realEntry[4]={};
    if (ProxyCopy(base+0x00092DD5,realEntry,4))
        Log("[PROXY55] real WSCivilianProp branch +92DD5 raw bytes: %08X %08X %08X %08X",
            realEntry[0],realEntry[1],realEntry[2],realEntry[3]);
    DWORD callerTail[8]={},calleeHead[16]={};
    if (ProxyCopy(base+kCall+5,callerTail,8))
        ProxyLogWords("native caller AFTER CALL (32 bytes, read-only)",callerTail,8);
    if (ProxyCopy(base+kCallee,calleeHead,16))
        ProxyLogWords("native callee ENTRY (64 bytes, read-only)",calleeHead,16);
    const intptr_t diff=reinterpret_cast<intptr_t>(&ProxyCallShim)-
        reinterpret_cast<intptr_t>(callsite+5);
    if (diff<INT32_MIN || diff>INT32_MAX)
    {
        Log("[SKIP] PROXY55 hook not rel32-reachable.");
        return false;
    }
    uint8_t patch[5]={0xE8,0,0,0,0};
    const int32_t rel=static_cast<int32_t>(diff);
    std::memcpy(patch+1,&rel,4);
    g_proxyNative=destination;
    g_proxyContinue=reinterpret_cast<uintptr_t>(callsite+5);
    if (!WriteBytes(callsite,patch,sizeof(patch)))
    {
        Log("[FAIL] PROXY55 native CALL write failed.");
        return false;
    }
    Log("[OK] PROXY55 hooked native creator CALL RVA 00092F38 -> 00588B30; post-return EAX capture.");
    Log("[PROXY55] guarded reads of ECX+207C, ECX[+08]/[+1C] pointees and EBX+1384 -> owner+2DC.");
    Log("[PROXY55] guarded 256-byte gate OWNER/TEMPLATE and read-only normal real-lookup availability are observed.");
    Log("[PROXY55] Lookup counters: null/empty/available from RVA 92DD5, correlated by actor to fallback RVA 92E03.");
    Log("[PROXY55] read-only diagnostics; no fallback suppression and no material edits.");
    if(!InstallProxyFallbackCorrelationProbe(exe))
        Log("[PROXY55] fallback gate capture unavailable; native call hook still pass-through.");
    if(!InstallProxyNormalLookupProbe(exe))
        Log("[PROXY55] real lookup capture unavailable; normal game path remains untouched.");
    return true;
}
static void ProxyDumpLatest()
{
    if (!InterlockedCompareExchange(&g_proxyHasLast,0,0))
    {
        Log("[PROXY55] F9: no native create return observed.");
        return;
    }
    ProxyCapture snap={};
    AcquireSRWLockShared(&g_proxyLastLock);
    snap=g_proxyLast;
    ReleaseSRWLockShared(&g_proxyLastLock);
    Log("[PROXY55] F9: native calls=%u fallback gate entries=%u nonzero EBX=%u near_same_thread_le_1sec=%u",
        static_cast<unsigned>(InterlockedCompareExchange(&g_proxyEvents,0,0)),
        static_cast<unsigned>(InterlockedCompareExchange(&g_fbEvents,0,0)),
        static_cast<unsigned>(InterlockedCompareExchange(&g_fbNonzero,0,0)),
        static_cast<unsigned>(InterlockedCompareExchange(&g_fbNearCall,0,0)));
    Log("[PROXY55] F9: actor+D78 matches=%u template_ARG0_matches=%u full_verified=%u mismatches=%u",
        static_cast<unsigned>(InterlockedCompareExchange(&g_fbActorMatch,0,0)),
        static_cast<unsigned>(InterlockedCompareExchange(&g_fbTemplateMatch,0,0)),
        static_cast<unsigned>(InterlockedCompareExchange(&g_fbDoubleMatch,0,0)),
        static_cast<unsigned>(InterlockedCompareExchange(&g_fbMismatch,0,0)));
    Log("[PROXY55] F9 LOOKUP normal_total=%u null=%u unreadable=%u empty=%u populated=%u fallback_actor_linked=%u fallback_unlinked=%u",
        static_cast<unsigned>(InterlockedCompareExchange(&g_lookupCount,0,0)),
        static_cast<unsigned>(InterlockedCompareExchange(&g_lookupNull,0,0)),
        static_cast<unsigned>(InterlockedCompareExchange(&g_lookupUnreadable,0,0)),
        static_cast<unsigned>(InterlockedCompareExchange(&g_lookupEmpty,0,0)),
        static_cast<unsigned>(InterlockedCompareExchange(&g_lookupAvailable,0,0)),
        static_cast<unsigned>(InterlockedCompareExchange(&g_lookupLinkedFallback,0,0)),
        static_cast<unsigned>(InterlockedCompareExchange(&g_lookupUnlinkedFallback,0,0)));
    Log("[PROXY55] F9: replaying latest native event; never confuse a lookup with a confirmed rendered model.");
    ProxyLogCapture(snap);
}
static DWORD WINAPI ProxyHotkeyThread(LPVOID)
{
    InterlockedExchange(&g_redAuditThreadRun,1);
    Log("[PROXY55] AUTO first 16 native calls, F6 quiet, F7 next 16, F8 next 64, F9 latest; fallback-gate counters always on.");
    while (InterlockedCompareExchange(&g_redAuditThreadRun,0,0))
    {
        if (GetAsyncKeyState(VK_F6)&1) { InterlockedExchange(&g_proxyBudget,0); InterlockedExchange(&g_proxyAutoFirst,0); Log("[PROXY55] F6: quiet; startup auto capture stopped."); }
        if (GetAsyncKeyState(VK_F7)&1) { InterlockedExchange(&g_proxyBudget,16); Log("[PROXY55] F7 16-event capture."); }
        if (GetAsyncKeyState(VK_F8)&1) { InterlockedExchange(&g_proxyBudget,64); Log("[PROXY55] F8 64-event capture."); }
        if (GetAsyncKeyState(VK_F9)&1) ProxyDumpLatest();
        Sleep(50);
    }
    return 0;
}
static bool StartProxyHotkeys()
{
    HANDLE t=CreateThread(nullptr,0,ProxyHotkeyThread,nullptr,0,nullptr);
    if (!t) { Log("[FAIL] PROXY55 hotkey thread creation."); return false; }
    CloseHandle(t);
    return true;
}
