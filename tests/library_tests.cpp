#include "library.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace pulse;
void check(bool value, const char* name) { if (!value) throw std::runtime_error(name); }
int main() {
    auto folder = std::filesystem::temp_directory_path() / (L"macropulse-library-test-" + std::to_wstring(GetCurrentProcessId()));
    folder = std::filesystem::absolute(folder).lexically_normal();
    auto tempRoot = std::filesystem::absolute(std::filesystem::temp_directory_path()).lexically_normal();
    if (tempRoot.filename().empty()) tempRoot = tempRoot.parent_path();
    if (folder.parent_path() != tempRoot || !folder.filename().wstring().starts_with(L"macropulse-library-test-")) return 1;
    std::filesystem::create_directories(folder); auto file = folder / L"library.dat", backup = folder / L"library.dat.bak";
    try {
        Library library; std::wstring error;
        check(addLibraryMacro(library, L"Routine équipe 日本語", {}, error), "save empty draft");
        auto first = library.selected;
        Step key; key.action = Action::Key; key.key = 'Q'; key.modifiers = 3;
        check(addLibraryMacro(library, L"Deuxième macro", {7, {key}}, error), "second macro");
        check(saveLibrary(file, library, error), "save internal library");
        Library restored; check(loadLibrary(file, restored, error), "load library");
        check(restored.entries.size() == 2 && restored.selected == library.selected && restored.entries[0].name == L"Routine équipe 日本語" && restored.entries[0].macro.steps.empty(), "names, drafts and selection round trip");
        check(restored.entries[1].macro.repeats == 7 && restored.entries[1].macro.steps[0].modifiers == 3, "actions round trip");
        check(addLibraryMacro(restored, L"Copie", restored.entries[1].macro, error), "duplicate independent macro");
        restored.entries[2].macro.steps[0].key = 'V'; check(restored.entries[1].macro.steps[0].key == 'Q', "copy has independent actions");
        restored.entries.erase(restored.entries.begin() + 1); restored.selected = first;
        check(saveLibrary(file, restored, error), "rename and deletion persistence");
        Library prior; check(loadLibrary(backup, prior, error) && prior.entries.size() == 2 && prior.entries[1].name == L"Deuxième macro", "previous generation backup");
        Library latest; check(loadLibrary(file, latest, error) && latest.entries[1].name == L"Copie" && latest.selected == first, "selection after deletion");
        const char* malformed[] = {"MACROPULSE_LIBRARY 2\n0 0", "MACROPULSE_LIBRARY 1\n9 0", "MACROPULSE_LIBRARY 1\n1 1001", "MACROPULSE_LIBRARY 1\n1 1\n1 \"Bad\" 1 -1", "MACROPULSE_LIBRARY 1\n1 1\n1 \"Bad\" 1 1\n2 0 0 0 0 0 255 0 1", "MACROPULSE_LIBRARY 1\n1 2\n1 \"A\" 1 0\n1 \"B\" 1 0", "MACROPULSE_LIBRARY 1\n1 1\n1 \"\xff\" 1 0", "MACROPULSE_LIBRARY 1\n0 0\nextra"};
        for (auto text : malformed) {
            { std::ofstream out(file, std::ios::binary); out << text; }
            check(!loadLibrary(file, latest, error) && latest.entries[1].name == L"Copie", "malformed load preserves memory");
        }
        check(saveLibrary(file, prior, error), "restore valid backup over corrupt primary");
        check(loadLibrary(backup, latest, error) && latest.entries[1].name == L"Deuxième macro", "repair never backs up corrupted content");
        Library invalid = prior; invalid.entries[1].id = invalid.entries[0].id;
        check(!saveLibrary(file, invalid, error) && loadLibrary(file, latest, error) && latest.entries[1].id != latest.entries[0].id, "invalid save cannot damage stored macros");
        auto obstructed = folder / L"blocked"; std::filesystem::create_directory(obstructed);
        check(!saveLibrary(obstructed, prior, error), "write failure is reported");
        Library empty; check(saveLibrary(file, empty, error) && loadLibrary(file, latest, error) && latest.entries.empty() && !latest.selected, "deleting last macro survives restart");
        check(cleanMacroName(L"  Bonjour  ") == L"Bonjour" && cleanMacroName(L"   ").empty(), "name validation");
        std::filesystem::remove_all(folder);
        std::cout << "Library persistence, backup, recovery and validation passed.\n"; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; std::filesystem::remove_all(folder); return 1; }
}
