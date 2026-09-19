SavedMacro* App::currentMacro() {
    auto found = std::find_if(library.entries.begin(), library.entries.end(), [&](const SavedMacro& entry) { return entry.id == library.selected; });
    return found == library.entries.end() ? nullptr : &*found;
}
std::wstring App::macroName() const {
    for (const auto& entry : library.entries) if (entry.id == library.selected) return entry.name;
    return L"No macro";
}
void App::restoreLibrary() {
    libraryReady = false; libraryWritable = true; dirty = false; library = {}; macro = {};
    if (smoke) {
        if (sessionTest) libraryFile = sessionTestFile.parent_path() / L"library.dat";
    } else if (!settingsFile.empty()) libraryFile = settingsFile.parent_path() / L"library.dat";
    std::wstring problem; std::error_code ec;
    auto backup = libraryFile; backup += L".bak";
    bool existing = !libraryFile.empty() && std::filesystem::exists(libraryFile, ec);
    bool hasBackup = !libraryFile.empty() && std::filesystem::exists(backup, ec);
    if ((!smoke || sessionTest == 2) && (existing || hasBackup)) {
        if (!loadLibrary(libraryFile, library, problem)) {
            if (loadLibrary(backup, library, problem)) { notice = L"Your library was recovered from the automatic backup."; dirty = true; }
            else { libraryWritable = false; notice = problem; }
        }
    } else {
        Macro recovered; auto name = L"My first macro";
        std::wstring recoveredName = name;
        if (!preferences.lastMacro.empty()) {
            if (loadMacro(preferences.lastMacro, recovered, problem)) {
                recoveredName = cleanMacroName(preferences.lastMacro.stem().wstring());
                if (recoveredName.empty()) recoveredName = name;
                notice = L"Your last macro was added to the library.";
            } else notice = L"The old macro could not be recovered. Its file has not been changed.";
        }
        addLibraryMacro(library, recoveredName, recovered, problem); dirty = true;
    }
    libraryReady = true;
    if (auto current = currentMacro()) macro = current->macro;
    refreshing = true; set(RepeatCount, std::to_wstring(macro.repeats)); set(MacroName, macroName()); refreshing = false;
    refreshLibrary(); refreshList(macro.steps.empty() ? -1 : 0);
    if (page == 1 && !currentMacro()) page = 3;
    if (!smoke && libraryWritable && dirty) persistLibrary();
}
void App::syncCurrentMacro() {
    if (!libraryReady || refreshing) return;
    auto current = currentMacro(); if (!current) return;
    int repeats;
    if (parseNumber(value(RepeatCount), 0, 1000000, repeats)) macro.repeats = static_cast<uint32_t>(repeats);
    auto name = cleanMacroName(value(MacroName)); if (!name.empty()) current->name = name;
    current->macro = macro;
    InvalidateRect(control(LibraryList), nullptr, FALSE);
}
bool App::persistLibrary() {
    KillTimer(hwnd, 3);
    if (!libraryReady || !libraryWritable || !dirty) return true;
    syncCurrentMacro(); std::wstring problem;
    if ((!smoke || sessionTest) && !saveLibrary(libraryFile, library, problem)) {
        notice = problem; InvalidateRect(hwnd, nullptr, FALSE); return false;
    }
    dirty = false;
    preferences.lastMacro.clear();
    InvalidateRect(hwnd, nullptr, FALSE); return true;
}
void App::refreshLibrary() {
    bool previous = refreshing; refreshing = true;
    auto list = control(LibraryList);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < library.entries.size(); ++i) {
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(library.entries[i].name.c_str()));
        if (library.entries[i].id == library.selected) SendMessageW(list, LB_SETCURSEL, i, 0);
    }
    set(MacroName, currentMacro() ? macroName() : L"");
    refreshing = previous;
}
void App::selectMacro(uint64_t id) {
    if (engine.snapshot().state != RunState::Idle || id == library.selected) return;
    syncCurrentMacro();
    auto found = std::find_if(library.entries.begin(), library.entries.end(), [&](const SavedMacro& entry) { return entry.id == id; });
    if (found == library.entries.end()) return;
    library.selected = id; macro = found->macro;
    refreshing = true; set(RepeatCount, std::to_wstring(macro.repeats)); set(MacroName, found->name); refreshing = false;
    refreshLibrary(); refreshList(macro.steps.empty() ? -1 : 0); setDirty(true); persistLibrary();
    if (!dirty) notice = L"Selected macro: " + macroName();
    editorState(); InvalidateRect(hwnd, nullptr, FALSE);
}
void App::createMacro(bool duplicate) {
    if (!libraryWritable || (duplicate && !currentMacro())) return;
    syncCurrentMacro(); std::wstring problem;
    std::wstring name = duplicate ? macroName().substr(0, 70) + L" — copy" : L"New macro";
    auto base = name; int suffix = 2;
    while (std::any_of(library.entries.begin(), library.entries.end(), [&](const SavedMacro& e) { return e.name == name; })) name = base + L" " + std::to_wstring(suffix++);
    if (!addLibraryMacro(library, name, duplicate ? macro : Macro{}, problem)) { error(problem); return; }
    macro = currentMacro()->macro;
    refreshing = true; set(RepeatCount, std::to_wstring(macro.repeats)); set(MacroName, name); refreshing = false;
    refreshLibrary(); refreshList(macro.steps.empty() ? -1 : 0); setDirty(true); persistLibrary();
    page = 3; layout(); SetFocus(control(MacroName)); SendMessageW(control(MacroName), EM_SETSEL, 0, -1);
    if (!dirty) notice = duplicate ? L"Macro duplicated. Give it a name." : L"Name your macro, then click Edit macro.";
}
void App::removeMacro(bool confirm) {
    if (!libraryWritable || !currentMacro()) return;
    if (confirm && pulse::messageBox(hwnd, (L"Delete “" + macroName() + L"” from your library?").c_str(), L"MacroPulse", MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES) return;
    auto index = std::find_if(library.entries.begin(), library.entries.end(), [&](const SavedMacro& e) { return e.id == library.selected; });
    auto offset = static_cast<size_t>(index - library.entries.begin());
    library.entries.erase(index);
    library.selected = library.entries.empty() ? 0 : library.entries[std::min(offset, library.entries.size() - 1)].id;
    macro = currentMacro() ? currentMacro()->macro : Macro{};
    refreshing = true; set(RepeatCount, std::to_wstring(macro.repeats)); refreshing = false;
    refreshLibrary(); refreshList(macro.steps.empty() ? -1 : 0); setDirty(true); persistLibrary();
    page = 3; layout(); if (!dirty) notice = L"Macro deleted.";
}
void App::drawLibraryRow(const DRAWITEMSTRUCT& item) {
    if (item.itemID >= library.entries.size()) return;
    const auto& entry = library.entries[item.itemID];
    int top = MulDiv(item.rcItem.top, 96, dpi), w = MulDiv(item.rcItem.right, 96, dpi);
    bool selectedEntry = (item.itemState & ODS_SELECTED) != 0;
    SetDCBrushColor(item.hDC, Panel); FillRect(item.hDC, &item.rcItem, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    box(item.hDC, 2, top + 3, w - 4, 68, selectedEntry ? RGB(53, 38, 88) : Panel, 10);
    glyph(item.hDC, 1, 16, top + 24, 24, selectedEntry ? Accent : Muted);
    text(item.hDC, entry.name, 54, top + 10, w - 70, 28, Text);
    auto detail = entry.macro.steps.empty() ? L"Ready to create" : std::to_wstring(entry.macro.steps.size()) + L" actions · " + (entry.macro.repeats ? std::to_wstring(entry.macro.repeats) + L" repetition(s)" : L"Continuous");
    text(item.hDC, detail, 54, top + 39, w - 70, 21, Muted, 3);
}
