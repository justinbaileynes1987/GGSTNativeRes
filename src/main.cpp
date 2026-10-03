// GGSTNativeRes - stops Guilty Gear -Strive- from forcing exclusive-fullscreen 1920x1080 at boot.
//
// At startup (before SYSTEM.sav is loaded) the game's screen-settings apply routine sees that the
// stored "resolution list fingerprint" doesn't match the monitor and resets the settings to a
// hardcoded default: the 1920x1080 entry of the resolution list, window mode 0 (Fullscreen).
// A few seconds later the save loads and the real settings are applied, causing a second switch.
//
// This DLL changes only the three constants that define that default (target width, target
// height, window mode) so the boot-time default matches the player's real settings.
//
// Separately, UE4's boot-time PreloadResolutionSettings clamps exclusive-Fullscreen resolutions to
// the monitor's EDID "native" size, which many newer monitors report as 1920x1080. One jump is
// flipped so Fullscreen uses the desktop size as the cap, like Borderless/Windowed do.
//
// No hooks, no detours, nothing else in the game is touched. Each fix is applied only if its code
// is found exactly as expected; otherwise (e.g. after a game update) it is skipped.
//
// Loaded as a proxy for xapofx1_5.dll (imported by GGST-Win64-Shipping.exe); its single export is
// forwarded to the real System32 copy. (sensapi.dll was used originally, but StriveLabs ships its
// own sensapi.dll loader into the same folder.)

#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <share.h>
#include <optional>
#include <string>
#include <vector>

static HMODULE g_self;
static wchar_t g_dir[MAX_PATH];  // folder containing this DLL
static FILE* g_log;

