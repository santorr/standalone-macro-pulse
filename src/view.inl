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
void App::paint(HDC target) {
    RECT bounds{}; GetClientRect(hwnd, &bounds);
    HDC dc = CreateCompatibleDC(target); HBITMAP bitmap = CreateCompatibleBitmap(target, bounds.right, bounds.bottom);
    auto old = SelectObject(dc, bitmap);
    SetDCBrushColor(dc, Bg); FillRect(dc, &bounds, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    box(dc, 0, 0, 228, height, Sidebar, 0);
    box(dc, 227, 0, 1, height, RGB(40, 35, 56), 0);
    if (brandIcon) DrawIconEx(dc, s(19), s(31), brandIcon, s(48), s(48), 0, nullptr, DI_NORMAL);
    text(dc, L"MacroPulse", 76, 35, 143, 32, Text, 2);
    text(dc, L"VOTRE ESPACE", 28, 111, 170, 24, Muted, 3);
    const int shortcutY = height - 252;
    text(dc, L"Raccourcis", 28, shortcutY, 178, 28, Text);
    const wchar_t* shortcutLabels[] = {L"Auto-clic", L"Macro", L"Arrêter", L"Position"};
    for (int i = 0; i < 4; ++i) {
        int y = shortcutY + 44 + i * 36;
        box(dc, 98, y, 108, 25, Field, 5);
        text(dc, shortcutName(i), 98, y, 108, 25, hotkeys[i] ? (i == 2 ? Success : Text) : Danger, 3, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        text(dc, shortcutLabels[i], 28, y, 66, 25, Muted, 3);
    }
    box(dc, 28, height - 42, 7, 7, Success, 4);
    text(dc, L"À vous de jouer", 45, height - 50, 155, 23, Muted, 3);

    int x = 252, available = width - x - 32;
    text(dc, L"AUTOMATISATION", x, 22, available - 180, 21, Accent, 3);
    text(dc, page == 2 ? L"Préférences" : page == 1 ? macroName() : page == 3 ? L"Mes macros" : L"Auto-clicker", x, 45, available - 200, 44, Text, 1);
    text(dc, page == 2 ? L"À votre façon. À chaque lancement." : page == 3 ? L"Toutes vos séquences, toujours à portée de main." : page == 1 ? L"Composez vos actions. Elles sont sauvegardées automatiquement." : L"Gardez le rythme. On s'occupe des clics.", x, 91, available, 24, Muted);
    auto snapshot = engine.snapshot(); bool active = snapshot.state != RunState::Idle;
    if (page != 3) {
    box(dc, width - 168, 44, 136, 34, active ? RGB(34, 43, 49) : Panel, 17);
    box(dc, width - 153, 57, 7, 7, active ? Success : Accent, 4);
    text(dc, snapshot.state == RunState::Countdown ? L"Préparez-vous" : active ? L"En cours" : !hotkeys[2] ? L"À configurer" : L"Prêt à lancer", width - 137, 44, 96, 34, !hotkeys[2] ? Danger : active ? Success : Text, 3);
    }

    if (page == 0) {
        const int cw = available * 3 / 5, rx = x + cw + 20, rw = available - cw - 20;
        card(dc, x, 138, cw, 416);
        box(dc, x + 24, 157, 29, 29, RGB(48, 36, 78), 8); glyph(dc, 0, x + 29, 162, 19, Accent);
        text(dc, L"Réglages du clic", x + 65, 154, cw - 89, 34, Text, 2);
        text(dc, L"Bouton de souris", x + 24, 189, cw - 48, 22, Muted, 3);
        text(dc, L"Intervalle", x + 24, 273, 180, 24, Muted, 3);
        text(dc, L"Nombre de clics", x + 40 + (cw - 64) / 2, 273, 190, 24, Muted, 3);
        text(dc, L"millisecondes entre deux clics", x + 24, 343, (cw - 64) / 2, 21, Muted, 3);
        text(dc, L"0 pour continuer sans limite", x + 40 + (cw - 64) / 2, 343, (cw - 64) / 2, 21, Muted, 3);
        text(dc, L"Où cliquer ?", x + 24, 365, cw - 48, 22, Muted, 3);
        bool fixed = SendMessageW(control(ClickPosition), CB_GETCURSEL, 0, 0) == 1;
        if (fixed) {
            text(dc, L"Position X", x + 24, 449, 180, 22, Muted, 3);
            text(dc, L"Position Y", x + 40 + (cw - 64) / 2, 449, 180, 22, Muted, 3);
            text(dc, shortcutName(3) + L" pour choisir la position de votre curseur", x + 24, 524, cw - 48, 20, Muted, 3);
        } else {
            glyph(dc, 4, x + 24, 467, 26, Accent);
            text(dc, L"Le clic vous suit", x + 64, 459, cw - 88, 25, Text);
            text(dc, L"Placez simplement le curseur sur votre cible.", x + 64, 487, cw - 88, 25, Muted, 3);
        }

        theme::gradient(dc, float(s(rx)), float(s(138)), float(s(rw)), float(s(260)), float(s(14)), RGB(49, 34, 84), Panel, RGB(75, 51, 121));
        // An abstract mouse silhouette makes the cadence card immediately recognizable.
        glyph(dc, 0, rx + rw - 113, 169, 92, RGB(75, 56, 113));
        text(dc, L"VOTRE RYTHME", rx + 22, 157, rw - 44, 24, RGB(192, 175, 231), 3);
        int interval = 0; std::wstring rate = L"—";
        if (parseNumber(value(ClickInterval), 1, 60000, interval)) {
            wchar_t buffer[64]; double cps = 1000.0 / interval;
            swprintf_s(buffer, std::abs(cps - std::round(cps)) < .001 ? L"%.0f" : L"%.1f", cps); rate = buffer;
        }
        text(dc, rate, rx + 22, 188, rw - 44, 72, Text, 4);
        text(dc, L"clics / seconde", rx + 24, 260, rw - 48, 27, RGB(195, 180, 224));
        text(dc, L"Choisissez un rythme", rx + 24, 305, rw - 48, 23, Muted, 3);
        card(dc, rx, 418, rw, 136);
        text(dc, L"Cette session", rx + 22, 431, rw - 44, 28, Text);
        const int half = (rw - 44) / 2;
        text(dc, std::to_wstring(snapshot.actions), rx + 22, 469, half, 34, Text, 2);
        wchar_t elapsed[64]; swprintf_s(elapsed, L"%.1f s", snapshot.elapsedSeconds);
        text(dc, elapsed, rx + 22 + half, 469, half, 34, Text, 2);
        text(dc, L"Actions", rx + 22, 510, half, 23, Muted, 3);
        text(dc, L"Temps écoulé", rx + 22 + half, 510, half, 23, Muted, 3);

        if (height >= 780) {
            card(dc, x, 574, available, 100);
            box(dc, x + 24, 598, 50, 50, RGB(45, 34, 72), 12); glyph(dc, 1, x + 37, 611, 24, Accent);
            text(dc, L"Un clic n'est que le début", x + 94, 592, available - 318, 30, Text, 2);
            text(dc, L"Enchaînez des touches, des clics et des pauses.", x + 94, 629, available - 318, 22, Muted, 3);
        }
    } else if (page == 1) {
        text(dc, L"Répétitions", width - 224, 122, 96, 29, Muted, 3);
        const int tableHeight = height - 530;
        card(dc, x, 174, available, tableHeight);
        text(dc, L"#", x + 22, 181, 34, 26, Muted, 3);
        text(dc, L"ACTION", x + 62, 181, 190, 26, Muted, 3);
        text(dc, L"ATTENTE", x + 263, 181, 102, 26, Muted, 3);
        text(dc, L"DÉTAILS", x + 373, 181, available - 397, 26, Muted, 3);
        const int editorY = height - 330;
        card(dc, x, editorY, available, 234);
        text(dc, selected() < 0 ? L"Nouvelle action" : L"Modifier l'action " + std::to_wstring(selected() + 1), x + 20, editorY + 5, 320, 25, Accent);
        text(dc, L"Action", x + 20, editorY + 31, 218, 23, Muted, 3);
        auto type = static_cast<Action>(SendMessageW(control(StepType), CB_GETCURSEL, 0, 0));
        text(dc, type == Action::Wait ? L"Durée (ms)" : L"Attente avant (ms)", x + 254, editorY + 31, 148, 23, Muted, 3);
        if (type != Action::Move && type != Action::Wait)
            text(dc, type == Action::Click ? L"Bouton de souris" : type == Action::Scroll ? L"Crans (+ haut / − bas)" : L"Touche ou raccourci", x + 414, editorY + 31, available - 434, 23, Muted, 3);
        if (type == Action::Click || type == Action::Move) {
            text(dc, L"Position", x + 20, editorY + 104, 218, 23, Muted, 3);
            text(dc, L"X", x + 254, editorY + 104, 142, 23, Muted, 3);
            text(dc, L"Y", x + 414, editorY + 104, 142, 23, Muted, 3);
            text(dc, shortcutName(3) + L" : capturer", x + 572, editorY + 131, available - 592, 30, Muted, 3);
        } else {
            glyph(dc, type == Action::Wait ? 2 : 5, x + 22, editorY + 119, 25, Accent);
            text(dc, type == Action::Wait ? L"Un temps de pause avant de continuer." : type == Action::Key ? L"Cliquez sur la touche, puis appuyez sur votre combinaison." : type == Action::Scroll ? L"Un nombre positif monte, un nombre négatif descend." : L"Cliquez sur la touche, puis appuyez et relâchez une touche seule.", x + 60, editorY + 109, available - 80, 27, Text);
            if (type == Action::KeyDown || type == Action::KeyUp)
                text(dc, L"Les touches sont relâchées à l'arrêt et en fin de boucle.", x + 60, editorY + 139, available - 80, 22, Muted, 3);
        }
    } else if (page == 3) {
        int libraryWidth = available * 54 / 100, detailX = x + libraryWidth + 20, detailWidth = available - libraryWidth - 20;
        card(dc, x, 138, libraryWidth, height - 238);
        text(dc, L"VOTRE BIBLIOTHÈQUE", x + 24, 153, libraryWidth - 90, 28, Muted, 3);
        text(dc, std::to_wstring(library.entries.size()), x + libraryWidth - 58, 153, 34, 28, Accent, 2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        card(dc, detailX, 138, detailWidth, 358);
        glyph(dc, 1, detailX + 20, 159, 24, Accent);
        text(dc, currentMacro() ? L"Votre sélection" : L"À vous de créer", detailX + 58, 153, detailWidth - 78, 32, Text, 2);
        text(dc, L"Nom de la macro", detailX + 20, 194, detailWidth - 40, 23, Muted, 3);
        text(dc, currentMacro() ? std::to_wstring(macro.steps.size()) + L" actions dans la séquence" : L"Créez votre première macro.", detailX + 20, 280, detailWidth - 40, 28, Text);
        text(dc, L"Sélectionnez, personnalisez, lancez.", detailX + 20, 317, detailWidth - 40, 23, Muted, 3);
        text(dc, dirty ? L"Sauvegarde en attente…" : libraryWritable ? L"Sauvegarde automatique" : L"Bibliothèque indisponible", detailX + 20, 463, detailWidth - 40, 22, dirty || !libraryWritable ? Danger : Success, 3);
    } else {
        card(dc, x, 138, available, 414);
        glyph(dc, 5, x + 24, 158, 25, Accent);
        text(dc, L"Raccourcis clavier", x + 62, 152, available - 86, 34, Text, 2);
        const wchar_t* names[] = {L"Auto-clicker", L"Exécuter la macro", L"Tout arrêter", L"Capturer la position"};
        const wchar_t* descriptions[] = {L"Lancer ou arrêter les clics", L"Lancer ou arrêter votre séquence", L"Interrompre immédiatement l'exécution", L"Choisir la position du curseur"};
        for (int i = 0; i < 4; ++i) {
            int y = 206 + i * 62;
            text(dc, names[i], x + 24, y, available - 346, 24, i == 2 ? Success : Text);
            text(dc, hotkeys[i] ? descriptions[i] : L"Raccourci indisponible : choisissez-en un autre", x + 24, y + 25, available - 346, 23, hotkeys[i] ? Muted : Danger, 3);
        }
        text(dc, L"Cliquez sur un raccourci, appuyez sur vos touches, puis appliquez. Échap annule.", x + 24, 447, available - 48, 23, Muted, 3);
        if (height >= 780) {
            card(dc, x, 574, available, 100);
            glyph(dc, 1, x + 26, 606, 28, Accent);
            text(dc, L"Retrouvez votre espace", x + 76, 590, available - 100, 30, Text, 2);
            text(dc, L"Vos réglages et votre bibliothèque sont retrouvés au lancement.", x + 76, 629, available - 100, 23, Muted, 3);
        }
    }

    // Edits are inset into rounded shells; the native text remains selectable and accessible.
    for (const auto& w : widgets) {
        if (w.page >= 0 && w.page != page) continue;
        if (!(GetWindowLongPtrW(w.hwnd, GWL_STYLE) & WS_VISIBLE)) continue;
        wchar_t klass[32]{}; GetClassNameW(w.hwnd, klass, 32); if (lstrcmpiW(klass, L"EDIT") != 0) continue;
        RECT rect{}; GetWindowRect(w.hwnd, &rect); MapWindowPoints(nullptr, hwnd, reinterpret_cast<POINT*>(&rect), 2);
        theme::surface(dc, float(rect.left - s(12)), float(rect.top - s(10)), float(rect.right - rect.left + s(24)), float(rect.bottom - rect.top + s(18)), float(s(8)), Field, GetFocus() == w.hwnd ? Accent : Line);
    }
    if (page != 2) text(dc, L"Départ différé (ms)", x, height - 67, 154, 32, Muted, 3);
    else text(dc, L"Vos réglages sont mémorisés automatiquement.", x, height - 67, available - 170, 32, Muted, 3);
    std::wstring bottom = notice;
    if (active) bottom = snapshot.state == RunState::Countdown ? L"Placez-vous sur votre cible…" : L"En cours · " + shortcutName(2) + L" pour arrêter";
    text(dc, bottom, x, height - 25, available, 20, failureShown ? Danger : Muted, 3);
    BitBlt(target, 0, 0, bounds.right, bounds.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, old); DeleteObject(bitmap); DeleteDC(dc);
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
    auto parentBrush = CreateSolidBrush(parentBackground); FillRect(item.hDC, &item.rcItem, parentBrush); DeleteObject(parentBrush);
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
    auto brush = CreateSolidBrush(Panel); FillRect(dc, &r, brush); DeleteObject(brush);
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
    auto app = reinterpret_cast<App*>(data); wchar_t klass[32]{}; GetClassNameW(c, klass, 32);
    if (msg == WM_ERASEBKGND) {
        // Native controls must never clear with the system's light theme first.
        int id = GetDlgCtrlID(c);
        COLORREF background = id == StepList || id == LibraryList ? Panel : id == NavClick || id == NavMacro || id == NavSettings ? Sidebar : Field;
        RECT rect{}; GetClientRect(c, &rect); auto dc = reinterpret_cast<HDC>(wp);
        SetDCBrushColor(dc, background); FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH))); return 1;
    }
    if (msg == WM_KILLFOCUS && app->keyCaptureTarget == GetDlgCtrlID(c)) app->endKeyCapture(L"Capture annulée.");
    if (GetDlgCtrlID(c) == StepList && app->macro.steps.empty() && (msg == WM_PAINT || msg == WM_PRINT || msg == WM_PRINTCLIENT)) {
        PAINTSTRUCT ps{}; HDC dc = msg == WM_PAINT ? BeginPaint(c, &ps) : reinterpret_cast<HDC>(wp);
        RECT rect{}; GetClientRect(c, &rect); int w = MulDiv(rect.right, 96, app->dpi), h = MulDiv(rect.bottom, 96, app->dpi);
        app->box(dc, 0, 0, w, h, Panel, 0);
        int center = h >= 140 ? h / 2 : std::max(12, h / 2 - 18);
        if (h >= 140) { app->box(dc, w / 2 - 25, center - 68, 50, 50, RGB(44, 33, 72), 12); app->glyph(dc, 1, w / 2 - 12, center - 55, 24, Accent); }
        app->text(dc, L"Votre première macro commence ici", 10, center - 4, w - 20, 28, Text, 0, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        app->text(dc, L"Choisissez une action ci-dessous pour l'ajouter.", 10, center + 28, w - 20, 23, Muted, 3, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        if (msg == WM_PAINT) EndPaint(c, &ps);
        return 0;
    }
    bool combo = lstrcmpiW(klass, L"COMBOBOX") == 0;
    if (combo && msg == WM_PAINT) { PAINTSTRUCT ps{}; auto dc = BeginPaint(c, &ps); app->drawCombo(c, dc); EndPaint(c, &ps); return 0; }
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
