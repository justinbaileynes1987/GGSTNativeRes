param([string]$LaunchArgs = "", [int]$Seconds = 60, [string]$Out = "", [switch]$Enforce, [int]$PollMs = 200)

Add-Type @"
using System;
using System.Runtime.InteropServices;
public class W {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Ansi)]
  public struct DEVMODE {
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst=32)] public string dmDeviceName;
    public short dmSpecVersion, dmDriverVersion, dmSize, dmDriverExtra; public int dmFields;
    public int dmPositionX, dmPositionY, dmDisplayOrientation, dmDisplayFixedOutput;
    public short dmColor, dmDuplex, dmYResolution, dmTTOption, dmCollate;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst=32)] public string dmFormName;
    public short dmLogPixels; public int dmBitsPerPel, dmPelsWidth, dmPelsHeight, dmDisplayFlags, dmDisplayFrequency;
    public int a,b,c,d,e,f,g,h;
  }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr h, int i);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  [DllImport("user32.dll", CharSet=CharSet.Ansi)] public static extern bool EnumDisplaySettings(string dev, int mode, ref DEVMODE dm);
}
"@

function Mode {
  $dm = New-Object W+DEVMODE; $dm.dmSize = [Runtime.InteropServices.Marshal]::SizeOf($dm)
  [void][W]::EnumDisplaySettings("\\.\DISPLAY1", -1, [ref]$dm)
  "$($dm.dmPelsWidth)x$($dm.dmPelsHeight)@$($dm.dmDisplayFrequency)"
}

Start-Process "C:\Program Files (x86)\Steam\steam.exe" -ArgumentList (@("-applaunch","1384160") + ($LaunchArgs -split ' ' | Where-Object { $_ }))
$t0 = Get-Date; $last = ""; $log = @()
while (((Get-Date) - $t0).TotalSeconds -lt $Seconds) {
  $p = Get-Process GGST-Win64-Shipping -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($p -and $p.MainWindowHandle -ne 0) {
    $r = New-Object W+RECT; $c = New-Object W+RECT
    [void][W]::GetWindowRect($p.MainWindowHandle, [ref]$r); [void][W]::GetClientRect($p.MainWindowHandle, [ref]$c)
    $style = '{0:X8}' -f [W]::GetWindowLong($p.MainWindowHandle, -16)
    if ($Enforce -and ($r.R-$r.L) -gt 1000 -and (($r.R-$r.L) -ne 2560 -or ($r.B-$r.T) -ne 1440)) {
      [void][W]::SetWindowPos($p.MainWindowHandle, [IntPtr]::Zero, 0, 0, 2560, 1440, 0x0014)  # NOZORDER|NOACTIVATE
      $log += "        ENFORCE: resized $($r.R-$r.L)x$($r.B-$r.T) -> 2560x1440"; Write-Output $log[-1]
    }
    $s = "win=($($r.L),$($r.T) $($r.R-$r.L)x$($r.B-$r.T)) client=$($c.R)x$($c.B) style=$style display=$(Mode)"
  } else { $s = "no window; display=$(Mode)" }
  if ($s -ne $last) { $line = "{0,6:N1}s  {1}" -f ((Get-Date)-$t0).TotalSeconds, $s; $log += $line; Write-Output $line; $last = $s }
  Start-Sleep -Milliseconds $PollMs
}
if ($Out) { $log | Set-Content $Out -Encoding utf8 }
