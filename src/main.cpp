// GGSTNativeRes - stops Guilty Gear -Strive- from forcing exclusive-fullscreen 1920x1080 at boot.
//
// At startup (before SYSTEM.sav is loaded) the game's screen-settings apply routine sees that the
// stored "resolution list fingerprint" doesn't match the monitor and resets the settings to a
// hardcoded default: the 1920x1080 entry of the resolution list, window mode 0 (Fullscreen).
// A few seconds later the save loads and the real settings are applied, causing a second switch.
//
// This DLL changes only the three constants that define that default (target width, target
// height, window mode) so the boot-time default matches the player's real settings. No hooks,
// no detours, nothing else in the game is touched. If the expected code isn't found (e.g. after a
// game update changed it), nothing is patched and the game behaves exactly as it does without it.
//
// Loaded as a proxy for sensapi.dll (imported by GGST-Win64-Shipping.exe); its 3 exports are
// forwarded to the real System32 copy.

#include <windows.h>
#include <sensapi.h>
#include <cstdint>
#include <cstdio>
#include <cwchar>
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
// sensapi.dll forwarding
// ---------------------------------------------------------------------------------------------
static HMODULE RealSensApi()
{
	static HMODULE real = [] {
		wchar_t path[MAX_PATH];
		GetSystemDirectoryW(path, MAX_PATH);
		wcscat_s(path, L"\\sensapi.dll");
		return LoadLibraryW(path);
	}();
	return real;
}

template <typename T> static T Real(const char* name)
{
	HMODULE m = RealSensApi();
	return m ? reinterpret_cast<T>(GetProcAddress(m, name)) : nullptr;
}

extern "C" BOOL WINAPI Proxy_IsNetworkAlive(LPDWORD flags)
{
	static auto fn = Real<decltype(&IsNetworkAlive)>("IsNetworkAlive");
	if (!fn) { SetLastError(ERROR_PROC_NOT_FOUND); return FALSE; }
	return fn(flags);
}
extern "C" BOOL WINAPI Proxy_IsDestinationReachableA(LPCSTR dest, LPQOCINFO info)
{
	static auto fn = Real<decltype(&IsDestinationReachableA)>("IsDestinationReachableA");
	if (!fn) { SetLastError(ERROR_PROC_NOT_FOUND); return FALSE; }
	return fn(dest, info);
}
extern "C" BOOL WINAPI Proxy_IsDestinationReachableW(LPCWSTR dest, LPQOCINFO info)
{
	static auto fn = Real<decltype(&IsDestinationReachableW)>("IsDestinationReachableW");
	if (!fn) { SetLastError(ERROR_PROC_NOT_FOUND); return FALSE; }
	return fn(dest, info);
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
		"; Delete sensapi.dll from the game folder to uninstall.\r\n"
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

// In the screen-settings apply routine (GGST-Win64-Shipping.exe):
//   precheck:  cmp [r14+rcx*4], 1920 / jb / cmp [r14+rcx*4+4], 1080 / jb   - is the largest mode >= default?
//   loop:      cmp [r14+rcx*4], 1920 / jne / cmp [r14+rcx*4+4], 1080 / je  - find the default's index
//   modestore: mov [rdi+X], dl / mov byte [rdi+Y], 0 / mov byte [rdi+Z], 0  - window mode = Fullscreen
static const char* kPrecheck  = "41 81 3C 8E 80 07 00 00 72 ?? 41 81 7C 8E 04 38 04 00 00 72";
static const char* kLoop      = "41 81 3C 8E 80 07 00 00 75 ?? 41 81 7C 8E 04 38 04 00 00 74";
static const char* kModeStore = "88 97 ?? ?? ?? ?? C6 87 ?? ?? ?? ?? 00 C6 87 ?? ?? ?? ?? 00 EB";
constexpr int kWidthOffset = 4, kHeightOffset = 15, kModeOffset = 12;

static DWORD WINAPI PatchThread(LPVOID)
{
	Settings s = LoadSettings();
	if (s.log) {
		std::wstring logPath = std::wstring(g_dir) + L"\\GGSTNativeRes.log";
		_wfopen_s(&g_log, logPath.c_str(), L"w");
	}
	Log("GGSTNativeRes loaded");

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
	if (width <= 0 || height <= 0) { Log("Could not determine a resolution - not patching."); return 0; }

	uint8_t* text; size_t textSize;
	if (!TextSection(text, textSize)) { Log("No .text section - not patching."); return 0; }

	// The exe is wrapped in SteamStub, which decrypts .text at startup. We're loaded before that,
	// so keep scanning until the code appears (it happens well before the game's own init runs).
	const Pattern pre = Parse(kPrecheck), loop = Parse(kLoop), store = Parse(kModeStore);
	const DWORD start = GetTickCount();
	std::vector<uint8_t*> a, b, c;
	for (;;) {
		a = FindAll(text, textSize, loop);
		if (!a.empty() || GetTickCount() - start > 30000) break;
		Sleep(1);
	}
	b = FindAll(text, textSize, pre);
	c = FindAll(text, textSize, store);
	Log("Scan took %lu ms: precheck=%zu loop=%zu modestore=%zu", GetTickCount() - start, b.size(), a.size(), c.size());

	// Safety: each pattern must match exactly once, in the order precheck < loop < modestore,
	// all within one small function. Anything else means the game changed - do nothing.
	if (a.size() != 1 || b.size() != 1 || c.size() != 1) { Log("Unexpected match count - game updated? Not patching."); return 0; }
	uint8_t *p = b[0], *l = a[0], *m = c[0];
	if (!(p < l && l < m && m - p < 0x80)) { Log("Matches not laid out as expected - not patching."); return 0; }

	Write<int32_t>(p + kWidthOffset, width);
	Write<int32_t>(p + kHeightOffset, height);
	Write<int32_t>(l + kWidthOffset, width);
	Write<int32_t>(l + kHeightOffset, height);
	Write<uint8_t>(m + kModeOffset, (uint8_t)mode);
	Log("Patched boot default: 1920x1080 Fullscreen -> %dx%d mode %d", width, height, mode);
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
