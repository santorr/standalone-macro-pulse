// Verify the main window defers erasing to its complete buffered frame, while
// native controls that still erase use their dark background.
void App::checkRendering() {
    auto check = [&](bool condition) { if (!condition) ++smokeExit; };
    HDC windowDC = GetDC(hwnd), dc = CreateCompatibleDC(windowDC);
    RECT bounds{}; GetClientRect(hwnd, &bounds);
    auto bitmap = CreateCompatibleBitmap(windowDC, bounds.right, bounds.bottom);
    auto old = SelectObject(dc, bitmap); ReleaseDC(hwnd, windowDC);
    auto checkErase = [&](HWND window) {
        RECT rect{}; GetClientRect(window, &rect);
        if (!rect.right || !rect.bottom) return;
        FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        check(SendMessageW(window, WM_ERASEBKGND, reinterpret_cast<WPARAM>(dc), 0) != 0);
        if (window == hwnd) { check(GetPixel(dc, 0, 0) == RGB(255, 255, 255)); return; }
        for (int y : {0L, rect.bottom / 2, rect.bottom - 1})
            for (int x : {0L, rect.right / 2, rect.right - 1}) {
                auto pixel = GetPixel(dc, x, y);
                check(pixel != CLR_INVALID && GetRValue(pixel) < 80 && GetGValue(pixel) < 80 && GetBValue(pixel) < 80);
            }
    };
    for (int round = 0; round < 4; ++round) for (int nav : {NavMacro, NavSettings, NavClick}) {
        command(nav);
        check(IsWindowVisible(hwnd) != FALSE);
        check(!IsWindowVisible(control(ClickButton)) && !IsWindowVisible(control(ClickPosition)));
        check(!IsWindowVisible(control(StepKey)) && !IsWindowVisible(control(StepWheel)));
        check(!IsWindowVisible(control(StepButton)));
        check((IsWindowVisible(control(LibraryList)) != FALSE) == (nav == NavMacro));
        check((IsWindowVisible(control(HotkeyClick)) != FALSE) == (nav == NavSettings));
        check((IsWindowVisible(control(ClickInterval)) != FALSE) == (nav == NavClick));
        checkErase(hwnd);
        for (const auto& widget : widgets) if (IsWindowVisible(widget.hwnd)) checkErase(widget.hwnd);
        check(value(ClickInterval) == L"100" && value(StepKey) == L"Ctrl+C");
    }
    SelectObject(dc, old); DeleteObject(bitmap); DeleteDC(dc);
}
