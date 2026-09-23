#!/usr/bin/env python3
"""The Saboteur Enhanced Patch v1Pv260 - cumulative Windows patcher."""
import base64, hashlib, json, os, shutil, sys, tempfile, time, traceback, zlib
from pathlib import Path

PATCHER_VERSION=1
PAYLOAD_VERSION=260
DESIGNATION="v1Pv260"
GAME_EXE="Saboteur.exe"
TARGET_SHA="a90acc384bab67b7ac54bb973a81852ab2f8bce232d9215cc569af5f5d725440"
TARGET_SIZE=14834176
ORIGINAL_SHA="e917fe956d09d39267021c09753aea1fc0002629b317818b80179fe78b35d8a6"
V258Y_SHA="032889675706926c60c54ea2ced31cbb6703b5b4ac9872f4da27413cc9993f4e"
V259_SHA="8f9883883abab91ae029b078326e93751664204e75ebb2f83044ae014d13fc0b"
PAYLOADS={
    ORIGINAL_SHA:("Original retail","retail_to_v260.sabdp1"),
    V258Y_SHA:("V258Y validated","v258y_to_v260.sabdp1"),
    V259_SHA:("V259 validated","v259_to_v260.sabdp1"),
}

def h(b): return hashlib.sha256(b).hexdigest()
def hf(p):
    x=hashlib.sha256()
    with p.open("rb") as f:
        for c in iter(lambda:f.read(1<<20),b""): x.update(c)
    return x.hexdigest()

def pdir():
    if getattr(sys,"frozen",False) and hasattr(sys,"_MEIPASS"): return Path(sys._MEIPASS)/"payloads"
    return Path(__file__).resolve().parent/"payloads"

def payload(name):
    s=(pdir()/name).read_text(encoding="ascii").strip()
    d=json.loads(zlib.decompress(base64.b85decode(s.encode("ascii"))).decode())
    if d.get("format")!="SABDP1": raise ValueError("Unsupported payload format")
    return d

def rebuild(src,name):
    d=payload(name)
    if len(src)!=d["source_size"] or h(src)!=d["source_sha256"]: raise ValueError("Source verification failed")
    out=bytearray(src)
    for n,(off,bh,ah) in enumerate(d["regions"]):
        b,a=bytes.fromhex(bh),bytes.fromhex(ah)
        if len(b)!=len(a): raise ValueError(f"Invalid payload region {n}")
        if bytes(out[off:off+len(b)])!=b: raise ValueError(f"Byte verification failed at 0x{off:08X}")
        out[off:off+len(a)]=a
    out=bytes(out)
    if len(out)!=d["target_size"] or h(out)!=d["target_sha256"]: raise ValueError("Target verification failed")
    return out

def log(f,s=""):
    print(s); f.write(s+"\n"); f.flush()

def find_exe(arg=None):
    c=[]
    if arg:
        p=Path(arg.strip('"')).expanduser(); c.append(p/GAME_EXE if p.is_dir() else p)
    c.append(Path.cwd()/GAME_EXE)
    c.append((Path(sys.executable).resolve().parent if getattr(sys,"frozen",False) else Path(__file__).resolve().parent.parent)/GAME_EXE)
    for p in c:
        if p.is_file() and p.name.lower()==GAME_EXE.lower(): return p.resolve()

def choose_exe():
    arg=sys.argv[1] if len(sys.argv)>1 and not sys.argv[1].startswith("--") else None
    p=find_exe(arg)
    if p: return p
    print(f"[ .. ] {GAME_EXE} not found next to the patcher or in the current directory.")
    try: s=input("Game folder or Saboteur.exe path (blank to cancel): ").strip()
    except EOFError: return None
    return find_exe(s) if s else None

def backup(p,sha):
    q=p.with_name(p.name+".Backup")
    if not q.exists(): shutil.copy2(p,q); return q
    try:
        if hf(q)==sha: return q
    except OSError: pass
    q=p.with_name(p.name+".Backup."+time.strftime("%Y%m%d-%H%M%S")); shutil.copy2(p,q); return q

