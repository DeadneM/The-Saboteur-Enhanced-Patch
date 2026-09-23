#!/usr/bin/env python3
import json
from pathlib import Path

m=json.loads(Path("HASHES_V260.json").read_text(encoding="utf-8"))
designation=m["designation"]
patcher=int(m["versions"]["patcher"])
payload=int(m["versions"]["payload"])
v=(patcher,payload,0,0)
exe_name=f"The_Saboteur_Enhanced_Patcher_{designation}.exe"
content=f"""# UTF-8
VSVersionInfo(
  ffi=FixedFileInfo(filevers={v}, prodvers={v}, mask=0x3f, flags=0x0, OS=0x40004, fileType=0x1, subtype=0x0, date=(0,0)),
  kids=[
    StringFileInfo([StringTable('040904B0', [
      StringStruct('CompanyName','DeadneM'),
      StringStruct('FileDescription','The Saboteur Enhanced Patch'),
      StringStruct('FileVersion','{designation}'),
      StringStruct('InternalName','The_Saboteur_Enhanced_Patcher'),
      StringStruct('OriginalFilename','{exe_name}'),
      StringStruct('ProductName','The Saboteur Enhanced Patch'),
      StringStruct('ProductVersion','{designation}')
    ])]),
    VarFileInfo([VarStruct('Translation',[1033,1200])])
  ]
)
"""
Path("installer/version_info.txt").write_text(content,encoding="utf-8")
print(f"Generated version resource for {designation}")
