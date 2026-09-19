#include "library.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <set>

namespace pulse {
std::wstring cleanMacroName(const std::wstring& name) {
    auto first = name.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return {};
    auto result = name.substr(first, name.find_last_not_of(L" \t\r\n") - first + 1);
    if (result.size() > 80 || std::any_of(result.begin(), result.end(), [](wchar_t c) { return c < 32; })) return {};
    return result;
}
static std::string utf8(const std::wstring& text) {
    int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    if (size) WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}
bool validLibrary(const Library& library, std::wstring& error) {
    auto fail = [&]() { error = L"La bibliothèque contient des données invalides."; return false; };
    if (library.entries.size() > MaxMacros) return fail();
    std::set<uint64_t> ids; size_t total = 0;
    for (const auto& entry : library.entries) {
        if (!entry.id || entry.id >= INT64_MAX || !ids.insert(entry.id).second || cleanMacroName(entry.name) != entry.name || entry.name.empty() || utf8(entry.name).empty() ||
            entry.macro.repeats > 1000000 || entry.macro.steps.size() > MaxSteps) return fail();
        total += entry.macro.steps.size(); if (total > 200000) return fail();
        // Empty and unfinished sequences are saved too; execution validates them separately.
        for (const auto& step : entry.macro.steps) if (!validStep(step, error)) return false;
    }
    if (library.entries.empty() ? library.selected != 0 : !ids.contains(library.selected)) return fail();
    return true;
}
bool addLibraryMacro(Library& library, const std::wstring& name, const Macro& macro, std::wstring& error) {
    if (library.entries.size() >= MaxMacros) { error = L"Votre bibliothèque a atteint 1 000 macros."; return false; }
    uint64_t id = 1; for (const auto& entry : library.entries) id = std::max(id, entry.id + 1);
    Library candidate = library; candidate.entries.push_back({id, cleanMacroName(name), macro}); candidate.selected = id;
    if (!validLibrary(candidate, error)) return false;
    library = std::move(candidate); return true;
}
bool loadLibrary(const std::filesystem::path& file, Library& library, std::wstring& error) {
    auto fail = [&]() { error = L"Impossible de lire votre bibliothèque. Vos données ont été conservées."; return false; };
    std::error_code ec;
    if (std::filesystem::file_size(file, ec) > 64 * 1024 * 1024 || ec) return fail();
    std::ifstream in(file, std::ios::binary); std::string magic; long long version, selected, count;
    if (!(in >> magic >> version >> selected >> count) || magic != "MACROPULSE_LIBRARY" || version != 1 || selected < 0 || count < 0 || count > MaxMacros) return fail();
    Library candidate; candidate.selected = static_cast<uint64_t>(selected); size_t total = 0;
    for (long long i = 0; i < count; ++i) {
        long long id, repeats, steps; std::string name;
        if (!(in >> id) || id <= 0) return fail();
        in >> std::ws; if (in.peek() != '"' || !(in >> std::quoted(name) >> repeats >> steps) || name.size() > 320 || repeats < 0 || repeats > 1000000 || steps < 0 || steps > MaxSteps) return fail();
        int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.data(), static_cast<int>(name.size()), nullptr, 0);
        if (!length) return fail();
        std::wstring wide(length, L'\0'); MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.data(), static_cast<int>(name.size()), wide.data(), length);
        SavedMacro entry{static_cast<uint64_t>(id), std::move(wide), {static_cast<uint32_t>(repeats), {}}};
        total += static_cast<size_t>(steps); if (total > 200000) return fail();
        for (long long j = 0; j < steps; ++j) {
            long long a, d, x, y, f, b, k, m, w;
            if (!(in >> a >> d >> x >> y >> f >> b >> k >> m >> w) || a < 0 || a > 6 || d < 0 || d > MaxDelayMs ||
                x < -100000 || x > 100000 || y < -100000 || y > 100000 || f < 0 || f > 1 || b < 0 || b > 2 || k < 0 || k > 255 || m < 0 || m > 15 || w < -100 || w > 100) return fail();
            entry.macro.steps.push_back({static_cast<Action>(a), static_cast<uint32_t>(d), static_cast<int>(x), static_cast<int>(y), f != 0, static_cast<Button>(b), static_cast<uint16_t>(k), static_cast<uint8_t>(m), static_cast<int>(w)});
        }
        candidate.entries.push_back(std::move(entry));
    }
    in >> std::ws; if (!in.eof() || !validLibrary(candidate, error)) return fail();
    library = std::move(candidate); return true;
}
bool saveLibrary(const std::filesystem::path& file, const Library& library, std::wstring& error) {
    if (!validLibrary(library, error)) return false;
    auto fail = [&]() { error = L"Vos modifications ne sont pas encore sauvegardées. Vérifiez l'espace disponible, puis réessayez."; return false; };
    if (file.empty()) return fail();
    std::error_code ec; if (!file.parent_path().empty()) std::filesystem::create_directories(file.parent_path(), ec);
    auto temp = file; temp += L".tmp-" + std::to_wstring(GetCurrentProcessId());
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    out << "MACROPULSE_LIBRARY 1\n" << library.selected << ' ' << library.entries.size() << '\n';
    for (const auto& entry : library.entries) {
        out << entry.id << ' ' << std::quoted(utf8(entry.name)) << ' ' << entry.macro.repeats << ' ' << entry.macro.steps.size() << '\n';
        for (const auto& s : entry.macro.steps) out << static_cast<int>(s.action) << ' ' << s.delayMs << ' ' << s.x << ' ' << s.y << ' ' << s.fixed << ' ' << static_cast<int>(s.button) << ' ' << s.key << ' ' << static_cast<int>(s.modifiers) << ' ' << s.wheel << '\n';
    }
    out.flush(); bool ok = out.good(); out.close(); ok = ok && !out.fail();
    if (ok) {
        // Keep the previous valid generation. Never replace a good backup with corrupt data.
        Library previous; std::wstring problem;
        if (loadLibrary(file, previous, problem)) {
            auto backup = file; backup += L".bak";
            if (ReplaceFileW(file.c_str(), temp.c_str(), backup.c_str(), 0, nullptr, nullptr)) return true;
        } else if (MoveFileExW(temp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
    }
    std::filesystem::remove(temp, ec); return fail();
}
}
