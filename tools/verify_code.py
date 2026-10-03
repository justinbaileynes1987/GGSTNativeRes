# Prints the evidence for the code-level claims in VERIFICATION.md, from a memory dump of the game
# (made with dump.py while the game runs - preferably with GGSTNativeRes disabled or removed, so
# the dump shows the original bytes).
#
# Functions are located by the strings they reference or by byte patterns, never by hardcoded
# addresses, so this works on any build where the claims still hold.
#
# Usage:  python verify_code.py GGST-dump.exe
# Requires: pip install pefile capstone
import bisect, re, struct, sys
import capstone, pefile

dump = sys.argv[1] if len(sys.argv) > 1 else "GGST-dump.exe"
pe = pefile.PE(dump, fast_load=True)
pe.parse_data_directories([pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXCEPTION"]])
img = open(dump, "rb").read()
text = next(s for s in pe.sections if s.Name.startswith(b".text"))
T0, T1 = text.VirtualAddress, text.VirtualAddress + text.Misc_VirtualSize
funcs = sorted((e.struct.BeginAddress, e.struct.EndAddress) for e in pe.DIRECTORY_ENTRY_EXCEPTION)
starts = [f[0] for f in funcs]
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
md.skipdata = True


def wstr_at(rva, n=60):
    s = img[rva:rva + n * 2].decode("utf-16le", "ignore").split("\0")[0]
    return s if len(s) >= 2 and s.isascii() and s.isprintable() else None


def astr_at(rva, n=60):
    s = img[rva:rva + n].split(b"\0")[0]
    return s.decode() if len(s) >= 3 and all(32 <= c < 127 for c in s) else None


def rip_target(ins):
    m = re.search(r"\[rip \+ (0x[0-9a-f]+)\]", ins.op_str)
    return ins.address + ins.size + int(m.group(1), 16) if m else None


def func_of(rva):
    i = bisect.bisect_right(starts, rva) - 1
    return funcs[i]


def disasm(start, end, mark=()):
    for ins in md.disasm(img[start:end], start):
        note = ""
        t = rip_target(ins)
        if t is not None:
            s = wstr_at(t) or astr_at(t)
            if s: note = f'   ; "{s}"'
        flag = ">>" if any(ins.address <= m < ins.address + ins.size for m in mark) else "  "
        print(f"{flag} {ins.address:#09x}: {ins.bytes.hex():<22} {ins.mnemonic} {ins.op_str}{note}")


def find_wstr(s):
    return [m.start() for m in re.finditer(re.escape(s.encode("utf-16le") + b"\0\0"), img)]


def xrefs(target):
    """RIP-relative instructions in .text that point at target (lea/mov reg, [rip+disp32])."""
    hits = []
    for m in re.finditer(rb"[\x48\x4c][\x8d\x8b][\x05\x0d\x15\x1d\x25\x2d\x35\x3d]", img[T0:T1]):
        i = T0 + m.start()
        if i + 7 + struct.unpack_from("<i", img, i + 3)[0] == target:
            hits.append(i)
    return hits


def scan(pattern):
    rx = b"".join(b"." if t == "??" else re.escape(bytes([int(t, 16)])) for t in pattern.split())
    return [m.start() for m in re.finditer(rx, img[T0:T1], re.S)]


def section(title):
    print("\n" + "=" * 100 + f"\n{title}\n" + "=" * 100)


# --------------------------------------------------------------------------------------------
section("0. Engine version string")
for m in re.finditer(rb"\+\+UE4\+Release-4\.\d+", img):
    print("  ", img[m.start():m.start() + 40].split(b"\0")[0].decode(errors="replace"))
    break
for m in re.finditer(re.escape("++UE4+Release-4.".encode("utf-16le")), img):
    print("  ", img[m.start():m.start() + 80].decode("utf-16le", "ignore").split("\0")[0])
    break

# --------------------------------------------------------------------------------------------
section("1. Game boot default (claim: picks the 1920x1080 entry and window mode 0 when the "
        "resolution-list fingerprint doesn't match)")
hits = [T0 + h for h in scan("41 81 3C 8E 80 07 00 00 75 ?? 41 81 7C 8E 04 38 04 00 00 74")]
print(f"pattern matches: {[hex(h) for h in hits]}  (0x780 = 1920, 0x438 = 1080)")
if hits:
    fs, fe = func_of(hits[0])
    disasm(fs, min(fe, fs + 0x1A0), mark=hits)

# --------------------------------------------------------------------------------------------
for key, title in [
    ("ResolutionSizeX", "2. Engine boot preload (claim: UGameUserSettings::PreloadResolutionSettings reads "
                        "Version/FullscreenMode/ResolutionSizeX/Y from the ini, then calls the override chain)"),
    ("ForceRes", "3. UGameEngine::DetermineGameWindowResolution (claim: in Fullscreen, caps the resolution at "
                 "the primary monitor's NativeWidth/Height; -ForceRes skips the cap)"),
    ("EDID", "4. EDID reader (claim: reads the 'EDID' registry value and decodes only the first detailed "
             "timing descriptor: width = (EDID[58]>>4)<<8 | EDID[56], height = (EDID[61]>>4)<<8 | EDID[59])"),
]:
    section(title)
    for s in find_wstr(key):
        for x in xrefs(s):
            if not (T0 <= x < T1): continue
            fs, fe = func_of(x)
            print(f'string L"{key}" at {s:#x}, referenced from {x:#x}, in function {fs:#x}-{fe:#x}')
            disasm(fs, fe, mark=[x])
            print()