static void Log(const char* fmt, ...)
{
	if (!g_log) return;
	SYSTEMTIME t; GetLocalTime(&t);
	fprintf(g_log, "[%02d:%02d:%02d.%03d] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
	va_list a; va_start(a, fmt); vfprintf(g_log, fmt, a); va_end(a);
	fputc('\n', g_log); fflush(g_log);
}

// ---------------------------------------------------------------------------------------------
// xapofx1_5.dll forwarding
// ---------------------------------------------------------------------------------------------
// xapofx1_5.dll (DirectX June 2010 redist, installed by Steam with the game) has a single export.
// The wrapper takes 4 pointer-sized args so rcx/rdx/r8/r9 pass through untouched whatever the
// real signature is (CreateFX takes 2 in 1.5, up to 4 in later XAPOFX versions).
static HMODULE RealXapofx()
{
	static HMODULE real = [] {
		wchar_t path[MAX_PATH];
		GetSystemDirectoryW(path, MAX_PATH);
		wcscat_s(path, L"\\xapofx1_5.dll");
		return LoadLibraryW(path);
	}();
	return real;
}

extern "C" HRESULT __cdecl Proxy_CreateFX(void* a, void* b, void* c, void* d)
{
	using CreateFX_t = HRESULT(__cdecl*)(void*, void*, void*, void*);
	static auto fn = RealXapofx() ? reinterpret_cast<CreateFX_t>(GetProcAddress(RealXapofx(), "CreateFX")) : nullptr;
	if (!fn) return E_NOTIMPL;
	return fn(a, b, c, d);
}

// ---------------------------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------------------------
struct Settings {
	int width = 0, height = 0;  // 0 = desktop resolution
	int mode = -1;              // -1 = whatever the game last saved, 0 fullscreen, 1 borderless, 2 windowed
	bool log = true;
};

static std::wstring ConfigPath() { return std::wstring(g_dir) + L"\\GGSTNativeRes.ini"; }

static void WriteDefaultConfig()
{
	const char* text =
		"; GGSTNativeRes - boot-time resolution fix for Guilty Gear -Strive-\r\n"
		"; Delete xapofx1_5.dll from the game folder to uninstall.\r\n"
		"[Settings]\r\n"
		"; Resolution the game should start in. 0 = your monitor's current desktop resolution.\r\n"
		"Width=0\r\n"
		"Height=0\r\n"
		"; Window mode to start in: -1 = same as your in-game setting (read from GameUserSettings.ini),\r\n"
		";                          0 = Fullscreen, 1 = Borderless, 2 = Windowed\r\n"
		"Mode=-1\r\n"
		"; Write GGSTNativeRes.log next to this file (1 = yes, 0 = no)\r\n"
		"Log=1\r\n";
	HANDLE f = CreateFileW(ConfigPath().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (f == INVALID_HANDLE_VALUE) return;
	DWORD n; WriteFile(f, text, (DWORD)strlen(text), &n, nullptr);
	CloseHandle(f);
}

static Settings LoadSettings()
{
	WriteDefaultConfig();  // no-op if it already exists
	Settings s;
	auto ini = ConfigPath();
	s.width  = GetPrivateProfileIntW(L"Settings", L"Width", 0, ini.c_str());
	s.height = GetPrivateProfileIntW(L"Settings", L"Height", 0, ini.c_str());
	s.mode   = GetPrivateProfileIntW(L"Settings", L"Mode", -1, ini.c_str());
	s.log    = GetPrivateProfileIntW(L"Settings", L"Log", 1, ini.c_str()) != 0;
	return s;
}

static std::optional<int> GameSavedWindowMode()
{
	wchar_t local[MAX_PATH];
	if (!GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH)) return std::nullopt;
	std::wstring ini = std::wstring(local) + L"\\GGST\\Saved\\Config\\WindowsNoEditor\\GameUserSettings.ini";
	int v = GetPrivateProfileIntW(L"/Script/Engine.GameUserSettings", L"FullscreenMode", -1, ini.c_str());
	if (v < 0 || v > 2) return std::nullopt;
	return v;
}

// ---------------------------------------------------------------------------------------------
// Pattern scanning / patching
// ---------------------------------------------------------------------------------------------
struct Pattern { std::vector<int> bytes; };  // -1 = wildcard

static Pattern Parse(const char* sig)
{
	Pattern p;
	for (const char* c = sig; *c;) {
		if (*c == ' ') { ++c; continue; }
		if (*c == '?') { p.bytes.push_back(-1); while (*c == '?') ++c; continue; }
		p.bytes.push_back((int)strtoul(c, const_cast<char**>(&c), 16));
	}
	return p;
}

// Returns every match (we require exactly one).
static std::vector<uint8_t*> FindAll(uint8_t* begin, size_t size, const Pattern& p)
{
	std::vector<uint8_t*> hits;
	const size_t n = p.bytes.size();
	const uint8_t first = (uint8_t)p.bytes[0];  // our patterns never start with a wildcard
	for (uint8_t* cur = begin, *end = begin + size - n; cur <= end;) {
		cur = (uint8_t*)memchr(cur, first, end - cur + 1);
		if (!cur) break;
		size_t j = 1;
		while (j < n && (p.bytes[j] < 0 || cur[j] == (uint8_t)p.bytes[j])) ++j;
		if (j == n) hits.push_back(cur);
		++cur;
	}
	return hits;
}

static bool TextSection(uint8_t*& start, size_t& size)
{
	auto base = (uint8_t*)GetModuleHandleW(nullptr);
	auto dos = (IMAGE_DOS_HEADER*)base;
	auto nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
	auto sec = IMAGE_FIRST_SECTION(nt);
	for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
		if (memcmp(sec->Name, ".text", 6) == 0) {
			start = base + sec->VirtualAddress;
			size = sec->Misc.VirtualSize;
			return true;
		}
	}
	return false;
}

template <typename T> static void Write(uint8_t* at, T value)
{
	DWORD old;
	VirtualProtect(at, sizeof(T), PAGE_EXECUTE_READWRITE, &old);
	memcpy(at, &value, sizeof(T));
	VirtualProtect(at, sizeof(T), old, &old);
	FlushInstructionCache(GetCurrentProcess(), at, sizeof(T));
}

// Fix 1 - the game's boot-time default (Arc System Works code, the screen-settings apply routine):
//   precheck:  cmp [r14+rcx*4], 1920 / jb / cmp [r14+rcx*4+4], 1080 / jb   - is the largest mode >= default?
//   loop:      cmp [r14+rcx*4], 1920 / jne / cmp [r14+rcx*4+4], 1080 / je  - find the default's index
//   modestore: mov [rdi+X], dl / mov byte [rdi+Y], 0 / mov byte [rdi+Z], 0  - window mode = Fullscreen
static const char* kPrecheck  = "41 81 3C 8E 80 07 00 00 72 ?? 41 81 7C 8E 04 38 04 00 00 72";
static const char* kLoop      = "41 81 3C 8E 80 07 00 00 75 ?? 41 81 7C 8E 04 38 04 00 00 74";
static const char* kModeStore = "88 97 ?? ?? ?? ?? C6 87 ?? ?? ?? ?? 00 C6 87 ?? ?? ?? ?? 00 EB";
constexpr int kWidthOffset = 4, kHeightOffset = 15, kModeOffset = 12;

