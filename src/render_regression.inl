// Pixel comparisons against uncached rendering catch stale labels, hover/focus
// state, selected presets, DPI changes and partial-update corruption.
void App::runRenderRegression() {
    auto check = [&](bool good) { if (!good) ++smokeExit; };
    auto same = [](const PaintBuffer& a, const PaintBuffer& b, int width, int height) {
        GdiFlush();
        for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x)
            if ((a.row(y)[x] & 0xffffff) != (b.row(y)[x] & 0xffffff)) return false;
        return true;
    };
    KillTimer(hwnd, 1); KillTimer(hwnd, 2); KillTimer(hwnd, 3);
    auto screen = GetDC(hwnd);
    PaintBuffer actual, expected;
    auto button = [&](int id, UINT state) {
        RECT rect{}; GetClientRect(control(id), &rect);
        if (!actual.ensure(screen, rect.right, rect.bottom) || !expected.ensure(screen, rect.right, rect.bottom)) { check(false); return; }
        DRAWITEMSTRUCT item{}; item.CtlType = ODT_BUTTON; item.CtlID = id; item.hwndItem = control(id); item.rcItem = rect; item.itemState = state;
        cachedControl(control(id), actual.dc(), rect, state, [&](HDC dc) { item.hDC = dc; drawButton(item); });
        item.hDC = expected.dc(); drawButton(item);
        check(same(actual, expected, rect.right, rect.bottom));
    };
    auto combo = [&]() {
        RECT rect{}; GetClientRect(control(StepType), &rect);
        if (!actual.ensure(screen, rect.right, rect.bottom) || !expected.ensure(screen, rect.right, rect.bottom)) { check(false); return; }
        cachedControl(control(StepType), actual.dc(), rect, 0, [&](HDC dc) { drawCombo(control(StepType), dc); });
        drawCombo(control(StepType), expected.dc()); check(same(actual, expected, rect.right, rect.bottom));
    };
    for (int scale : {96, 144, 192}) {
        dpi = scale; makeFonts();
        check(SendMessageW(control(StepType), CB_GETITEMHEIGHT, 0, 0) == s(34));
        check(SendMessageW(control(LibraryList), LB_GETITEMHEIGHT, 0, 0) == s(76));
        SetWindowPos(hwnd, nullptr, 0, 0, s(1180), s(820), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        for (int tab = 0; tab < 4; ++tab) {
            page = tab; layout();
            auto positions = renderMetrics.positions; auto columns = renderMetrics.columnWrites;
            layout(); check(renderMetrics.positions == positions && renderMetrics.columnWrites == columns);
            RedrawWindow(hwnd, nullptr, nullptr, RDW_UPDATENOW | RDW_ALLCHILDREN);
            // Inactive pages must remain hidden after layout and repaint.
            check((IsWindowVisible(control(LibraryList)) != FALSE) == (tab == 3));
            check((IsWindowVisible(control(HotkeyClick)) != FALSE) == (tab == 2));
        }
        page = 2; layout();
        for (auto label : {L"F6", L"Ctrl+Mouse4", L"WheelDown"}) {
            set(HotkeyClick, label); button(HotkeyClick, 0);
            SetPropW(control(HotkeyClick), L"PulseHover", reinterpret_cast<HANDLE>(1)); button(HotkeyClick, 0);
            RemovePropW(control(HotkeyClick), L"PulseHover"); button(HotkeyClick, 0);
            keyboardNavigation = true; button(HotkeyClick, ODS_FOCUS); button(HotkeyClick, ODS_SELECTED);
            keyboardNavigation = false; button(HotkeyClick, 0);
        }
        keyCaptureTarget = HotkeyClick; button(HotkeyClick, 0);
        captureHeld[VK_CONTROL] = true; button(HotkeyClick, 0);
        captureHeld[VK_CONTROL] = false; keyCaptureTarget = 0; button(HotkeyClick, 0);
        page = 0; layout();
        for (auto interval : {L"20", L"100", L"200"}) { set(ClickInterval, interval); button(PresetNormal, 0); }
        for (int choice : {0, 2, 1}) { SendMessageW(control(ClickButton), CB_SETCURSEL, choice, 0); button(MouseMiddle, 0); }
        button(Start, ODS_DISABLED); button(Start, 0);
        preferences.hotkeys[0] = {VK_F10, 1}; button(Start, 0);
        preferences.hotkeys = DefaultHotkeys; button(Start, 0);
        page = 1; layout();
        for (int choice : {0, 2, 5}) { SendMessageW(control(StepType), CB_SETCURSEL, choice, 0); combo(); }
        EnableWindow(control(StepType), FALSE); combo(); EnableWindow(control(StepType), TRUE); combo();
        // Updating the footer must match a fresh full frame, including every
        // untouched pixel. This also checks reuse after growing the DIB.
        RECT bounds{}; GetClientRect(hwnd, &bounds);
        if (actual.ensure(screen, bounds.right, bounds.bottom) && expected.ensure(screen, bounds.right, bounds.bottom)) {
            notice = L"Before"; paint(actual.dc());
            notice = L"After partial repaint";
            RECT footer{0, std::max(0L, bounds.bottom - s(30)), bounds.right, bounds.bottom};
            paint(actual.dc(), &footer); paint(expected.dc()); check(same(actual, expected, bounds.right, bounds.bottom));
        } else check(false);
    }
    // A populated list keeps its selection/scroll position through width-only
    // resize. No EnsureVisible jump is permitted on each WM_SIZE.
    dpi = 96; makeFonts(); page = 1;
    SetWindowPos(hwnd, nullptr, 0, 0, 1100, 800, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE); layout();
    Step wait; wait.action = Action::Wait; wait.delayMs = 1; macro.steps.assign(200, wait); refreshList(150);
    ListView_EnsureVisible(control(StepList), 150, FALSE);
    auto top = ListView_GetTopIndex(control(StepList));
    auto cycle = [&]() {
        for (int width : {1300, 1100}) {
            SetWindowPos(hwnd, nullptr, 0, 0, width, 800, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            RedrawWindow(hwnd, nullptr, nullptr, RDW_UPDATENOW | RDW_ALLCHILDREN);
            check(selected() == 150 && ListView_GetTopIndex(control(StepList)) == top);
        }
    };
    cycle(); cycle(); // Warm native theme resources and bounded control buffers.
    auto resources = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    for (int i = 0; i < 12; ++i) cycle();
    check(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) <= resources + 2);
    // State changes on the same page must still enable emergency stop even
    // when cached geometry makes layout a no-op. Wait-only work injects nothing.
    page = 0; layout();
    std::wstring problem; wait.delayMs = 5000;
    check(engine.startMacro(Macro{1, {wait}}, 0, problem)); layout();
    check(IsWindowEnabled(control(Stop)) && !IsWindowEnabled(control(ClickInterval)));
    engine.stop(); layout();
    check(!IsWindowEnabled(control(Stop)) && IsWindowEnabled(control(ClickInterval)));
    ReleaseDC(hwnd, screen); dirty = false; DestroyWindow(hwnd);
}
