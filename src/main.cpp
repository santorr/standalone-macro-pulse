#include "engine.hpp"
#include "preferences.hpp"
#include "library.hpp"
#include "theme.hpp"
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace pulse;
namespace {
constexpr COLORREF Bg = RGB(15, 14, 25), Sidebar = RGB(20, 18, 34), Panel = RGB(26, 24, 43);
constexpr COLORREF Field = RGB(35, 32, 55), Line = RGB(51, 46, 74), Text = RGB(244, 242, 255);
constexpr COLORREF Muted = RGB(155, 150, 176), Accent = RGB(156, 120, 255), Danger = RGB(248, 130, 157);
constexpr COLORREF Purple = RGB(119, 77, 233), Success = RGB(107, 220, 180);
enum Id {
    NavClick = 100, NavMacro, Start, Stop, ClickButton, ClickInterval, ClickCount, ClickPosition, ClickX, ClickY, StartDelay,
    NewMacro, EditMacro, CopyMacro, RemoveMacro, StepList, StepType, StepDelay, StepKey, StepButton, StepPosition, StepX, StepY, StepWheel,
    AddStep, UpdateStep, DeleteStep, UpStep, DownStep, DuplicateStep, RepeatCount,
    MouseLeft, MouseRight, MouseMiddle, FollowCursor, FixedPoint, PresetSlow, PresetNormal, PresetFast, CreateMacro,
    NavSettings, HotkeyClick, HotkeyMacro, HotkeyStop, HotkeyCapture, ApplyHotkeys, DefaultKeys,
    LibraryList, MacroName, BackLibrary
};
struct Widget { HWND hwnd; int id; int page; };
class App {
public:
    HWND hwnd = nullptr;
    bool smoke = false;
    std::filesystem::path previewDirectory;
    int smokeExit = 0;
    int sessionTest = 0;
    std::filesystem::path sessionTestFile;
    App() = default;
    ~App() { for (auto f : fonts) if (f) DeleteObject(f); DeleteObject(fieldBrush); if (brandIcon) DestroyIcon(brandIcon); }
    LRESULT message(UINT msg, WPARAM wp, LPARAM lp);
    bool captureMessage(const MSG& message);
private:
    Engine engine;
    Macro macro;
    Library library;
    std::filesystem::path libraryFile;
    bool libraryReady = false, libraryWritable = true;
    SavedMacro* currentMacro();
    void restoreLibrary();
    void refreshLibrary();
    void selectMacro(uint64_t id);
    void syncCurrentMacro();
    bool persistLibrary();
    void createMacro(bool duplicate);
    void removeMacro(bool confirm = true);
    void drawLibraryRow(const DRAWITEMSTRUCT& item);
    std::wstring macroName() const;
    bool dirty = false, refreshing = false, lastActive = false, failureShown = false;
    bool hotkeys[4]{};
    Preferences preferences;
    std::filesystem::path settingsFile;
    bool preferencesReady = false;
    bool arranging = false;
    bool keyboardNavigation = false;
    std::unique_ptr<HotkeyRegistry> shortcutRegistry;
    int keyCaptureTarget = 0;
    bool shortcutsSuspended = false;
    HWND textInput = nullptr;
    void beginTextInput(HWND input);
    std::array<bool, 256> captureHeld{};
    uint16_t loneModifier = 0;
    void beginKeyCapture(int id);
    void endKeyCapture(const std::wstring& message);
    void resumeShortcuts();
    void acceptCapturedKey(uint16_t key, uint8_t modifiers);
    std::wstring bindingLabel(int id) const;
    int page = 0, width = 1180, height = 820, dpi = 96;
    HFONT fonts[5]{};
    HICON brandIcon = nullptr;
    HBRUSH fieldBrush = CreateSolidBrush(Field);
    std::vector<Widget> widgets;
    std::wstring notice;
    int s(int v) const { return MulDiv(v, dpi, 96); }
    HWND control(int id) const { return GetDlgItem(hwnd, id); }
    void create();
    void makeFonts();
    HWND add(int id, const wchar_t* klass, const wchar_t* text, DWORD style, int widgetPage);
    void combo(int id, const std::vector<std::wstring>& items);
    void layout();
    void paint(HDC dc);
    void drawButton(const DRAWITEMSTRUCT& item);
    void drawCombo(HWND control, HDC dc);
    void drawRow(const DRAWITEMSTRUCT& item);
    static LRESULT CALLBACK widgetProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
    void text(HDC dc, const std::wstring& str, int x, int y, int w, int h, COLORREF color = Text, int font = 0, UINT flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    void box(HDC dc, int x, int y, int w, int h, COLORREF color, int radius = 14);
    void card(HDC dc, int x, int y, int w, int h);
    void glyph(HDC dc, int kind, int x, int y, int size, COLORREF color);
    void exportPreview(const wchar_t* name);
    std::wstring value(int id) const;
    void set(int id, const std::wstring& v) { SetWindowTextW(control(id), v.c_str()); }
    bool number(int id, int lo, int hi, int& out, const wchar_t* name);
    void error(const std::wstring& errorText) { MessageBoxW(hwnd, errorText.c_str(), L"MacroPulse", MB_OK | MB_ICONWARNING); }
    void command(int id);
    void refreshList(int select = -1);
    int selected() const { return ListView_GetNextItem(control(StepList), -1, LVNI_SELECTED); }
    void populate();
    void editorState();
    bool readStep(Step& step);
    bool readRepeats();
    void start(int mode);
    void stop();
    void tick();
    void setDirty(bool v);
    void capture();
    void runSmoke();
    void checkRendering();
    void restorePreferences();
    void populatePreferences();
    Preferences collectPreferences() const;
    void schedulePreferences();
    bool persistPreferences();
    bool applyHotkeyEdits(bool feedback = true);
    std::wstring shortcutName(size_t action) const { auto key = preferences.hotkeys[action]; return keyName(key.key, key.modifiers); }
};
LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        app = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        app->hwnd = hwnd; SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    return app ? app->message(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}
void App::makeFonts() {
    for (auto f : fonts) if (f) DeleteObject(f);
    int sizes[] = {14, 32, 19, 12, 54};
    for (int i = 0; i < 5; ++i)
        fonts[i] = CreateFontW(-s(sizes[i]), 0, 0, 0, (i == 1 || i == 2 || i == 4) ? FW_SEMIBOLD : FW_NORMAL,
                              FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH, L"Segoe UI");
    for (const auto& w : widgets) SendMessageW(w.hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(fonts[0]), TRUE);
    if (brandIcon) DestroyIcon(brandIcon);
    brandIcon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101), IMAGE_ICON, s(48), s(48), 0));
}
HWND App::add(int id, const wchar_t* klass, const wchar_t* label, DWORD style, int widgetPage) {
    HWND c = CreateWindowExW(0, klass, label, WS_CHILD | WS_TABSTOP | style,
                            0, 0, 1, 1, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    widgets.push_back({c, id, widgetPage});
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(fonts[0]), TRUE);
    if (std::wstring(klass) == L"EDIT") { SendMessageW(c, EM_SETLIMITTEXT, 64, 0); SendMessageW(c, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, 0); }
    if (std::wstring(klass) == L"COMBOBOX") {
        SetWindowTheme(c, L"", L""); SendMessageW(c, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), s(34));
        SendMessageW(c, CB_SETITEMHEIGHT, 0, s(34));
    }
    SetWindowSubclass(c, widgetProc, 1, reinterpret_cast<DWORD_PTR>(this));
    return c;
}
void App::combo(int id, const std::vector<std::wstring>& items) {
    for (const auto& item : items) SendMessageW(control(id), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
    SendMessageW(control(id), CB_SETCURSEL, 0, 0);
}
void App::create() {
    dpi = static_cast<int>(GetDpiForWindow(hwnd)); makeFonts();
    BOOL dark = TRUE; DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
    COLORREF caption = Sidebar; DwmSetWindowAttribute(hwnd, 35, &caption, sizeof(caption));
    auto button = [&](int id, const wchar_t* t, int p) { add(id, L"BUTTON", t, BS_OWNERDRAW, p); };
    auto edit = [&](int id, const wchar_t* t, int p) { add(id, L"EDIT", t, ES_AUTOHSCROLL, p); };
    auto drop = [&](int id, int p) { add(id, L"COMBOBOX", L"", CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL, p); };
    button(NavClick, L"Auto-clicker", -1); button(NavMacro, L"Mes macros", -1);
    button(NavSettings, L"Préférences", -1);
    button(Start, L"Démarrer", -1); button(Stop, L"Arrêter", -1);
    edit(StartDelay, L"1500", -1);
    drop(ClickButton, 0); combo(ClickButton, {L"Clic gauche", L"Clic droit", L"Clic milieu"});
    edit(ClickInterval, L"100", 0); edit(ClickCount, L"0", 0);
    drop(ClickPosition, 0); combo(ClickPosition, {L"Suivre le curseur", L"Position fixe"});
    edit(ClickX, L"0", 0); edit(ClickY, L"0", 0);
    button(MouseLeft, L"Gauche", 0); button(MouseRight, L"Droit", 0); button(MouseMiddle, L"Milieu", 0);
    button(FollowCursor, L"Suivre le curseur", 0); button(FixedPoint, L"Position fixe", 0);
    button(PresetSlow, L"5 / s", 0); button(PresetNormal, L"10 / s", 0); button(PresetFast, L"50 / s", 0);
    button(CreateMacro, L"Créer une macro", 0);
    button(NewMacro, L"Nouvelle macro", 3); button(EditMacro, L"Modifier la macro", 3);
    button(CopyMacro, L"Dupliquer", 3); button(RemoveMacro, L"Supprimer", 3);
    button(BackLibrary, L"‹  Mes macros", 1); edit(MacroName, L"", 3);
    SendMessageW(control(MacroName), EM_SETLIMITTEXT, 80, 0);
    add(LibraryList, L"LISTBOX", L"Bibliothèque de macros", LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL, 3);
    SetWindowTheme(control(LibraryList), L"", L""); SendMessageW(control(LibraryList), LB_SETITEMHEIGHT, 0, s(76));
    edit(RepeatCount, L"1", 1);
    auto list = add(StepList, WC_LISTVIEWW, L"Étapes de la macro", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_OWNERDRAWFIXED | LVS_NOCOLUMNHEADER, 1);
    SetWindowSubclass(list, widgetProc, 1, reinterpret_cast<DWORD_PTR>(this));
    ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    ListView_SetBkColor(list, Panel); ListView_SetTextBkColor(list, Panel); ListView_SetTextColor(list, Text);
    SetWindowTheme(list, L"", L"");
    const wchar_t* names[] = {L"N°", L"Action", L"Délai avant", L"Paramètres"};
    for (int i = 0; i < 4; ++i) { LVCOLUMNW col{}; col.mask = LVCF_TEXT | LVCF_WIDTH; col.pszText = const_cast<wchar_t*>(names[i]); col.cx = s(150); ListView_InsertColumn(list, i, &col); }
    drop(StepType, 1);
    std::vector<std::wstring> actions; for (int i = 0; i < 7; ++i) actions.push_back(actionName(static_cast<Action>(i)));
    combo(StepType, actions);
    edit(StepDelay, L"100", 1); button(StepKey, L"Ctrl+C", 1);
    drop(StepButton, 1); combo(StepButton, {L"Clic gauche", L"Clic droit", L"Clic milieu"});
    drop(StepPosition, 1); combo(StepPosition, {L"Position actuelle", L"Position fixe"});
    edit(StepX, L"0", 1); edit(StepY, L"0", 1); edit(StepWheel, L"1", 1);
    button(AddStep, L"Ajouter", 1); button(UpdateStep, L"Appliquer", 1); button(DeleteStep, L"Supprimer", 1);
    button(UpStep, L"Monter", 1); button(DownStep, L"Descendre", 1); button(DuplicateStep, L"Dupliquer", 1);
    for (int i = 0; i < 4; ++i) button(HotkeyClick + i, L"", 2);
    button(ApplyHotkeys, L"Appliquer les raccourcis", 2); button(DefaultKeys, L"Valeurs par défaut", 2);
    restorePreferences();
    restoreLibrary();
    shortcutRegistry = std::make_unique<HotkeyRegistry>(
        [this](int id, const Hotkey& key) { return smoke || RegisterHotKey(hwnd, id, MOD_NOREPEAT | nativeModifiers(key), key.key) != FALSE; },
        [this](int id) { if (!smoke) UnregisterHotKey(hwnd, id); });
    shortcutRegistry->initialize(preferences.hotkeys);
    for (int i = 0; i < 4; ++i) hotkeys[i] = shortcutRegistry->active(i);
    if (std::any_of(std::begin(hotkeys), std::end(hotkeys), [](bool v) { return !v; })) {
        notice = L"Un raccourci est déjà utilisé. Choisissez une autre combinaison dans Préférences.";
        page = 2;
    }
    preferencesReady = true;
    SetTimer(hwnd, 1, 100, nullptr); layout(); editorState();
    if (dirty) SetTimer(hwnd, 3, 600, nullptr);
    if (smoke) PostMessageW(hwnd, WM_APP + 1, 0, 0);
}
void App::layout() {
    if (arranging || widgets.empty()) return;
    arranging = true;
    RECT r{}; GetClientRect(hwnd, &r); width = MulDiv(r.right, 96, dpi); height = MulDiv(r.bottom, 96, dpi);
    int x = 252, available = width - x - 32, listHeight = height - 530, editorY = 180 + listHeight + 20;
    auto place = [&](int id, int px, int py, int w, int h = 38) {
        wchar_t klass[32]{}; GetClassNameW(control(id), klass, 32);
        if (lstrcmpiW(klass, L"EDIT") == 0) { px += 12; py += 10; w -= 24; h -= 18; }
        SetWindowPos(control(id), nullptr, s(px), s(py), s(w), s(h), SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
    };
    place(NavClick, 16, 148, 196, 48); place(NavMacro, 16, 204, 196, 48);
    place(NavSettings, 16, 260, 196, 48);
    place(StartDelay, x + 158, height - 68, 86);
    place(Start, width - 374, height - 72, 184, 44); place(Stop, width - 178, height - 72, 146, 44);
    int cardW = (available * 3) / 5;
    place(ClickButton, x + 24, 214, cardW - 48, 220);
    place(ClickInterval, x + 24, 301, (cardW - 64) / 2);
    place(ClickCount, x + 40 + (cardW - 64) / 2, 301, (cardW - 64) / 2);
    place(ClickPosition, x + 24, 388, cardW - 48, 160);
    place(ClickX, x + 24, 475, (cardW - 64) / 2); place(ClickY, x + 40 + (cardW - 64) / 2, 475, (cardW - 64) / 2);
    int segment = (cardW - 64) / 3;
    place(MouseLeft, x + 24, 214, segment, 42); place(MouseRight, x + 32 + segment, 214, segment, 42); place(MouseMiddle, x + 40 + segment * 2, 214, segment, 42);
    place(FollowCursor, x + 24, 390, (cardW - 56) / 2, 42); place(FixedPoint, x + 32 + (cardW - 56) / 2, 390, (cardW - 56) / 2, 42);
    int rightX = x + cardW + 20, rightWidth = available - cardW - 20, presetWidth = (rightWidth - 60) / 3;
    place(PresetSlow, rightX + 22, 338, presetWidth); place(PresetNormal, rightX + 30 + presetWidth, 338, presetWidth); place(PresetFast, rightX + 38 + presetWidth * 2, 338, presetWidth);
    place(CreateMacro, width - 224, 605, 168, 40);
    place(BackLibrary, x, 118, 145);
    int libraryWidth = available * 54 / 100, detailX = x + libraryWidth + 20, detailWidth = available - libraryWidth - 20;
    place(NewMacro, width - 212, 43, 180, 42);
    place(LibraryList, x + 12, 194, libraryWidth - 24, height - 310);
    place(MacroName, detailX + 20, 220, detailWidth - 40);
    place(EditMacro, detailX + 20, 363, detailWidth - 40, 42);
    place(CopyMacro, detailX + 20, 419, (detailWidth - 52) / 2, 38);
    place(RemoveMacro, detailX + 32 + (detailWidth - 52) / 2, 419, (detailWidth - 52) / 2, 38);
    place(RepeatCount, width - 120, 118, 88);
    place(StepList, x + 8, 214, available - 16, listHeight - 40);
    int columns[] = {44, 205, 110, available - 402};
    for (int i = 0; i < 4; ++i) ListView_SetColumnWidth(control(StepList), i, s(columns[i]));
    place(StepType, x + 20, editorY + 55, 218, 300); place(StepDelay, x + 254, editorY + 55, 142);
    place(StepKey, x + 414, editorY + 55, available - 434); place(StepButton, x + 414, editorY + 55, available - 434, 200);
    place(StepWheel, x + 414, editorY + 55, available - 434);
    place(StepPosition, x + 20, editorY + 129, 218, 140);
    place(StepX, x + 254, editorY + 129, 142); place(StepY, x + 414, editorY + 129, 142);
    place(AddStep, x + 20, editorY + 181, 114); place(UpdateStep, x + 144, editorY + 181, 112);
    place(DuplicateStep, x + 266, editorY + 181, 108); place(UpStep, x + 384, editorY + 181, 38);
    place(DownStep, x + 430, editorY + 181, 38); place(DeleteStep, x + available - 132, editorY + 181, 112);
    for (int i = 0; i < 4; ++i) place(HotkeyClick + i, x + available - 300, 208 + i * 62, 270);
    place(ApplyHotkeys, x + 24, 478, 228, 42); place(DefaultKeys, x + 266, 478, 188, 42);
    const std::wstring startLabel = page ? L"Exécuter" : L"Démarrer";
    if (value(Start) != startLabel) set(Start, startLabel);
    editorState(); if (selected() >= 0) ListView_EnsureVisible(control(StepList), selected(), FALSE);
    arranging = false;
    RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}
#include "view.inl"
#include "preferences_ui.inl"
#include "key_capture.inl"
#include "render_checks.inl"
#include "library_ui.inl"
std::wstring App::value(int id) const {
    int len = GetWindowTextLengthW(control(id)); std::wstring result(static_cast<size_t>(len) + 1, L'\0');
    GetWindowTextW(control(id), result.data(), len + 1); result.resize(len); return result;
}
bool App::number(int id, int lo, int hi, int& out, const wchar_t* name) {
    if (parseNumber(value(id), lo, hi, out)) return true;
    error(std::wstring(name) + L" : entrez un entier entre " + std::to_wstring(lo) + L" et " + std::to_wstring(hi) + L".");
    SetFocus(control(id)); SendMessageW(control(id), EM_SETSEL, 0, -1); return false;
}
void App::setDirty(bool v) {
    dirty = v;
    if (v && libraryReady && !refreshing) { syncCurrentMacro(); SetTimer(hwnd, 3, 600, nullptr); }
}
void App::editorState() {
    bool active = engine.snapshot().state != RunState::Idle;
    bool clickFixed = SendMessageW(control(ClickPosition), CB_GETCURSEL, 0, 0) == 1;
    auto type = static_cast<Action>(SendMessageW(control(StepType), CB_GETCURSEL, 0, 0));
    bool mouse = type == Action::Click || type == Action::Move;
    bool fixed = type == Action::Move || SendMessageW(control(StepPosition), CB_GETCURSEL, 0, 0) == 1;
    int sel = selected();
    // Compute the final state once. Never show all page controls then hide the
    // conditional ones: overlapping native controls would paint between steps.
    for (const auto& w : widgets) {
        bool visible = w.page < 0 || w.page == page, enabled = !active;
        switch (w.id) {
        case NavClick: case NavMacro: case NavSettings: enabled = true; break;
        case Start: visible = page != 2; enabled &= (smoke || hotkeys[2]) && (page == 0 || (currentMacro() && !macro.steps.empty())); break;
        case StartDelay: visible = page != 2; break;
        case Stop: visible = page != 2 || active; enabled = active; break;
        case ClickButton: case ClickPosition: visible = false; break;
        case ClickX: case ClickY: visible &= clickFixed; enabled &= clickFixed; break;
        case CreateMacro: visible &= height >= 780; break;
        case StepKey: visible &= type == Action::Key || type == Action::KeyDown || type == Action::KeyUp; break;
        case StepButton: visible &= type == Action::Click; break;
        case StepWheel: visible &= type == Action::Scroll; break;
        case StepPosition: visible &= mouse; enabled &= type != Action::Move; break;
        case StepX: case StepY: visible &= mouse; enabled &= fixed; break;
        case UpdateStep: case DeleteStep: case DuplicateStep: enabled &= sel >= 0; break;
        case UpStep: enabled &= sel > 0; break;
        case DownStep: enabled &= sel >= 0 && static_cast<size_t>(sel + 1) < macro.steps.size(); break;
        case NewMacro: enabled &= libraryWritable; break;
        case MacroName: case EditMacro: case CopyMacro: case RemoveMacro: enabled &= libraryWritable && currentMacro() != nullptr; break;
        case AddStep: enabled &= libraryWritable && currentMacro() != nullptr; break;
        }
        if ((IsWindowEnabled(w.hwnd) != FALSE) != enabled) EnableWindow(w.hwnd, enabled);
        bool shown = (GetWindowLongPtrW(w.hwnd, GWL_STYLE) & WS_VISIBLE) != 0;
        if (shown != visible) SetWindowPos(w.hwnd, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW | (visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
    }
    if (!arranging) RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
}
void App::refreshList(int sel) {
    refreshing = true; auto list = control(StepList);
    // WM_SETREDRAW(TRUE) also sets WS_VISIBLE: never apply it to a hidden list.
    bool listShown = (GetWindowLongPtrW(list, GWL_STYLE) & WS_VISIBLE) != 0;
    if (listShown) SendMessageW(list, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(list);
    for (size_t i = 0; i < macro.steps.size(); ++i) {
        auto index = std::to_wstring(i + 1); LVITEMW item{}; item.mask = LVIF_TEXT; item.iItem = static_cast<int>(i); item.pszText = index.data(); ListView_InsertItem(list, &item);
        auto name = actionName(macro.steps[i].action), delay = std::to_wstring(macro.steps[i].delayMs) + L" ms", desc = describe(macro.steps[i]);
        ListView_SetItemText(list, static_cast<int>(i), 1, name.data()); ListView_SetItemText(list, static_cast<int>(i), 2, delay.data()); ListView_SetItemText(list, static_cast<int>(i), 3, desc.data());
    }
    if (sel >= 0 && sel < static_cast<int>(macro.steps.size())) { ListView_SetItemState(list, sel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED); ListView_EnsureVisible(list, sel, FALSE); }
    if (listShown) SendMessageW(list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(list, nullptr, FALSE); refreshing = false;
    if (sel >= 0) populate(); editorState(); InvalidateRect(hwnd, nullptr, FALSE);
}
void App::populate() {
    int i = selected(); if (i < 0 || static_cast<size_t>(i) >= macro.steps.size()) return;
    const auto& step = macro.steps[i];
    SendMessageW(control(StepType), CB_SETCURSEL, static_cast<int>(step.action), 0);
    SendMessageW(control(StepButton), CB_SETCURSEL, static_cast<int>(step.button), 0);
    SendMessageW(control(StepPosition), CB_SETCURSEL, step.fixed || step.action == Action::Move ? 1 : 0, 0);
    set(StepDelay, std::to_wstring(step.delayMs)); set(StepKey, keyName(step.key, step.modifiers));
    set(StepX, std::to_wstring(step.x)); set(StepY, std::to_wstring(step.y)); set(StepWheel, std::to_wstring(step.wheel));
    editorState(); InvalidateRect(hwnd, nullptr, FALSE);
}
bool App::readStep(Step& step) {
    step.action = static_cast<Action>(SendMessageW(control(StepType), CB_GETCURSEL, 0, 0));
    int delay = 0;
    if (!number(StepDelay, 0, MaxDelayMs, delay, L"Délai")) return false;
    step.delayMs = static_cast<uint32_t>(delay);
    step.button = static_cast<Button>(SendMessageW(control(StepButton), CB_GETCURSEL, 0, 0));
    step.fixed = step.action == Action::Move || SendMessageW(control(StepPosition), CB_GETCURSEL, 0, 0) == 1;
    if ((step.action == Action::Click || step.action == Action::Move) && step.fixed &&
        (!number(StepX, -100000, 100000, step.x, L"X") || !number(StepY, -100000, 100000, step.y, L"Y"))) return false;
    if (step.action == Action::Key || step.action == Action::KeyDown || step.action == Action::KeyUp)
        if (!parseKey(value(StepKey), step.key, step.modifiers)) { error(L"Touche inconnue. Exemples : A, Enter, Ctrl+C, Shift+Tab."); return false; }
    if (conflictsWithHotkeys(step, preferences.hotkeys)) { error(L"Cette touche est réservée à un de vos raccourcis. Modifiez-le dans Préférences pour utiliser cette touche dans une macro."); return false; }
    if (step.action == Action::Scroll && !number(StepWheel, -100, 100, step.wheel, L"Molette")) return false;
    std::wstring err; if (!validStep(step, err)) { error(err); return false; } return true;
}
bool App::readRepeats() {
    int repeats = 0; if (!number(RepeatCount, 0, 1000000, repeats, L"Répétitions")) return false;
    macro.repeats = static_cast<uint32_t>(repeats); return true;
}
void App::start(int mode) {
    if (engine.snapshot().state != RunState::Idle) { stop(); return; }
    if (!hotkeys[2]) { error(L"Le raccourci d'arrêt " + shortcutName(2) + L" est indisponible. Choisissez-en un autre dans Préférences."); return; }
    int delay = 0; if (!number(StartDelay, 0, 60000, delay, L"Délai de départ")) return;
    std::wstring err; bool ok = false;
    if (mode == 0) {
        ClickConfig config; int interval = 0, count = 0;
        if (!number(ClickInterval, 1, 60000, interval, L"Intervalle") || !number(ClickCount, 0, 1000000, count, L"Nombre de clics")) return;
        config.intervalMs = static_cast<uint32_t>(interval); config.count = static_cast<uint32_t>(count);
        config.button = static_cast<Button>(SendMessageW(control(ClickButton), CB_GETCURSEL, 0, 0));
        config.fixed = SendMessageW(control(ClickPosition), CB_GETCURSEL, 0, 0) == 1;
        if (config.fixed && (!number(ClickX, -100000, 100000, config.x, L"X") || !number(ClickY, -100000, 100000, config.y, L"Y"))) return;
        ok = engine.startClicker(config, static_cast<uint32_t>(delay), err);
    } else {
        if (!currentMacro()) { notice = L"Créez une macro pour commencer."; return; }
        if (!readRepeats()) return;
        for (size_t i = 0; i < macro.steps.size(); ++i) if (conflictsWithHotkeys(macro.steps[i], preferences.hotkeys)) {
            error(L"L'action " + std::to_wstring(i + 1) + L" utilise une touche réservée à vos raccourcis. Modifiez cette action ou vos raccourcis dans Préférences."); return;
        }
        ok = engine.startMacro(macro, static_cast<uint32_t>(delay), err);
    }
    if (!ok) { error(err); return; }
    page = mode == 0 ? 0 : 1; failureShown = false; lastActive = true; layout(); schedulePreferences();
}
void App::stop() { engine.stop(); notice = L"Exécution arrêtée · " + std::to_wstring(engine.snapshot().actions) + L" actions."; tick(); }
void App::tick() {
    auto snapshot = engine.snapshot(); bool active = snapshot.state != RunState::Idle;
    bool repaint = active || lastActive || (snapshot.inputFailed && !failureShown);
    if (lastActive && !active) {
        notice = L"Exécution terminée · " + std::to_wstring(snapshot.actions) + L" actions · " + std::to_wstring(snapshot.cycles) + L" boucles.";
        editorState();
    }
    lastActive = active;
    if (snapshot.inputFailed && !failureShown) {
        failureShown = true; notice = L"Entrée refusée ou erreur du moteur. Vérifiez les droits de la fenêtre cible.";
    }
    if (repaint) InvalidateRect(hwnd, nullptr, FALSE);
}
void App::capture() {
    if (page == 2 || page == 3 || engine.snapshot().state != RunState::Idle) return;
    POINT point{}; if (!GetCursorPos(&point)) return;
    int xid = page ? StepX : ClickX, yid = page ? StepY : ClickY;
    set(xid, std::to_wstring(point.x)); set(yid, std::to_wstring(point.y));
    SendMessageW(control(page ? StepPosition : ClickPosition), CB_SETCURSEL, 1, 0);
    notice = L"Position capturée : " + std::to_wstring(point.x) + L", " + std::to_wstring(point.y) + (page ? L". Ajoutez ou appliquez l'étape." : L".");
    editorState(); InvalidateRect(hwnd, nullptr, FALSE); schedulePreferences();
}
void App::command(int id) {
    if (keyCaptureTarget) endKeyCapture(L"Capture annulée.");
    resumeShortcuts();
    if (shortcutsSuspended) return;
    if (id == StepKey || (id >= HotkeyClick && id <= HotkeyCapture)) { beginKeyCapture(id); return; }
    if (id == NavClick || id == NavMacro || id == NavSettings || id == BackLibrary) {
        if (libraryReady) persistLibrary();
        page = id == NavClick ? 0 : id == NavSettings ? 2 : 3; refreshLibrary(); layout(); schedulePreferences(); return;
    }
    if (id == Stop) { stop(); return; }
    if (engine.snapshot().state != RunState::Idle) return;
    int sel = selected();
    switch (id) {
    case ApplyHotkeys: applyHotkeyEdits(); break;
    case DefaultKeys:
        for (int i = 0; i < 4; ++i) set(HotkeyClick + i, keyName(DefaultHotkeys[i].key));
        notice = L"Cliquez sur Appliquer pour rétablir les raccourcis par défaut."; break;
    case MouseLeft: case MouseRight: case MouseMiddle:
        SendMessageW(control(ClickButton), CB_SETCURSEL, id - MouseLeft, 0);
        for (int key = MouseLeft; key <= MouseMiddle; ++key) InvalidateRect(control(key), nullptr, FALSE);
        break;
    case FollowCursor: case FixedPoint:
        SendMessageW(control(ClickPosition), CB_SETCURSEL, id - FollowCursor, 0); editorState();
        InvalidateRect(control(FollowCursor), nullptr, FALSE); InvalidateRect(control(FixedPoint), nullptr, FALSE); break;
    case PresetSlow: case PresetNormal: case PresetFast: {
        const wchar_t* intervals[] = {L"200", L"100", L"20"}; set(ClickInterval, intervals[id - PresetSlow]); break;
    }
    case Start: start(page); break;
    case NewMacro: case CreateMacro: createMacro(false); break;
    case CopyMacro: createMacro(true); break;
    case RemoveMacro: removeMacro(); break;
    case EditMacro: if (currentMacro()) { page = 1; layout(); schedulePreferences(); } break;
    case AddStep: case UpdateStep: {
        if (id == AddStep && macro.steps.size() >= MaxSteps) { error(L"Limite de 10 000 étapes atteinte."); break; }
        Step step; if (!readStep(step)) break;
        if (id == AddStep) { macro.steps.push_back(step); sel = static_cast<int>(macro.steps.size() - 1); }
        else if (sel >= 0) macro.steps[sel] = step; else break;
        setDirty(true); refreshList(sel); notice = L"Étape enregistrée dans la séquence."; break;
    }
    case DeleteStep:
        if (sel >= 0) { macro.steps.erase(macro.steps.begin() + sel); setDirty(true); refreshList(std::min(sel, static_cast<int>(macro.steps.size()) - 1)); } break;
    case DuplicateStep:
        if (sel >= 0 && macro.steps.size() < MaxSteps) { Step copy = macro.steps[sel]; macro.steps.insert(macro.steps.begin() + sel + 1, copy); setDirty(true); refreshList(sel + 1); } break;
    case UpStep: case DownStep: {
        int dest = sel + (id == UpStep ? -1 : 1);
        if (sel >= 0 && dest >= 0 && dest < static_cast<int>(macro.steps.size())) { std::swap(macro.steps[sel], macro.steps[dest]); setDirty(true); refreshList(dest); } break;
    }
    }
    InvalidateRect(hwnd, nullptr, FALSE); schedulePreferences();
}
void App::runSmoke() {
    // Exercise real controls and message handlers, but never inject input or register hotkeys.
    auto check = [&](bool condition) { if (!condition) ++smokeExit; };
    auto keyEvent = [&](UINT event, UINT key, bool repeat = false) {
        MSG input{}; input.hwnd = control(keyCaptureTarget ? keyCaptureTarget : HotkeyClick); input.message = event; input.wParam = key;
        input.lParam = repeat ? (1LL << 30) : 0;
        check(captureMessage(input));
    };
    if (sessionTest == 3) {
        std::filesystem::create_directories(sessionTestFile.parent_path());
        auto backup = libraryFile; backup += L".bak";
        std::filesystem::remove(libraryFile); std::filesystem::remove(backup);
        auto legacy = sessionTestFile.parent_path() / L"Ancienne routine.mpulse";
        Step wait; wait.action = Action::Wait; std::wstring problem;
        check(saveMacro(legacy, {3, {wait}}, problem)); preferences.lastMacro = legacy;
        restoreLibrary(); check(library.entries.size() == 1 && macroName() == L"Ancienne routine" && macro.repeats == 3);
        check(persistLibrary() && preferences.lastMacro.empty());
        set(MacroName, L"Routine renommée"); check(persistLibrary());
        { std::ofstream out(libraryFile); out << "broken primary"; }
        sessionTest = 2; restoreLibrary();
        check(libraryWritable && dirty && macroName() == L"Ancienne routine" && macro.steps.size() == 1);
        check(persistLibrary());
        { std::ofstream out(libraryFile); out << "broken primary"; }
        { std::ofstream out(backup); out << "broken backup"; }
        restoreLibrary(); check(!libraryWritable && library.entries.empty());
        createMacro(false); check(library.entries.empty());
        check(std::filesystem::file_size(libraryFile) == 14 && std::filesystem::exists(legacy));
        DestroyWindow(hwnd); return;
    }
    if (sessionTest) {
        if (sessionTest == 1) {
            set(ClickInterval, L"37"); set(ClickCount, L"123"); set(ClickX, L"-1920"); set(ClickY, L"500"); set(StartDelay, L"2345");
            command(MouseRight); command(FixedPoint); command(NavSettings);
            command(HotkeyClick); keyEvent(WM_KEYDOWN, 'K'); keyEvent(WM_KEYUP, 'K'); check(applyHotkeyEdits(false));
            Step wait; wait.action = Action::Wait; macro = {3, {wait}};
            set(RepeatCount, L"3"); set(MacroName, L"Routine équipe 日本語"); setDirty(true); check(persistLibrary());
            auto first = library.selected; createMacro(true); SetFocus(control(NavSettings));
            set(MacroName, L"Routine copie"); set(RepeatCount, L"7"); setDirty(true); check(persistLibrary());
            selectMacro(first); command(NavSettings);
            std::wstring problem; check(savePreferences(sessionTestFile, collectPreferences(), problem));
        } else {
            check(value(ClickInterval) == L"37" && value(ClickCount) == L"123" && value(ClickX) == L"-1920" && value(ClickY) == L"500" && value(StartDelay) == L"2345");
            check(SendMessageW(control(ClickPosition), CB_GETCURSEL, 0, 0) == 1 && SendMessageW(control(ClickButton), CB_GETCURSEL, 0, 0) == 1);
            check(page == 2 && shortcutName(0) == L"K" && value(HotkeyClick) == L"K");
            check(library.entries.size() == 2 && macroName() == L"Routine équipe 日本語" && macro.steps.size() == 1 && macro.repeats == 3 && value(RepeatCount) == L"3");
            check(library.entries[1].name == L"Routine copie" && library.entries[1].macro.repeats == 7);
            check(!dirty && engine.snapshot().state == RunState::Idle);
        }
        dirty = false; DestroyWindow(hwnd); return;
    }
    check(widgets.size() == 50);
    check(persistLibrary() && !dirty);
    for (const auto& w : widgets) check(IsWindow(w.hwnd) != FALSE);
    check(brandIcon != nullptr);
    checkRendering();
    if (!previewDirectory.empty()) {
        command(NavClick); SetFocus(control(NavClick)); exportPreview(L"navigation-mouse.png");
        MSG navigation{}; navigation.message = WM_KEYDOWN; navigation.wParam = VK_TAB; captureMessage(navigation);
        exportPreview(L"navigation-keyboard.png");
        navigation.message = WM_LBUTTONDOWN; captureMessage(navigation);
    }
    exportPreview(L"auto-clicker.png");
    command(FixedPoint); check(IsWindowVisible(control(ClickX)) && SendMessageW(control(ClickPosition), CB_GETCURSEL, 0, 0) == 1);
    command(FollowCursor); check(!IsWindowVisible(control(ClickX)));
    command(MouseRight); check(SendMessageW(control(ClickButton), CB_GETCURSEL, 0, 0) == 1); command(MouseLeft);
    command(PresetFast); check(value(ClickInterval) == L"20"); command(PresetNormal);
    command(NavSettings);
    command(HotkeyClick); check(keyCaptureTarget == HotkeyClick && !shortcutRegistry->active(2));
    exportPreview(L"preferences-capture.png");
    keyEvent(WM_KEYDOWN, VK_F6); check(value(HotkeyClick) == L"F6" && shortcutsSuspended);
    message(WM_HOTKEY, 0x510, MAKELPARAM(0, VK_F6)); check(engine.snapshot().state == RunState::Idle);
    keyEvent(WM_KEYDOWN, VK_F6, true); keyEvent(WM_KEYUP, VK_F6);
    check(!shortcutsSuspended && shortcutRegistry->active(2));
    command(HotkeyClick); keyEvent(WM_KEYDOWN, VK_CONTROL); keyEvent(WM_KEYDOWN, VK_F10);
    check(value(HotkeyClick) == L"Ctrl+F10" && !applyHotkeyEdits(false));
    keyEvent(WM_KEYUP, VK_F10); check(shortcutsSuspended);
    keyEvent(WM_KEYUP, VK_CONTROL); check(!shortcutsSuspended && applyHotkeyEdits(false));
    command(HotkeyClick); keyEvent(WM_KEYDOWN, VK_ESCAPE); keyEvent(WM_KEYUP, VK_ESCAPE);
    check(value(HotkeyClick) == L"Ctrl+F10" && !keyCaptureTarget && !shortcutsSuspended);
    command(HotkeyClick); keyEvent(WM_KEYDOWN, VK_F12); keyEvent(WM_KEYUP, VK_F12);
    check(keyCaptureTarget == HotkeyClick && value(HotkeyClick) == L"Ctrl+F10");
    message(WM_ACTIVATEAPP, FALSE, 0); resumeShortcuts(); check(!keyCaptureTarget && !shortcutsSuspended);
    command(HotkeyClick); SendMessageW(control(HotkeyClick), WM_KILLFOCUS, 0, 0); resumeShortcuts();
    check(!keyCaptureTarget && !shortcutsSuspended);
    command(HotkeyClick); MSG outside{}; outside.hwnd = hwnd; outside.message = WM_LBUTTONDOWN;
    check(!captureMessage(outside) && !keyCaptureTarget && !shortcutsSuspended);
    check(shortcutName(0) == L"Ctrl+F10");
    set(HotkeyMacro, L"Ctrl+F10"); check(!applyHotkeyEdits(false) && preferences.hotkeys[1] == DefaultHotkeys[1]);
    set(HotkeyMacro, L"F7"); notice.clear(); exportPreview(L"preferences.png");
    command(DefaultKeys); check(applyHotkeyEdits(false)); notice.clear();
    command(NavMacro); exportPreview(L"library.png"); command(EditMacro); exportPreview(L"macros-empty.png");
    command(EditMacro); set(StepDelay, L"25"); command(AddStep);
    SendMessageW(control(StepType), CB_SETCURSEL, static_cast<int>(Action::Key), 0); editorState();
    command(StepKey); keyEvent(WM_KEYDOWN, VK_ESCAPE); keyEvent(WM_KEYUP, VK_ESCAPE); check(value(StepKey) == L"Esc");
    command(StepKey); keyEvent(WM_KEYDOWN, VK_SHIFT); keyEvent(WM_KEYUP, VK_SHIFT); check(value(StepKey) == L"Shift");
    command(StepKey); keyEvent(WM_KEYDOWN, VK_TAB); keyEvent(WM_KEYUP, VK_TAB); check(value(StepKey) == L"Tab");
    command(StepKey); keyEvent(WM_SYSKEYDOWN, VK_MENU); keyEvent(WM_SYSKEYDOWN, 'Q');
    keyEvent(WM_SYSKEYUP, 'Q'); keyEvent(WM_SYSKEYUP, VK_MENU); check(value(StepKey) == L"Alt+Q");
    command(StepKey); keyEvent(WM_KEYDOWN, VK_CONTROL); keyEvent(WM_KEYDOWN, 'C');
    keyEvent(WM_KEYUP, 'C'); keyEvent(WM_KEYUP, VK_CONTROL); command(AddStep);
    check(macro.steps.size() == 2 && macro.steps[1].key == 'C');
    command(UpStep); check(macro.steps[0].key == 'C');
    command(DuplicateStep); check(macro.steps.size() == 3);
    command(DeleteStep); check(macro.steps.size() == 2);
    SendMessageW(control(StepType), CB_SETCURSEL, static_cast<int>(Action::Key), 0);
    set(StepKey, L"Ctrl+V"); command(UpdateStep); check(macro.steps[1].action == Action::Key && macro.steps[1].key == 'V');
    exportPreview(L"macros.png");
    auto original = library.selected;
    command(BackLibrary); command(CopyMacro); SetFocus(control(NavMacro));
    check(library.entries.size() == 2 && macro.steps.size() == 2 && library.selected != original);
    set(MacroName, L"Copie personnalisée"); command(EditMacro); set(StepKey, L"Alt+Q"); command(UpdateStep);
    check(persistLibrary()); selectMacro(original); check(macro.steps[0].key == 'C' && macro.steps[1].key == 'V');
    selectMacro(library.entries[1].id); removeMacro(false); check(library.entries.size() == 1 && library.selected == original);
    command(NewMacro); check(textInput == control(MacroName) && !shortcutRegistry->active(0));
    MSG typed{}; typed.message = WM_KEYDOWN; typed.wParam = 'G'; check(!captureMessage(typed));
    message(WM_HOTKEY, 0x510, MAKELPARAM(0, VK_F6)); check(engine.snapshot().state == RunState::Idle);
    SetFocus(control(NavMacro)); check(!textInput && shortcutRegistry->active(0)); check(macro.steps.empty());
    set(MacroName, L"Routine du soir"); check(persistLibrary()); command(BackLibrary); exportPreview(L"library.png");
    selectMacro(original); command(EditMacro);
    command(NavClick); check(IsWindowVisible(control(ClickInterval)) && !IsWindowVisible(control(StepList)));
    command(NavMacro); check(IsWindowVisible(control(LibraryList)) && !IsWindowVisible(control(ClickInterval))); command(EditMacro);
    // Ensure every visible control fits in the smallest supported client area.
    SetWindowPos(hwnd, nullptr, 0, 0, s(1000), s(690), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    for (int mode : {0, 1, 2, 3}) {
        command(mode == 2 ? NavSettings : mode == 3 ? NavMacro : mode == 1 ? EditMacro : NavClick);
        RECT client{}; GetClientRect(hwnd, &client);
        for (const auto& w : widgets) if (IsWindowVisible(w.hwnd)) {
            RECT rect{}; GetWindowRect(w.hwnd, &rect); MapWindowPoints(nullptr, hwnd, reinterpret_cast<POINT*>(&rect), 2);
            check(rect.left >= 0 && rect.top >= 0 && rect.right <= client.right && rect.bottom <= client.bottom);
        }
        exportPreview(mode == 3 ? L"library-compact.png" : mode == 2 ? L"preferences-compact.png" : mode ? L"macros-compact.png" : L"auto-clicker-compact.png");
    }
    dirty = false; DestroyWindow(hwnd);
}
LRESULT App::message(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: create(); return 0;
    case WM_SIZE: if (wp != SIZE_MINIMIZED) layout(); return 0;
    case WM_GETMINMAXINFO: {
        auto info = reinterpret_cast<MINMAXINFO*>(lp); info->ptMinTrackSize = {s(1000), s(690)}; return 0;
    }
    case WM_DPICHANGED: {
        dpi = HIWORD(wp); makeFonts(); auto rect = reinterpret_cast<RECT*>(lp);
        SetWindowPos(hwnd, nullptr, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE); layout(); return 0;
    }
    case WM_ERASEBKGND: {
        RECT rect{}; GetClientRect(hwnd, &rect);
        SetDCBrushColor(reinterpret_cast<HDC>(wp), Bg); FillRect(reinterpret_cast<HDC>(wp), &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH))); return 1;
    }
    case WM_PAINT: { PAINTSTRUCT ps{}; HDC dc = BeginPaint(hwnd, &ps); paint(dc); EndPaint(hwnd, &ps); return 0; }
    case WM_DRAWITEM: {
        const auto& item = *reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        if (item.CtlType == ODT_LISTBOX) drawLibraryRow(item);
        else if (item.CtlType == ODT_LISTVIEW) drawRow(item);
        else if (item.CtlType == ODT_COMBOBOX) {
            auto brush = CreateSolidBrush((item.itemState & ODS_SELECTED) ? RGB(65, 44, 103) : Field); FillRect(item.hDC, &item.rcItem, brush); DeleteObject(brush);
            if (item.itemID != static_cast<UINT>(-1)) {
                wchar_t label[128]{};
                if (SendMessageW(item.hwndItem, CB_GETLBTEXTLEN, item.itemID, 0) < 128) SendMessageW(item.hwndItem, CB_GETLBTEXT, item.itemID, reinterpret_cast<LPARAM>(label));
                RECT rect = item.rcItem; rect.left += s(12); SelectObject(item.hDC, fonts[0]); SetTextColor(item.hDC, Text); SetBkMode(item.hDC, TRANSPARENT);
                DrawTextW(item.hDC, label, -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
        } else drawButton(item);
        return TRUE;
    }
    case WM_MEASUREITEM: {
        auto measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lp); measure->itemHeight = s(measure->CtlType == ODT_LISTBOX ? 76 : measure->CtlType == ODT_LISTVIEW ? 46 : 34); return TRUE;
    }
    case WM_CTLCOLORBTN: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wp); SetTextColor(dc, Text); SetBkColor(dc, Field); return reinterpret_cast<LRESULT>(fieldBrush);
    }
    case WM_COMMAND:
        if (LOWORD(wp) == LibraryList) {
            if (HIWORD(wp) == LBN_SELCHANGE && !refreshing) {
                int index = static_cast<int>(SendMessageW(control(LibraryList), LB_GETCURSEL, 0, 0));
                if (index >= 0 && index < static_cast<int>(library.entries.size())) selectMacro(library.entries[index].id);
            }
            if (HIWORD(wp) == LBN_DBLCLK) command(EditMacro);
            return 0;
        }
        if (HIWORD(wp) == BN_CLICKED) command(LOWORD(wp));
        if (HIWORD(wp) == CBN_SELCHANGE) { editorState(); InvalidateRect(hwnd, nullptr, FALSE); }
        if (HIWORD(wp) == EN_CHANGE) {
            if ((LOWORD(wp) == RepeatCount || LOWORD(wp) == MacroName) && !refreshing) setDirty(true);
            if (LOWORD(wp) == ClickInterval) for (int id = PresetSlow; id <= PresetFast; ++id) if (auto preset = control(id)) InvalidateRect(preset, nullptr, FALSE);
            InvalidateRect(hwnd, nullptr, FALSE);
            if (LOWORD(wp) < HotkeyClick || LOWORD(wp) > HotkeyCapture) schedulePreferences();
        }
        if (HIWORD(wp) == EN_SETFOCUS || HIWORD(wp) == EN_KILLFOCUS) InvalidateRect(hwnd, nullptr, FALSE);
        if (HIWORD(wp) == EN_SETFOCUS) beginTextInput(reinterpret_cast<HWND>(lp));
        if (HIWORD(wp) == EN_KILLFOCUS) {
            if (LOWORD(wp) == MacroName && currentMacro() && cleanMacroName(value(MacroName)).empty()) {
                refreshing = true; set(MacroName, macroName()); refreshing = false;
                notice = L"Le nom doit contenir de 1 à 80 caractères.";
            }
            if (textInput == reinterpret_cast<HWND>(lp)) { textInput = nullptr; resumeShortcuts(); }
        }
        return 0;
    case WM_NOTIFY: {
        auto nm = reinterpret_cast<NMHDR*>(lp);
        if (nm->idFrom == StepList && nm->code == LVN_ITEMCHANGED && !refreshing) { populate(); editorState(); }
        if (nm->idFrom == StepList && nm->code == LVN_KEYDOWN && reinterpret_cast<NMLVKEYDOWN*>(lp)->wVKey == VK_DELETE) command(DeleteStep);
        if (nm->idFrom == StepList && nm->code == LVN_GETEMPTYMARKUP) {
            auto empty = reinterpret_cast<NMLVEMPTYMARKUP*>(lp); empty->dwFlags = EMF_CENTERED;
            wcscpy_s(empty->szMarkup, L"Votre première macro commence ici\nChoisissez une action ci-dessous pour l'ajouter."); return TRUE;
        }
        break;
    }
    case WM_HOTKEY: {
        if (!shortcutRegistry || shortcutsSuspended) return 0;
        int action = shortcutRegistry->actionFor(static_cast<int>(wp)); if (action < 0) return 0;
        const auto& key = preferences.hotkeys[action];
        if (HIWORD(lp) != key.key || static_cast<UINT>(LOWORD(lp) & (MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_WIN)) != nativeModifiers(key)) return 0; // Ignore stale queued bindings.
        if (action == 2) { stop(); return 0; }
        if (page == 2 && GetForegroundWindow() == hwnd) return 0; // Editing shortcuts must never start input.
        if (action == 0) start(0); else if (action == 1) start(1); else capture(); return 0;
    }
    case WM_TIMER: if (wp == 2) persistPreferences(); else if (wp == 3) persistLibrary(); else { resumeShortcuts(); tick(); } return 0;
    case WM_ACTIVATEAPP:
        if (!wp && keyCaptureTarget) endKeyCapture(L"Capture annulée.");
        if (!wp && textInput) { textInput = nullptr; resumeShortcuts(); }
        if (wp) { wchar_t klass[32]{}; GetClassNameW(GetFocus(), klass, 32); if (lstrcmpiW(klass, L"EDIT") == 0) beginTextInput(GetFocus()); }
        break;
    case WM_APP + 1: runSmoke(); return 0;
    case WM_CLOSE: engine.stop(); if (persistLibrary()) { persistPreferences(); DestroyWindow(hwnd); } else { error(notice); tick(); } return 0;
    case WM_QUERYENDSESSION: engine.stop(); persistPreferences(); return persistLibrary();
    case WM_DESTROY:
        engine.stop(); KillTimer(hwnd, 1); KillTimer(hwnd, 2); KillTimer(hwnd, 3); if (shortcutRegistry) shortcutRegistry->clear();
        PostQuitMessage(smokeExit); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    struct DrawingRuntime {
        ULONG_PTR token = 0;
        DrawingRuntime() { Gdiplus::GdiplusStartupInput input; Gdiplus::GdiplusStartup(&token, &input, nullptr); }
        ~DrawingRuntime() { if (token) Gdiplus::GdiplusShutdown(token); }
    } drawing;
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES}; InitCommonControlsEx(&icc);
    App app;
    int argc = 0; auto args = CommandLineToArgvW(GetCommandLineW(), &argc);
    app.smoke = argc > 1 && std::wstring(args[1]) == L"--smoke-test";
    if (argc == 3 && (std::wstring(args[1]) == L"--session-write-test" || std::wstring(args[1]) == L"--session-read-test")) {
        app.smoke = true; app.sessionTest = std::wstring(args[1]) == L"--session-write-test" ? 1 : 2; app.sessionTestFile = args[2];
    }
    if (argc == 3 && std::wstring(args[1]) == L"--library-storage-test") {
        app.smoke = true; app.sessionTest = 3; app.sessionTestFile = args[2];
    }
    if (argc == 3 && std::wstring(args[1]) == L"--render-preview") { app.smoke = true; app.previewDirectory = args[2]; std::filesystem::create_directories(app.previewDirectory); }
    if (args) LocalFree(args);
    struct InstanceGuard { HANDLE mutex = nullptr; ~InstanceGuard() { if (mutex) CloseHandle(mutex); } } instanceGuard;
    if (!app.smoke) {
        instanceGuard.mutex = CreateMutexW(nullptr, FALSE, L"Local\\MacroPulse.Library");
        if (!instanceGuard.mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
            auto existing = FindWindowW(L"MacroPulseWindow", nullptr);
            if (existing) { ShowWindow(existing, SW_RESTORE); SetForegroundWindow(existing); }
            else MessageBoxW(nullptr, L"MacroPulse est déjà ouvert ou n'a pas pu démarrer.", L"MacroPulse", MB_OK);
            return 0;
        }
    }
    WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = wndProc; wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(101));
    wc.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(101), IMAGE_ICON, 16, 16, LR_SHARED));
    wc.lpszClassName = L"MacroPulseWindow"; RegisterClassExW(&wc);
    UINT dpi = GetDpiForSystem(); RECT size{0, 0, MulDiv(1180, dpi, 96), MulDiv(820, dpi, 96)};
    AdjustWindowRectExForDpi(&size, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi);
    RECT workArea{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    auto window = CreateWindowExW(WS_EX_COMPOSITED, wc.lpszClassName, L"MacroPulse", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                  CW_USEDEFAULT, CW_USEDEFAULT, std::min(size.right - size.left, workArea.right - workArea.left),
                                  std::min(size.bottom - size.top, workArea.bottom - workArea.top), nullptr, nullptr, instance, &app);
    if (!window) return 1;
    ShowWindow(window, app.smoke ? SW_SHOWNOACTIVATE : show); UpdateWindow(window);
    MSG msg{}; BOOL result;
    while ((result = GetMessageW(&msg, nullptr, 0, 0)) > 0) if (!app.captureMessage(msg) && !IsDialogMessageW(window, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return result == -1 ? 1 : static_cast<int>(msg.wParam);
}
