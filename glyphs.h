#pragma once
#include <cstdint>

// Layout and key values verified against this game's shipped debug symbols.
struct Prompt { const char* texture; const char* label; float scale; uint32_t padding; };
static_assert(sizeof(Prompt) == 24);
inline bool IsPlayStation(int type) { return type == 5 || type == 12 || type == 13; }
inline const char* SonyTexture(int key) {
    static constexpr const char* buttons[] = {
        "console_ps5_leftstick_up_button_ui", "console_ps5_leftstick_down_button_ui",
        "console_ps5_leftstick_right_button_ui", "console_ps5_leftstick_left_button_ui",
        "console_ps5_rightstick_up_button_ui", "console_ps5_rightstick_down_button_ui",
        "console_ps5_rightstick_right_button_ui", "console_ps5_rightstick_left_button_ui",
        "console_ps5_cross_button_ui", "console_ps5_circle_button_ui",
        "console_ps5_square_button_ui", "console_ps5_triangle_button_ui",
        "console_ps5_dpad_up_button_ui", "console_ps5_dpad_down_button_ui",
        "console_ps5_dpad_right_button_ui", "console_ps5_dpad_left_button_ui",
        "console_ps5_leftstick_click_button_ui", "console_ps5_rightstick_click_button_ui",
        "console_ps5_l1_button_ui", "console_ps5_r1_button_ui",
        "console_ps5_l2_button_ui", "console_ps5_r2_button_ui",
        "console_ps5_share_button_ui", "console_ps5_menu_button_ui"
    };
    if (key >= 125 && key <= 148) return buttons[key - 125];
    if (key == 152) return "console_ps5_share_button_ui";
    if (key == 153) return "console_ps5_leftstick_all_button_ui";
    if (key == 154) return "console_ps5_rightstick_all_button_ui";
    return nullptr;
}
inline bool ApplySony(Prompt& prompt, int key, int type) {
    const char* texture = IsPlayStation(type) ? SonyTexture(key) : nullptr;
    if (!texture) return false;
    prompt.texture = texture;
    prompt.label = nullptr;
    prompt.scale = (key >= 133 && key <= 136) ? 1.0f : 0.9f;
    return true;
}
