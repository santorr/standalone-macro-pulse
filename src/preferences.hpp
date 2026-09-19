#pragma once
#include "engine.hpp"
#include <array>
#include <functional>

namespace pulse {
struct Hotkey {
    uint16_t key = 0;
    uint8_t modifiers = 0; // Same modifier bits as macro keys, not Win32 MOD_*.
    bool operator==(const Hotkey&) const = default;
};
using Hotkeys = std::array<Hotkey, 4>;
inline constexpr Hotkeys DefaultHotkeys{{{VK_F6, 0}, {VK_F7, 0}, {VK_F8, 0}, {VK_F9, 0}}};
struct Preferences {
    ClickConfig click;
    uint32_t startDelayMs = 1500;
    int page = 0;
    Hotkeys hotkeys = DefaultHotkeys;
    std::filesystem::path lastMacro;
};
bool validHotkeys(const Hotkeys& keys, std::wstring& error);
bool parseHotkey(const std::wstring& text, Hotkey& key);
UINT nativeModifiers(const Hotkey& key);
bool conflictsWithHotkeys(const Step& step, const Hotkeys& keys);
bool validPreferences(const Preferences& settings, std::wstring& error);
std::filesystem::path preferencesPath();
bool loadPreferences(const std::filesystem::path& file, Preferences& settings, std::wstring& error);
bool savePreferences(const std::filesystem::path& file, const Preferences& settings, std::wstring& error);

// Acquire replacements before releasing existing bindings. A conflict leaves the
// entire previous set intact, including its emergency-stop binding.
class HotkeyRegistry {
public:
    using Acquire = std::function<bool(int, const Hotkey&)>;
    using Release = std::function<void(int)>;
    HotkeyRegistry(Acquire acquire, Release release) : acquire_(std::move(acquire)), release_(std::move(release)) {}
    ~HotkeyRegistry() { clear(); }
    void initialize(const Hotkeys& keys);
    bool apply(const Hotkeys& keys, size_t& failedIndex);
    bool active(size_t index) const { return registrations_[index].id != 0; }
    int actionFor(int id) const;
    void clear();
    void suspendLaunchShortcuts();
    void resumeMissing(const Hotkeys& keys);
private:
    struct Binding { int id = 0; Hotkey key; };
    std::array<Binding, 4> registrations_{};
    Acquire acquire_;
    Release release_;
};
}
