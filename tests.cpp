#include "mod.cpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
void Check(bool condition, const char* message) { if (!condition) { printf("FAIL: %s\n",message); exit(1); } }
uint64_t mockHandles[4]{};
int mockTypes[4]{};
uint64_t MockController(void*, int slot) { return mockHandles[slot]; }
int MockType(void*, uint64_t handle) { return mockTypes[handle-1]; }
Prompt* MockOriginal(void*, Prompt* out, int, int16_t) {
    *out = {"original", "label", 0.7f, 123}; return out;
}
int mockFactoryMask;
bool mockMissingInit;
void* MockFactory() { return reinterpret_cast<void*>(1); }
bool MockInit(void*,bool) { return true; }
FARPROC WINAPI MockResolve(HMODULE, LPCSTR name) {
    if (!strcmp(name,"SteamAPI_SteamInput_v007")) return (mockFactoryMask & 2) ? reinterpret_cast<FARPROC>(MockFactory) : nullptr;
    if (!strcmp(name,"SteamAPI_SteamInput_v006")) return (mockFactoryMask & 1) ? reinterpret_cast<FARPROC>(MockFactory) : nullptr;
    if (!strcmp(name,"SteamAPI_ISteamInput_Init")) return mockMissingInit ? nullptr : reinterpret_cast<FARPROC>(MockInit);
    if (!strcmp(name,"SteamAPI_ISteamInput_GetControllerForGamepadIndex")) return reinterpret_cast<FARPROC>(MockController);
    if (!strcmp(name,"SteamAPI_ISteamInput_GetInputTypeForHandle")) return reinterpret_cast<FARPROC>(MockType);
    return nullptr;
}
int main(int argc, char** argv) {
    for (int mask=0;mask<4;++mask) {
        mockFactoryMask=mask;
        Check(BindSteamInput(nullptr,MockResolve)==(mask!=0),"factory availability");
        Check(steamInterfaceVersion==((mask&2)?7:(mask&1)?6:0),"v007 preferred, v006 fallback");
        if (mask) Check(getSteamInput()==reinterpret_cast<void*>(1) && inputInit(nullptr,false),"resolved factory/init callable");
    }
    mockMissingInit=true;
    Check(!BindSteamInput(nullptr,MockResolve),"missing required export rejected");
    mockMissingInit=false;
    if (argc==3 && !strcmp(argv[1],"--steam-dll")) {
        HMODULE steam = LoadLibraryExA(argv[2],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        Check(steam!=nullptr,"load actual Steam API DLL");
        Check(BindSteamInput(steam),"resolve required exports from installed game's Steam DLL");
        printf("PASS: actual Steam API DLL exports resolved, interface v%03d. No Steam initialization or game launch.\n",steamInterfaceVersion);
        FreeLibrary(steam);
    }
    printf("PASS: Steam Input v007/v006 factory selection and missing-export regression tests.\n");
    for (int key = -1; key < 160; ++key) {
        for (int type = 0; type <= 19; ++type) {
            Prompt p{"original", "label", 0.7f, 123};
            bool replaced = ApplySony(p,key,type);
            bool expected = (type==5 || type==12 || type==13) && ((key>=125 && key<=148) || (key>=152 && key<=154));
            Check(replaced == expected, "device/key classification");
            if (!expected) Check(!strcmp(p.texture,"original") && !strcmp(p.label,"label") && p.scale==0.7f, "preserve Xbox, unknown and keyboard prompts");
            else Check(strstr(p.texture,"console_ps5_") == p.texture && !p.label && p.scale>0, "PlayStation output");
        }
    }
    Check(!strcmp(SonyTexture(133),"console_ps5_cross_button_ui"),"A -> Cross");
    Check(!strcmp(SonyTexture(134),"console_ps5_circle_button_ui"),"B -> Circle");
    Check(!strcmp(SonyTexture(135),"console_ps5_square_button_ui"),"X -> Square");
    Check(!strcmp(SonyTexture(136),"console_ps5_triangle_button_ui"),"Y -> Triangle");
    // Exercise the real polling and hook functions through simulated Steam APIs.
    steamInput = reinterpret_cast<void*>(1);
    getController = MockController;
    getType = MockType;
    originalXInput = MockOriginal;
    originalWindows = MockOriginal;
    mockHandles[0]=1; mockTypes[0]=13; // DualSense
    mockHandles[1]=2; mockTypes[1]=3;  // Xbox
    lastPoll=0; UpdateSteamTypes();
    Prompt output{};
    Check(XInputPrompt(nullptr,&output,0,133)==&output && !strcmp(output.texture,SonyTexture(133)),"PS5 slot hook ABI/output");
    XInputPrompt(nullptr,&output,1,133);
    Check(!strcmp(output.texture,"original"),"mixed Xbox slot remains Xbox");
    XInputPrompt(nullptr,&output,0,7);
    Check(!strcmp(output.texture,"original"),"keyboard prompt untouched on PS5 slot");
    XInputPrompt(nullptr,&output,-1,133);
    Check(!strcmp(output.texture,"original"),"invalid slot preserves original");
    mockTypes[0]=3; mockTypes[1]=13;
    lastPoll=0; UpdateSteamTypes();
    XInputPrompt(nullptr,&output,0,133);
    Check(!strcmp(output.texture,"original"),"live swap PS5 to Xbox");
    XInputPrompt(nullptr,&output,1,133);
    Check(!strcmp(output.texture,SonyTexture(133)),"live swap Xbox to PS5");
    mockHandles[1]=0; lastPoll=0; UpdateSteamTypes();
    XInputPrompt(nullptr,&output,1,133);
    Check(!strcmp(output.texture,"original"),"disconnect clears cached family");
    mockTypes[0]=13; lastPoll=0; UpdateSteamTypes();
    BYTE nativeObject[0x600]{};
    nativeObject[0x1d8]=2;
    nativeObject[0x1d9]=1;
    WindowsPrompt(nativeObject,&output,0,133);
    Check(!strcmp(output.texture,SonyTexture(133)),"single native gamepad Steam fallback");
    // Reproduce a connected keyboard plus one gamepad: the old implementation
    // counted the keyboard and silently refused the PlayStation override.
    nativeObject[0x1d8+0x108]=1;
    nativeObject[0x1d9+0x108]=1;
    WindowsPrompt(nativeObject,&output,0,133);
    Check(!strcmp(output.texture,SonyTexture(133)),"keyboard must not suppress sole PS5 controller");
    nativeObject[0x1d8]=1;
    nativeObject[0x1d8+0x108]=2;
    WindowsPrompt(nativeObject,&output,1,133);
    Check(!strcmp(output.texture,SonyTexture(133)),"keyboard in slot 0 plus PS5 gamepad in slot 1");
    WindowsPrompt(nativeObject,&output,1,7);
    Check(!strcmp(output.texture,"original"),"native keyboard key remains unchanged");
    mockTypes[0]=3; lastPoll=0; UpdateSteamTypes();
    WindowsPrompt(nativeObject,&output,1,133);
    Check(!strcmp(output.texture,"original"),"keyboard plus Xbox retains Xbox prompts");
    mockTypes[0]=13; lastPoll=0; UpdateSteamTypes();
    nativeObject[0x1d8+0x108]=0;
    WindowsPrompt(nativeObject,&output,1,133);
    Check(!strcmp(output.texture,"original"),"connected invalid type is not a gamepad");
    nativeObject[0x1d8+0x108]=2;
    nativeObject[0x1d9+0x108]=0;
    WindowsPrompt(nativeObject,&output,1,133);
    Check(!strcmp(output.texture,"original"),"disconnected gamepad is not counted");
    nativeObject[0x1d9+0x108]=1;
    nativeObject[0x1d8]=2;
    WindowsPrompt(nativeObject,&output,0,133);
    Check(!strcmp(output.texture,"original"),"mixed native gamepads do not guess slot identities");
    HMODULE proxy = LoadLibraryW(L"build\\XInput1_4.dll"); Check(proxy != nullptr,"load proxy");
    auto state = reinterpret_cast<decltype(&XInputGetState)>(GetProcAddress(proxy,MAKEINTRESOURCEA(2)));
    auto vibration = reinterpret_cast<decltype(&XInputSetState)>(GetProcAddress(proxy,MAKEINTRESOURCEA(3)));
    Check(state && vibration,"game imports ordinal 2 and 3");
    XINPUT_STATE s{}; XINPUT_VIBRATION v{};
    Check(state(999,&s)==ERROR_BAD_ARGUMENTS || state(999,&s)==ERROR_DEVICE_NOT_CONNECTED,"system XInput forwarding");
    Check(vibration(999,&v)!=ERROR_SUCCESS,"system vibration forwarding");
    // Exercise compatibility guards and the actual hook installer without
    // launching the game, using an isolated image with synthetic vtables.
    BYTE* fixture = static_cast<BYTE*>(VirtualAlloc(nullptr,0x610000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    Check(fixture != nullptr,"allocate build fixture");
    game = fixture;
    reinterpret_cast<IMAGE_DOS_HEADER*>(fixture)->e_lfanew = 0x100;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(fixture+0x100);
    const BYTE xSignature[] = {0x49,0x0f,0xbf,0xc1,0x48,0x8d,0x0c,0x40};
    const BYTE nativeLayout[] = {0x48,0x69,0xc0,0x08,0x01,0x00,0x00,0x80,0xbc,0x08,0xd9,0x01,0x00,0x00,0x00};
    for (const auto& build : supportedBuilds) {
        nt->FileHeader.TimeDateStamp=build.timestamp;
        nt->OptionalHeader.SizeOfImage=build.imageSize;
        memcpy(fixture+build.xinputPrompt,xSignature,sizeof(xSignature));
        memcpy(fixture+build.windowsPrompt+10,nativeLayout,sizeof(nativeLayout));
        auto** xs = reinterpret_cast<void**>(fixture+build.xinputVtable+0x40);
        auto** ws = reinterpret_cast<void**>(fixture+build.windowsVtable+0x40);
        *xs=fixture+build.xinputPrompt;
        *ws=fixture+build.windowsPrompt;
        fixture[build.windowsPrompt+10] ^= 1;
        Check(!InstallGlyphHooks() && *xs==fixture+build.xinputPrompt,"reject native ABI mismatch without mutation");
        fixture[build.windowsPrompt+10] ^= 1;
        fixture[build.xinputPrompt] ^= 1;
        Check(!InstallGlyphHooks() && *xs==fixture+build.xinputPrompt,"reject XInput signature mismatch");
        fixture[build.xinputPrompt] ^= 1;
        *ws=nullptr;
        Check(!InstallGlyphHooks() && *xs==fixture+build.xinputPrompt,"reject altered vtable without partial hook");
        *ws=fixture+build.windowsPrompt;
        Check(InstallGlyphHooks(),"install hooks for supported build");
        Check(*xs==reinterpret_cast<void*>(XInputPrompt) && *ws==reinterpret_cast<void*>(WindowsPrompt),"both vtables hooked");
        Check(originalXInput==reinterpret_cast<PromptFn>(fixture+build.xinputPrompt),"correct original XInput function saved");
        Check(originalWindows==reinterpret_cast<PromptFn>(fixture+build.windowsPrompt),"correct original native function saved");
    }
    nt->FileHeader.TimeDateStamp=0;
    Check(!InstallGlyphHooks(),"unknown build rejected");
    Check(!FindBuild(supportedBuilds[0].timestamp,0),"wrong image size rejected");
    VirtualFree(fixture,0,MEM_RELEASE);
    game=nullptr;
    printf("PASS: all %zu supported builds, hook installation, unknown build rejection, ABI/signature/vtable guards.\n",sizeof(supportedBuilds)/sizeof(supportedBuilds[0]));
    printf("PASS: 3,220 key/device cases; real hook/poll functions with simulated Steam slots, hot-swap and disconnect; native fallback; DLL loading, ordinal imports and Windows XInput forwarding.\n");
}
