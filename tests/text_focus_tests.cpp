#include "text_focus.hpp"
#include "preferences.hpp"
#include <wrl.h>
#include <future>
#include <map>
#include <iostream>
#include <stdexcept>
#include <algorithm>

using namespace pulse;
void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
void registryTests() {
    Hotkeys keys = DefaultHotkeys; keys[0] = {'G', 0}; keys[1] = {'M', 0}; keys[3] = {'P', 0};
    std::map<int, Hotkey> bindings; bool conflict = false; int releasesOfStop = 0;
    HotkeyRegistry registry([&](int id, const Hotkey& key) {
        if (conflict && key == keys[0]) return false;
        bindings[id] = key; return true;
    }, [&](int id) { if (bindings.at(id) == keys[2]) ++releasesOfStop; bindings.erase(id); });
    registry.initialize(keys); check(bindings.size() == 4, "initial bindings");
    auto original = bindings;
    registry.suspendLaunchShortcuts();
    check(!registry.active(0) && !registry.active(1) && registry.active(2) && !registry.active(3), "typing releases start and capture, keeps stop");
    check(bindings.size() == 1 && bindings.begin()->second == keys[2] && !releasesOfStop, "G is actually unregistered, not merely ignored");
    registry.suspendLaunchShortcuts(); check(bindings.size() == 1, "repeated focus events do not disturb stop");
    conflict = true; registry.resumeMissing(keys);
    check(!registry.active(0) && registry.active(1) && registry.active(2) && registry.active(3), "handle a shortcut claimed by another app while typing");
    check(!releasesOfStop, "emergency stop has no registration gap");
    conflict = false; registry.resumeMissing(keys); check(registry.active(0), "conflicted binding can be acquired later");
    auto swapped = keys; std::swap(swapped[0], swapped[2]); size_t failed = 0;
    check(registry.apply(swapped, failed), "apply swapped start and stop");
    registry.suspendLaunchShortcuts();
    check(bindings.size() == 1 && bindings.begin()->second == swapped[2], "protect stop by action, not hard-coded registration id");
    registry.resumeMissing(swapped); check(bindings.size() == 4, "restore swapped bindings with unique ids");
    // The cleanup callback's stop counter uses the original key, deliberately
    // independent from the swapped action mapping.
}
void nativeRegistrationTests() {
    HWND one = CreateWindowExW(0, L"STATIC", L"hotkey test", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
    HWND two = CreateWindowExW(0, L"STATIC", L"hotkey test", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
    check(one && two, "create isolated registration windows");
    {
        HotkeyRegistry registry([&](int id, const Hotkey& key) { return RegisterHotKey(one, id, MOD_NOREPEAT | nativeModifiers(key), key.key) != FALSE; },
                                [&](int id) { UnregisterHotKey(one, id); });
        // Never inject input or reserve a user's ordinary typing keys in tests.
        Hotkeys keys{{{VK_F21, 7}, {VK_F22, 7}, {VK_F23, 7}, {VK_F24, 7}}}; registry.initialize(keys);
        if (registry.active(0) && registry.active(2)) {
            registry.suspendLaunchShortcuts();
            check(RegisterHotKey(two, 1, nativeModifiers(keys[0]), keys[0].key) != FALSE, "Windows really releases the launch shortcut");
            check(!RegisterHotKey(two, 2, nativeModifiers(keys[2]), keys[2].key), "Windows keeps the stop shortcut reserved");
            UnregisterHotKey(two, 1); registry.resumeMissing(keys);
            check(registry.active(0) && registry.active(2), "native shortcuts restored after typing");
        } else std::cout << "Native shortcut check skipped: test combinations are already used.\n";
    }
    DestroyWindow(two); DestroyWindow(one);
}
int main() {
    try {
        registryTests(); nativeRegistrationTests();
        HWND parent = CreateWindowExW(0, L"STATIC", L"Isolated text focus test", WS_OVERLAPPEDWINDOW, 0, 0, 400, 300, nullptr, nullptr, nullptr, nullptr);
        HWND edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 100, 30, parent, nullptr, nullptr, nullptr);
        HWND readonly = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_READONLY, 0, 40, 100, 30, parent, nullptr, nullptr, nullptr);
        HWND password = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_PASSWORD, 0, 80, 100, 30, parent, nullptr, nullptr, nullptr);
        HWND button = CreateWindowExW(0, L"BUTTON", L"Test", WS_CHILD, 0, 120, 100, 30, parent, nullptr, nullptr, nullptr);
        check(parent && edit && readonly && password && button, "create controls");
        check(nativeTextFocus(edit) == TextFocus::Editing && nativeTextFocus(password) == TextFocus::Editing, "native edit and password fields");
        check(nativeTextFocus(readonly) == TextFocus::Other, "read-only native text does not pause shortcuts");
        EnableWindow(edit, FALSE); check(nativeTextFocus(edit) == TextFocus::Other, "disabled edit is not writable"); EnableWindow(edit, TRUE);
        check(nativeTextFocus(nullptr) == TextFocus::Unknown, "unknown focus is explicit");
        auto test = std::async(std::launch::async, [=] {
            check(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)), "initialize UIA MTA");
            {
                Microsoft::WRL::ComPtr<IUIAutomation> automation;
                check(SUCCEEDED(CoCreateInstance(CLSID_CUIAutomation8, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&automation))), "create Windows UIA client");
                Microsoft::WRL::ComPtr<IUIAutomation2> options;
                if (SUCCEEDED(automation.As(&options))) { options->put_ConnectionTimeout(500); options->put_TransactionTimeout(500); }
                for (auto [window, expected] : {std::pair{edit, TextFocus::Editing}, {password, TextFocus::Editing}, {readonly, TextFocus::Other}, {button, TextFocus::Other}}) {
                    Microsoft::WRL::ComPtr<IUIAutomationElement> element;
                    check(SUCCEEDED(automation->ElementFromHandle(window, &element)), "inspect actual Windows control");
                    check(automationTextFocus(element.Get()) == expected, "UIA classifies editable, password, readonly and button without reading text");
                }
            }
            CoUninitialize();
        });
        // Keep the provider's window thread responsive while the MTA queries it.
        while (test.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready) {
            MSG msg{}; while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        }
        test.get(); DestroyWindow(parent);
        std::cout << "Typing protection tests passed; no keyboard input was injected.\n"; return 0;
    } catch (const std::exception& ex) { std::cerr << ex.what() << '\n'; return 1; }
}
