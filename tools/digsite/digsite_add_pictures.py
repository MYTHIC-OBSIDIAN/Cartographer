# LAN edition mainmenu.map: adds extra pictures (multiplayer49 bitmaps 6 and up, see digsite_apply_patches_on_map_load in
# map_repository.cpp) to a built map, rewriting its table in the compact 'DGX2' format (18 pictures) when needed.
#   python3 digsite_add_pictures.py in.map out.map picture.png [picture.png ...]
# Each picture is 440x414 (resized to it), stored zlib-compressed A8R8G8B8 like the map's other previews.
import struct, sys, zlib
from PIL import Image
sys.path.insert(0, __file__.rsplit('/', 1)[0] if '/' in __file__ else '.')
from h2v import H2V

TABLE_OFFSET, TABLE_SIZE = 0x50, 0x74
MAX_V2 = (TABLE_SIZE - 8) // 6

src, out, pictures = sys.argv[1], sys.argv[2], sys.argv[3:]
m = H2V(src); d = m.d
tag = m.tag('ui\\code_global_bitmaps\\multiplayer49', 'bitm')
table = m.off(tag[2]) + TABLE_OFFSET
magic, count = d[table:table + 4], struct.unpack_from('<I', d, table + 4)[0]
if magic == b'DGXP':
    entries = [struct.unpack_from('<II', d, table + 8 + 8 * i) for i in range(count)]
elif magic == b'DGX2':
    entries = []
    for i in range(count):
        e = table + 8 + 6 * i
        entries.append((int.from_bytes(d[e:e + 3], 'little') << 9, int.from_bytes(d[e + 3:e + 6], 'little')))
else:
    sys.exit('no extra pictures table')

for path in pictures:
    im = Image.open(path).convert('RGBA')
    if im.size != (440, 414):
        im = im.resize((440, 414), Image.LANCZOS)
    raw = zlib.compress(im.tobytes('raw', 'BGRA'), 9)
    offset = (len(d) + 0x1FF) & ~0x1FF
    d.extend(b'\0' * (offset - len(d)))
    d.extend(raw)
    entries.append((offset, len(raw)))
    print(f'{path} -> extra picture {len(entries) - 1} (multiplayer49 bitmap {6 + len(entries) - 1}) raw @ {offset:#x} ({len(raw)} bytes)')
d.extend(b'\0' * (((len(d) + 0xFFF) & ~0xFFF) - len(d)))
struct.pack_into('<I', d, 8, len(d))

assert len(entries) <= MAX_V2, f'{len(entries)} pictures, room for {MAX_V2}'
d[table:table + TABLE_SIZE] = b'\0' * TABLE_SIZE
d[table:table + 4] = b'DGX2'
struct.pack_into('<I', d, table + 4, len(entries))
for i, (offset, size) in enumerate(entries):
    assert offset % 0x200 == 0 and offset >> 9 < 1 << 24 and size < 1 << 24
    e = table + 8 + 6 * i
    d[e:e + 3] = (offset >> 9).to_bytes(3, 'little')
    d[e + 3:e + 6] = size.to_bytes(3, 'little')
open(out, 'wb').write(d)
print(f'{len(entries)} extra pictures (room for {MAX_V2}), map size {len(d):#x}')
