"""Inspect AC4 ISO directory metadata or disassemble an EE address range."""
import argparse, json, pathlib, struct
from ps2recomp.elf import ElfFile
from ps2recomp.r5900 import decode, GPR_NAMES

def inventory(iso):
    with iso.open('rb') as f:
        f.seek(16*2048); pvd=f.read(2048)
        if pvd[:7]!=b'\x01CD001\x01': raise ValueError('ISO9660 PVD missing')
        def record(d,p):
            return (d[p],int.from_bytes(d[p+2:p+6],'little'),int.from_bytes(d[p+10:p+14],'little'),d[p+25],d[p+33:p+33+d[p+32]].decode('ascii').split(';')[0])
        visited=set(); entries=[]
        def walk(lba,size,parent):
            if lba in visited: raise ValueError('Cyclic directory')
            visited.add(lba)
            if lba*2048+size>iso.stat().st_size: raise ValueError('Extent beyond disc')
            f.seek(lba*2048); data=f.read(size); pos=0
            while pos<len(data):
                if not data[pos]: pos=(pos//2048+1)*2048; continue
                n,sector,length,flags,name=record(data,pos); pos+=n
                if name in ('\x00','\x01'): continue
                path=parent+'/'+name
                entries.append(dict(path=path,lba=sector,size=length,directory=bool(flags&2)))
                if flags&2: walk(sector,length,path)
        _,sector,size,_,_=record(pvd,156); walk(sector,size,'')
        return entries

def disassemble(elf,start,count):
    for a in range(start,start+count*4,4):
        i=decode(elf.word(a),a)
        print(f'{a:08X} {i.word:08X} {i.name:12s} rs={GPR_NAMES[i.rs]:4s} rt={GPR_NAMES[i.rt]:4s} rd={GPR_NAMES[i.rd]:4s} imm={i.simm:6d} target={i.target:08X} branch={i.btarget:08X}')

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('mode',choices=['iso','disasm']); ap.add_argument('path',type=pathlib.Path)
    ap.add_argument('--start',type=lambda x:int(x,0),default=0x100008); ap.add_argument('--count',type=int,default=32)
    args=ap.parse_args()
    if args.mode=='iso': print(json.dumps(inventory(args.path),indent=2))
    else: disassemble(ElfFile(str(args.path)),args.start,args.count)
