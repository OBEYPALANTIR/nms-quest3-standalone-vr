#!/usr/bin/env python3
"""Write half-resolution copies of DDS files by dropping the top mip and fixing the header.

No re-encode: mip 1 of every layer becomes mip 0. Handles legacy and DX10 headers,
cubemaps and arrays. Skips (and reports) single-mip, volume, unknown-format and
size-mismatch files. NMS's custom header dword at file offset 0x74 (dwCaps3) is copied
unchanged.

usage: halve_dds.py SRC_DIR OUT_DIR [--limit N]   (writes OUT_DIR/report.tsv too)
       halve_dds.py --selftest
"""
import os, struct, sys

DDSD_PITCH, DDSD_LINEARSIZE, DDSD_MIPMAPCOUNT = 0x8, 0x80000, 0x20000
CAPS2_CUBEMAP, CAPS2_VOLUME = 0x200, 0x200000
FOURCC_BLOCK = {b'DXT1': 8, b'ATI1': 8, b'BC4U': 8, b'BC4S': 8,
                b'DXT2': 16, b'DXT3': 16, b'DXT4': 16, b'DXT5': 16,
                b'ATI2': 16, b'BC5U': 16, b'BC5S': 16}
DXGI_BLOCK = {**{f: 8 for f in (70, 71, 72, 79, 80, 81)},
              **{f: 16 for f in (73, 74, 75, 76, 77, 78, 82, 83, 84, 94, 95, 96, 97, 98, 99)}}
DXGI_BPP = {2: 16, 10: 8, 11: 8, 24: 4, 26: 4, 27: 4, 28: 4, 29: 4, 41: 4, 49: 2,
            56: 2, 61: 1, 85: 2, 87: 4, 88: 4, 91: 4}


