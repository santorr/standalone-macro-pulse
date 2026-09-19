#pragma once
#include <windows.h>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace pulse {
enum class Action : int { Click, Move, Key, KeyDown, KeyUp, Wait, Scroll };
enum class Button : int { Left, Right, Middle };
struct Step {
    Action action = Action::Click;
    uint32_t delayMs = 100;
    int x = 0, y = 0;
    bool fixed = false;
    Button button = Button::Left;
    uint16_t key = 'A';
    uint8_t modifiers = 0; // 1 Ctrl, 2 Alt, 4 Shift, 8 Win
    int wheel = 1;
};
struct Macro {
    uint32_t repeats = 1; // 0 = until stopped
    std::vector<Step> steps;
};
inline constexpr size_t MaxSteps = 10000;
inline constexpr uint32_t MaxDelayMs = 86400000;
bool validStep(const Step& step, std::wstring& error);
bool validMacro(const Macro& macro, std::wstring& error);
bool parseNumber(const std::wstring& text, int minimum, int maximum, int& value);
bool parseKey(const std::wstring& text, uint16_t& key, uint8_t& modifiers);
std::wstring keyName(uint16_t key, uint8_t modifiers = 0);
std::wstring actionName(Action action);
std::wstring describe(const Step& step);
bool saveMacro(const std::filesystem::path& path, const Macro& macro, std::wstring& error);
bool loadMacro(const std::filesystem::path& path, Macro& macro, std::wstring& error);
}
