// Native UI rendering. Included after App's declaration in main.cpp.
void App::box(HDC dc, int x, int y, int w, int h, COLORREF color, int radius) {
    theme::surface(dc, float(s(x)), float(s(y)), float(s(w)), float(s(h)), float(s(radius)), color);
}
void App::card(HDC dc, int x, int y, int w, int h) {
    theme::surface(dc, float(s(x)), float(s(y)), float(s(w)), float(s(h)), float(s(14)), Panel, RGB(43, 38, 62));
}
void App::glyph(HDC dc, int kind, int x, int y, int size, COLORREF color) {
    theme::symbol(dc, kind, float(s(x)), float(s(y)), float(s(size)), color);
}
void App::text(HDC dc, const std::wstring& str, int x, int y, int w, int h, COLORREF color, int font, UINT flags) {
    RECT r{s(x), s(y), s(x + w), s(y + h)};
    SelectObject(dc, fonts[font]); SetBkMode(dc, TRANSPARENT); SetTextColor(dc, color);
    DrawTextW(dc, str.c_str(), -1, &r, flags | DT_NOPREFIX | DT_END_ELLIPSIS);
}
void App::paint(HDC target, const RECT* region) {
    RenderTiming timing(resizeReport.empty() ? nullptr : &renderMetrics.paintMs);
    ++renderMetrics.paints;
    RECT bounds{}; GetClientRect(hwnd, &bounds);
    if (bounds.right <= 0 || bounds.bottom <= 0) return;
    auto previousAllocations = paintBuffer.allocations();
    bool buffered = paintBuffer.ensure(target, bounds.right, bounds.bottom);
    renderMetrics.allocations += paintBuffer.allocations() - previousAllocations;
    HDC dc = buffered ? paintBuffer.dc() : target;
    int saved = SaveDC(dc);
    RECT update = region ? *region : bounds;
    IntersectClipRect(dc, update.left, update.top, update.right, update.bottom);
    SetDCBrushColor(dc, Bg); FillRect(dc, &bounds, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    box(dc, 0, 0, 228, height, Sidebar, 0);
    box(dc, 227, 0, 1, height, RGB(40, 35, 56), 0);
    if (brandIcon) DrawIconEx(dc, s(19), s(31), brandIcon, s(48), s(48), 0, nullptr, DI_NORMAL);
    text(dc, L"MacroPulse", 76, 35, 143, 32, Text, 2);
    text(dc, L"YOUR WORKSPACE", 28, 111, 170, 24, Muted, 3);
    const int shortcutY = height - 252;
    text(dc, L"Shortcuts", 28, shortcutY, 178, 28, Text);
    const wchar_t* shortcutLabels[] = {L"Auto-click", L"Macro", L"Stop", L"Position"};
    for (int i = 0; i < 4; ++i) {
        int y = shortcutY + 44 + i * 36;
        box(dc, 98, y, 108, 25, Field, 5);
        text(dc, shortcutName(i), 98, y, 108, 25, hotkeys[i] ? (i == 2 ? Success : Text) : Danger, 3, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        text(dc, shortcutLabels[i], 28, y, 66, 25, Muted, 3);
    }
    box(dc, 28, height - 42, 7, 7, Success, 4);
    text(dc, L"Version " + std::wstring(AppVersion), 45, height - 50, 155, 23, Muted, 3);

    int x = 252, available = width - x - 32;
    text(dc, L"AUTOMATION", x, 22, available - 180, 21, Accent, 3);
    text(dc, page == 2 ? L"Preferences" : page == 1 ? macroName() : page == 3 ? L"My macros" : L"Auto-clicker", x, 45, available - 200, 44, Text, 1);
    text(dc, page == 2 ? L"Your preferences. Every time." : page == 3 ? L"All your sequences, always within reach." : page == 1 ? L"Build your sequence. Changes are saved automatically." : L"Set the pace. We'll handle the clicks.", x, 91, available, 24, Muted);
    auto snapshot = engine.snapshot(); bool active = snapshot.state != RunState::Idle;
    if (page != 3) {
    box(dc, width - 168, 44, 136, 34, active ? RGB(34, 43, 49) : Panel, 17);
    box(dc, width - 153, 57, 7, 7, active ? Success : Accent, 4);
    text(dc, snapshot.state == RunState::Countdown ? L"Get ready" : active ? L"Running" : !hotkeys[2] ? L"Setup needed" : L"Ready to run", width - 137, 44, 96, 34, !hotkeys[2] ? Danger : active ? Success : Text, 3);
    }

    if (page == 0) {
        const int cw = available * 3 / 5, rx = x + cw + 20, rw = available - cw - 20;
        card(dc, x, 138, cw, 416);
        box(dc, x + 24, 157, 29, 29, RGB(48, 36, 78), 8); glyph(dc, 0, x + 29, 162, 19, Accent);
        text(dc, L"Click settings", x + 65, 154, cw - 89, 34, Text, 2);
        text(dc, L"Mouse button", x + 24, 189, cw - 48, 22, Muted, 3);
        text(dc, L"Interval", x + 24, 273, 180, 24, Muted, 3);
        text(dc, L"Click count", x + 40 + (cw - 64) / 2, 273, 190, 24, Muted, 3);
        text(dc, L"milliseconds between clicks", x + 24, 343, (cw - 64) / 2, 21, Muted, 3);
        text(dc, L"0 to keep going indefinitely", x + 40 + (cw - 64) / 2, 343, (cw - 64) / 2, 21, Muted, 3);
        text(dc, L"Where to click", x + 24, 365, cw - 48, 22, Muted, 3);
        bool fixed = SendMessageW(control(ClickPosition), CB_GETCURSEL, 0, 0) == 1;
        if (fixed) {
            text(dc, L"Position X", x + 24, 449, 180, 22, Muted, 3);
            text(dc, L"Position Y", x + 40 + (cw - 64) / 2, 449, 180, 22, Muted, 3);
            text(dc, shortcutName(3) + L" to capture your cursor position", x + 24, 524, cw - 48, 20, Muted, 3);
        } else {
            glyph(dc, 4, x + 24, 467, 26, Accent);
            text(dc, L"Clicks follow you", x + 64, 459, cw - 88, 25, Text);
            text(dc, L"Just place the cursor over your target.", x + 64, 487, cw - 88, 25, Muted, 3);
        }

        theme::gradient(dc, float(s(rx)), float(s(138)), float(s(rw)), float(s(260)), float(s(14)), RGB(49, 34, 84), Panel, RGB(75, 51, 121));
        // An abstract mouse silhouette makes the cadence card immediately recognizable.
        glyph(dc, 0, rx + rw - 113, 169, 92, RGB(75, 56, 113));
        text(dc, L"YOUR PACE", rx + 22, 157, rw - 44, 24, RGB(192, 175, 231), 3);
        int interval = 0; std::wstring rate = L"—";
        if (parseNumber(value(ClickInterval), 1, 60000, interval)) {
            wchar_t buffer[64]; double cps = 1000.0 / interval;
            swprintf_s(buffer, std::abs(cps - std::round(cps)) < .001 ? L"%.0f" : L"%.1f", cps); rate = buffer;
        }
        text(dc, rate, rx + 22, 188, rw - 44, 72, Text, 4);
        text(dc, L"clicks / second", rx + 24, 260, rw - 48, 27, RGB(195, 180, 224));
        text(dc, L"Choose a preset", rx + 24, 305, rw - 48, 23, Muted, 3);
        card(dc, rx, 418, rw, 136);
        text(dc, L"This session", rx + 22, 431, rw - 44, 28, Text);
        const int half = (rw - 44) / 2;
        text(dc, std::to_wstring(snapshot.actions), rx + 22, 469, half, 34, Text, 2);
        wchar_t elapsed[64]; swprintf_s(elapsed, L"%.1f s", snapshot.elapsedSeconds);
        text(dc, elapsed, rx + 22 + half, 469, half, 34, Text, 2);
        text(dc, L"Actions", rx + 22, 510, half, 23, Muted, 3);
        text(dc, L"Elapsed time", rx + 22 + half, 510, half, 23, Muted, 3);

        if (height >= 780) {
            card(dc, x, 574, available, 100);
            box(dc, x + 24, 598, 50, 50, RGB(45, 34, 72), 12); glyph(dc, 1, x + 37, 611, 24, Accent);
            text(dc, L"A click is just the start", x + 94, 592, available - 318, 30, Text, 2);
            text(dc, L"Combine keys, clicks and pauses.", x + 94, 629, available - 318, 22, Muted, 3);
        }
    } else if (page == 1) {
        text(dc, L"Repetitions", width - 224, 122, 96, 29, Muted, 3);
        const int tableHeight = height - 530;
        card(dc, x, 174, available, tableHeight);
        text(dc, L"#", x + 22, 181, 34, 26, Muted, 3);
        text(dc, L"ACTION", x + 62, 181, 190, 26, Muted, 3);
        text(dc, L"DELAY", x + 263, 181, 102, 26, Muted, 3);
        text(dc, L"DETAILS", x + 373, 181, available - 397, 26, Muted, 3);
        const int editorY = height - 330;
        card(dc, x, editorY, available, 234);
        text(dc, selected() < 0 ? L"New action" : L"Edit action " + std::to_wstring(selected() + 1), x + 20, editorY + 5, 320, 25, Accent);
        text(dc, L"Action", x + 20, editorY + 31, 218, 23, Muted, 3);
        auto type = static_cast<Action>(SendMessageW(control(StepType), CB_GETCURSEL, 0, 0));
        text(dc, type == Action::Wait ? L"Duration (ms)" : L"Delay before (ms)", x + 254, editorY + 31, 148, 23, Muted, 3);
        if (type != Action::Move && type != Action::Wait)
            text(dc, type == Action::Click ? L"Mouse button" : type == Action::Scroll ? L"Notches (+ up / − down)" : L"Key or shortcut", x + 414, editorY + 31, available - 434, 23, Muted, 3);
        if (type == Action::Click || type == Action::Move) {
            text(dc, L"Position", x + 20, editorY + 104, 218, 23, Muted, 3);
            text(dc, L"X", x + 254, editorY + 104, 142, 23, Muted, 3);
            text(dc, L"Y", x + 414, editorY + 104, 142, 23, Muted, 3);
            text(dc, shortcutName(3) + L": capture", x + 572, editorY + 131, available - 592, 30, Muted, 3);
        } else {
            glyph(dc, type == Action::Wait ? 2 : 5, x + 22, editorY + 119, 25, Accent);
            text(dc, type == Action::Wait ? L"Pause before continuing." : type == Action::Key ? L"Click the key button, then press your combination." : type == Action::Scroll ? L"Positive scrolls up; negative scrolls down." : L"Click the key button, then press and release one key.", x + 60, editorY + 109, available - 80, 27, Text);
            if (type == Action::KeyDown || type == Action::KeyUp)
                text(dc, L"Held keys are released when stopped and at the end of each loop.", x + 60, editorY + 139, available - 80, 22, Muted, 3);
        }
    } else if (page == 3) {
        int libraryWidth = available * 54 / 100, detailX = x + libraryWidth + 20, detailWidth = available - libraryWidth - 20;
        card(dc, x, 138, libraryWidth, height - 238);
        text(dc, L"YOUR LIBRARY", x + 24, 153, libraryWidth - 90, 28, Muted, 3);
        text(dc, std::to_wstring(library.entries.size()), x + libraryWidth - 58, 153, 34, 28, Accent, 2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        card(dc, detailX, 138, detailWidth, 358);
        glyph(dc, 1, detailX + 20, 159, 24, Accent);
        text(dc, currentMacro() ? L"Your selection" : L"Make it yours", detailX + 58, 153, detailWidth - 78, 32, Text, 2);
        text(dc, L"Macro name", detailX + 20, 194, detailWidth - 40, 23, Muted, 3);
        text(dc, currentMacro() ? std::to_wstring(macro.steps.size()) + L" actions in the sequence" : L"Create your first macro.", detailX + 20, 280, detailWidth - 40, 28, Text);
        text(dc, L"Select, customize, run.", detailX + 20, 317, detailWidth - 40, 23, Muted, 3);
        text(dc, dirty ? L"Saving changes…" : libraryWritable ? L"Saved automatically" : L"Library unavailable", detailX + 20, 463, detailWidth - 40, 22, dirty || !libraryWritable ? Danger : Success, 3);
    } else {
        card(dc, x, 138, available, 414);
        glyph(dc, 5, x + 24, 158, 25, Accent);
        text(dc, L"Input shortcuts", x + 62, 152, available - 86, 34, Text, 2);
        const wchar_t* names[] = {L"Auto-clicker", L"Run macro", L"Stop everything", L"Capture position"};
        const wchar_t* descriptions[] = {L"Start or stop clicking", L"Start or stop your sequence", L"Stop all actions immediately", L"Capture the cursor position"};
        for (int i = 0; i < 4; ++i) {
            int y = 206 + i * 62;
            text(dc, names[i], x + 24, y, available - 346, 24, i == 2 ? Success : Text);
            text(dc, hotkeys[i] ? descriptions[i] : L"Shortcut unavailable: choose another", x + 24, y + 25, available - 346, 23, hotkeys[i] ? Muted : Danger, 3);
        }
        text(dc, L"Select a shortcut, then press a key, click it again or scroll. Esc cancels.", x + 24, 447, available - 48, 23, Muted, 3);
        text(dc, L"Mouse shortcuts keep normal clicks. Launch shortcuts pause in supported text fields.", x + 24, 523, available - 48, 23, Muted, 3);
        if (height >= 780) {
            card(dc, x, 574, available, 100);
            glyph(dc, 1, x + 26, 606, 28, Accent);
            text(dc, L"Pick up where you left off", x + 76, 590, available - 100, 30, Text, 2);
            text(dc, L"Your settings and library are restored at startup.", x + 76, 629, available - 100, 23, Muted, 3);
        }
    }

    // Edits are inset into rounded shells; the native text remains selectable and accessible.
    for (const auto& w : widgets) {
        if (w.page >= 0 && w.page != page) continue;
        if (!(GetWindowLongPtrW(w.hwnd, GWL_STYLE) & WS_VISIBLE)) continue;
        if (!w.edit) continue;
        RECT rect{}; GetWindowRect(w.hwnd, &rect); MapWindowPoints(nullptr, hwnd, reinterpret_cast<POINT*>(&rect), 2);
        theme::surface(dc, float(rect.left - s(12)), float(rect.top - s(10)), float(rect.right - rect.left + s(24)), float(rect.bottom - rect.top + s(18)), float(s(8)), Field, GetFocus() == w.hwnd ? Accent : Line);
    }
    if (page != 2) text(dc, L"Start delay (ms)", x, height - 67, 154, 32, Muted, 3);
    else text(dc, L"Your settings are saved automatically.", x, height - 67, available - 250, 32, Muted, 3);
    std::wstring bottom = notice;
    if (active) bottom = snapshot.state == RunState::Countdown ? L"Move to your target…" : L"Running · " + shortcutName(2) + L" to stop";
    text(dc, bottom, x, height - 25, available, 20, failureShown ? Danger : Muted, 3);
    if (buffered) BitBlt(target, update.left, update.top, update.right - update.left, update.bottom - update.top, dc, update.left, update.top, SRCCOPY);
    RestoreDC(dc, saved);
}

void App::cachedControl(HWND c, HDC target, const RECT& bounds, UINT state, const std::function<void(HDC)>& draw) {
    int id = GetDlgCtrlID(c);
    auto& image = controlImages[id - NavClick]; if (!image) image = std::make_unique<ControlImage>();
    ControlVisual visual{bounds.right, bounds.bottom, dpi, page, keyCaptureTarget,
        static_cast<int>(SendMessageW(control(ClickButton), CB_GETCURSEL, 0, 0)),
        static_cast<int>(SendMessageW(control(ClickPosition), CB_GETCURSEL, 0, 0)), state,
        keyboardNavigation, GetPropW(c, L"PulseHover") != nullptr, IsWindowEnabled(c) != FALSE, GetFocus() == c,
        (id == StepKey || (id >= HotkeyClick && id <= HotkeyCapture)) ? bindingLabel(id) : value(id),
        value(ClickInterval), preferences.hotkeys};
    if (!image->buffer.ensure(target, bounds.right, bounds.bottom, 32, 16)) { draw(target); return; }
    if (!image->visual || *image->visual != visual) {
        auto dc = image->buffer.dc(); int saved = SaveDC(dc);
        IntersectClipRect(dc, bounds.left, bounds.top, bounds.right, bounds.bottom);
        draw(dc); RestoreDC(dc, saved); image->visual = std::move(visual);
    }
    BitBlt(target, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top,
        image->buffer.dc(), bounds.left, bounds.top, SRCCOPY);
}

void App::drawButton(const DRAWITEMSTRUCT& item) {
    int id = static_cast<int>(item.CtlID), w = MulDiv(item.rcItem.right, 96, dpi), h = MulDiv(item.rcItem.bottom, 96, dpi);
    bool disabled = (item.itemState & ODS_DISABLED) != 0, pressed = (item.itemState & ODS_SELECTED) != 0;
    bool keyboardFocus = keyboardNavigation && !disabled && (item.itemState & ODS_FOCUS);
    bool hover = GetPropW(item.hwndItem, L"PulseHover") != nullptr;
    bool nav = id == NavClick || id == NavMacro || id == NavSettings;
    bool binding = id == StepKey || (id >= HotkeyClick && id <= HotkeyCapture);
    COLORREF parentBackground = nav ? Sidebar : binding || id == EditMacro || id == CopyMacro || id == RemoveMacro || (id >= MouseLeft && id <= PresetFast) || (id >= AddStep && id <= DuplicateStep) || id == CreateMacro || id == ApplyHotkeys || id == DefaultKeys ? Panel : Bg;
    if (id >= PresetSlow && id <= PresetFast) parentBackground = RGB(32, 26, 52);
    SetDCBrushColor(item.hDC, parentBackground); FillRect(item.hDC, &item.rcItem, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    bool chosen = (id == NavClick && page == 0) || (id == NavMacro && (page == 1 || page == 3)) || (id == NavSettings && page == 2);
    if (id >= MouseLeft && id <= MouseMiddle) chosen = SendMessageW(control(ClickButton), CB_GETCURSEL, 0, 0) == id - MouseLeft;
    if (id == FollowCursor || id == FixedPoint) chosen = SendMessageW(control(ClickPosition), CB_GETCURSEL, 0, 0) == id - FollowCursor;
    if (id >= PresetSlow && id <= PresetFast) { int intervals[] = {200, 100, 20}; chosen = value(ClickInterval) == std::to_wstring(intervals[id - PresetSlow]); }
    bool primary = id == Start || id == AddStep || id == ApplyHotkeys || id == NewMacro || id == EditMacro;
    COLORREF background = nav ? Sidebar : Field, border = nav ? Sidebar : Line;
    if (chosen) { background = RGB(53, 38, 88); border = RGB(100, 73, 153); }
    if (hover && !disabled) background = chosen ? RGB(67, 46, 111) : RGB(47, 41, 67);
    if (pressed) background = RGB(65, 44, 109);
    if (keyCaptureTarget == id) { background = RGB(53, 38, 88); border = Accent; }
    if (disabled) background = RGB(29, 26, 43);
    if (keyboardFocus) border = Accent;
    if (primary && !disabled) theme::gradient(item.hDC, 0, 0, float(item.rcItem.right), float(item.rcItem.bottom), float(s(8)), pressed ? RGB(102, 64, 205) : hover ? RGB(166, 123, 255) : RGB(149, 105, 255), Purple, keyboardFocus ? RGB(215, 195, 255) : RGB(162, 128, 253));
    else theme::surface(item.hDC, 0, 0, float(item.rcItem.right), float(item.rcItem.bottom), float(s(nav ? 10 : 8)), background, border);
    if (nav && chosen) box(item.hDC, 0, 12, 3, h - 24, Accent, 2);
    COLORREF foreground = disabled ? RGB(100, 94, 119) : id == Stop || id == DeleteStep || id == RemoveMacro ? Danger : chosen ? RGB(215, 195, 255) : Text;
    if (id == UpStep || id == DownStep) {
        glyph(item.hDC, id == UpStep ? 9 : 10, (w - 20) / 2, (h - 20) / 2, 20, foreground);
        return;
    }
    int icon = -1;
    if (nav) icon = id == NavClick ? 0 : id == NavMacro ? 1 : 5;
    else if (id >= MouseLeft && id <= MouseMiddle) icon = 0;
    else if (id == FollowCursor || id == FixedPoint) icon = 4;
    else if (id == Start) icon = 7;
    else if (id == AddStep) icon = 8;
    else if (id == Stop) icon = 3;
    if (icon >= 0) glyph(item.hDC, icon, nav ? 14 : 12, (h - 20) / 2, 20, foreground);
    int left = icon >= 0 ? (nav ? 46 : 38) : 8;
    const bool showKey = (id == Start || id == Stop) && shortcutName(id == Stop ? 2 : page == 1 || page == 3 ? 1 : 0).size() <= 4;
    int right = showKey ? 44 : 8;
    text(item.hDC, binding ? bindingLabel(id) : value(id), left, 0, w - left - right, h, foreground, 0, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | (icon >= 0 ? DT_LEFT : DT_CENTER));
    if (showKey) {
        box(item.hDC, w - 38, (h - 24) / 2, 29, 24, primary && !disabled ? RGB(104, 66, 194) : RGB(43, 33, 56), 5);
        text(item.hDC, shortcutName(id == Stop ? 2 : page == 1 || page == 3 ? 1 : 0), w - 38, (h - 24) / 2, 29, 24, foreground, 3, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

void App::drawCombo(HWND c, HDC dc) {
    RECT r{}; GetClientRect(c, &r); bool enabled = IsWindowEnabled(c) != FALSE;
    SetDCBrushColor(dc, Panel); FillRect(dc, &r, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    theme::surface(dc, 0, 0, float(r.right), float(r.bottom), float(s(8)), Field, GetFocus() == c ? Accent : Line);
    int selectedItem = static_cast<int>(SendMessageW(c, CB_GETCURSEL, 0, 0));
    wchar_t label[128]{}; if (selectedItem >= 0 && SendMessageW(c, CB_GETLBTEXTLEN, selectedItem, 0) < 128) SendMessageW(c, CB_GETLBTEXT, selectedItem, reinterpret_cast<LPARAM>(label));
    text(dc, label, 12, 0, MulDiv(r.right, 96, dpi) - 42, MulDiv(r.bottom, 96, dpi), enabled ? Text : Muted);
    Gdiplus::Graphics g(dc); g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen pen(theme::color(Muted), float(s(1)));
    float cx = float(r.right - s(18)), cy = float(r.bottom / 2);
    g.DrawLine(&pen, cx - s(4), cy - s(2), cx, cy + s(2)); g.DrawLine(&pen, cx, cy + s(2), cx + s(4), cy - s(2));
}

LRESULT CALLBACK App::widgetProc(HWND c, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR data) {
    auto app = reinterpret_cast<App*>(data);
    int id = GetDlgCtrlID(c);
    const auto& widget = app->widgets[app->widgetIndices[id - NavClick]];
    RenderTiming timing(msg == WM_PAINT && !app->resizeReport.empty() ? &app->renderMetrics.childMs : nullptr);
    if (msg == WM_PAINT) ++app->renderMetrics.childPaints;
    if (msg == WM_ERASEBKGND) {
        // Native controls must never clear with the system's light theme first.
        COLORREF background = id == StepList || id == LibraryList ? Panel : id == NavClick || id == NavMacro || id == NavSettings ? Sidebar : Field;
        RECT rect{}; GetClientRect(c, &rect); auto dc = reinterpret_cast<HDC>(wp);
        SetDCBrushColor(dc, background); FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH))); return 1;
    }
    if (msg == WM_KILLFOCUS && app->keyCaptureTarget == GetDlgCtrlID(c)) app->endKeyCapture(L"Capture cancelled.");
    if (GetDlgCtrlID(c) == StepList && app->macro.steps.empty() && (msg == WM_PAINT || msg == WM_PRINT || msg == WM_PRINTCLIENT)) {
        PAINTSTRUCT ps{}; HDC target = msg == WM_PAINT ? BeginPaint(c, &ps) : reinterpret_cast<HDC>(wp), dc = target;
        RECT rect{}; GetClientRect(c, &rect);
        auto buffer = msg == WM_PAINT ? BeginBufferedPaint(target, &rect, BPBF_TOPDOWNDIB, nullptr, &dc) : nullptr;
        if (!buffer) dc = target;
        int w = MulDiv(rect.right, 96, app->dpi), h = MulDiv(rect.bottom, 96, app->dpi);
        app->box(dc, 0, 0, w, h, Panel, 0);
        int center = h >= 140 ? h / 2 : std::max(12, h / 2 - 18);
        if (h >= 140) { app->box(dc, w / 2 - 25, center - 68, 50, 50, RGB(44, 33, 72), 12); app->glyph(dc, 1, w / 2 - 12, center - 55, 24, Accent); }
        app->text(dc, L"Your first macro starts here", 10, center - 4, w - 20, 28, Text, 0, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        app->text(dc, L"Choose an action below to add it.", 10, center + 28, w - 20, 23, Muted, 3, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        if (buffer) EndBufferedPaint(buffer, TRUE);
        if (msg == WM_PAINT) EndPaint(c, &ps);
        return 0;
    }
    bool combo = widget.combo;
    if ((combo || widget.listbox) && msg == WM_PAINT) {
        PAINTSTRUCT ps{}; auto target = BeginPaint(c, &ps); HDC dc = target;
        RECT rect{}; GetClientRect(c, &rect);
        if (combo) app->cachedControl(c, target, rect, 0, [&](HDC bufferDC) { app->drawCombo(c, bufferDC); });
        else {
            auto buffer = BeginBufferedPaint(target, &rect, BPBF_TOPDOWNDIB, nullptr, &dc);
            if (!buffer) dc = target;
            DefSubclassProc(c, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
            if (buffer) EndBufferedPaint(buffer, TRUE);
        }
        EndPaint(c, &ps); return 0;
    }
    if (combo && (msg == WM_PRINTCLIENT || msg == WM_PRINT)) { app->drawCombo(c, reinterpret_cast<HDC>(wp)); return 0; }
    if (msg == WM_MOUSEMOVE && !GetPropW(c, L"PulseHover")) {
        SetPropW(c, L"PulseHover", reinterpret_cast<HANDLE>(1)); TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, c, 0}; TrackMouseEvent(&track); InvalidateRect(c, nullptr, FALSE);
    }
    if (msg == WM_MOUSELEAVE) { RemovePropW(c, L"PulseHover"); InvalidateRect(c, nullptr, FALSE); }
    if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS || msg == WM_ENABLE) InvalidateRect(c, nullptr, FALSE);
    if (msg == WM_NCDESTROY) { RemovePropW(c, L"PulseHover"); RemoveWindowSubclass(c, widgetProc, 1); }
    return DefSubclassProc(c, msg, wp, lp);
}

void App::drawRow(const DRAWITEMSTRUCT& item) {
    if (item.itemID >= macro.steps.size()) return;
    auto& step = macro.steps[item.itemID]; int top = MulDiv(item.rcItem.top, 96, dpi), rowWidth = MulDiv(item.rcItem.right, 96, dpi);
    bool selectedRow = (item.itemState & ODS_SELECTED) != 0;
    box(item.hDC, 0, top + 2, rowWidth - 1, 42, selectedRow ? RGB(51, 37, 83) : Panel, 8);
    text(item.hDC, std::to_wstring(item.itemID + 1), 10, top, 32, 46, selectedRow ? Accent : Muted, 3);
    int icon = step.action == Action::Click ? 0 : step.action == Action::Move ? 4 : step.action == Action::Wait ? 2 : 5;
    glyph(item.hDC, icon, 48, top + 13, 20, Accent);
    text(item.hDC, actionName(step.action), 80, top, 168, 46, Text);
    text(item.hDC, std::to_wstring(step.delayMs) + L" ms", 255, top, 102, 46, Muted);
    text(item.hDC, describe(step), 365, top, rowWidth - 381, 46, selectedRow ? Text : Muted);
    if (keyboardNavigation && (item.itemState & ODS_FOCUS) && GetFocus() == control(StepList)) {
        Gdiplus::Graphics g(item.hDC); g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::GraphicsPath outline; theme::rounded(outline, 1, float(item.rcItem.top + s(2)), float(item.rcItem.right - s(3)), float(s(42)), float(s(8)));
        Gdiplus::Pen pen(theme::color(Accent)); g.DrawPath(&pen, &outline);
    }
}

void App::exportPreview(const wchar_t* name) {
    if (previewDirectory.empty()) return;
    // Render the application's own paint path and child controls, without desktop capture.
    RECT r{}; GetClientRect(hwnd, &r); HDC screen = GetDC(hwnd), dc = CreateCompatibleDC(screen);
    auto bitmap = CreateCompatibleBitmap(screen, r.right, r.bottom); auto old = SelectObject(dc, bitmap);
    paint(dc);
    for (const auto& w : widgets) if (IsWindowVisible(w.hwnd)) {
        RECT rect{}; GetWindowRect(w.hwnd, &rect); MapWindowPoints(nullptr, hwnd, reinterpret_cast<POINT*>(&rect), 2);
        int saved = SaveDC(dc); SetViewportOrgEx(dc, rect.left, rect.top, nullptr);
        IntersectClipRect(dc, 0, 0, rect.right - rect.left, rect.bottom - rect.top);
        SendMessageW(w.hwnd, WM_PRINT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT | PRF_CHILDREN | PRF_ERASEBKGND); RestoreDC(dc, saved);
    }
    SelectObject(dc, old);
    Gdiplus::Bitmap image(bitmap, nullptr);
    const CLSID pngEncoder{0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
    if (image.Save((previewDirectory / name).c_str(), &pngEncoder) != Gdiplus::Ok) ++smokeExit;
    DeleteObject(bitmap); DeleteDC(dc); ReleaseDC(hwnd, screen);
}
