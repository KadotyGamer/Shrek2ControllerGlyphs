#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <Xinput.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <atomic>
#include "glyphs.h"

namespace {
HMODULE selfModule;
BYTE* game;
wchar_t logPath[MAX_PATH];
using PromptFn = Prompt* (*)(void*, Prompt*, int, int16_t);
PromptFn originalXInput, originalWindows;
void (*originalCallbacks)();
std::atomic<int> slotTypes[4]{};
std::atomic<unsigned> replacements{};
bool installed;
bool attempted;
void* steamInput;
void* (*getSteamInput)();
bool (*inputInit)(void*, bool);
uint64_t (*getController)(void*, int);
int (*getType)(void*, uint64_t);
int steamInterfaceVersion;
int steamInitIssue;
ULONGLONG lastPoll;
int forceMode; // 0 auto, 1 PlayStation, 2 Xbox

struct GameBuild {
    DWORD timestamp, imageSize, changelist;
    DWORD xinputPrompt, windowsPrompt, xinputVtable, windowsVtable;
};
constexpr GameBuild supportedBuilds[] = {
    {0x6ae2e602, 0x18cbd000, 574912, 0x2fea40, 0x2fb520, 0x512aa0, 0x511d68},
    {0x6af383da, 0x18cbd000, 583774, 0x2fea50, 0x2fb530, 0x512a70, 0x511d38},
    {0x6af3b8d9, 0x18cbd000, 583916, 0x2fea50, 0x2fb530, 0x512a70, 0x511d38},
};
const GameBuild* FindBuild(DWORD timestamp, DWORD size) {
    for (const auto& build : supportedBuilds)
        if (build.timestamp == timestamp && build.imageSize == size) return &build;
    return nullptr;
}

void Log(const char* format, ...) {
    if (!logPath[0]) return;
    FILE* f{};
    if (_wfopen_s(&f, logPath, L"a") || !f) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fprintf(f, "%02u:%02u:%02u ", t.wHour, t.wMinute, t.wSecond);
    va_list args; va_start(args, format); vfprintf(f, format, args); va_end(args);
    fputc('\n', f); fclose(f);
}

bool SwapPointer(void** address, void* expected, void* replacement) {
    if (*address != expected) return false;
    DWORD protection;
    if (!VirtualProtect(address, sizeof(void*), PAGE_READWRITE, &protection)) return false;
    void* previous = InterlockedCompareExchangePointer(address, replacement, expected);
    DWORD ignored; VirtualProtect(address, sizeof(void*), protection, &ignored);
    return previous == expected;
}

Prompt* XInputPrompt(void* object, Prompt* out, int slot, int16_t key) {
    originalXInput(object, out, slot, key);
    const int type = forceMode ? (forceMode == 1 ? 13 : 3) :
        (slot >= 0 && slot < 4 ? slotTypes[slot].load() : 0);
    if (ApplySony(*out, key, type) && replacements.fetch_add(1) == 0)
        Log("First PlayStation prompt rendered: slot=%d key=%d texture=%s (XInput)", slot, key, out->texture);
    return out;
}

int CountNativeGamepads(const void* object, uint32_t& state) {
    state = 0;
    int count = 0;
    for (int i = 0; i < 4; ++i) {
        const BYTE* device = reinterpret_cast<const BYTE*>(object) + 0x1d8 + i*0x108;
        const BYTE type = device[0];
        const bool connected = device[1] != 0;
        state |= uint32_t((type & 0x7f) | (connected ? 0x80 : 0)) << (i*8);
        // DeviceType: 0 Invalid, 1 Keyboard, 2 Gamepad, 3 NintendoGamepad,
        // 4 LeftJoycon, 5 RightJoycon. Only connected gamepads affect ambiguity.
        if (connected && type >= 2 && type <= 5) ++count;
    }
    return count;
}

// Native Sony devices already have the game's own detection. For Steam virtual
// GameInput devices, override only when exactly one gamepad is present, avoiding
// incorrect slot assumptions in mixed-controller native co-op.
Prompt* WindowsPrompt(void* object, Prompt* out, int slot, int16_t key) {
    originalWindows(object, out, slot, key);
    if (!SonyTexture(key)) return out;
    if (forceMode == 2) {
        *out = reinterpret_cast<Prompt*>(game + 0x60b0f0)[key];
        return out;
    }
    int type = 0;
    if (forceMode == 1) type = 13;
    else if (!forceMode && slot >= 0 && slot < 4) {
        uint32_t nativeState;
        const int nativeCount = CountNativeGamepads(object,nativeState);
        int detectedCount = 0;
        uint32_t steamState = 0;
        for (int i = 0; i < 4; ++i) {
            const int detected = slotTypes[i].load();
            steamState |= uint32_t(detected & 0xff) << (i*8);
            if (detected != 0) { ++detectedCount; type = detected; }
        }
        if (nativeCount != 1 || detectedCount != 1) type = 0;
        // Log topology changes, not each frame. These distinguish disconnected,
        // keyboard-only and ambiguous multi-gamepad cases without device IDs.
        static std::atomic<uint64_t> lastState[4] = {UINT64_MAX,UINT64_MAX,UINT64_MAX,UINT64_MAX};
        static std::atomic<unsigned> changes{};
        const uint64_t snapshot = (uint64_t(steamState)<<32) | nativeState;
        if (lastState[slot].exchange(snapshot)!=snapshot && changes.fetch_add(1)<32)
            Log("GameInput prompt: slot=%d key=%d nativeState=%08lx gamepads=%d SteamControllers=%d selectedType=%d",
                slot,key,static_cast<unsigned long>(nativeState),nativeCount,detectedCount,type);
    }
    if (ApplySony(*out, key, type) && replacements.fetch_add(1) == 0)
        Log("First PlayStation prompt rendered: slot=%d key=%d texture=%s (GameInput)", slot, key, out->texture);
    return out;
}

bool InstallGlyphHooks() {
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(game + reinterpret_cast<IMAGE_DOS_HEADER*>(game)->e_lfanew);
    const GameBuild* build = FindBuild(nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage);
    if (!build) {
        Log("Unsupported Shrek.exe build. Glyph hooks disabled; controller input still forwarded."); return false;
    }
    const BYTE signature[] = {0x49,0x0f,0xbf,0xc1,0x48,0x8d,0x0c,0x40};
    if (memcmp(game + build->xinputPrompt, signature, sizeof(signature))) {
        Log("XInput prompt signature mismatch; no hooks installed."); return false;
    }
    // All supported builds use 0x108-byte native device records and the same
    // connected flag at object+0x1d9. Validate those assumptions before hooking.
    const BYTE nativeLayout[] = {0x48,0x69,0xc0,0x08,0x01,0x00,0x00,
                                0x80,0xbc,0x08,0xd9,0x01,0x00,0x00,0x00};
    if (memcmp(game + build->windowsPrompt + 10, nativeLayout, sizeof(nativeLayout))) {
        Log("Native input layout mismatch; no hooks installed."); return false;
    }
    auto** xslot = reinterpret_cast<void**>(game + build->xinputVtable + 0x40);
    auto** wslot = reinterpret_cast<void**>(game + build->windowsVtable + 0x40);
    if (*xslot != game+build->xinputPrompt || *wslot != game+build->windowsPrompt) {
        Log("Prompt vtables differ from supported build; no hooks installed."); return false;
    }
    originalXInput = reinterpret_cast<PromptFn>(*xslot);
    originalWindows = reinterpret_cast<PromptFn>(*wslot);
    if (!SwapPointer(xslot, reinterpret_cast<void*>(originalXInput), reinterpret_cast<void*>(XInputPrompt))) return false;
    if (!SwapPointer(wslot, reinterpret_cast<void*>(originalWindows), reinterpret_cast<void*>(WindowsPrompt))) {
        SwapPointer(xslot, reinterpret_cast<void*>(XInputPrompt), reinterpret_cast<void*>(originalXInput)); return false;
    }
    Log("Installed prompt hooks for Shrek CL %lu. Mode=%s", build->changelist, forceMode == 1 ? "PlayStation" : forceMode == 2 ? "Xbox" : "Auto");
    return true;
}

void InitLogging() {
    wchar_t modulePath[MAX_PATH]; GetModuleFileNameW(selfModule, modulePath, MAX_PATH);
    wchar_t* slash = wcsrchr(modulePath, L'\\'); if (!slash) return;
    slash[1] = 0;
    wcscpy_s(logPath, modulePath); wcscat_s(logPath, L"ShrekControllerGlyphs.log");
    wchar_t config[MAX_PATH]; wcscpy_s(config, modulePath); wcscat_s(config, L"ShrekControllerGlyphs.ini");
    wchar_t mode[32]; GetPrivateProfileStringW(L"Glyphs", L"Mode", L"Auto", mode, 32, config);
    if (!_wcsicmp(mode, L"PlayStation")) forceMode = 1;
    if (!_wcsicmp(mode, L"Xbox")) forceMode = 2;
    FILE* f{}; if (!_wfopen_s(&f, logPath, L"w") && f) fclose(f);
    Log("Shrek Controller Glyphs 1.0.5 loaded.");
}

using ExportResolver = FARPROC (WINAPI*)(HMODULE, LPCSTR);
bool BindSteamInput(HMODULE steam, ExportResolver resolve = GetProcAddress) {
    // Flat API functions belong to the SDK shipped with this DLL. Prefer its
    // current factory, falling back to v006 for the original game release.
    steamInterfaceVersion = 7;
    getSteamInput = reinterpret_cast<decltype(getSteamInput)>(resolve(steam, "SteamAPI_SteamInput_v007"));
    if (!getSteamInput) {
        steamInterfaceVersion = 6;
        getSteamInput = reinterpret_cast<decltype(getSteamInput)>(resolve(steam, "SteamAPI_SteamInput_v006"));
    }
    if (!getSteamInput) steamInterfaceVersion = 0;
    inputInit = reinterpret_cast<decltype(inputInit)>(resolve(steam, "SteamAPI_ISteamInput_Init"));
    getController = reinterpret_cast<decltype(getController)>(resolve(steam, "SteamAPI_ISteamInput_GetControllerForGamepadIndex"));
    getType = reinterpret_cast<decltype(getType)>(resolve(steam, "SteamAPI_ISteamInput_GetInputTypeForHandle"));
    return getSteamInput && inputInit && getController && getType;
}

void ReportSteamInitIssue(int issue, const char* message) {
    if (steamInitIssue != issue) {
        steamInitIssue = issue;
        Log("Steam Input: %s Retrying automatically.", message);
    }
}

void UpdateSteamTypes() {
    const auto now = GetTickCount64();
    if (lastPoll && now - lastPoll < 200) return;
    lastPoll = now;
    if (!steamInput) {
        HMODULE steam = GetModuleHandleW(L"steam_api64.dll");
        if (!steam) { ReportSteamInitIssue(1,"steam_api64.dll is not loaded."); return; }
        if (!BindSteamInput(steam)) {
            ReportSteamInitIssue(2,"Required exports or supported v007/v006 interface factory are missing."); return;
        }
        void* input = getSteamInput();
        if (!input) { ReportSteamInitIssue(3,"Interface is not available yet; launch through Steam."); return; }
        if (!inputInit(input, false)) { ReportSteamInitIssue(4,"Interface initialization returned false."); return; }
        steamInput = input;
        steamInitIssue = 0;
        Log("Steam Input v%03d initialized; checking hardware identity for XInput slots 0-3.",steamInterfaceVersion);
        return; // The next SteamAPI_RunCallbacks supplies the first input frame.
    }
    for (int i = 0; i < 4; ++i) {
        const uint64_t handle = getController(steamInput, i);
        const int type = handle ? getType(steamInput, handle) : 0;
        const int previous = slotTypes[i].exchange(type);
        if (type != previous)
            Log("Slot %d: Steam type=%d, glyphs=%s%s", i, type, IsPlayStation(type) ? "PlayStation" : "original/Xbox", handle ? "" : " (no Steam translation)");
    }
}

void RunCallbacks() {
    originalCallbacks();
    if (!attempted) {
        attempted = true;
        InitLogging();
        installed = InstallGlyphHooks();
    }
    if (installed) UpdateSteamTypes();
}

// Only patch the importing game's callback pointer during process attachment.
// No Steam calls, file I/O, threads, or LoadLibrary under the loader lock.
void AttachCallbackHook() {
    wchar_t path[MAX_PATH]; GetModuleFileNameW(nullptr, path, MAX_PATH);
    const wchar_t* name = wcsrchr(path, L'\\');
    if (!name || _wcsicmp(name+1, L"Shrek.exe")) return;
    game = reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr));
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(game + reinterpret_cast<IMAGE_DOS_HEADER*>(game)->e_lfanew);
    DWORD rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!rva) return;
    for (auto* desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(game+rva); desc->Name; ++desc) {
        if (_stricmp(reinterpret_cast<char*>(game+desc->Name), "steam_api64.dll") || !desc->OriginalFirstThunk) continue;
        auto* names = reinterpret_cast<IMAGE_THUNK_DATA64*>(game+desc->OriginalFirstThunk);
        auto* slots = reinterpret_cast<IMAGE_THUNK_DATA64*>(game+desc->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++slots) {
            if (IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal)) continue;
            auto* import = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(game+names->u1.AddressOfData);
            if (strcmp(import->Name, "SteamAPI_RunCallbacks")) continue;
            originalCallbacks = reinterpret_cast<decltype(originalCallbacks)>(slots->u1.Function);
            SwapPointer(reinterpret_cast<void**>(&slots->u1.Function), reinterpret_cast<void*>(originalCallbacks), reinterpret_cast<void*>(RunCallbacks));
            return;
        }
    }
}

