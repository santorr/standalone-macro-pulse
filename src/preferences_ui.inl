void App::populatePreferences() {
    refreshing = true;
    set(ClickInterval, std::to_wstring(preferences.click.intervalMs)); set(ClickCount, std::to_wstring(preferences.click.count));
    set(ClickX, std::to_wstring(preferences.click.x)); set(ClickY, std::to_wstring(preferences.click.y));
    set(StartDelay, std::to_wstring(preferences.startDelayMs));
    SendMessageW(control(ClickButton), CB_SETCURSEL, static_cast<int>(preferences.click.button), 0);
    SendMessageW(control(ClickPosition), CB_SETCURSEL, preferences.click.fixed ? 1 : 0, 0);
    for (int i = 0; i < 4; ++i) set(HotkeyClick + i, shortcutName(i));
    page = preferences.page;
    refreshing = false;
}
void App::restorePreferences() {
    if (smoke && sessionTest == 2) {
        std::wstring problem; if (!loadPreferences(sessionTestFile, preferences, problem)) ++smokeExit;
    }
    if (!smoke) {
        settingsFile = preferencesPath(); std::error_code ec;
        if (!settingsFile.empty() && std::filesystem::exists(settingsFile, ec)) {
            std::wstring problem; if (!loadPreferences(settingsFile, preferences, problem)) notice = problem;
        }
    }
    populatePreferences();
}
Preferences App::collectPreferences() const {
    Preferences result = preferences;
    int n;
    // A partially typed or invalid field must not overwrite its last valid value.
    if (parseNumber(value(ClickInterval), 1, 60000, n)) result.click.intervalMs = static_cast<uint32_t>(n);
    if (parseNumber(value(ClickCount), 0, 1000000, n)) result.click.count = static_cast<uint32_t>(n);
    if (parseNumber(value(ClickX), -100000, 100000, n)) result.click.x = n;
    if (parseNumber(value(ClickY), -100000, 100000, n)) result.click.y = n;
    if (parseNumber(value(StartDelay), 0, 60000, n)) result.startDelayMs = static_cast<uint32_t>(n);
    result.click.button = static_cast<Button>(SendMessageW(control(ClickButton), CB_GETCURSEL, 0, 0));
    result.click.fixed = SendMessageW(control(ClickPosition), CB_GETCURSEL, 0, 0) == 1;
    result.page = page;
    return result;
}
void App::schedulePreferences() {
    if (preferencesReady && !refreshing && !smoke) SetTimer(hwnd, 2, 600, nullptr);
}
bool App::persistPreferences() {
    if (!preferencesReady || smoke) return true;
    KillTimer(hwnd, 2);
    auto latest = collectPreferences(); std::wstring problem;
    if (!savePreferences(settingsFile, latest, problem)) { notice = problem; InvalidateRect(hwnd, nullptr, FALSE); return false; }
    preferences = std::move(latest); return true;
}
bool App::applyHotkeyEdits(bool feedback) {
    if (shortcutsSuspended) return false;
    Hotkeys candidate;
    auto fail = [&](const std::wstring& message) { notice = message; if (feedback) error(message); return false; };
    for (int i = 0; i < 4; ++i) if (!parseHotkey(value(HotkeyClick + i), candidate[i]))
        return fail(L"Invalid shortcut. Choose a key or combination. F12 and Alt+F4 are reserved.");
    std::wstring problem;
    if (!validHotkeys(candidate, problem)) return fail(problem);
    for (const auto& step : macro.steps) if (conflictsWithHotkeys(step, candidate))
        return fail(L"The key " + keyName(step.key) + L" is already used in your macro. Choose another key for this shortcut.");
    size_t failed = 0;
    if (!shortcutRegistry->apply(candidate, failed))
        return fail(keyName(candidate[failed].key, candidate[failed].modifiers) + L" is already used by another application. Your previous shortcuts are still active.");
    preferences.hotkeys = candidate;
    for (int i = 0; i < 4; ++i) { hotkeys[i] = shortcutRegistry->active(i); set(HotkeyClick + i, shortcutName(i)); }
    notice = L"Your shortcuts are ready.";
    persistPreferences(); layout(); return true;
}
