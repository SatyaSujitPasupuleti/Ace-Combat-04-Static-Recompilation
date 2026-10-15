"""Asset-free authored mip-chain/Q-LOD regression at native and 5x scale."""
import math, os, struct, subprocess, tempfile
from pathlib import Path
import argparse

def fixture(path, mmin=5, bias=0, lod_scale=0, update_mip=False):
    stream=bytearray(); count=0
    def reg(r,v):
        nonlocal count
        stream.extend(struct.pack('<BBQ',1,r,v));count+=1
    def priv(r,v):
        nonlocal count
        stream.extend(struct.pack('<BIQ',3,r,v));count+=1
    def vertex(x,y,q):
        qb=struct.unpack('<I',struct.pack('<f',q))[0]
        st=struct.unpack('<I',struct.pack('<f',q*0.5))[0]
        reg(1,0x80808080 | qb<<32);reg(2,st | st<<32);reg(5,x*16 | (y*16<<16))
    bases=[0x200000,0x210000,0x220000]
    for field in range(4):
        priv(0x12000000,1);priv(0x12000070,1<<9);priv(0x12000080,63<<32 | 63<<44)
        reg(0x1a,1);reg(0x18,0);reg(0x40,63<<16 | 63<<48)
        reg(0x4e,1<<32);reg(0x47,0);reg(0x46,1);reg(0x4c,1<<16)
        for lv,(base,color) in enumerate(zip(bases,[0x800000ff,0x8000ff00,0x80ff0000])):
            if update_mip and field == 3 and lv == 1: color = 0x8000ffff
            size=16>>lv
            reg(0x50,(base//256<<32)|(1<<48));reg(0x51,0)
            reg(0x52,size | size<<32);reg(0x53,0)
            for i in range(size*size//2):reg(0x54,color | color<<32)
        reg(0x14,2<<2 | 1<<5 | mmin<<6 | lod_scale<<19 | (bias&4095)<<32)
        reg(6,bases[0]//256 | 1<<14 | 4<<26 | 4<<30 | 1<<34 | 1<<35)
        reg(0x34,(bases[1]//256 | 1<<14) | ((bases[2]//256 | 1<<14)<<20))
        reg(8,5)
        for band,q in enumerate([1,0.5,0.25,math.sqrt(0.5)]):
            reg(0,4 | 1<<4)
            for x,y in [(band*16,0),(band*16+16,0),(band*16,64),(band*16+16,64)]:vertex(x,y,q)
        stream.extend(struct.pack('<BII',11,field,field&1));count+=1
    stream.append(13);count+=1
    sizes=[11208,2448,64,4194304,16384,16384,16384,4096,4096]
    hdr=bytearray(176);hdr[:8]=b'GSCAP2\0\0'
    struct.pack_into('<11I',hdr,8,2,0,*sizes)
    struct.pack_into('<QQQQ',hdr,96,176+sum(sizes),len(stream),0,count)
    struct.pack_into('<I',hdr,128,4)
    path.write_bytes(hdr+bytes(sum(sizes))+stream)

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--replay',type=Path,required=True);args=ap.parse_args()
    replay=args.replay.resolve()
    with tempfile.TemporaryDirectory(prefix='gs-mips-') as td:
        root=Path(td)
        for mode,bias,lod_scale,update_mip in [(2,0,0,False),(3,0,0,False),(4,0,0,False),(5,0,0,False),(5,-16,1,False),(5,0,0,True)]:
            cap=root/'mips.gscap';fixture(cap,mode,bias,lod_scale,update_mip)
            for scale in (1,5):
                shot=root/'shot.ppm';env=os.environ.copy()
                env.update(PS2_UPSCALE=str(scale),PS2_SHADER_DIR=str(replay.parent/'shaders'))
                env.pop('PS2_NO_MIP',None)
                run=subprocess.run([str(replay),str(cap),'--fields','4','--shot',str(shot)],env=env,capture_output=True)
                if run.returncode:raise RuntimeError(run.stderr.decode(errors='replace'))
                with shot.open('rb') as f:
                    assert f.readline().strip()==b'P6'
                    w,h=map(int,f.readline().split());assert f.readline().strip()==b'255';data=f.read()
                samples=[tuple(data[((32*scale)*w+(x*scale))*3:((32*scale)*w+(x*scale))*3+3]) for x in (8,24,40,56)]
                expected=[(255,0,0),(0,255,0),(0,0,255),(128,128,0) if mode&1 else (0,255,0)]
                if bias: expected[3]=(255,0,0)
                if update_mip:
                    expected[1]=(255,255,0)
                    expected[3]=(255,128,0)
                assert all(abs(a-b)<=2 for got,want in zip(samples,expected) for a,b in zip(got,want)),(mode,scale,samples,expected)
                print('authored Q LOD',mode,bias,lod_scale,scale,'updated' if update_mip else 'static',samples,'PASS')
if __name__=='__main__':main()