INIT_ONCE realOnce = INIT_ONCE_STATIC_INIT;
HMODULE realXInput;
BOOL CALLBACK LoadReal(PINIT_ONCE, PVOID, PVOID*) {
    wchar_t path[MAX_PATH]; const UINT n = GetSystemDirectoryW(path, MAX_PATH);
    if (!n || n >= MAX_PATH-20) return FALSE;
    wcscat_s(path, L"\\XInput1_4.dll");
    realXInput = LoadLibraryExW(path, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    return realXInput != nullptr;
}
FARPROC Real(const char* name) {
    if (!InitOnceExecuteOnce(&realOnce, LoadReal, nullptr, nullptr)) return nullptr;
    return GetProcAddress(realXInput, name);
}
}

extern "C" DWORD WINAPI ProxyGetState(DWORD index, XINPUT_STATE* state) {
    static auto fn = reinterpret_cast<decltype(&XInputGetState)>(Real("XInputGetState"));
    return fn ? fn(index,state) : ERROR_DEVICE_NOT_CONNECTED;
}
extern "C" DWORD WINAPI ProxySetState(DWORD index, XINPUT_VIBRATION* vibration) {
    static auto fn = reinterpret_cast<decltype(&XInputSetState)>(Real("XInputSetState"));
    return fn ? fn(index,vibration) : ERROR_DEVICE_NOT_CONNECTED;
}
extern "C" DWORD WINAPI ProxyGetCapabilities(DWORD index, DWORD flags, XINPUT_CAPABILITIES* caps) {
    static auto fn = reinterpret_cast<decltype(&XInputGetCapabilities)>(Real("XInputGetCapabilities"));
    return fn ? fn(index,flags,caps) : ERROR_DEVICE_NOT_CONNECTED;
}
extern "C" void WINAPI ProxyEnable(BOOL enable) {
    using EnableFn = void (WINAPI*)(BOOL);
    static auto fn = reinterpret_cast<EnableFn>(Real("XInputEnable")); if (fn) fn(enable);
}
extern "C" DWORD WINAPI ProxyGetBatteryInformation(DWORD index, BYTE type, XINPUT_BATTERY_INFORMATION* info) {
    static auto fn = reinterpret_cast<decltype(&XInputGetBatteryInformation)>(Real("XInputGetBatteryInformation"));
    return fn ? fn(index,type,info) : ERROR_DEVICE_NOT_CONNECTED;
}
extern "C" DWORD WINAPI ProxyGetKeystroke(DWORD index, DWORD reserved, XINPUT_KEYSTROKE* key) {
    static auto fn = reinterpret_cast<decltype(&XInputGetKeystroke)>(Real("XInputGetKeystroke"));
    return fn ? fn(index,reserved,key) : ERROR_DEVICE_NOT_CONNECTED;
}
extern "C" DWORD WINAPI ProxyGetAudioDeviceIds(DWORD index, LPWSTR render, UINT* renderCount, LPWSTR capture, UINT* captureCount) {
    static auto fn = reinterpret_cast<decltype(&XInputGetAudioDeviceIds)>(Real("XInputGetAudioDeviceIds"));
    return fn ? fn(index,render,renderCount,capture,captureCount) : ERROR_DEVICE_NOT_CONNECTED;
}
extern "C" DWORD WINAPI ProxyGetStateEx(DWORD index, XINPUT_STATE* state) {
    static auto fn = reinterpret_cast<decltype(&XInputGetState)>(Real(MAKEINTRESOURCEA(100)));
    return fn ? fn(index,state) : ProxyGetState(index,state);
}
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        selfModule = module;
        AttachCallbackHook();
    }
    return TRUE;
}
