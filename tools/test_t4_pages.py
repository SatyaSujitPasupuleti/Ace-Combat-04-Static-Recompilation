"""Asset-free integration: changed T4 pages, palette and texture alpha state."""
from pathlib import Path
import os,struct,subprocess,hashlib,json
r=Path(__file__).resolve().parents[1];root=r/'work/native60-live-20261013/t4-page-tests'
def fixture(path,fields):
 stream=bytearray();count=0
 def reg(k,v):
  nonlocal count
  stream.extend(struct.pack('<BBQ',1,k,v));count+=1
 def priv(k,v):
  nonlocal count
  stream.extend(struct.pack('<BIQ',3,k,v));count+=1
 def pixels(index,x=0,y=0,size=256):
  reg(0x50,(0x200000//256<<32)|(4<<48)|(20<<56))
  reg(0x51,(x<<32)|(y<<48));reg(0x52,size|(size<<32));reg(0x53,0)
  word=int(str(hex(index)[2:])*16,16)
  for _ in range(size*size//16):reg(0x54,word)
 for field in range(fields):
  priv(0x12000000,1);priv(0x12000070,2<<9);priv(0x12000080,127<<32|127<<44)
  reg(0x1a,1);reg(0x18,0);reg(0x40,127<<16|127<<48)
  reg(0x4e,1<<32);reg(0x47,0);reg(0x46,1);reg(0x4c,2<<16)
  reg(0x14,0);reg(8,5);reg(0x3b,128<<32 if field>=5 else 0)
  if field in (0,4):
   reg(0x50,(0x300000//256<<32)|(1<<48));reg(0x51,0)
   reg(0x52,16|1<<32);reg(0x53,0)
   palette=[0x80000000 | ((i*13+field*17)&255) | ((i*7+field*31)&255)<<8 | ((255-i*11)&255)<<16 for i in range(16)]
   for i in range(0,16,2):reg(0x54,palette[i]|palette[i+1]<<32)
  if field==0:pixels(1)
  elif field in (1,2,3):pixels(field+1,(field&1)*128,((field>>1)&1)*128,128)
  elif field==6:pixels(6,0,0,128)
  elif field==7:pixels(1,0,0,128)
  reg(6,0x200000//256|4<<14|20<<20|8<<26|8<<30|1<<34|1<<35|(0x300000//256)<<37|1<<61)
  reg(0,6|1<<4|1<<8);reg(1,0x3f80000080808080)
  reg(3,8|8<<16);reg(5,0)
  reg(3,(256*16-8)|((256*16-8)<<16));reg(5,128*16|((128*16)<<16))
  stream.extend(struct.pack('<BII',11,field,field&1));count+=1
 stream.append(13);count+=1
 sizes=[11208,2448,64,4194304,16384,16384,16384,4096,4096]
 header=bytearray(176);header[:8]=b'GSCAP2\0\0';struct.pack_into('<11I',header,8,2,0,*sizes)
 struct.pack_into('<QQQQ',header,96,176+sum(sizes),len(stream),0,count);struct.pack_into('<I',header,128,fields)
 path.write_bytes(header+bytes(sum(sizes))+stream)
def main():
 root.mkdir(exist_ok=True)
 env=os.environ.copy()
 for k in list(env):
  if k.startswith('PS2_'):del env[k]
 env['PATH']=str(r.parent/'toolchain/msys64/ucrt64/bin')+';'+env['PATH']
 env.update(PS2_SHADER_DIR=str(r/'build/native-poc/shaders'),PS2_UPSCALE='1',PS2_PRESENT_MODE='immediate')
 results=[]
 for fields in range(1,9):
  cap=root/f'pages-{fields}.gscap';fixture(cap,fields);hashes=[]
  for mode in ('before','after'):
   if mode=='after':env.update(PS2_T4_PARTIAL='1',PS2_T4_SOURCE_HASH='1')
   else:env.pop('PS2_T4_PARTIAL',None);env.pop('PS2_T4_SOURCE_HASH',None)
   shot=root/f'pages-{fields}-{mode}.ppm'
   with (root/f'pages-{fields}-{mode}.log').open('wb') as log:
    subprocess.run([str(r/'build/native-poc/gsreplay.exe'),str(cap),'--loop','1','--shot',str(shot)],env=env,cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=30)
   hashes.append(hashlib.sha256(shot.read_bytes()).hexdigest())
  assert hashes[0]==hashes[1],(fields,hashes)
  results.append({'fields':fields,'identical':True,'sha256':hashes[0]})
  print('T4 changed-page/palette/state prefix',fields,'PASS',flush=True)
 # Fixture must exercise visible texture changes, not compare blank images.
 assert len({x['sha256'] for x in results})>=6,results
 (root/'results.json').write_text(json.dumps(results,indent=2))
if __name__=='__main__':main()
