#include "updater.hpp"
#include "version.hpp"
#include <winrt/Windows.Data.Json.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace pulse::updates;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class F> void rejects(F f, const char* message) {
    bool rejected = false; try { f(); } catch (...) { rejected = true; } check(rejected, message);
}
std::string metadata(const std::string& version = "1.6.0") {
    return "{\"body\":\"Escaped \\\"tag_name\\\": \\\"v99.0.0\\\" and unicode \\u00e9\",\"draft\":false,\"prerelease\":false,\"tag_name\":\"v" + version +
        "\",\"assets\":[{\"name\":\"MacroPulse-" + version + "-windows-x64.exe\",\"state\":\"uploaded\",\"size\":1234,\"digest\":\"sha256:" + std::string(64, 'a') +
        "\",\"browser_download_url\":\"https://github.com/santorr/standalone-macro-pulse/releases/download/v" + version + "/MacroPulse-" + version + "-windows-x64.exe\"}]}";
}
std::string changed(std::string text, const std::string& from, const std::string& to) {
    auto at = text.find(from); check(at != std::string::npos, "fixture field missing"); text.replace(at, from.size(), to); return text;
}
void write(const std::filesystem::path& path, const std::string& data) { std::ofstream out(path, std::ios::binary); out << data; }
std::string read(const std::filesystem::path& path) { std::ifstream in(path, std::ios::binary); return {std::istreambuf_iterator<char>(in), {}}; }
int wmain(int argc, wchar_t** argv) {
    try {
        winrt::init_apartment();
        if (argc == 3 && std::wstring(argv[1]) == L"--live-release") {
            std::ifstream in(argv[2], std::ios::binary); std::string json{std::istreambuf_iterator<char>(in), {}};
            auto release = parseRelease(json, L"0.0.0"); check(release.has_value(), "live release metadata");
            auto payload = download(*release); verifyPayload(payload, *release); discardDownload(payload);
            std::cout << "Official release downloaded over HTTPS and verified (size, SHA-256, PE and version).\n"; return 0;
        }
        for (auto invalid : {L"", L"1.2", L"v1.2.3", L"1.2.3.4", L"1.2.3-beta", L"01.2.3", L"1.2.-1", L"1.2.65536", L"999999999999.0.0"})
            check(!parseVersion(invalid), "reject malformed versions");
        check(parseVersion(L"1.10.0") > parseVersion(L"1.9.99"), "compare version numbers numerically");
        auto good = metadata(); auto release = parseRelease(good, L"1.5.0");
        check(release && release->version == L"1.6.0" && release->size == 1234, "parse escaped JSON and official asset");
        check(!parseRelease(good, L"1.6.0") && !parseRelease(good, L"2.0.0"), "no reinstall or downgrade");
        check(!parseRelease(changed(good, "\"draft\":false", "\"draft\":true"), L"1.0.0"), "ignore drafts");
        check(!parseRelease(changed(good, "\"prerelease\":false", "\"prerelease\":true"), L"1.0.0"), "ignore prereleases");
        for (const auto& bad : {std::string("not json"), changed(good, "sha256:", "sha512:"), changed(good, "https://github.com", "http://github.com"),
             changed(good, "santorr/", "attacker/"), changed(good, "windows-x64.exe", "windows-arm64.exe"),
             changed(good, "\"size\":1234", "\"size\":999999999"), changed(good, "\"size\":1234", "\"size\":-1"),
             changed(good, "\"size\":1234", "\"size\":1.5"), changed(good, "uploaded", "new"), changed(good, "\"digest\":", "\"missing\":"),
             changed(good, "sha256:" + std::string(64, 'a'), "sha256:bad")})
            rejects([&] { parseRelease(bad, L"1.0.0"); }, "reject unsafe or incomplete release metadata");
        check(argc == 4, "expected application, fixture and test directory");
        auto directory = std::filesystem::path(argv[3]) / std::to_wstring(GetCurrentProcessId());
        std::filesystem::create_directories(directory);
        auto file = directory / L"hash.txt"; write(file, "abc");
        check(sha256(file) == L"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256 known vector");
        auto exe = std::filesystem::path(argv[1]);
        Release actual{pulse::AppVersion, {}, sha256(exe), std::filesystem::file_size(exe)}; verifyPayload(exe, actual);
        auto corrupt = actual; corrupt.sha256[0] = corrupt.sha256[0] == L'0' ? L'1' : L'0';
        rejects([&] { verifyPayload(exe, corrupt); }, "reject checksum mismatch");
        corrupt = actual; ++corrupt.size; rejects([&] { verifyPayload(exe, corrupt); }, "reject truncated files");
        corrupt = actual; corrupt.version = L"0.0.0"; rejects([&] { verifyPayload(exe, corrupt); }, "reject mismatched embedded version");
        Release notExe{pulse::AppVersion, {}, sha256(file), 3}; rejects([&] { verifyPayload(file, notExe); }, "reject non-PE payload");
        auto target = directory / L"current.exe", staged = directory / L"staged.exe", backup = directory / L"current.exe.previous";
        write(target, "old"); write(staged, "new");
        HANDLE locked = CreateFileW(target.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        check(locked != INVALID_HANDLE_VALUE, "lock target fixture");
        check(!replaceExecutable(target, staged, backup), "do not replace a locked executable"); CloseHandle(locked);
        check(read(target) == "old" && read(staged) == "new", "failed replacement preserves original");
        check(replaceExecutable(target, staged, backup) && read(target) == "new" && read(backup) == "old", "atomic replace keeps backup");
        check(restoreExecutable(target, backup) && read(target) == "old", "rollback restores old executable");
        write(staged, "second"); check(replaceExecutable(target, staged, backup), "second replace");
        write(staged, "third"); check(replaceExecutable(target, staged, backup) && read(backup) == "second", "replace refreshes an existing backup");
        // A real helper process waits for an isolated host to exit, replaces it,
        // and restarts a harmless fixture. It never touches the user's app/data.
        auto host = directory / L"MacroPulse test host 日本語.exe";
        auto sourceHost = std::filesystem::path(argv[2]).parent_path() / L"update_test_host.exe";
        std::filesystem::copy_file(sourceHost, host);
        auto downloadDir = directory / L"MacroPulse-update-test"; std::filesystem::create_directory(downloadDir);
        auto payload = downloadDir / L"payload.exe"; std::filesystem::copy_file(argv[2], payload);
        write(directory / L"settings.dat", "user settings preserved");
        std::wstring command = L"\"" + host.wstring() + L"\" --start-test \"" + payload.wstring() + L"\"";
        STARTUPINFOW startup{}; startup.cb = sizeof(startup); PROCESS_INFORMATION process{};
        check(CreateProcessW(host.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, directory.c_str(), &startup, &process) != FALSE, "start test host");
        check(WaitForSingleObject(process.hProcess, 30000) == WAIT_OBJECT_0, "host finishes update handoff");
        DWORD exitCode = 1; GetExitCodeProcess(process.hProcess, &exitCode); CloseHandle(process.hThread); CloseHandle(process.hProcess);
        check(exitCode == 0, "host authorizes upgrade");
        auto marker = directory / L"restarted.txt";
        for (int i = 0; i < 100 && !std::filesystem::exists(marker); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        check(std::filesystem::exists(marker), "new executable restarted");
        check(sha256(host) == sha256(argv[2]), "installed exact verified payload");
        auto hostBackup = host; hostBackup += L".previous";
        check(sha256(hostBackup) == sha256(sourceHost), "original host preserved as backup");
        check(read(directory / L"settings.dat") == "user settings preserved", "settings survive upgrade");
        std::cout << "Updater validation and real replacement/restart tests passed.\n";
        return 0;
    } catch (const std::exception& ex) { std::cerr << ex.what() << '\n'; return 1; }
      catch (const winrt::hresult_error& ex) { std::wcerr << ex.message().c_str() << '\n'; return 1; }
}
