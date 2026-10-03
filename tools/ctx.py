import pefile, capstone, sys, bisect
pe = pefile.PE("GGST-dump.exe", fast_load=True); pe.parse_data_directories([3])
base = pe.OPTIONAL_HEADER.ImageBase; img = open("GGST-dump.exe","rb").read()
funcs = sorted((e.struct.BeginAddress, e.struct.EndAddress) for e in pe.DIRECTORY_ENTRY_EXCEPTION)
starts = [f[0] for f in funcs]
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64); md.skipdata = True
def func_of(rva):
    i = bisect.bisect_right(starts, rva) - 1
    return funcs[i]
def show(rva, before=24, after=24):
    fs, fe = func_of(rva)
    ins = list(md.disasm(img[fs:fe], base+fs))
    idx = next((i for i,x in enumerate(ins) if x.address-base >= rva-8), len(ins)-1)
    print(f"== hit {rva:#x} in func {fs:#x}-{fe:#x} (size {fe-fs})")
    for x in ins[max(0,idx-before):idx+after]:
        mark = ">>" if abs(x.address-base - rva) < 8 else "  "
        print(f"{mark}{x.address-base:#09x}: {x.mnemonic} {x.op_str}")
for a in sys.argv[1:]:
    show(int(a,16)); print()