// Fix 2 - Unreal's UGameEngine::DetermineGameWindowResolution (used by PreloadResolutionSettings at
// boot). In Fullscreen it caps the resolution at the primary monitor's "native" size, which UE4
// takes from the first detailed timing in the monitor's EDID. Many newer monitors list a 1920x1080
// compatibility mode there, so 1440p/4K gets clamped to 1080p. Turning this jne into a jmp makes
// Fullscreen use the desktop size as the cap - the same thing Borderless/Windowed already do.
//   mov ebx, [rbp-35h] / test r15d, r15d / jne skip / movsxd rax, [rbp-29h] / test eax, eax / jle / mov rcx, [rbp-31h]
static const char* kNativeCap = "8B 5D CB 45 85 FF 75 ?? 48 63 45 D7 85 C0 7E ?? 48 8B 4D CF";
constexpr int kNativeCapJumpOffset = 6;

#ifdef GGSTNR_DIAG
// ---------------------------------------------------------------------------------------------
// Diagnostic build only (build.bat diag): logs every FSystemResolution::RequestResolutionChange
// call and every primary-display mode change, on one timeline. Never shipped in release builds.
// ---------------------------------------------------------------------------------------------
#include <intrin.h>
static const char* kRequestResChange =
	"48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 4C 89 74 24 20 55 48 8B EC 48 83 EC 50 33 FF "
	"48 8D 35 ?? ?? ?? ?? 48 89 7D E0 8B DA 48 89 7D E8 44 8B F1 45 85 C0";
constexpr size_t kStolen = 15;  // three position-independent "mov [rsp+N], reg" instructions
using RequestResChange_t = void(__fastcall*)(int32_t, int32_t, int32_t);
static RequestResChange_t g_origRRC;

static void __fastcall HookRRC(int32_t x, int32_t y, int32_t mode)
{
	auto base = (uintptr_t)GetModuleHandleW(nullptr);
	Log("RequestResolutionChange(%d, %d, mode %d) from exe+%#llx", x, y, mode,
	    (unsigned long long)((uintptr_t)_ReturnAddress() - base));
	g_origRRC(x, y, mode);
}

static void JmpAbs(uint8_t* at, void* to)
{
	at[0] = 0xFF; at[1] = 0x25; memset(at + 2, 0, 4);  // jmp [rip+0]
	memcpy(at + 6, &to, 8);
}

static void InstallDiagHook(uint8_t* text, size_t textSize)
{
	// SteamStub may still be decrypting later parts of .text, so retry for a while.
	std::vector<uint8_t*> hits;
	const DWORD start = GetTickCount();
	while ((hits = FindAll(text, textSize, Parse(kRequestResChange))).empty() && GetTickCount() - start < 10000)
		Sleep(1);
	Log("DIAG: RequestResolutionChange scan waited %lu ms", GetTickCount() - start);
	if (hits.size() != 1) { Log("DIAG: RequestResolutionChange not found (%zu)", hits.size()); return; }
	uint8_t* target = hits[0];
	auto gate = (uint8_t*)VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	memcpy(gate, target, kStolen);
	JmpAbs(gate + kStolen, target + kStolen);
	g_origRRC = (RequestResChange_t)gate;
	DWORD old;
	VirtualProtect(target, kStolen, PAGE_EXECUTE_READWRITE, &old);
	JmpAbs(target, (void*)&HookRRC);
	target[14] = 0x90;
	VirtualProtect(target, kStolen, old, &old);
	FlushInstructionCache(GetCurrentProcess(), target, kStolen);
	Log("DIAG: hooked RequestResolutionChange at exe+%#llx",
	    (unsigned long long)(target - (uint8_t*)GetModuleHandleW(nullptr)));
}

static DWORD WINAPI DisplayWatchThread(LPVOID)
{
	DWORD lastW = 0, lastH = 0;
	for (;;) {
		DEVMODEW dm{}; dm.dmSize = sizeof(dm);
		if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dm) &&
		    (dm.dmPelsWidth != lastW || dm.dmPelsHeight != lastH)) {
			lastW = dm.dmPelsWidth; lastH = dm.dmPelsHeight;
			Log("DISPLAY mode now %lux%lu@%lu", lastW, lastH, dm.dmDisplayFrequency);
		}
		Sleep(10);
	}
}
#endif

