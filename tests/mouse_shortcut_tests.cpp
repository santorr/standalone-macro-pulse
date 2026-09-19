#include "mouse_shortcuts.hpp"
#include <iostream>
#include <stdexcept>
#include <map>
using namespace pulse;
void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
DWORD mouseData(int high) { return static_cast<DWORD>(static_cast<WORD>(high)) << 16; }
void modelTests() {
    Hotkey parsed; std::wstring error;
    for (uint16_t code : {uint16_t(VK_LBUTTON), uint16_t(VK_RBUTTON), uint16_t(VK_MBUTTON), uint16_t(VK_XBUTTON1), uint16_t(VK_XBUTTON2), WheelUp, WheelDown, WheelLeft, WheelRight}) {
        for (uint8_t mods = 0; mods <= 7; ++mods) {
            Hotkey expected{code, mods};
            check(parseHotkey(hotkeyName(expected), parsed) && parsed == expected, "every mouse input and modifier combination round-trips");
            auto keys = DefaultHotkeys; keys[0] = expected;
            check(validHotkeys(keys, error), "valid mouse shortcut");
            keys[1] = expected; check(!validHotkeys(keys, error), "duplicate mouse shortcut rejected");
        }
        Step step; step.action = Action::Key; step.key = code;
        check(!validStep(step, error), "mouse input must not become a keyboard macro event");
    }
    check(parseHotkey(L" shift + ctrl + Mouse4 ", parsed) && parsed == Hotkey{VK_XBUTTON1, 5}, "modifier order and whitespace");
    for (auto invalid : {L"Win+Mouse4", L"Mouse6", L"Wheel", L"MouseLeft+A", L"Ctrl+"}) check(!parseHotkey(invalid, parsed), "unsupported combinations rejected");
    for (auto name : {L"VolumeMute", L"MediaPlayPause", L"BrowserBack", L"Ctrl+MediaNext"}) check(parseHotkey(name, parsed), "media/browser shortcut support");
    Preferences p; p.hotkeys = {{{VK_LBUTTON, 0}, {VK_XBUTTON1, 5}, {VK_XBUTTON2, 0}, {WheelDown, 2}}};
    auto file = std::filesystem::temp_directory_path() / (L"macropulse-mouse-" + std::to_wstring(GetCurrentProcessId()) + L".dat");
    check(savePreferences(file, p, error), "save mixed mouse settings");
    Preferences loaded; check(loadPreferences(file, loaded, error) && loaded.hotkeys == p.hotkeys, "mouse and wheel settings persist exactly");
    std::filesystem::remove(file);
}
void eventTests() {
    MouseShortcutFilter f;
    check(!f.process(WM_MOUSEMOVE, 0, 0, 0, 0), "movement never triggers");
    check(!f.process(WM_LBUTTONDOWN, 0, LLMHF_INJECTED, 0, 0), "auto-clicker input ignored");
    check(!f.process(WM_XBUTTONDOWN, mouseData(XBUTTON1), LLMHF_LOWER_IL_INJECTED, 0, 0), "lower-integrity injection ignored");
    check(f.process(WM_LBUTTONDOWN, 0, 0, 0, 0) == Hotkey{VK_LBUTTON, 0}, "real left click triggers");
    check(!f.process(WM_LBUTTONDOWN, 0, 0, 0, 1), "held button cannot retrigger");
    check(!f.process(WM_LBUTTONUP, 0, LLMHF_INJECTED, 0, 2), "injected release cannot release a held physical button");
    check(!f.process(WM_LBUTTONDOWN, 0, 0, 0, 3), "physical state survives injected release");
    check(!f.process(WM_LBUTTONUP, 0, 0, 0, 4), "release does not trigger");
    check(f.process(WM_LBUTTONDOWN, 0, 0, 5, 5) == Hotkey{VK_LBUTTON, 5}, "Ctrl Shift click");
    check(f.process(WM_RBUTTONDOWN, 0, 0, 2, 6) == Hotkey{VK_RBUTTON, 2}, "Alt right click");
    check(f.process(WM_MBUTTONDOWN, 0, 0, 0, 7) == Hotkey{VK_MBUTTON, 0}, "middle click");
    check(f.process(WM_XBUTTONDOWN, mouseData(XBUTTON1), 0, 0, 8) == Hotkey{VK_XBUTTON1, 0}, "first side button");
    check(f.process(WM_XBUTTONDOWN, mouseData(XBUTTON2), 0, 7, 9) == Hotkey{VK_XBUTTON2, 7}, "second side button plus all modifiers");
    check(!f.process(WM_XBUTTONDOWN, mouseData(3), 0, 0, 10), "invalid side button ignored");
    check(!f.process(WM_RBUTTONUP, 0, 0, 0, 11), "right release");
    check(!f.process(WM_RBUTTONDOWN, 0, 0, 8, 12), "Windows combinations do not trigger");
    check(!f.process(WM_MOUSEWHEEL, mouseData(120), LLMHF_INJECTED, 0, 20), "macro scrolling ignored");
    check(!f.process(WM_MOUSEWHEEL, mouseData(30), 0, 0, 21), "high resolution partial scroll");
    check(f.process(WM_MOUSEWHEEL, mouseData(90), 0, 0, 22) == Hotkey{WheelUp, 0}, "high resolution scroll accumulated");
    check(!f.process(WM_MOUSEWHEEL, mouseData(120), 0, 0, 23), "fast wheel burst cannot rapidly toggle");
    check(f.process(WM_MOUSEWHEEL, mouseData(-120), 0, 1, 300) == Hotkey{WheelDown, 1}, "wheel down plus Ctrl");
    check(f.process(WM_MOUSEHWHEEL, mouseData(-120), 0, 0, 600) == Hotkey{WheelLeft, 0}, "horizontal left");
    check(f.process(WM_MOUSEHWHEEL, mouseData(120), 0, 0, 900) == Hotkey{WheelRight, 0}, "horizontal right");
    check(f.process(WM_MOUSEHWHEEL, mouseData(-120), 0, 0, 901) == Hotkey{WheelLeft, 0}, "a different wheel shortcut can stop immediately");
    check(!f.process(WM_MOUSEWHEEL, mouseData(60), 0, 0, 1200), "partial delta held");
    check(!f.process(WM_MOUSEWHEEL, mouseData(60), 0, 1, 1201), "changing modifiers resets partial delta");
    check(!f.process(WM_MOUSEWHEEL, mouseData(60), 0, 1, 1802), "old partial delta expires");
}
void registryTests() {
    HWND window = CreateWindowExW(0, L"STATIC", L"Mouse shortcut test", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
    check(window != nullptr, "create isolated target");
    {
        MouseShortcuts mouse(window);
        check(mouse.acquire(1, {VK_XBUTTON1, 7}), "install actual hook on its own thread");
        check(!mouse.acquire(2, {VK_XBUTTON1, 7}), "duplicate native binding rejected");
        check(mouse.release(1) && !mouse.release(1), "native binding released exactly once");
        check(mouse.acquire(2, {WheelUp, 7}), "wheel binding uses same native listener");
    }
    // Destruction joins the hook thread; a second lifetime must still work.
    { MouseShortcuts mouse(window); check(mouse.acquire(1, {VK_XBUTTON2, 7}), "recreate listener after shutdown"); }
    DestroyWindow(window);
    std::map<int, Hotkey> active; Hotkey blocked{VK_F11, 0};
    HotkeyRegistry registry([&](int id, const Hotkey& key) { if (key == blocked) return false; active[id] = key; return true; }, [&](int id) { active.erase(id); });
    auto keys = DefaultHotkeys; keys[0] = {VK_LBUTTON, 0}; keys[2] = {VK_XBUTTON2, 0}; registry.initialize(keys);
    registry.suspendLaunchShortcuts(); check(active.size() == 1 && active.begin()->second == keys[2], "mouse emergency stop survives typing pause");
    registry.resumeMissing(keys); auto before = active;
    auto changed = keys; changed[0] = {WheelUp, 0}; changed[1] = blocked; size_t failed = 0;
    check(!registry.apply(changed, failed) && failed == 1 && active == before, "mixed mouse keyboard conflict rolls back");
    std::swap(keys[0], keys[2]); check(registry.apply(keys, failed), "swap mouse start and stop without gaps");
    registry.suspendLaunchShortcuts(); check(active.size() == 1 && active.begin()->second == keys[2], "stop action protected after swap");
}
int main() {
    try { modelTests(); eventTests(); registryTests(); std::cout << "Mouse shortcuts passed; no input was injected.\n"; return 0; }
    catch (const std::exception& ex) { std::cerr << ex.what() << '\n'; return 1; }
}
