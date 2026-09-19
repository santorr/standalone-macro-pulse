#include <windows.h>
#include <filesystem>
#include <fstream>
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    wchar_t path[32768]{};
    if (!GetModuleFileNameW(nullptr, path, 32768)) return 1;
    std::ofstream out(std::filesystem::path(path).parent_path() / L"restarted.txt");
    out << "Restarted the verified upgrade."; return out.good() ? 0 : 1;
}
