#!/usr/bin/env python3
"""校验 vda.img 的 GPT 主/备一致性，并回读 cust 分区内容与源镜像逐字节对比。"""
import hashlib
import os
import struct
import zlib

VDA = "/Volumes/work/cloud-phone-hub/engine/out/dist/xiaomi/gold/OS3.0.10.0.VNQCNXM/vda.img"
CUST = "/Volumes/work/cloud-phone-hub/engine/out/roms/xiaomi/gold/OS3.0.10.0.VNQCNXM/cust.img"

f = open(VDA, "r+b")
size = os.path.getsize(VDA)
sectors = size // 512
print(f"盘大小 {size} B = {sectors} 扇区 = {size/2**30:.2f} GiB")


def read_at(off, n):
    f.seek(off)
    return f.read(n)


def check_header(raw, label):
    crc_stored, = struct.unpack_from("<I", raw, 16)
    patched = bytearray(raw)
    patched[16:20] = b"\x00\x00\x00\x00"
    crc = zlib.crc32(bytes(patched)) & 0xFFFFFFFF
    alt, = struct.unpack_from("<Q", raw, 32)
    last, = struct.unpack_from("<Q", raw, 48)
    entries_lba, = struct.unpack_from("<Q", raw, 72)
    count, esize, ecrc = struct.unpack_from("<III", raw, 80)
    print(f"{label}: 头CRC {'ok' if crc == crc_stored else 'BAD'} "
          f"alternate={alt} lastUsable={last} entriesLBA={entries_lba} "
          f"count={count} esize={esize}")
    return {"ok": crc == crc_stored, "alt": alt, "last": last,
            "entries_lba": entries_lba, "count": count, "esize": esize, "ecrc": ecrc}


primary = check_header(read_at(512, 92), "主头")
backup = check_header(read_at((sectors - 1) * 512, 92), "备头")
assert primary["alt"] == sectors - 1, "主头 alternate 应指向末扇区"
assert backup["alt"] == 1, "备头 alternate 应指向 LBA1"
assert primary["last"] == backup["last"] == sectors - 34, "lastUsable 应为 末扇区-34"

ptable = read_at(primary["entries_lba"] * 512, primary["count"] * primary["esize"])
btable = read_at(backup["entries_lba"] * 512, backup["count"] * backup["esize"])
print(f"主表CRC {'ok' if zlib.crc32(ptable) & 0xFFFFFFFF == primary['ecrc'] else 'BAD'}"
      f"；备份表与主表 {'字节一致' if ptable == btable else '不一致'}")

parts = []
for i in range(primary["count"]):
    ent = ptable[i * primary["esize"]:(i + 1) * primary["esize"]]
    if ent[:16] == b"\x00" * 16:
        continue
    first, last = struct.unpack_from("<QQ", ent, 32)
    flags, = struct.unpack_from("<Q", ent, 48)
    name = ent[56:128].decode("utf-16-le").rstrip("\x00")
    parts.append((i, name, first, last, flags))
print(f"分区 {len(parts)} 个:")
for slot, name, first, last, flags in parts:
    print(f"  [{slot:2d}] {name:<12} LBA {first}-{last} ({(last-first+1)*512/2**20:.2f} MiB) flags={flags:#x}")

cust = [p for p in parts if p[1] == "cust"]
assert cust, "GPT 里没有 cust 分区"
slot, name, first, last = cust[0][:4]
print(f"cust: LBA {first}-{last}，容量 {(last-first+1)*512} B，起始偏移 {first*512}")

src_size = os.path.getsize(CUST)
print(f"源 cust.img {src_size} B（sparse 已转 raw）")
part_size = (last - first + 1) * 512
assert part_size >= src_size, "cust 分区容量小于源镜像"
f.seek(first * 512)
sha_part = hashlib.sha256()
remain = src_size
while remain:
    chunk = f.read(min(16 << 20, remain))
    sha_part.update(chunk)
    remain -= len(chunk)
sha_src = hashlib.sha256()
with open(CUST, "rb") as src:
    for chunk in iter(lambda: src.read(16 << 20), b""):
        sha_src.update(chunk)
print(f"分区内容 sha256 {sha_part.hexdigest()}")
print(f"源镜像   sha256 {sha_src.hexdigest()}")
print("cust 内容逐字节一致" if sha_part.hexdigest() == sha_src.hexdigest() else "cust 内容不一致")
