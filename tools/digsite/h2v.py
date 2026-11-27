import struct
class H2V:
    def __init__(s,path,data=None):
        s.d=bytearray(open(path,'rb').read()) if data is None else data; d=s.d
        s.io,s.isz=struct.unpack_from('<II',d,0x10)
        s.ntags=struct.unpack_from('<I',d,0x2cc)[0]
        no,nsz,nio=struct.unpack_from('<III',d,0x2d0)
        h=struct.unpack_from('<8I',d,s.io)
        s.hdr=h
        # tag groups at index start + 0x20, 12 bytes each; then tags
        gcount=h[1]
        tagtab=s.io+0x20+gcount*12
        s.tags=[]
        for i in range(s.ntags):
            g,datum,addr,size=struct.unpack_from('<4sIII',d,tagtab+16*i)
            no_=struct.unpack_from('<i',d,nio+4*i)[0]
            name=d[no+no_:d.find(b'\0',no+no_)].decode('latin-1') if no_>=0 else ''
            s.tags.append((g[::-1].decode('latin-1'),datum,addr,size,name))
        s.base=[t for t in s.tags if t[2]][0][2]
        s.meta=s.io+s.isz
    def off(s,addr): return addr-s.base+s.meta
    def tag(s,name,group=None):
        for t in s.tags:
            if t[4]==name and (group is None or t[0]==group): return t
