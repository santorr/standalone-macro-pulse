#include "model.hpp"
#include <algorithm>
#include <cwctype>
#include <fstream>
#include <sstream>
#include <limits>

namespace pulse {
bool parseNumber(const std::wstring& text, int minimum, int maximum, int& value) {
    if (text.empty()) return false;
    size_t used = 0;
    try {
        auto n = std::stoll(text, &used);
        if (used != text.size() || n < minimum || n > maximum) return false;
        value = static_cast<int>(n);
        return true;
    } catch (...) { return false; }
}
static std::wstring trim(std::wstring text) {
    auto a = text.find_first_not_of(L" \t\r\n");
    if (a == std::wstring::npos) return {};
    return text.substr(a, text.find_last_not_of(L" \t\r\n") - a + 1);
}
bool parseKey(const std::wstring& text, uint16_t& key, uint8_t& modifiers) {
    auto t = trim(text);
    std::transform(t.begin(), t.end(), t.begin(), [](wchar_t c) { return static_cast<wchar_t>(towupper(c)); });
    uint8_t mods = 0;
    size_t pos;
    while ((pos = t.find(L'+')) != std::wstring::npos) {
        auto part = trim(t.substr(0, pos));
        if (part == L"CTRL") mods |= 1;
        else if (part == L"ALT") mods |= 2;
        else if (part == L"SHIFT" || part == L"MAJ") mods |= 4;
        else if (part == L"WIN") mods |= 8;
        else return false;
        t = trim(t.substr(pos + 1));
    }
    uint16_t vk = 0;
    if (t.size() == 1 && ((t[0] >= L'A' && t[0] <= L'Z') || (t[0] >= L'0' && t[0] <= L'9'))) vk = static_cast<uint16_t>(t[0]);
    else if (t.size() > 1 && t[0] == L'F') {
        int n;
        if (!parseNumber(t.substr(1), 1, 24, n)) return false;
        vk = static_cast<uint16_t>(VK_F1 + n - 1);
    } else {
        const std::pair<const wchar_t*, int> keys[] = {
            {L"SPACE", VK_SPACE}, {L"ESPACE", VK_SPACE}, {L"ENTER", VK_RETURN}, {L"ENTREE", VK_RETURN},
            {L"TAB", VK_TAB}, {L"ESC", VK_ESCAPE}, {L"BACKSPACE", VK_BACK}, {L"DELETE", VK_DELETE},
            {L"INSERT", VK_INSERT}, {L"HOME", VK_HOME}, {L"END", VK_END}, {L"PAGEUP", VK_PRIOR}, {L"PAGEDOWN", VK_NEXT},
            {L"LEFT", VK_LEFT}, {L"RIGHT", VK_RIGHT}, {L"UP", VK_UP}, {L"DOWN", VK_DOWN},
            {L"CTRL", VK_CONTROL}, {L"ALT", VK_MENU}, {L"SHIFT", VK_SHIFT}, {L"WIN", VK_LWIN},
            {L"NUM0", VK_NUMPAD0}, {L"NUM1", VK_NUMPAD1}, {L"NUM2", VK_NUMPAD2}, {L"NUM3", VK_NUMPAD3},
            {L"NUM4", VK_NUMPAD4}, {L"NUM5", VK_NUMPAD5}, {L"NUM6", VK_NUMPAD6}, {L"NUM7", VK_NUMPAD7}, {L"NUM8", VK_NUMPAD8}, {L"NUM9", VK_NUMPAD9},
            {L"NUMADD", VK_ADD}, {L"NUMSUBTRACT", VK_SUBTRACT}, {L"NUMMULTIPLY", VK_MULTIPLY}, {L"NUMDIVIDE", VK_DIVIDE}, {L"NUMDECIMAL", VK_DECIMAL},
            {L"CAPSLOCK", VK_CAPITAL}, {L"NUMLOCK", VK_NUMLOCK}, {L"SCROLLLOCK", VK_SCROLL}, {L"PAUSE", VK_PAUSE}, {L"PRINTSCREEN", VK_SNAPSHOT},
            {L"OEM1", VK_OEM_1}, {L"OEMPLUS", VK_OEM_PLUS}, {L"OEMCOMMA", VK_OEM_COMMA}, {L"OEMMINUS", VK_OEM_MINUS}, {L"OEMPERIOD", VK_OEM_PERIOD},
            {L"OEM2", VK_OEM_2}, {L"OEM3", VK_OEM_3}, {L"OEM4", VK_OEM_4}, {L"OEM5", VK_OEM_5}, {L"OEM6", VK_OEM_6}, {L"OEM7", VK_OEM_7}, {L"OEM102", VK_OEM_102}
        };
        for (auto [name, code] : keys) if (t == name) { vk = static_cast<uint16_t>(code); break; }
    }
    if (!vk) return false; // Reserved keys are checked against the user's current bindings in the UI.
    key = vk; modifiers = mods;
    return true;
}
std::wstring keyName(uint16_t key, uint8_t modifiers) {
    std::wstring text;
    if (modifiers & 1) text += L"Ctrl+";
    if (modifiers & 2) text += L"Alt+";
    if (modifiers & 4) text += L"Shift+";
    if (modifiers & 8) text += L"Win+";
    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9')) return text + static_cast<wchar_t>(key);
    if (key >= VK_F1 && key <= VK_F24) return text + L"F" + std::to_wstring(key - VK_F1 + 1);
    const std::pair<int, const wchar_t*> names[] = {
        {VK_SPACE,L"Space"},{VK_RETURN,L"Enter"},{VK_TAB,L"Tab"},{VK_ESCAPE,L"Esc"},{VK_BACK,L"Backspace"},
        {VK_DELETE,L"Delete"},{VK_INSERT,L"Insert"},{VK_HOME,L"Home"},{VK_END,L"End"},
        {VK_PRIOR,L"PageUp"},{VK_NEXT,L"PageDown"},{VK_LEFT,L"Left"},{VK_RIGHT,L"Right"},{VK_UP,L"Up"},{VK_DOWN,L"Down"},
        {VK_CONTROL,L"Ctrl"},{VK_MENU,L"Alt"},{VK_SHIFT,L"Shift"},{VK_LWIN,L"Win"},
        {VK_NUMPAD0,L"Num0"},{VK_NUMPAD1,L"Num1"},{VK_NUMPAD2,L"Num2"},{VK_NUMPAD3,L"Num3"},{VK_NUMPAD4,L"Num4"},
        {VK_NUMPAD5,L"Num5"},{VK_NUMPAD6,L"Num6"},{VK_NUMPAD7,L"Num7"},{VK_NUMPAD8,L"Num8"},{VK_NUMPAD9,L"Num9"},
        {VK_ADD,L"NumAdd"},{VK_SUBTRACT,L"NumSubtract"},{VK_MULTIPLY,L"NumMultiply"},{VK_DIVIDE,L"NumDivide"},{VK_DECIMAL,L"NumDecimal"},
        {VK_CAPITAL,L"CapsLock"},{VK_NUMLOCK,L"NumLock"},{VK_SCROLL,L"ScrollLock"},{VK_PAUSE,L"Pause"},{VK_SNAPSHOT,L"PrintScreen"},
        {VK_OEM_1,L"OEM1"},{VK_OEM_PLUS,L"OEMPlus"},{VK_OEM_COMMA,L"OEMComma"},{VK_OEM_MINUS,L"OEMMinus"},{VK_OEM_PERIOD,L"OEMPeriod"},
        {VK_OEM_2,L"OEM2"},{VK_OEM_3,L"OEM3"},{VK_OEM_4,L"OEM4"},{VK_OEM_5,L"OEM5"},{VK_OEM_6,L"OEM6"},{VK_OEM_7,L"OEM7"},{VK_OEM_102,L"OEM102"}
    };
    for (auto [code, name] : names) if (key == code) return text + name;
    return text + L"?";
}
bool validStep(const Step& s, std::wstring& error) {
    if (s.action < Action::Click || s.action > Action::Scroll || s.button < Button::Left || s.button > Button::Middle ||
        s.delayMs > MaxDelayMs || s.x < -100000 || s.x > 100000 || s.y < -100000 || s.y > 100000 ||
        s.modifiers > 15 || s.wheel < -100 || s.wheel > 100 || (s.action == Action::Scroll && s.wheel == 0)) {
        error = L"Une étape contient une valeur hors limites."; return false;
    }
    if (s.action == Action::Key || s.action == Action::KeyDown || s.action == Action::KeyUp) {
        uint16_t key = 0; uint8_t mods = 0;
        if (!parseKey(keyName(s.key, s.modifiers), key, mods) || key != s.key || mods != s.modifiers) {
            error = L"Touche inconnue."; return false;
        }
        if (s.action != Action::Key && s.modifiers) { error = L"Appuyer / relâcher attend une touche seule."; return false; }
    }
    return true;
}
bool validMacro(const Macro& macro, std::wstring& error) {
    if (macro.steps.empty() || macro.steps.size() > MaxSteps || macro.repeats > 1000000) {
        error = L"Une macro doit contenir 1 à 10 000 étapes et 0 à 1 000 000 répétitions."; return false;
    }
    uint64_t duration = 0;
    for (const auto& step : macro.steps) { if (!validStep(step, error)) return false; duration += step.delayMs; }
    if (!macro.repeats && !duration) { error = L"Une boucle infinie doit contenir au moins 1 ms d'attente."; return false; }
    return true;
}
std::wstring actionName(Action a) {
    const wchar_t* names[] = {L"Clic souris", L"Déplacement", L"Touche / raccourci", L"Appuyer touche", L"Relâcher touche", L"Pause", L"Molette"};
    return names[static_cast<int>(a)];
}
std::wstring describe(const Step& s) {
    switch (s.action) {
    case Action::Click: {
        const wchar_t* b[] = {L"Gauche", L"Droit", L"Milieu"};
        return std::wstring(b[static_cast<int>(s.button)]) + (s.fixed ? L" · (" + std::to_wstring(s.x) + L", " + std::to_wstring(s.y) + L")" : L" · position actuelle");
    }
    case Action::Move: return L"X " + std::to_wstring(s.x) + L"   Y " + std::to_wstring(s.y);
    case Action::Key: case Action::KeyDown: case Action::KeyUp: return keyName(s.key, s.modifiers);
    case Action::Wait: return L"Attendre " + std::to_wstring(s.delayMs) + L" ms";
    case Action::Scroll: return std::to_wstring(s.wheel) + L" cran(s)";
    }
    return {};
}
bool saveMacro(const std::filesystem::path& path, const Macro& macro, std::wstring& error) {
    if (!validMacro(macro, error)) return false;
    auto temp = path; temp += L".tmp-" + std::to_wstring(GetCurrentProcessId());
    std::ofstream file(temp, std::ios::binary | std::ios::trunc);
    file << "MACROPULSE 1\n" << macro.repeats << ' ' << macro.steps.size() << '\n';
    for (const auto& s : macro.steps)
        file << static_cast<int>(s.action) << ' ' << s.delayMs << ' ' << s.x << ' ' << s.y << ' ' << s.fixed << ' '
             << static_cast<int>(s.button) << ' ' << s.key << ' ' << static_cast<int>(s.modifiers) << ' ' << s.wheel << '\n';
    file.flush();
    bool ok = file.good(); file.close(); ok = ok && !file.fail();
    if (ok && MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
    std::error_code ec; std::filesystem::remove(temp, ec);
    error = L"Impossible d'enregistrer ce fichier. Vérifiez le dossier et les droits d'accès."; return false;
}
bool loadMacro(const std::filesystem::path& path, Macro& macro, std::wstring& error) {
    std::error_code ec;
    if (std::filesystem::file_size(path, ec) > 4 * 1024 * 1024 || ec) { error = L"Fichier inaccessible ou trop volumineux (4 Mo maximum)."; return false; }
    std::ifstream file(path, std::ios::binary);
    std::string magic; int version; long long repeats, count;
    auto fail = [&]() { error = L"Fichier MacroPulse invalide ou version non prise en charge."; return false; };
    if (!(file >> magic >> version >> repeats >> count) || magic != "MACROPULSE" || version != 1 || repeats < 0 || repeats > 1000000 || count < 1 || count > static_cast<long long>(MaxSteps)) return fail();
    Macro candidate; candidate.repeats = static_cast<uint32_t>(repeats);
    for (long long i = 0; i < count; ++i) {
        long long a, d, x, y, f, b, k, m, w;
        if (!(file >> a >> d >> x >> y >> f >> b >> k >> m >> w) || a < 0 || a > 6 || d < 0 || d > MaxDelayMs ||
            x < -100000 || x > 100000 || y < -100000 || y > 100000 || f < 0 || f > 1 || b < 0 || b > 2 ||
            k < 0 || k > 255 || m < 0 || m > 15 || w < -100 || w > 100) return fail();
        candidate.steps.push_back({static_cast<Action>(a), static_cast<uint32_t>(d), static_cast<int>(x), static_cast<int>(y), f != 0,
                                  static_cast<Button>(b), static_cast<uint16_t>(k), static_cast<uint8_t>(m), static_cast<int>(w)});
    }
    file >> std::ws;
    if (!file.eof()) return fail();
    if (!validMacro(candidate, error)) return false;
    macro = std::move(candidate); return true;
}
}