static DWORD WINAPI PatchThread(LPVOID)
{
	Settings s = LoadSettings();
	if (s.log) {
		std::wstring logPath = std::wstring(g_dir) + L"\\GGSTNativeRes.log";
		g_log = _wfsopen(logPath.c_str(), L"w", _SH_DENYNO);  // shared, so it can be read while the game runs
	}
	Log("GGSTNativeRes loaded");
#ifdef GGSTNR_DIAG
	if (HANDLE t = CreateThread(nullptr, 0, DisplayWatchThread, nullptr, 0, nullptr)) CloseHandle(t);
#endif

	int width = s.width, height = s.height;
	if (width <= 0 || height <= 0) {
		DEVMODEW dm{}; dm.dmSize = sizeof(dm);
		if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dm)) {
			width = (int)dm.dmPelsWidth; height = (int)dm.dmPelsHeight;
		}
	}
	int mode = s.mode;
	if (mode < 0 || mode > 2) mode = GameSavedWindowMode().value_or(0);
	Log("Target: %dx%d, mode %d (%s)", width, height, mode,
	    mode == 0 ? "Fullscreen" : mode == 1 ? "Borderless" : "Windowed");
	const bool haveTarget = width > 0 && height > 0;
	if (!haveTarget) Log("Could not determine a resolution - boot default will not be patched.");

	uint8_t* text; size_t textSize;
	if (!TextSection(text, textSize)) { Log("No .text section - not patching."); return 0; }

	// The exe is wrapped in SteamStub, which decrypts .text at startup - we're loaded before that, and
	// later parts of .text can appear after earlier ones. Keep scanning until every pattern has
	// appeared (or we give up), then apply each fix independently.
	const Pattern pre = Parse(kPrecheck), loop = Parse(kLoop), store = Parse(kModeStore), cap = Parse(kNativeCap);
	const DWORD start = GetTickCount();
	std::vector<uint8_t*> a, b, c, d;
	for (;;) {
		if (a.empty()) a = FindAll(text, textSize, loop);
		if (d.empty()) d = FindAll(text, textSize, cap);
		if ((!a.empty() && !d.empty()) || GetTickCount() - start > 30000) break;
		Sleep(1);
	}
	b = FindAll(text, textSize, pre);
	c = FindAll(text, textSize, store);
	Log("Scan took %lu ms: precheck=%zu loop=%zu modestore=%zu nativecap=%zu",
	    GetTickCount() - start, b.size(), a.size(), c.size(), d.size());

	// Fix 1. Safety: each pattern must match exactly once, in the order precheck < loop < modestore,
	// all within one small function. Anything else means the game changed - leave it alone.
	if (!haveTarget) {}
	else if (a.size() != 1 || b.size() != 1 || c.size() != 1)
		Log("Boot default: unexpected match count - game updated? Not patched.");
	else if (uint8_t *p = b[0], *l = a[0], *m = c[0]; !(p < l && l < m && m - p < 0x80))
		Log("Boot default: matches not laid out as expected - not patched.");
	else {
		Write<int32_t>(p + kWidthOffset, width);
		Write<int32_t>(p + kHeightOffset, height);
		Write<int32_t>(l + kWidthOffset, width);
		Write<int32_t>(l + kHeightOffset, height);
		Write<uint8_t>(m + kModeOffset, (uint8_t)mode);
		Log("Patched boot default: 1920x1080 Fullscreen -> %dx%d mode %d", width, height, mode);
	}

	// Fix 2.
	if (d.size() != 1 || d[0][kNativeCapJumpOffset] != 0x75)
		Log("Fullscreen native-size cap: unexpected match - game updated? Not patched.");
	else {
		Write<uint8_t>(d[0] + kNativeCapJumpOffset, 0xEB);  // jne -> jmp
		Log("Patched fullscreen native-size cap: uses desktop size instead of monitor EDID");
	}
#ifdef GGSTNR_DIAG
	InstallDiagHook(text, textSize);
	return 0;  // keep the log open for the hook and display watcher
#endif
	if (g_log) { fclose(g_log); g_log = nullptr; }
	return 0;
}

BOOL WINAPI DllMain(HMODULE module, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH) {
		g_self = module;
		DisableThreadLibraryCalls(module);
		GetModuleFileNameW(module, g_dir, MAX_PATH);
		if (wchar_t* slash = wcsrchr(g_dir, L'\\')) *slash = 0;
		// Only act inside the game itself.
		wchar_t exe[MAX_PATH];
		GetModuleFileNameW(nullptr, exe, MAX_PATH);
		const wchar_t* name = wcsrchr(exe, L'\\');
		if (name && _wcsicmp(name + 1, L"GGST-Win64-Shipping.exe") == 0)
			if (HANDLE t = CreateThread(nullptr, 0, PatchThread, nullptr, 0, nullptr)) CloseHandle(t);
	}
	return TRUE;
}
