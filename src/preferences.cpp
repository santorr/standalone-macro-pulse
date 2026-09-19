#include "preferences.hpp"
#include <shlobj.h>
#include <fstream>
#include <iomanip>
#include <algorithm>

namespace pulse {
static bool validHotkey(const Hotkey& key) {
    if (key.modifiers > 7 || key.key == VK_F12 || (key.key == VK_F4 && (key.modifiers & 2))) return false;
    if (key.key == VK_CONTROL || key.key == VK_SHIFT || key.key == VK_MENU || key.key == VK_LWIN) return false;
    uint16_t parsed = 0; uint8_t modifiers = 0;
    return parseKey(keyName(key.key, key.modifiers), parsed, modifiers) && parsed == key.key && modifiers == key.modifiers;
}
bool validHotkeys(const Hotkeys& keys, std::wstring& error) {
    for (size_t i = 0; i < keys.size(); ++i) {
        if (!validHotkey(keys[i])) { error = L"Choose a key on its own or with Ctrl, Alt or Shift. F12, Alt+F4 and Windows key combinations are reserved."; return false; }
        for (size_t j = 0; j < i; ++j) if (keys[i] == keys[j]) { error = L"Each action must have a different shortcut."; return false; }
    }
    return true;
}
bool parseHotkey(const std::wstring& text, Hotkey& key) {
    Hotkey candidate;
    if (!parseKey(text, candidate.key, candidate.modifiers) || !validHotkey(candidate)) return false;
    key = candidate; return true;
}
UINT nativeModifiers(const Hotkey& key) {
    return ((key.modifiers & 1) ? MOD_CONTROL : 0) | ((key.modifiers & 2) ? MOD_ALT : 0) | ((key.modifiers & 4) ? MOD_SHIFT : 0);
}
bool conflictsWithHotkeys(const Step& step, const Hotkeys& keys) {
    if (step.action != Action::Key && step.action != Action::KeyDown && step.action != Action::KeyUp) return false;
    // Conservative across separately-held modifiers and repetitions: reserve the
    // primary keys, regardless of modifiers on this individual step.
    return std::any_of(keys.begin(), keys.end(), [&](const Hotkey& key) { return key.key == step.key; });
}
bool validPreferences(const Preferences& p, std::wstring& error) {
    if (!p.click.intervalMs || p.click.intervalMs > 60000 || p.click.count > 1000000 || p.startDelayMs > 60000 ||
        p.click.button < Button::Left || p.click.button > Button::Middle || p.click.x < -100000 || p.click.x > 100000 ||
        p.click.y < -100000 || p.click.y > 100000 || p.page < 0 || p.page > 3 || p.lastMacro.native().size() > 32767) {
        error = L"Invalid settings."; return false;
    }
    return validHotkeys(p.hotkeys, error);
}
std::filesystem::path preferencesPath() {
    PWSTR base = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &base))) return {};
    std::filesystem::path path = std::filesystem::path(base) / L"MacroPulse" / L"settings.dat";
    CoTaskMemFree(base); return path;
}
bool savePreferences(const std::filesystem::path& file, const Preferences& p, std::wstring& error) {
    if (!validPreferences(p, error)) return false;
    if (file.empty()) { error = L"Your settings folder is inaccessible."; return false; }
    std::error_code ec;
    if (!file.parent_path().empty()) std::filesystem::create_directories(file.parent_path(), ec);
    auto temp = file; temp += L".tmp-" + std::to_wstring(GetCurrentProcessId());
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    auto utf8 = p.lastMacro.u8string();
    out << "MACROPULSE_SETTINGS 1\n" << p.click.intervalMs << ' ' << p.click.count << ' ' << static_cast<int>(p.click.button) << ' '
        << p.click.fixed << ' ' << p.click.x << ' ' << p.click.y << ' ' << p.startDelayMs << ' ' << p.page << '\n';
    for (auto key : p.hotkeys) out << key.key << ' ' << static_cast<int>(key.modifiers) << '\n';
    out << std::quoted(std::string(utf8.begin(), utf8.end())) << '\n';
    out.flush(); bool ok = out.good(); out.close(); ok = ok && !out.fail();
    if (ok && MoveFileExW(temp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
    std::filesystem::remove(temp, ec); error = L"Could not save settings. Check access to the MacroPulse folder."; return false;
}
bool loadPreferences(const std::filesystem::path& file, Preferences& settings, std::wstring& error) {
    std::error_code ec;
    auto fail = [&]() { error = L"Saved settings could not be read. Default values are being used."; return false; };
    if (std::filesystem::file_size(file, ec) > 131072 || ec) return fail();
    std::ifstream in(file, std::ios::binary); std::string magic, macroPath;
    long long version, interval, count, button, fixed, x, y, delay, page;
    if (!(in >> magic >> version >> interval >> count >> button >> fixed >> x >> y >> delay >> page) ||
        magic != "MACROPULSE_SETTINGS" || version != 1 || interval < 1 || interval > 60000 || count < 0 || count > 1000000 ||
        button < 0 || button > 2 || fixed < 0 || fixed > 1 || x < -100000 || x > 100000 || y < -100000 || y > 100000 ||
        delay < 0 || delay > 60000 || page < 0 || page > 3) return fail();
    Preferences candidate;
    candidate.click = {static_cast<uint32_t>(interval), static_cast<uint32_t>(count), static_cast<Button>(button), fixed != 0, static_cast<int>(x), static_cast<int>(y)};
    candidate.startDelayMs = static_cast<uint32_t>(delay); candidate.page = static_cast<int>(page);
    for (auto& key : candidate.hotkeys) {
        long long vk, modifiers;
        if (!(in >> vk >> modifiers) || vk < 0 || vk > 255 || modifiers < 0 || modifiers > 7) return fail();
        key = {static_cast<uint16_t>(vk), static_cast<uint8_t>(modifiers)};
    }
    in >> std::ws; if (in.peek() != '"' || !(in >> std::quoted(macroPath))) return fail();
    in >> std::ws; if (!in.eof() || macroPath.find('\0') != std::string::npos) return fail();
    if (!macroPath.empty() && !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, macroPath.data(), static_cast<int>(macroPath.size()), nullptr, 0)) return fail();
    try { candidate.lastMacro = std::filesystem::path(std::u8string(macroPath.begin(), macroPath.end())); } catch (...) { return fail(); }
    if (!validPreferences(candidate, error)) return false;
    settings = std::move(candidate); return true;
}
void HotkeyRegistry::clear() { for (auto& r : registrations_) { if (r.id) release_(r.id); r = {}; } }
void HotkeyRegistry::suspendLaunchShortcuts() {
    for (size_t i = 0; i < registrations_.size(); ++i) if (i != 2 && registrations_[i].id) {
        release_(registrations_[i].id); registrations_[i] = {};
    }
}
void HotkeyRegistry::resumeMissing(const Hotkeys& keys) {
    std::wstring error; if (!validHotkeys(keys, error)) return;
    for (size_t i = 0; i < registrations_.size(); ++i) if (!registrations_[i].id) {
        int id = 0x510;
        while (std::any_of(registrations_.begin(), registrations_.end(), [&](const Binding& item) { return item.id == id; })) ++id;
        if (acquire_(id, keys[i])) registrations_[i] = {id, keys[i]};
    }
}
int HotkeyRegistry::actionFor(int id) const {
    for (int i = 0; i < 4; ++i) if (registrations_[i].id && registrations_[i].id == id) return i;
    return -1;
}
void HotkeyRegistry::initialize(const Hotkeys& keys) {
    clear(); std::wstring error; if (!validHotkeys(keys, error)) return;
    for (size_t i = 0; i < 4; ++i) if (acquire_(0x510 + static_cast<int>(i), keys[i])) registrations_[i] = {0x510 + static_cast<int>(i), keys[i]};
}
bool HotkeyRegistry::apply(const Hotkeys& keys, size_t& failedIndex) {
    std::wstring error; if (!validHotkeys(keys, error)) { failedIndex = 0; return false; }
    std::array<Binding, 4> next{};
    std::array<int, 4> acquired{};
    for (size_t i = 0; i < 4; ++i) {
        auto reuse = std::find_if(registrations_.begin(), registrations_.end(), [&](const Binding& r) { return r.id && r.key == keys[i]; });
        if (reuse != registrations_.end()) { next[i] = *reuse; continue; }
        int id = 0x510;
        auto used = [&](int value) { return std::any_of(registrations_.begin(), registrations_.end(), [&](const Binding& r) { return r.id == value; }) || std::find(acquired.begin(), acquired.end(), value) != acquired.end(); };
        while (used(id)) ++id;
        if (!acquire_(id, keys[i])) { for (int added : acquired) if (added) release_(added); failedIndex = i; return false; }
        acquired[i] = id; next[i] = {id, keys[i]};
    }
    for (const auto& old : registrations_) if (old.id && std::none_of(next.begin(), next.end(), [&](const Binding& r) { return r.id == old.id; })) release_(old.id);
    registrations_ = next; return true;
}
}
