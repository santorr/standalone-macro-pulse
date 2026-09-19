#pragma once
#include "preferences.hpp"
#include <optional>
#include <map>
#include <mutex>
#include <thread>

namespace pulse {
// Wheel directions are shortcut-only codes, outside the Win32 virtual-key range.
inline constexpr uint16_t WheelUp = 0x100, WheelDown = 0x101, WheelLeft = 0x102, WheelRight = 0x103;
bool isMouseInput(uint16_t key);
std::wstring hotkeyName(const Hotkey& key);
struct MouseInput { uint16_t key; bool down; int delta = 0; };
std::optional<MouseInput> decodeMouseInput(UINT message, DWORD data);
// A pure event filter shared by the native hook and regression tests.
class MouseShortcutFilter {
public:
    std::optional<Hotkey> process(UINT message, DWORD data, DWORD flags, uint8_t modifiers, ULONGLONG now);
private:
    std::array<bool, 7> held_{};
    Hotkey wheel_{}, lastWheelKey_{};
    int wheelDelta_ = 0;
    ULONGLONG lastWheel_ = 0, lastWheelEvent_ = 0;
    bool wheelTriggered_ = false;
};
class MouseShortcuts {
public:
    explicit MouseShortcuts(HWND owner) : owner_(owner) {}
    ~MouseShortcuts();
    bool acquire(int id, const Hotkey& key);
    bool release(int id);
private:
    HWND owner_;
    std::thread worker_;
    DWORD threadId_ = 0;
    std::mutex mutex_;
    bool start();
    std::map<int, Hotkey> bindings_;
    MouseShortcutFilter filter_;
    static thread_local MouseShortcuts* current_;
    static LRESULT CALLBACK hookProc(int code, WPARAM message, LPARAM data);
};
}
