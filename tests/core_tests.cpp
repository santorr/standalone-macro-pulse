#include "engine.hpp"
#include "preferences.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>
#include <map>
#include <stdexcept>
#include <vector>

using namespace pulse;
using Clock = std::chrono::steady_clock;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
bool done(Engine& engine, int timeoutMs = 2000) {
    auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
    while (Clock::now() < deadline) { if (engine.snapshot().state == RunState::Idle) { engine.stop(); return true; } Sleep(1); }
    engine.stop(); return false;
}
void modelTests() {
    int n = 0; uint16_t key = 0; uint8_t mods = 0; std::wstring error;
    require(parseNumber(L"-42", -100, 100, n) && n == -42, "negative coordinate");
    require(!parseNumber(L"3ms", 0, 100, n) && !parseNumber(L"99999999999999999999999", 0, 100, n), "numeric validation");
    require(parseKey(L" Ctrl + Shift + S ", key, mods) && key == 'S' && mods == 5, "chord parser");
    require(parseKey(L"F8", key, mods) && parseKey(L"Ctrl+F9", key, mods), "function keys supported; bindings reserved by UI");
    require(!parseKey(L"Ctrl+", key, mods), "empty chord");
    require(parseKey(L"Left", key, mods) && key == VK_LEFT, "navigation key");
    Step chord; chord.action = Action::Key; chord.key = 'S'; chord.modifiers = 5;
    Step move; move.action = Action::Move; move.x = -1920; move.y = 450;
    Macro macro{3, {chord, move}};
    auto file = std::filesystem::temp_directory_path() / (L"macropulse-test-" + std::to_wstring(GetCurrentProcessId()) + L"-é.mpulse");
    require(saveMacro(file, macro, error), "save macro");
    Macro loaded; require(loadMacro(file, loaded, error), "load macro");
    require(loaded.repeats == 3 && loaded.steps.size() == 2 && loaded.steps[1].x == -1920 && loaded.steps[0].modifiers == 5, "round trip");
    macro.repeats = 5; require(saveMacro(file, macro, error) && loadMacro(file, loaded, error) && loaded.repeats == 5, "atomic overwrite");
    const char* badFiles[] = {
        "MACROPULSE 2\n1 1\n0 100 0 0 0 0 65 0 1\n", "MACROPULSE 1\n1 10001\n", "MACROPULSE 1\n1 1\n0 -1 0 0 0 0 65 0 1\n",
        "MACROPULSE 1\n1 1\n99 100 0 0 0 0 65 0 1\n", "MACROPULSE 1\n1 1\n0 100 0 0 2 0 65 0 1\n",
        "MACROPULSE 1\n1 1\n2 100 0 0 0 0 255 0 1\n", "MACROPULSE 1\n1 1\n0 100 0 0 0 0 65 0 1\ntrailing",
        "MACROPULSE 1\n1 1\n3 100 0 0 0 0 65 1 1\n"
    };
    for (auto bad : badFiles) { { std::ofstream out(file); out << bad; } require(!loadMacro(file, loaded, error), "reject malformed file"); require(loaded.repeats == 5, "load is transactional"); }
    std::filesystem::remove(file);
    Macro empty; require(!validMacro(empty, error), "empty macro rejected");
    Step wait; wait.action = Action::Wait; wait.delayMs = 0;
    require(!validMacro({0, {wait}}, error), "unbounded zero-delay spin rejected");
    wait.delayMs = 1; require(validMacro({0, {wait}}, error), "bounded delay loop accepted");
}
void engineTests() {
    std::wstring error;
    std::vector<INPUT> events;
    Engine engine([&](UINT count, INPUT* in) { events.insert(events.end(), in, in + count); return count; });
    ClickConfig click; click.intervalMs = 2; click.count = 5;
    require(engine.startClicker(click, 0, error) && done(engine), "finite clicker completes");
    require(events.size() == 10 && engine.snapshot().actions == 5, "exact click count");
    for (size_t i = 0; i < events.size(); i += 2) require(events[i].mi.dwFlags == MOUSEEVENTF_LEFTDOWN && events[i + 1].mi.dwFlags == MOUSEEVENTF_LEFTUP, "paired mouse events");
    events.clear(); click.intervalMs = 60000; click.count = 0;
    require(engine.startClicker(click, 60000, error), "long countdown"); Sleep(10);
    auto start = Clock::now(); engine.stop();
    require(Clock::now() - start < std::chrono::milliseconds(500) && events.empty(), "interruptible countdown");
    Step down; down.action = Action::KeyDown; down.key = VK_CONTROL; down.delayMs = 0;
    Step wait; wait.action = Action::Wait; wait.delayMs = 60000;
    require(engine.startMacro({1, {down, wait}}, 0, error), "key hold macro starts");
    auto deadline = Clock::now() + std::chrono::seconds(1);
    while (engine.snapshot().actions < 1 && Clock::now() < deadline) Sleep(1);
    require(engine.snapshot().actions == 1, "key down reached");
    engine.stop(); require(events.size() == 2 && (events.back().ki.dwFlags & KEYEVENTF_KEYUP), "stop releases held key");
    events.clear();
    Step chord; chord.action = Action::Key; chord.delayMs = 0; chord.key = 'C'; chord.modifiers = 1;
    Step up = down; up.action = Action::KeyUp;
    require(engine.startMacro({2, {down, chord, up}}, 0, error) && done(engine), "chord macro completes");
    require(events.size() == 8 && engine.snapshot().cycles == 2 && engine.snapshot().actions == 6, "held modifier preserved across chord");
    require(events[0].ki.wVk == VK_CONTROL && events[1].ki.wVk == 'C' && events[2].ki.wVk == 'C' && events[3].ki.wVk == VK_CONTROL, "chord event order");
    events.clear();
    require(engine.startMacro({2, {down}}, 0, error) && done(engine) && events.size() == 4, "cycle boundary releases keys");
    events.clear();
    int calls = 0;
    Engine partial([&](UINT count, INPUT* in) { ++calls; UINT accepted = calls == 1 ? 1 : count; events.insert(events.end(), in, in + accepted); return accepted; });
    click.intervalMs = 1; click.count = 5;
    require(partial.startClicker(click, 0, error) && done(partial), "partial input stops engine");
    require(partial.snapshot().inputFailed && partial.snapshot().actions == 0 && events.size() == 2 && events.back().mi.dwFlags == MOUSEEVENTF_LEFTUP, "partial click cleaned up");
    Engine rejected([](UINT, INPUT*) { return 0U; });
    require(rejected.startMacro({1, {chord}}, 0, error) && done(rejected) && rejected.snapshot().inputFailed, "input rejection reported");
    click.intervalMs = 0; require(!engine.startClicker(click, 0, error), "invalid click config rejected");
    // Timing is intentionally a loose lower-bound assertion, not an OS latency guarantee.
    wait.delayMs = 30; auto began = Clock::now();
    require(engine.startMacro({1, {wait, wait}}, 0, error) && done(engine), "pause macro");
    require(Clock::now() - began >= std::chrono::milliseconds(58), "pauses respect minimum duration");
    for (int i = 0; i < 20; ++i) { click.intervalMs = 1; click.count = 1; require(engine.startClicker(click, 0, error) && done(engine), "repeated restart"); }
}
void preferencesTests() {
    std::wstring error; Hotkey chord;
    require(parseHotkey(L"Ctrl+Shift+F6", chord) && chord.modifiers == 5 && chord.key == VK_F6, "hotkey parser");
    require(nativeModifiers(chord) == (MOD_CONTROL | MOD_SHIFT), "native modifier translation");
    for (const auto* valid : {L"A", L"7", L"Shift+A", L"Space", L"Enter", L"Left", L"Num0", L"Ctrl+NumAdd", L"OEM102"}) require(parseHotkey(valid, chord), "single keys and combinations supported");
    for (const auto* invalid : {L"Win+F6", L"F12", L"Alt+F4", L"Ctrl", L"Shift", L"Alt", L"Ctrl+", L"?"}) require(!parseHotkey(invalid, chord), "reject unsafe or reserved global key");
    for (int vk = 1; vk < 256; ++vk) {
        auto name = keyName(static_cast<uint16_t>(vk), 5);
        if (name.back() == L'?') continue;
        uint16_t parsed; uint8_t mods;
        require(parseKey(name, parsed, mods) && parsed == vk && mods == 5, "all supported captured keys round trip");
    }
    auto duplicate = DefaultHotkeys; duplicate[1] = duplicate[0]; require(!validHotkeys(duplicate, error), "duplicate global keys");
    Step step; step.action = Action::Key; step.key = VK_F6;
    require(conflictsWithHotkeys(step, DefaultHotkeys), "macro key collision");
    auto changed = DefaultHotkeys; changed[0] = {VK_F10, 1};
    require(!conflictsWithHotkeys(step, changed), "former hotkey now usable by macro");
    step.key = VK_F10; step.modifiers = 0; require(conflictsWithHotkeys(step, changed), "held modifiers cannot bypass key protection");
    Preferences prefs; prefs.click = {37, 123, Button::Right, true, -1920, 500}; prefs.startDelayMs = 2345; prefs.page = 2;
    prefs.hotkeys = changed; prefs.lastMacro = L"C:\\Mes macros\\équipe 日本語.mpulse";
    auto file = std::filesystem::temp_directory_path() / (L"macropulse-prefs-" + std::to_wstring(GetCurrentProcessId()) + L".dat");
    require(savePreferences(file, prefs, error), "save preferences");
    Preferences loaded; require(loadPreferences(file, loaded, error), "load preferences");
    require(loaded.click.intervalMs == 37 && loaded.click.count == 123 && loaded.click.button == Button::Right && loaded.click.fixed && loaded.click.x == -1920 && loaded.click.y == 500 && loaded.startDelayMs == 2345 && loaded.page == 2 && loaded.lastMacro == prefs.lastMacro && loaded.hotkeys == changed, "all preferences round trip with Unicode");
    prefs.click.intervalMs = 42; require(savePreferences(file, prefs, error) && loadPreferences(file, loaded, error) && loaded.click.intervalMs == 42, "replace existing preferences");
    for (const char* bad : {"MACROPULSE_SETTINGS 2", "MACROPULSE_SETTINGS 1\n0 0 0 0 0 0 1500 0\n", "MACROPULSE_SETTINGS 1\n100 0 0 0 0 0 1500 0\n117 0\n117 0\n119 0\n120 0\n\"\"", "MACROPULSE_SETTINGS 1\n100 0 0 0 0 0 1500 0\n117 0\n118 0\n119 0\n120 0\n\"\"\ntrailing"}) {
        { std::ofstream out(file); out << bad; }
        require(!loadPreferences(file, loaded, error) && loaded.click.intervalMs == 42, "bad preferences do not partially replace state");
    }
    std::filesystem::remove(file);
    std::map<int, Hotkey> registered; Hotkey blocked{VK_F11, 2}; int calls = 0;
    {
        HotkeyRegistry registry([&](int id, const Hotkey& key) {
            ++calls;
            if (key == blocked || std::any_of(registered.begin(), registered.end(), [&](const auto& p) { return p.second == key; })) return false;
            registered[id] = key; return true;
        }, [&](int id) { registered.erase(id); });
        registry.initialize(DefaultHotkeys); require(registered.size() == 4 && registry.active(2), "register initial bindings");
        auto old = registered; auto replacement = DefaultHotkeys; replacement[0] = {VK_F10, 1}; replacement[1] = blocked;
        size_t failed = 0; require(!registry.apply(replacement, failed) && failed == 1 && registered == old && registry.active(2), "conflict rolls back only new bindings; stop preserved");
        auto swapped = DefaultHotkeys; std::swap(swapped[0], swapped[2]); auto before = calls;
        require(registry.apply(swapped, failed) && calls == before && registry.actionFor(0x512) == 0 && registry.actionFor(0x510) == 2, "swapping bindings reuses registrations without gaps");
        replacement[1] = {VK_F11, 1}; require(registry.apply(replacement, failed) && registered.size() == 4, "replace all bindings transactionally");
    }
    require(registered.empty(), "registry cleans up all bindings");
    {
        bool stopBlocked = true;
        HotkeyRegistry registry([&](int, const Hotkey& key) { return !(stopBlocked && key.key == VK_F8); }, [](int) {});
        registry.initialize(DefaultHotkeys); require(!registry.active(2) && registry.active(0), "startup conflict is reported");
        stopBlocked = false; size_t failed = 0;
        require(registry.apply(DefaultHotkeys, failed) && registry.active(2), "retry same settings after conflict clears");
    }
}
void nativeHotkeyTests() {
    struct HiddenTarget {
        HWND window = CreateWindowExW(0, L"STATIC", L"MacroPulse registration test", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
        ~HiddenTarget() { if (window) { UnregisterHotKey(window, 0x710); DestroyWindow(window); } }
    } first, second;
    require(first.window && second.window, "native hotkey test windows");
    UINT modifiers = MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT;
    for (UINT key = VK_F13; key <= VK_F24; ++key) {
        if (!RegisterHotKey(first.window, 0x710, modifiers, key)) continue;
        require(!RegisterHotKey(second.window, 0x710, modifiers, key), "Windows rejects an already-owned hotkey");
        require(UnregisterHotKey(first.window, 0x710) != FALSE, "release native binding");
        require(RegisterHotKey(second.window, 0x710, modifiers, key) != FALSE, "native binding can be reassigned after release");
        std::cout << "Native hotkey conflict/release verified (no input injected).\n"; return;
    }
    std::cout << "Native hotkey check skipped: all test combinations are occupied.\n";
}
void benchmark() {
    for (uint32_t interval : {1U, 5U, 10U}) {
        std::vector<Clock::time_point> times;
        Engine engine([&](UINT count, INPUT*) { times.push_back(Clock::now()); return count; });
        ClickConfig config; config.intervalMs = interval; config.count = 200;
        std::wstring error; require(engine.startClicker(config, 0, error) && done(engine, 10000), "benchmark completes");
        std::vector<double> intervals;
        for (size_t i = 1; i < times.size(); ++i) intervals.push_back(std::chrono::duration<double, std::milli>(times[i] - times[i - 1]).count());
        std::sort(intervals.begin(), intervals.end());
        auto seconds = std::chrono::duration<double>(times.back() - times.front()).count();
        std::cout << "Requested " << interval << " ms | " << (times.size() - 1) / seconds << " actions/s | median " << intervals[intervals.size() / 2]
                  << " ms | p95 " << intervals[intervals.size() * 95 / 100] << " ms | max " << intervals.back() << " ms\n";
    }
    std::cout << "Scheduler benchmark only: mock input sink, no real clicks. Not a target-application throughput measurement.\n";
}
int main(int argc, char**) {
    try { modelTests(); engineTests(); preferencesTests(); nativeHotkeyTests(); std::cout << "All core tests passed.\n"; if (argc > 1) benchmark(); return 0; }
    catch (const std::exception& e) { std::cerr << "FAILED: " << e.what() << '\n'; return 1; }
}
