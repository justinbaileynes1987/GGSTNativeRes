# Shows, for every active monitor, what Unreal Engine 4 will think its "native" resolution is,
# next to what the monitor actually supports. Read-only; needs no game and no admin rights.
#
# UE4 (WindowsApplication.cpp, GetMonitorInfo -> GetSizeForDevID -> GetMonitorSizeFromEDID) takes the
# monitor's model ID from its device ID, finds the matching monitor device via SetupAPI, reads the
# "EDID" registry value and decodes ONLY the first detailed timing descriptor (bytes 54..71):
#   width  = ((EDID[58] >> 4) << 8) | EDID[56]
#   height = ((EDID[61] >> 4) << 8) | EDID[59]
#
# Usage: python check_edid.py
import ctypes, ctypes.wintypes as wt, winreg

class DISPLAY_DEVICEW(ctypes.Structure):
    _fields_ = [("cb", wt.DWORD), ("DeviceName", wt.WCHAR * 32), ("DeviceString", wt.WCHAR * 128),
                ("StateFlags", wt.DWORD), ("DeviceID", wt.WCHAR * 128), ("DeviceKey", wt.WCHAR * 128)]

class DEVMODEW(ctypes.Structure):
    _fields_ = [("dmDeviceName", wt.WCHAR * 32), ("dmSpecVersion", wt.WORD), ("dmDriverVersion", wt.WORD),
                ("dmSize", wt.WORD), ("dmDriverExtra", wt.WORD), ("dmFields", wt.DWORD),
                ("dmPositionX", wt.LONG), ("dmPositionY", wt.LONG), ("dmDisplayOrientation", wt.DWORD),
                ("dmDisplayFixedOutput", wt.DWORD), ("dmColor", ctypes.c_short), ("dmDuplex", ctypes.c_short),
                ("dmYResolution", ctypes.c_short), ("dmTTOption", ctypes.c_short), ("dmCollate", ctypes.c_short),
                ("dmFormName", wt.WCHAR * 32), ("dmLogPixels", wt.WORD), ("dmBitsPerPel", wt.DWORD),
                ("dmPelsWidth", wt.DWORD), ("dmPelsHeight", wt.DWORD), ("dmDisplayFlags", wt.DWORD),
                ("dmDisplayFrequency", wt.DWORD), ("rest", wt.DWORD * 8)]

user32 = ctypes.WinDLL("user32")
ATTACHED, PRIMARY, ACTIVE = 0x1, 0x4, 0x1

def dtd(b):
    """Decode an 18-byte detailed timing descriptor -> (w, h, hz) or None if not a timing."""
    clock = (b[0] | b[1] << 8) * 10_000
    if clock == 0:
        return None
    w = b[2] | (b[4] >> 4) << 8; hb = b[3] | (b[4] & 0xF) << 8
    h = b[5] | (b[7] >> 4) << 8; vb = b[6] | (b[7] & 0xF) << 8
    hz = clock / ((w + hb) * (h + vb)) if (w + hb) and (h + vb) else 0
    return w, h, round(hz, 2)

def all_timings(e):
    out = [("base block DTD %d" % (i + 1), dtd(e[54 + 18 * i:72 + 18 * i])) for i in range(4)]
    for x in range(1, 1 + e[126]):
        blk = e[128 * x:128 * (x + 1)]
        if len(blk) < 128:
            break
        if blk[0] == 0x02:  # CTA-861 extension: DTDs start at byte d
            d = blk[2]
            if d < 4:  # no DTDs in this block
                continue
            for i, off in enumerate(range(d, 127 - 18, 18)):
                t = dtd(blk[off:off + 18])
                if not t:  # a zero pixel clock ends the DTD list (the rest is padding)
                    break
                out.append((f"ext {x} (CTA-861) DTD {i + 1}", t))
        elif blk[0] == 0x70:  # DisplayID extension: look for Type I (0x03) / Type VII (0x22) timings
            i = 5
            while i < 5 + blk[2]:
                tag, ln = blk[i], blk[i + 2]
                body = blk[i + 3:i + 3 + ln]
                if tag in (0x03, 0x22):
                    step = 20
                    for k in range(0, ln - step + 1, step):
                        t = body[k:k + step]
                        clk = (t[0] | t[1] << 8 | t[2] << 16) + 1  # in 10 kHz (Type I) / 1 kHz (Type VII)
                        w = (t[4] | t[5] << 8) + 1; hb = (t[6] | t[7] << 8) + 1
                        h = (t[12] | t[13] << 8) + 1; vb = (t[14] | t[15] << 8) + 1
                        scale = 10_000 if tag == 0x03 else 1_000
                        out.append((f"ext {x} (DisplayID type {'I' if tag == 0x03 else 'VII'})",
                                    (w, h, round(clk * scale / ((w + hb) * (h + vb)), 2))))
                if ln == 0 and tag == 0:
                    break
                i += 3 + ln
    return out

def registry_edids(model):
    """All EDIDs Windows has cached for this monitor model (one per device instance)."""
    res = []
    try:
        k = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, rf"SYSTEM\CurrentControlSet\Enum\DISPLAY\{model}")
    except OSError:
        return res
    j = 0
    while True:
        try:
            inst = winreg.EnumKey(k, j); j += 1
        except OSError:
            break
        try:
            e = winreg.QueryValueEx(winreg.OpenKey(k, inst + r"\Device Parameters"), "EDID")[0]
            res.append((inst, bytes(e)))
        except OSError:
            pass
    return res

def name_of(e):
    for d in range(54, 109, 18):
        if e[d] == 0 and e[d + 1] == 0 and e[d + 3] == 0xFC:
            return e[d + 5:d + 18].decode("ascii", "ignore").strip()
    return "?"

i = 0
while True:
    a = DISPLAY_DEVICEW(); a.cb = ctypes.sizeof(a)
    if not user32.EnumDisplayDevicesW(None, i, ctypes.byref(a), 0):
        break
    i += 1
    if not a.StateFlags & ATTACHED:
        continue
    dm = DEVMODEW(); dm.dmSize = ctypes.sizeof(dm)
    user32.EnumDisplaySettingsW(a.DeviceName, -1, ctypes.byref(dm))
    m = DISPLAY_DEVICEW(); m.cb = ctypes.sizeof(m)
    mi = 0
    while user32.EnumDisplayDevicesW(a.DeviceName, mi, ctypes.byref(m), 0):
        mi += 1
        if not m.StateFlags & ACTIVE:
            continue
        # UE4: Info.Name = DeviceID.Mid(8, DeviceID.Find("\\", from 9) - 8), e.g. "MONITOR\AUS27F2\{...}" -> "AUS27F2"
        model = m.DeviceID[8:m.DeviceID.find("\\", 9)]
        print(f"{a.DeviceName}  primary={bool(a.StateFlags & PRIMARY)}  desktop mode={dm.dmPelsWidth}x{dm.dmPelsHeight}@{dm.dmDisplayFrequency}")
        print(f"    monitor device ID: {m.DeviceID}   -> UE4 model key: {model}")
        for inst, e in registry_edids(model):
            first = dtd(e[54:72])
            print(f"    registry EDID [{inst}] name={name_of(e)}  extension blocks={e[126]}")
            print(f"      UE4 'native' (1st DTD): {first[0]}x{first[1]} @ {first[2]} Hz" if first else "      no DTD")
            for label, t in all_timings(e):
                if t: print(f"        {label:32} {t[0]}x{t[1]} @ {t[2]} Hz")
        print()
