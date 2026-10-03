import pefile, struct, re, capstone
pe = pefile.PE("GGST-dump.exe", fast_load=True)
base = pe.OPTIONAL_HEADER.ImageBase
img = open("GGST-dump.exe", "rb").read()
text = next(s for s in pe.sections if s.Name.startswith(b".text"))
t0, t1 = text.VirtualAddress, text.VirtualAddress + text.Misc_VirtualSize
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64); md.detail = False

def dis(rva, n=40, before=0x40):
    start = rva - before
    out = []
    for ins in md.disasm(img[start:start + before + n * 6], base + start):
        out.append(f"  {ins.address - base:#09x}: {ins.mnemonic} {ins.op_str}")
    return "\n".join(out)

# 1) packed FIntPoint(1920,1080) as 64-bit immediate (mov r64, imm64 = 48/49 B8+r imm64)
q = struct.pack("<II", 1920, 1080)
hits = [m.start() for m in re.finditer(re.escape(q), img[t0:t1])]
print(f"qword 1920|1080 in .text: {len(hits)}")
for h in hits[:20]:
    print(dis(t0 + h - 2, 12, 0x30)); print("  ----")

# 2) imm32 1920 followed within 24 bytes by imm32 1080 in .text
p = re.compile(re.escape(struct.pack("<I", 1920)) + b".{0,24}?" + re.escape(struct.pack("<I", 1080)), re.S)
hits2 = [m.start() for m in p.finditer(img[t0:t1])]
print(f"imm32 1920..1080 pairs in .text: {len(hits2)}")
for h in hits2[:40]:
    print(f"  at {t0+h:#x}")
