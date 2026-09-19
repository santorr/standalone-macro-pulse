#include "updater.hpp"
#include "update_test_version.hpp"
#include <iostream>
int wmain(int argc, wchar_t** argv) {
    if (argc > 1 && std::wstring(argv[1]) == L"--apply-update") return pulse::updates::runInstaller(argc, argv);
    if (argc == 2 && std::wstring(argv[1]) == L"--check-live") {
        try { auto release = pulse::updates::checkLatest(); std::wcout << (release ? release->version : L"No newer stable release") << '\n'; return 0; }
        catch (...) { std::cerr << "Live GitHub check failed\n"; return 1; }
    }
    if (argc != 3 || std::wstring(argv[1]) != L"--start-test") return 2;
    try {
        std::filesystem::path file = argv[2];
        pulse::updates::Release release{TestUpgradeVersion, {}, pulse::updates::sha256(file), std::filesystem::file_size(file)};
        std::wstring error;
        if (!pulse::updates::launchInstaller(file, release, error)) { std::wcerr << error; return 1; }
        return 0;
    } catch (...) { return 3; }
}