def mip_size(w, h, block, bpp):
    if block:
        return max(1, (w + 3) // 4) * max(1, (h + 3) // 4) * block
    return w * h * bpp


def parse(data):
    """Return dict describing the layout, or raise ValueError(reason)."""
    if len(data) < 128 or data[:4] != b'DDS ':
        raise ValueError('not DDS')
    flags, h, w, pitch, depth, mips = struct.unpack_from('<6I', data, 8)
    pf_flags, fourcc, rgbbits = struct.unpack_from('<I4sI', data, 80)
    caps2 = struct.unpack_from('<I', data, 0x70)[0]
    mips = max(1, mips)
    hdr, layers, block, bpp = 128, 1, 0, 0
    if fourcc == b'DX10':
        if len(data) < 148:
            raise ValueError('truncated DX10 header')
        dxgi, dim, misc, arr = struct.unpack_from('<4I', data, 128)
        hdr = 148
        if dim == 4:
            raise ValueError('volume texture')
        layers = max(1, arr) * (6 if misc & 0x4 else 1)
        block, bpp = DXGI_BLOCK.get(dxgi, 0), DXGI_BPP.get(dxgi, 0)
        if not (block or bpp):
            raise ValueError('unknown DXGI format %d' % dxgi)
    else:
        if caps2 & CAPS2_VOLUME or depth > 1:
            raise ValueError('volume texture')
        if caps2 & CAPS2_CUBEMAP:
            layers = bin(caps2 & 0xFC00).count('1') or 6  # shortcut: assumes partial cubes list faces in order
        if pf_flags & 0x4:
            block = FOURCC_BLOCK.get(fourcc, 0)
            if not block:
                raise ValueError('unknown FourCC %r' % fourcc)
        elif rgbbits in (8, 16, 32, 64):
            bpp = rgbbits // 8
        else:
            raise ValueError('unknown pixel format')
    sizes = []
    for m in range(mips):
        sizes.append(mip_size(max(1, w >> m), max(1, h >> m), block, bpp))
    payload = layers * sum(sizes)
    if len(data) - hdr != payload:
        raise ValueError('size mismatch: payload %d expected %d' % (len(data) - hdr, payload))
    return dict(flags=flags, w=w, h=h, mips=mips, hdr=hdr, layers=layers,
                block=block, bpp=bpp, sizes=sizes)


def halve(data):
    """Return (new_bytes, info). Raise ValueError(reason) to skip."""
    info = parse(data)
    if info['mips'] < 2:
        raise ValueError('single mip')
    hdr, sizes, chain = info['hdr'], info['sizes'], sum(info['sizes'])
    body = bytearray()
    for layer in range(info['layers']):
        start = hdr + layer * chain + sizes[0]
        body += data[start:hdr + (layer + 1) * chain]
    out = bytearray(data[:hdr])
    nw, nh = max(1, info['w'] >> 1), max(1, info['h'] >> 1)
    flags = info['flags'] | DDSD_MIPMAPCOUNT
    pitch = sizes[1] if info['block'] else nw * info['bpp']
    if not flags & (DDSD_PITCH | DDSD_LINEARSIZE):
        pitch = 0  # header never declared it; keep it undeclared
    struct.pack_into('<I', out, 8, flags)
    struct.pack_into('<IIII', out, 12, nh, nw, pitch, struct.unpack_from('<I', data, 24)[0])
    struct.pack_into('<I', out, 28, info['mips'] - 1)
    return bytes(out + body), info


def run(src, dst, limit):
    os.makedirs(dst, exist_ok=True)
    rows, before, after, skipped = [], 0, 0, {}
    files = sorted(os.path.join(r, n) for r, _, fs in os.walk(src) for n in fs if n.lower().endswith('.dds'))[:limit]
    for p in files:
        data = open(p, 'rb').read()
        rel = os.path.relpath(p, src)
        try:
            new, info = halve(data)
        except ValueError as e:
            reason = str(e).split(':')[0]
            skipped[reason] = skipped.get(reason, 0) + 1
            rows.append((rel, len(data), len(data), 'SKIP ' + str(e)))
            before += len(data); after += len(data)
            continue
        o = os.path.join(dst, rel)
        os.makedirs(os.path.dirname(o), exist_ok=True)
        open(o, 'wb').write(new)
        before += len(data); after += len(new)
        rows.append((rel, len(data), len(new), '%dx%d m%d -> %dx%d m%d L%d' % (
            info['w'], info['h'], info['mips'], max(1, info['w'] >> 1), max(1, info['h'] >> 1),
            info['mips'] - 1, info['layers'])))
    with open(os.path.join(dst, 'report.tsv'), 'w') as f:
        f.write('file\tbytes_before\tbytes_after\tnote\n')
        for r in rows:
            f.write('%s\t%d\t%d\t%s\n' % r)
    done = len(files) - sum(skipped.values())
    print('files=%d halved=%d skipped=%d %s' % (len(files), done, sum(skipped.values()), skipped))
    print('bytes_before=%d bytes_after=%d ratio=%.4f (skipped files counted unchanged)' % (
        before, after, after / before if before else 0))


def selftest():
    def make(w, h, mips, layers, dx10=None, fourcc=b'DXT1', caps2=0, linear=True, nms74=1):
        hd = bytearray(128)
        hd[:4] = b'DDS '
        struct.pack_into('<7I', hd, 4, 124, 0x1007 | DDSD_MIPMAPCOUNT | (DDSD_LINEARSIZE if linear else 0), h, w, 0, 0, mips)
        struct.pack_into('<II4s', hd, 76, 32, 0x4, b'DX10' if dx10 is not None else fourcc)
        struct.pack_into('<III', hd, 0x6C, 0x401008, caps2, nms74)
        if dx10 is not None:
            hd += struct.pack('<5I', dx10[0], 3, dx10[1], dx10[2], 0)
        blk = DXGI_BLOCK[dx10[0]] if dx10 is not None else FOURCC_BLOCK[fourcc]
        body = bytearray()
        for L in range(layers):
            for m in range(mips):
                body += bytes([L * 16 + m]) * mip_size(max(1, w >> m), max(1, h >> m), blk, 0)
        return bytes(hd + body)

    # DX10 BC7 cubemap array (2 cubes = 12 layers), 16x8 with 5 mips
    src = make(16, 8, 5, 12, dx10=(98, 0x4, 2))
    out, info = halve(src)
    assert info['layers'] == 12 and info['mips'] == 5
    assert struct.unpack_from('<III', out, 12) == (4, 8, 32), struct.unpack_from('<III', out, 12)  # h, w, linear size of 8x4 BC7
    assert struct.unpack_from('<I', out, 28)[0] == 4
    assert out[128:148] == src[128:148]  # DX10 ext untouched
    assert struct.unpack_from('<I', out, 0x74)[0] == 1  # NMS dword preserved
    want = b''.join(bytes([L * 16 + m]) * mip_size(max(1, 16 >> m), max(1, 8 >> m), 16, 0)
                    for L in range(12) for m in range(1, 5))
    assert out[148:] == want
    assert parse(out)['mips'] == 4  # result re-parses with consistent size
    # legacy DXT1 non-multiple-of-4: 6x6 (2x2 blocks) -> 3x3 (1 block) -> 1x1 (1 block)
    src = make(6, 6, 3, 1)
    out, _ = halve(src)
    assert struct.unpack_from('<III', out, 12) == (3, 3, 8) and len(out) == 128 + 8 + 8
    # legacy cubemap with all 6 faces
    out, info = halve(make(8, 8, 4, 6, caps2=CAPS2_CUBEMAP | 0xFC00, fourcc=b'ATI2'))
    assert info['layers'] == 6 and len(out) == 128 + 6 * (16 + 16 + 16)
    # skips
    for bad, why in ((make(8, 8, 1, 1), 'single mip'),
                     (make(8, 8, 2, 1)[:-1], 'size mismatch'),
                     (make(8, 8, 2, 1, caps2=CAPS2_VOLUME), 'volume texture')):
        try:
            halve(bad)
            raise AssertionError('expected skip: ' + why)
        except ValueError as e:
            assert str(e).startswith(why), (str(e), why)
    print('selftest OK')


if __name__ == '__main__':
    a = sys.argv[1:]
    if a == ['--selftest']:
        selftest()
    elif len(a) in (2, 4):
        run(a[0], a[1], int(a[3]) if len(a) == 4 and a[2] == '--limit' else 10 ** 9)
    else:
        sys.exit(__doc__)
