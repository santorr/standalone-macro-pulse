#include "mouse_shortcuts.hpp"
#include <algorithm>
#include <future>

namespace pulse {
bool isMouseInput(uint16_t key) {
    return key == VK_LBUTTON || key == VK_RBUTTON || key == VK_MBUTTON || key == VK_XBUTTON1 || key == VK_XBUTTON2 ||
        (key >= WheelUp && key <= WheelRight);
}
std::wstring hotkeyName(const Hotkey& key) {
    const std::pair<int, const wchar_t*> names[] = {
        {VK_LBUTTON, L"MouseLeft"}, {VK_RBUTTON, L"MouseRight"}, {VK_MBUTTON, L"MouseMiddle"},
        {VK_XBUTTON1, L"Mouse4"}, {VK_XBUTTON2, L"Mouse5"},
        {WheelUp, L"WheelUp"}, {WheelDown, L"WheelDown"}, {WheelLeft, L"WheelLeft"}, {WheelRight, L"WheelRight"}
    };
    for (auto [code, name] : names) if (key.key == code) {
        auto prefix = keyName('A', key.modifiers); prefix.pop_back(); return prefix + name;
    }
    return keyName(key.key, key.modifiers);
}
std::optional<MouseInput> decodeMouseInput(UINT message, DWORD data) {
    switch (message) {
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: return MouseInput{VK_LBUTTON, true};
    case WM_LBUTTONUP: return MouseInput{VK_LBUTTON, false};
    case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: return MouseInput{VK_RBUTTON, true};
    case WM_RBUTTONUP: return MouseInput{VK_RBUTTON, false};
    case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: return MouseInput{VK_MBUTTON, true};
    case WM_MBUTTONUP: return MouseInput{VK_MBUTTON, false};
    case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK: case WM_XBUTTONUP:
        if (HIWORD(data) != XBUTTON1 && HIWORD(data) != XBUTTON2) return {};
        return MouseInput{static_cast<uint16_t>(HIWORD(data) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2), message != WM_XBUTTONUP};
    case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL: {
        int delta = static_cast<short>(HIWORD(data)); if (!delta) return {};
        return MouseInput{static_cast<uint16_t>(message == WM_MOUSEWHEEL ? (delta > 0 ? WheelUp : WheelDown) : (delta > 0 ? WheelRight : WheelLeft)), true, delta};
    }
    default: return {};
    }
}
std::optional<Hotkey> MouseShortcutFilter::process(UINT message, DWORD data, DWORD flags, uint8_t modifiers, ULONGLONG now) {
    // Includes SendInput from this app: generated clicks must never toggle it.
    if (flags & (LLMHF_INJECTED | LLMHF_LOWER_IL_INJECTED)) return {};
    auto input = decodeMouseInput(message, data); if (!input) return {};
    Hotkey key{input->key, modifiers};
    if (input->delta) {
        if (key != wheel_ || now - lastWheelEvent_ > 500) wheelDelta_ = 0;
        wheel_ = key; lastWheelEvent_ = now; wheelDelta_ += std::abs(input->delta);
        if (wheelDelta_ < WHEEL_DELTA) return {};
        wheelDelta_ %= WHEEL_DELTA;
        if (wheelTriggered_ && key == lastWheelKey_ && now - lastWheel_ < 250) return {};
        wheelTriggered_ = true; lastWheel_ = now; lastWheelKey_ = key;
    } else {
        bool repeated = held_[input->key]; held_[input->key] = input->down;
        if (!input->down || repeated) return {};
    }
    if (modifiers > 7) return {}; // Preserve Windows combinations.
    return key;
}
thread_local MouseShortcuts* MouseShortcuts::current_ = nullptr;
MouseShortcuts::~MouseShortcuts() {
    if (worker_.joinable()) { PostThreadMessageW(threadId_, WM_QUIT, 0, 0); worker_.join(); }
}
bool MouseShortcuts::start() {
    if (worker_.joinable()) return true;
    std::promise<bool> ready; auto started = ready.get_future();
    try {
        worker_ = std::thread([this, ready = std::move(ready)]() mutable {
            threadId_ = GetCurrentThreadId();
            MSG msg{}; PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
            current_ = this;
            auto hook = SetWindowsHookExW(WH_MOUSE_LL, hookProc, GetModuleHandleW(nullptr), 0);
            ready.set_value(hook != nullptr);
            if (hook) {
                // Independent pump keeps the hook responsive during dialogs,
                // disk writes and engine shutdown on the interface thread.
                while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
                UnhookWindowsHookEx(hook);
            }
            current_ = nullptr;
        });
    } catch (...) { return false; }
    if (started.get()) return true;
    worker_.join(); return false;
}
bool MouseShortcuts::acquire(int id, const Hotkey& key) {
    if (!isMouseInput(key.key) || key.modifiers > 7 || !start()) return false;
    std::lock_guard lock(mutex_);
    if (bindings_.contains(id) || std::any_of(bindings_.begin(), bindings_.end(), [&](const auto& item) { return item.second == key; })) return false;
    bindings_.emplace(id, key); return true;
}
bool MouseShortcuts::release(int id) { std::lock_guard lock(mutex_); return bindings_.erase(id) != 0; }
LRESULT CALLBACK MouseShortcuts::hookProc(int code, WPARAM message, LPARAM data) {
    auto self = current_;
    if (code == HC_ACTION && self) {
        const auto& event = *reinterpret_cast<const MSLLHOOKSTRUCT*>(data);
        if ((event.flags & (LLMHF_INJECTED | LLMHF_LOWER_IL_INJECTED)) || !decodeMouseInput(static_cast<UINT>(message), event.mouseData))
            return CallNextHookEx(nullptr, code, message, data);
        uint8_t mods = ((GetAsyncKeyState(VK_CONTROL) & 0x8000) ? 1 : 0) |
            ((GetAsyncKeyState(VK_MENU) & 0x8000) ? 2 : 0) | ((GetAsyncKeyState(VK_SHIFT) & 0x8000) ? 4 : 0) |
            (((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000) ? 8 : 0);
        auto key = self->filter_.process(static_cast<UINT>(message), event.mouseData, event.flags, mods, GetTickCount64());
        if (key) {
            std::lock_guard lock(self->mutex_);
            for (const auto& [id, binding] : self->bindings_) if (binding == *key) {
                // Queue only; no UI, engine work, or provider calls inside the hook.
                PostMessageW(self->owner_, WM_HOTKEY, id, MAKELPARAM(nativeModifiers(binding), binding.key)); break;
            }
        }
    }
    // Mouse shortcuts preserve ordinary clicks/scrolling, including emergency stop.
    return CallNextHookEx(nullptr, code, message, data);
}
}
