import struct,sys
d=open(sys.argv[1],'rb').read()
w=struct.unpack_from('<16I',d,0)
print('magic',d[:8],'size',len(d),'flags',hex(w[12]),'eventsSelfManaged(0x40)=',bool(w[12]&0x40))
tg=[i for i in range(0,len(d)-3) if struct.unpack_from('<I',d,i)[0]==0x33500]
print('targets word 0x33500 at offsets',[hex(i) for i in tg])
cnt,off=struct.unpack_from('<2I',d,0x80)
ids=[hex(struct.unpack_from('<I',d,off+4*i)[0]) for i in range(cnt)]
print('header targets: count',cnt,'offset',hex(off),'aligned' if off%4==0 else 'UNALIGNED','ids',ids,'memregion field',(w[12]>>2)&3,'(1 = 2 MiB)')
