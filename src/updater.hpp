#pragma once
#include <windows.h>
#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <stop_token>

namespace pulse::updates {
using Version = std::array<unsigned, 3>;
struct Release {
    std::wstring version, url, sha256;
    uint64_t size = 0;
};
std::optional<Version> parseVersion(std::wstring_view text);
std::optional<Release> parseRelease(std::string_view json, std::wstring_view currentVersion);
std::optional<Release> checkLatest(std::stop_token stop = {});
std::filesystem::path download(const Release& release, std::stop_token stop = {});
std::wstring sha256(const std::filesystem::path& file);
void verifyPayload(const std::filesystem::path& file, const Release& release);
std::filesystem::path executablePath();
// Only called after explicit consent and successful saving of application state.
bool launchInstaller(const std::filesystem::path& payload, const Release& release, std::wstring& error);
int runInstaller(int argc, wchar_t** argv);
bool replaceExecutable(const std::filesystem::path& target, const std::filesystem::path& staged,
                       const std::filesystem::path& backup);
bool restoreExecutable(const std::filesystem::path& target, const std::filesystem::path& backup);
void finishUpdate(const std::filesystem::path& directory) noexcept;
void discardDownload(const std::filesystem::path& payload) noexcept;
}
