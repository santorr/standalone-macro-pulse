#pragma once
#include "model.hpp"

namespace pulse {
struct SavedMacro {
    uint64_t id = 0;
    std::wstring name;
    Macro macro;
};
struct Library {
    uint64_t selected = 0;
    std::vector<SavedMacro> entries;
};
inline constexpr size_t MaxMacros = 1000;
bool validLibrary(const Library& library, std::wstring& error);
bool loadLibrary(const std::filesystem::path& file, Library& library, std::wstring& error);
bool saveLibrary(const std::filesystem::path& file, const Library& library, std::wstring& error);
bool addLibraryMacro(Library& library, const std::wstring& name, const Macro& macro, std::wstring& error);
std::wstring cleanMacroName(const std::wstring& name);
}
