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
int main() {
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
    nativeObject[0x1d9]=1;
    WindowsPrompt(nativeObject,&output,0,133);
    Check(!strcmp(output.texture,SonyTexture(133)),"single native gamepad Steam fallback");
    nativeObject[0x1d9+0x108]=1;
    WindowsPrompt(nativeObject,&output,0,133);
    Check(!strcmp(output.texture,"original"),"mixed native gamepads do not guess slot identities");
    HMODULE proxy = LoadLibraryW(L"build\\XInput1_4.dll"); Check(proxy != nullptr,"load proxy");
    auto state = reinterpret_cast<decltype(&XInputGetState)>(GetProcAddress(proxy,MAKEINTRESOURCEA(2)));
    auto vibration = reinterpret_cast<decltype(&XInputSetState)>(GetProcAddress(proxy,MAKEINTRESOURCEA(3)));
    Check(state && vibration,"game imports ordinal 2 and 3");
    XINPUT_STATE s{}; XINPUT_VIBRATION v{};
    Check(state(999,&s)==ERROR_BAD_ARGUMENTS || state(999,&s)==ERROR_DEVICE_NOT_CONNECTED,"system XInput forwarding");
    Check(vibration(999,&v)!=ERROR_SUCCESS,"system vibration forwarding");
    printf("PASS: 3,220 key/device cases; real hook/poll functions with simulated Steam slots, hot-swap and disconnect; native fallback; DLL loading, ordinal imports and Windows XInput forwarding.\n");
}