def install(p,data):
    fd,n=tempfile.mkstemp(prefix="Saboteur.Enhanced.",suffix=".tmp",dir=p.parent); os.close(fd)
    stage=Path(n); roll=p.with_name(p.name+".EnhancedRollback.tmp")
    try:
        stage.write_bytes(data)
        if hf(stage)!=TARGET_SHA: raise RuntimeError("Staged target hash mismatch")
        roll.unlink(missing_ok=True); os.replace(p,roll)
        try: os.replace(stage,p)
        except Exception: os.replace(roll,p); raise
        if hf(p)!=TARGET_SHA:
            p.unlink(missing_ok=True); os.replace(roll,p); raise RuntimeError("Installed target hash mismatch; rollback restored")
        roll.unlink(missing_ok=True)
    finally: stage.unlink(missing_ok=True)

def verify_payloads():
    for label,name in [
        ("Retail -> V260","retail_to_v260.sabdp1"),
        ("V258Y -> V260","v258y_to_v260.sabdp1"),
        ("V259 -> V260","v259_to_v260.sabdp1"),
    ]:
        d=payload(name); assert d["target_sha256"]==TARGET_SHA and d["target_size"]==TARGET_SIZE
        for off,b,a in d["regions"]: assert off>=0 and len(bytes.fromhex(b))==len(bytes.fromhex(a))
        print(f"[OK] {label}: {len(d['regions'])} regions")
    print(f"[OK] Target V{PAYLOAD_VERSION}: {TARGET_SHA}"); return 0

def main():
    if "--verify-payloads" in sys.argv: return verify_payloads()
    print("="*68); print("The Saboteur - Enhanced Patch"); print(f"Unified cumulative patcher {DESIGNATION} | Target V{PAYLOAD_VERSION}"); print("="*68); print()
    p=choose_exe()
    if not p: print("[ERROR] Saboteur.exe not found. No files were changed."); return 2
    with (p.parent/"Saboteur_Enhanced_Patcher.log").open("w",encoding="utf-8",newline="\n") as f:
        log(f,f"[OK] Game directory: {p.parent}"); log(f,"[ .. ] Detecting and verifying Saboteur.exe...")
        src=p.read_bytes(); s=h(src); log(f,f"[ .. ] SHA-256: {s}")
        if s==TARGET_SHA and len(src)==TARGET_SIZE: log(f,f"[OK] Enhanced Patch V{PAYLOAD_VERSION} is already installed."); log(f,"No files were changed."); return 0
        k=PAYLOADS.get(s)
        if not k:
            log(f,""); log(f,"[ERROR] Unsupported or modified Saboteur.exe."); log(f,"Accepted exact SHA-256 states:")
            log(f,f"  Original retail: {ORIGINAL_SHA}"); log(f,f"  V258Y:           {V258Y_SHA}"); log(f,f"  V259:            {V259_SHA}"); log(f,f"  V260 target:     {TARGET_SHA}")
            log(f,"No files were changed and no backup was created."); return 3
        label,name=k; log(f,f"[OK] Source recognized: {label}"); log(f,f"[ .. ] Building cumulative V{PAYLOAD_VERSION} in memory...")
        out=rebuild(src,name); log(f,f"[OK] Reconstructed target SHA-256: {h(out)}")
        q=backup(p,s); log(f,f"[OK] Backup: {q.name}"); log(f,"[ .. ] Installing verified target...")
        install(p,out); log(f,f"[OK] Installed Enhanced Patch V{PAYLOAD_VERSION}."); log(f,f"[OK] Final SHA-256: {hf(p)}"); log(f,"\nInstallation complete.")
    return 0

if __name__=="__main__":
    try: code=main()
    except PermissionError as e: print(f"[ERROR] Permission denied: {e}\nClose the game and any program using Saboteur.exe, then try again."); code=10
    except Exception as e:
        print(f"[ERROR] {e}"); code=11
        try:
            with (Path.cwd()/"Saboteur_Enhanced_Patcher_error.log").open("w",encoding="utf-8") as f: traceback.print_exc(file=f)
        except Exception: pass
    if getattr(sys,"frozen",False):
        try: input("\nPress Enter to close...")
        except EOFError: pass
    raise SystemExit(code)
