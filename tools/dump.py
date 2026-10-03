# Read-only dump of GGST-Win64-Shipping.exe's in-memory image (after SteamStub has unpacked it).
import ctypes, ctypes.wintypes as wt, struct, subprocess, sys

PROCESS_QUERY_INFORMATION, PROCESS_VM_READ = 0x400, 0x10
k32 = ctypes.WinDLL("kernel32", use_last_error=True)
psapi = ctypes.WinDLL("psapi", use_last_error=True)

pid = int(subprocess.check_output(
    ["powershell", "-NoProfile", "-Command", "(Get-Process GGST-Win64-Shipping).Id"]).strip())
h = k32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, pid)
if not h: sys.exit(f"OpenProcess failed {ctypes.get_last_error()}")

mods = (wt.HMODULE * 1024)(); needed = wt.DWORD()
psapi.EnumProcessModulesEx(h, mods, ctypes.sizeof(mods), ctypes.byref(needed), 3)
base = mods[0]

def read(addr, size):
    buf = ctypes.create_string_buffer(size); n = ctypes.c_size_t()
    ok = k32.ReadProcessMemory(h, ctypes.c_void_p(addr), buf, size, ctypes.byref(n))
    return buf.raw[:n.value] if ok else None

hdr = read(base, 0x1000)
pe = struct.unpack_from("<I", hdr, 0x3C)[0]
size_of_image = struct.unpack_from("<I", hdr, pe + 0x18 + 0x38)[0]
print(f"pid={pid} base={base:#x} SizeOfImage={size_of_image:#x}")

img = bytearray(size_of_image)
PAGE = 0x1000
bad = 0
for off in range(0, size_of_image, PAGE):
    d = read(base + off, PAGE)
    if d: img[off:off + len(d)] = d
    else: bad += 1
print(f"unreadable pages: {bad}")

# Rewrite section headers so raw offsets == virtual offsets (loadable by pefile/IDA/Ghidra as-is)
nsec = struct.unpack_from("<H", img, pe + 6)[0]
opt_size = struct.unpack_from("<H", img, pe + 20)[0]
sec = pe + 24 + opt_size
for i in range(nsec):
    o = sec + i * 40
    name = img[o:o + 8].rstrip(b"\0").decode(errors="replace")
    vsize, va = struct.unpack_from("<II", img, o + 8)
    struct.pack_into("<II", img, o + 16, vsize, va)  # SizeOfRawData, PointerToRawData
    print(f"  {name:8} va={va:#010x} size={vsize:#x}")
struct.pack_into("<Q", img, pe + 24 + 24, base)  # ImageBase = actual base (no relocation needed)

out = sys.argv[1] if len(sys.argv) > 1 else "GGST-dump.exe"
open(out, "wb").write(img)
print("wrote", out)
