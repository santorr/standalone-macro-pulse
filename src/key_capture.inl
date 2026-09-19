// Capture only messages addressed to this application, without a keyboard hook.
void App::beginTextInput(HWND input) {
    if (!shortcutRegistry || keyCaptureTarget || engine.snapshot().state != RunState::Idle) return;
    textInput = input; shortcutsSuspended = true; captureHeld.fill(false);
    shortcutRegistry->clear();
}
void App::beginKeyCapture(int id) {
    if (engine.snapshot().state != RunState::Idle || shortcutsSuspended) return;
    SetFocus(control(id));
    keyCaptureTarget = id; shortcutsSuspended = true; loneModifier = 0; captureHeld.fill(false);
    if (!smoke) for (int key : {VK_CONTROL, VK_MENU, VK_SHIFT, VK_LWIN, VK_RWIN}) captureHeld[key] = (GetAsyncKeyState(key) & 0x8000) != 0;
    shortcutRegistry->clear();
    notice = id == StepKey ? L"Appuyez sur vos touches · Cliquez ailleurs pour annuler." : L"Appuyez sur vos touches · Échap pour annuler.";
    InvalidateRect(control(id), nullptr, FALSE); InvalidateRect(hwnd, nullptr, FALSE);
}
void App::endKeyCapture(const std::wstring& message) {
    int previous = keyCaptureTarget; keyCaptureTarget = 0;
    notice = message;
    if (previous) InvalidateRect(control(previous), nullptr, FALSE);
    InvalidateRect(hwnd, nullptr, FALSE);
    // Resume only after release, so the captured key cannot trigger an action.
}
void App::resumeShortcuts() {
    if (!shortcutsSuspended || keyCaptureTarget || textInput) return;
    if (!smoke) for (int key = VK_BACK; key < 256; ++key) captureHeld[key] = (GetAsyncKeyState(key) & 0x8000) != 0;
    if (std::any_of(captureHeld.begin(), captureHeld.end(), [](bool down) { return down; })) return;
    MSG queued{}; while (PeekMessageW(&queued, hwnd, WM_HOTKEY, WM_HOTKEY, PM_REMOVE)) {}
    shortcutRegistry->initialize(preferences.hotkeys); shortcutsSuspended = false;
    for (int i = 0; i < 4; ++i) hotkeys[i] = shortcutRegistry->active(i);
    if (std::any_of(std::begin(hotkeys), std::end(hotkeys), [](bool active) { return !active; }))
        notice = L"Un raccourci est devenu indisponible. Choisissez-en un autre dans Préférences.";
    editorState(); InvalidateRect(hwnd, nullptr, FALSE);
}
void App::acceptCapturedKey(uint16_t key, uint8_t modifiers) {
    auto name = keyName(key, modifiers);
    bool valid;
    if (keyCaptureTarget == StepKey) {
        Step step; step.action = static_cast<Action>(SendMessageW(control(StepType), CB_GETCURSEL, 0, 0));
        step.key = key; step.modifiers = modifiers; std::wstring problem;
        valid = validStep(step, problem);
        if (!valid) notice = problem;
    } else {
        Hotkey candidate; valid = parseHotkey(name, candidate);
        if (!valid) notice = L"Touche réservée ou non prise en charge. Essayez une autre touche · Échap pour annuler.";
    }
    if (!valid) { loneModifier = 0; InvalidateRect(hwnd, nullptr, FALSE); return; }
    set(keyCaptureTarget, name);
    endKeyCapture(keyCaptureTarget == StepKey ? L"Touche capturée. Ajoutez ou appliquez l'étape." : L"Touche capturée. Cliquez sur Appliquer les raccourcis pour enregistrer.");
}
bool App::captureMessage(const MSG& message) {
    // This runs before IsDialogMessage, which consumes Tab and arrow navigation.
    bool keyboard = keyboardNavigation;
    if (!shortcutsSuspended && message.message == WM_KEYDOWN &&
        (message.wParam == VK_TAB || (message.wParam >= VK_LEFT && message.wParam <= VK_DOWN))) keyboard = true;
    if (message.message == WM_LBUTTONDOWN || message.message == WM_RBUTTONDOWN || message.message == WM_MBUTTONDOWN || message.message == WM_NCLBUTTONDOWN) keyboard = false;
    if (keyboardNavigation != keyboard) {
        keyboardNavigation = keyboard;
        RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
    if (textInput) return false; // Letters used by global shortcuts must remain typeable in names and fields.
    if (!shortcutsSuspended) return false;
    const auto msg = message.message;
    if (keyCaptureTarget && message.hwnd != control(keyCaptureTarget) &&
        (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN || msg == WM_NCLBUTTONDOWN)) {
        endKeyCapture(L"Capture annulée."); resumeShortcuts(); return false;
    }
    bool down = msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN;
    bool up = msg == WM_KEYUP || msg == WM_SYSKEYUP;
    if (!down && !up) return msg == WM_CHAR || msg == WM_SYSCHAR || msg == WM_DEADCHAR || msg == WM_SYSDEADCHAR;
    auto key = static_cast<uint16_t>(message.wParam);
    if (key >= captureHeld.size()) return true;
    bool repeated = down && (captureHeld[key] || (message.lParam & (1LL << 30)));
    captureHeld[key] = down;
    if (!keyCaptureTarget) { resumeShortcuts(); return true; }
    if (key == VK_ESCAPE && keyCaptureTarget != StepKey) { if (down) endKeyCapture(L"Capture annulée."); return true; }
    auto isHeld = [&](int generic, int left, int right) { return captureHeld[generic] || captureHeld[left] || captureHeld[right]; };
    uint8_t mods = (isHeld(VK_CONTROL, VK_LCONTROL, VK_RCONTROL) ? 1 : 0) |
        (isHeld(VK_MENU, VK_LMENU, VK_RMENU) ? 2 : 0) | (isHeld(VK_SHIFT, VK_LSHIFT, VK_RSHIFT) ? 4 : 0) |
        ((captureHeld[VK_LWIN] || captureHeld[VK_RWIN]) ? 8 : 0);
    uint16_t modifier = 0;
    if (key == VK_CONTROL || key == VK_LCONTROL || key == VK_RCONTROL) modifier = VK_CONTROL;
    if (key == VK_MENU || key == VK_LMENU || key == VK_RMENU) modifier = VK_MENU;
    if (key == VK_SHIFT || key == VK_LSHIFT || key == VK_RSHIFT) modifier = VK_SHIFT;
    if (key == VK_LWIN || key == VK_RWIN) modifier = VK_LWIN;
    if (modifier) {
        if (down && !repeated) loneModifier = modifier;
        // A modifier alone remains useful for macro press/release steps.
        if (up && !mods && loneModifier && keyCaptureTarget == StepKey) acceptCapturedKey(loneModifier, 0);
        if (keyCaptureTarget) InvalidateRect(control(keyCaptureTarget), nullptr, FALSE);
    } else if (down && !repeated) {
        loneModifier = 0; acceptCapturedKey(key, mods);
    }
    resumeShortcuts(); return true;
}
std::wstring App::bindingLabel(int id) const {
    if (keyCaptureTarget == id) {
        std::wstring label;
        if (captureHeld[VK_CONTROL] || captureHeld[VK_LCONTROL] || captureHeld[VK_RCONTROL]) label += L"Ctrl + ";
        if (captureHeld[VK_MENU] || captureHeld[VK_LMENU] || captureHeld[VK_RMENU]) label += L"Alt + ";
        if (captureHeld[VK_SHIFT] || captureHeld[VK_LSHIFT] || captureHeld[VK_RSHIFT]) label += L"Shift + ";
        if (captureHeld[VK_LWIN] || captureHeld[VK_RWIN]) label += L"Win + ";
        return label.empty() ? L"Appuyez sur une touche…" : label + L"…";
    }
    auto label = value(id); uint16_t key; uint8_t mods;
    if (parseKey(label, key, mods) && key >= VK_OEM_1 && key <= VK_OEM_102) {
        wchar_t name[64]{}; auto scan = MapVirtualKeyW(key, MAPVK_VK_TO_VSC);
        if (scan && GetKeyNameTextW(static_cast<LONG>(scan << 16), name, 64)) {
            auto prefix = keyName('A', mods); prefix.pop_back(); return prefix + name;
        }
    }
    return label;
}
