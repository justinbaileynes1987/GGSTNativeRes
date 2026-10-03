import pefile, capstone, sys
pe = pefile.PE("GGST-dump.exe", fast_load=True)
base = pe.OPTIONAL_HEADER.ImageBase; img = open("GGST-dump.exe","rb").read()
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64); md.skipdata = True
s, e = int(sys.argv[1],16), int(sys.argv[2],16)
for x in md.disasm(img[s:e], base+s):
    print(f"{x.address-base:#09x}: {x.bytes.hex():<24} {x.mnemonic} {x.op_str}")
