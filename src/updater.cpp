#include "dialogs.hpp"
#include "updater.hpp"
#include "version.hpp"
#include <winhttp.h>
#include <objbase.h>
#include <bcrypt.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace pulse::updates {
namespace {
constexpr uint64_t MaxDownload = 64 * 1024 * 1024;
constexpr wchar_t ReleasePrefix[] = L"https://github.com/santorr/standalone-macro-pulse/releases/download/v";
struct Handle {
    HANDLE value = nullptr;
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
struct Internet {
    HINTERNET value = nullptr;
    ~Internet() { if (value) WinHttpCloseHandle(value); }
};
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
std::wstring uniqueId() {
    GUID guid{}; require(SUCCEEDED(CoCreateGuid(&guid)), "Could not create an update identifier.");
    wchar_t text[40]{}; StringFromGUID2(guid, text, 40); return text;
}
bool validDigest(std::wstring_view text) {
    return text.size() == 64 && std::all_of(text.begin(), text.end(), [](wchar_t c) {
        return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f');
    });
}
std::wstring expectedUrl(const std::wstring& version) {
    return std::wstring(ReleasePrefix) + version + L"/MacroPulse-" + version + L"-windows-x64.exe";
}
// Only HTTPS redirects to GitHub's release hosts are accepted. No tokens,
// settings or macro data are sent. Windows certificate validation stays enabled.
std::vector<char> request(std::wstring url, size_t limit, std::stop_token stop) {
    Internet session{WinHttpOpen(L"MacroPulse updater", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
    require(session.value != nullptr, "Could not initialize the update connection.");
    require(WinHttpSetTimeouts(session.value, 5000, 5000, 5000, 5000) != FALSE, "Could not set network timeouts.");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(90);
    for (int redirect = 0; redirect < 5; ++redirect) {
        require(!stop.stop_requested(), "The update was cancelled.");
        URL_COMPONENTS parts{}; parts.dwStructSize = sizeof(parts);
        parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = parts.dwUserNameLength = parts.dwPasswordLength = DWORD(-1);
        require(WinHttpCrackUrl(url.c_str(), 0, 0, &parts) != FALSE, "Invalid update URL.");
        std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
        require(parts.nScheme == INTERNET_SCHEME_HTTPS && parts.nPort == 443 && !parts.dwUserNameLength && !parts.dwPasswordLength &&
                (host == L"api.github.com" || host == L"github.com" || host == L"release-assets.githubusercontent.com" || host == L"objects.githubusercontent.com"),
                "The update server redirected to an untrusted location.");
        std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
        if (parts.dwExtraInfoLength) path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
        Internet connection{WinHttpConnect(session.value, host.c_str(), parts.nPort, 0)};
        require(connection.value != nullptr, "Could not connect to GitHub.");
        Internet req{WinHttpOpenRequest(connection.value, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)};
        require(req.value != nullptr, "Could not create the update request.");
        DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        require(WinHttpSetOption(req.value, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy)) != FALSE, "Could not configure redirects.");
        DWORD auth = WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
        require(WinHttpSetOption(req.value, WINHTTP_OPTION_AUTOLOGON_POLICY, &auth, sizeof(auth)) != FALSE, "Could not configure authentication.");
        const wchar_t* headers = host == L"api.github.com" ? L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n" : L"Accept: application/octet-stream\r\n";
        require(WinHttpSendRequest(req.value, headers, DWORD(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(req.value, nullptr),
                "GitHub could not be reached. Check your connection and try again.");
        DWORD status = 0, length = sizeof(status);
        require(WinHttpQueryHeaders(req.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &length, nullptr) != FALSE,
                "Could not read the update response.");
        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            length = 0; WinHttpQueryHeaders(req.value, WINHTTP_QUERY_LOCATION, nullptr, nullptr, &length, nullptr);
            require(length > 0 && length <= 32768, "Invalid update redirect.");
            std::vector<wchar_t> location(length / sizeof(wchar_t) + 1);
            require(WinHttpQueryHeaders(req.value, WINHTTP_QUERY_LOCATION, nullptr, location.data(), &length, nullptr) != FALSE, "Invalid update redirect.");
            url = location.data(); continue;
        }
        require(status == 200, status == 403 || status == 429 ? "GitHub's request limit was reached. Try again later." : "GitHub did not return a valid update.");
        std::vector<char> body; char buffer[16384]; DWORD received;
        for (;;) {
            require(!stop.stop_requested() && std::chrono::steady_clock::now() < deadline, "The update download timed out.");
            require(WinHttpReadData(req.value, buffer, sizeof(buffer), &received) != FALSE, "The update download was interrupted.");
            if (!received) break;
            require(body.size() + received <= limit, "The update response exceeds the size limit.");
            body.insert(body.end(), buffer, buffer + received);
        }
        return body;
    }
    throw std::runtime_error("Too many update redirects.");
}
std::wstring quote(const std::wstring& argument) {
    std::wstring result = L"\""; size_t slashes = 0;
    for (wchar_t c : argument) {
        if (c == L'\\') { ++slashes; continue; }
        result.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\'); slashes = 0; result += c;
    }
    result.append(slashes * 2, L'\\'); return result + L"\"";
}
bool startProcess(const std::filesystem::path& exe, std::wstring command, PROCESS_INFORMATION& process) {
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    return CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                          exe.parent_path().c_str(), &startup, &process) != FALSE;
}
bool restart(const std::filesystem::path& target, const std::filesystem::path& cleanup = {}) {
    PROCESS_INFORMATION process{};
    auto command = quote(target.wstring());
    if (!cleanup.empty()) command += L" --finish-update " + quote(cleanup.wstring());
    if (!startProcess(target, command, process)) return false;
    CloseHandle(process.hThread); CloseHandle(process.hProcess); return true;
}
}

std::optional<Version> parseVersion(std::wstring_view text) {
    Version version{}; size_t at = 0;
    for (size_t i = 0; i < 3; ++i) {
        size_t start = at;
        while (at < text.size() && text[at] >= L'0' && text[at] <= L'9') {
            version[i] = version[i] * 10 + static_cast<unsigned>(text[at++] - L'0');
            if (version[i] > 65535) return {};
        }
        if (start == at || (at - start > 1 && text[start] == L'0')) return {};
        if (i < 2 && (at == text.size() || text[at++] != L'.')) return {};
    }
    return at == text.size() ? std::optional(version) : std::nullopt;
}
std::optional<Release> parseRelease(std::string_view json, std::wstring_view currentVersion) {
    using namespace winrt::Windows::Data::Json;
    require(json.size() <= 2 * 1024 * 1024, "Release metadata is too large.");
    auto current = parseVersion(currentVersion); require(current.has_value(), "Invalid installed version.");
    auto root = JsonObject::Parse(winrt::to_hstring(json));
    if (root.GetNamedBoolean(L"draft") || root.GetNamedBoolean(L"prerelease")) return {};
    std::wstring tag(root.GetNamedString(L"tag_name"));
    require(tag.starts_with(L"v"), "Invalid release tag.");
    auto version = parseVersion(std::wstring_view(tag).substr(1)); require(version.has_value(), "Unsupported release version.");
    if (*version <= *current) return {};
    Release release; release.version = tag.substr(1);
    const auto name = L"MacroPulse-" + release.version + L"-windows-x64.exe";
    bool found = false;
    for (const auto& value : root.GetNamedArray(L"assets")) {
        auto asset = value.GetObject();
        if (asset.GetNamedString(L"name") != name) continue;
        require(!found, "Duplicate release assets."); found = true;
        require(asset.GetNamedString(L"state") == L"uploaded", "The update is not fully uploaded.");
        release.url = asset.GetNamedString(L"browser_download_url");
        require(release.url == expectedUrl(release.version), "The update asset belongs to an unexpected repository.");
        std::wstring digest(asset.GetNamedString(L"digest"));
        require(digest.starts_with(L"sha256:") && validDigest(std::wstring_view(digest).substr(7)), "The update has no valid SHA-256 digest.");
        release.sha256 = digest.substr(7);
        double size = asset.GetNamedNumber(L"size");
        require(size > 0 && size <= MaxDownload && size == static_cast<double>(static_cast<uint64_t>(size)), "Invalid update size.");
        release.size = static_cast<uint64_t>(size);
    }
    require(found, "This release has no compatible Windows x64 executable."); return release;
}
std::optional<Release> checkLatest(std::stop_token stop) {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    struct Apartment { ~Apartment() { winrt::uninit_apartment(); } } apartment;
    auto bytes = request(L"https://api.github.com/repos/santorr/standalone-macro-pulse/releases/latest", 2 * 1024 * 1024, stop);
    return parseRelease(std::string_view(bytes.data(), bytes.size()), AppVersion);
}
std::wstring sha256(const std::filesystem::path& file) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    require(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0, "SHA-256 is unavailable.");
    struct Algorithm { BCRYPT_ALG_HANDLE value; ~Algorithm() { BCryptCloseAlgorithmProvider(value, 0); } } cleanup{algorithm};
    BCRYPT_HASH_HANDLE hash = nullptr;
    require(BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0, "Could not create a SHA-256 hash.");
    struct Hash { BCRYPT_HASH_HANDLE value; ~Hash() { BCryptDestroyHash(value); } } hashCleanup{hash};
    std::ifstream in(file, std::ios::binary); require(in.is_open(), "Could not read the update file.");
    char buffer[16384];
    while (in) {
        in.read(buffer, sizeof(buffer));
        require(BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer), static_cast<ULONG>(in.gcount()), 0) >= 0, "Could not hash the update file.");
    }
    require(in.eof(), "Could not read the complete update file.");
    unsigned char digest[32]{}; require(BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0, "Could not finish the update hash.");
    std::wstring result; constexpr wchar_t hex[] = L"0123456789abcdef";
    for (auto byte : digest) { result += hex[byte >> 4]; result += hex[byte & 15]; } return result;
}
void verifyPayload(const std::filesystem::path& file, const Release& release) {
    require(parseVersion(release.version).has_value() && validDigest(release.sha256), "Invalid update metadata.");
    require(release.size > 0 && release.size <= MaxDownload && std::filesystem::file_size(file) == release.size, "The downloaded file has an unexpected size.");
    require(sha256(file) == release.sha256, "SHA-256 verification failed. The update was not installed.");
    DWORD ignored = 0, length = GetFileVersionInfoSizeW(file.c_str(), &ignored);
    require(length > 0, "The update is not a versioned Windows executable.");
    std::vector<char> data(length);
    require(GetFileVersionInfoW(file.c_str(), 0, length, data.data()) != FALSE, "Could not read the update version.");
    VS_FIXEDFILEINFO* info = nullptr; UINT bytes = 0;
    require(VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &bytes) && bytes >= sizeof(*info), "Invalid executable version.");
    auto expected = *parseVersion(release.version);
    require(info->dwSignature == 0xfeef04bd && HIWORD(info->dwFileVersionMS) == expected[0] && LOWORD(info->dwFileVersionMS) == expected[1] &&
            HIWORD(info->dwFileVersionLS) == expected[2] && LOWORD(info->dwFileVersionLS) == 0, "The executable version does not match the release.");
    std::ifstream in(file, std::ios::binary); IMAGE_DOS_HEADER dos{}; in.read(reinterpret_cast<char*>(&dos), sizeof(dos));
    require(in.good() && dos.e_magic == IMAGE_DOS_SIGNATURE && dos.e_lfanew > 0 && static_cast<uint64_t>(dos.e_lfanew) + sizeof(IMAGE_NT_HEADERS64) <= release.size, "Invalid Windows executable.");
    in.seekg(dos.e_lfanew); IMAGE_NT_HEADERS64 pe{}; in.read(reinterpret_cast<char*>(&pe), sizeof(pe));
    require(in.good() && pe.Signature == IMAGE_NT_SIGNATURE && pe.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 &&
            pe.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC && !(pe.FileHeader.Characteristics & IMAGE_FILE_DLL), "The update is not a Windows x64 application.");
}
std::filesystem::path executablePath() {
    std::wstring path(32768, L'\0'); DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    require(length > 0 && length < path.size(), "Could not locate MacroPulse."); path.resize(length); return path;
}
std::filesystem::path download(const Release& release, std::stop_token stop) {
    require(parseVersion(release.version).has_value() && release.url == expectedUrl(release.version) && release.size > 0 && release.size <= MaxDownload,
            "Invalid download metadata.");
    auto directory = std::filesystem::temp_directory_path() / (L"MacroPulse-update-" + uniqueId());
    require(std::filesystem::create_directory(directory), "Could not create the update folder.");
    auto payload = directory / L"payload.exe";
    try {
        auto bytes = request(release.url, static_cast<size_t>(release.size), stop);
        std::ofstream out(payload, std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size())); out.close();
        require(!out.fail(), "Could not save the update. Check available disk space.");
        verifyPayload(payload, release); return payload;
    } catch (...) { discardDownload(payload); throw; }
}
void discardDownload(const std::filesystem::path& payload) noexcept {
    // Delete only known files, never recursively delete a supplied directory.
    if (payload.filename() != L"payload.exe" || !payload.parent_path().filename().wstring().starts_with(L"MacroPulse-update-")) return;
    std::error_code ec; std::filesystem::remove(payload, ec);
    std::filesystem::remove(payload.parent_path() / L"updater.exe", ec);
    std::filesystem::remove(payload.parent_path(), ec);
}
void finishUpdate(const std::filesystem::path& directory) noexcept {
    try {
        // A restarted app cleans only a direct child of the Windows temp folder.
        if (!directory.filename().wstring().starts_with(L"MacroPulse-update-") ||
            !std::filesystem::equivalent(directory.parent_path(), std::filesystem::temp_directory_path()) ||
            (GetFileAttributesW(directory.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT)) return;
        for (int i = 0; i < 50; ++i) {
            discardDownload(directory / L"payload.exe");
            std::error_code ec; if (!std::filesystem::exists(directory, ec)) return;
            Sleep(100);
        }
    } catch (...) {}
}
bool replaceExecutable(const std::filesystem::path& target, const std::filesystem::path& staged, const std::filesystem::path& backup) {
    return ReplaceFileW(target.c_str(), staged.c_str(), backup.c_str(), 0, nullptr, nullptr) != FALSE;
}
bool restoreExecutable(const std::filesystem::path& target, const std::filesystem::path& backup) {
    if (GetFileAttributesW(backup.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
    return MoveFileExW(backup.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}
bool launchInstaller(const std::filesystem::path& payload, const Release& release, std::wstring& error) {
    try {
        verifyPayload(payload, release);
        auto target = executablePath(), helper = payload.parent_path() / L"updater.exe";
        require(CopyFileW(target.c_str(), helper.c_str(), TRUE) != FALSE, "Could not prepare the update helper.");
        auto name = L"Local\\MacroPulse.Update." + uniqueId();
        Handle ready{CreateEventW(nullptr, TRUE, FALSE, name.c_str())};
        Handle commit{CreateEventW(nullptr, TRUE, FALSE, (name + L".commit").c_str())};
        require(ready.value && commit.value, "Could not initialize the update handoff.");
        std::wstring command = quote(helper.wstring()) + L" --apply-update " + std::to_wstring(GetCurrentProcessId()) + L" " + quote(name) + L" " +
                               quote(target.wstring()) + L" " + quote(release.version) + L" " + quote(release.sha256) + L" " + std::to_wstring(release.size);
        PROCESS_INFORMATION process{};
        require(startProcess(helper, command, process), "Could not start the update helper.");
        Handle child{process.hProcess}, thread{process.hThread}; HANDLE events[]{ready.value, child.value};
        require(WaitForMultipleObjects(2, events, FALSE, 20000) == WAIT_OBJECT_0,
                "Could not prepare the replacement. Keep MacroPulse in a writable folder and try again.");
        require(SetEvent(commit.value) != FALSE, "Could not authorize the replacement."); return true;
    } catch (const std::exception& ex) { error = std::wstring(ex.what(), ex.what() + strlen(ex.what())); return false; }
}
int runInstaller(int argc, wchar_t** argv) {
    std::filesystem::path staged, payload, target, backup;
    bool parentExited = false, replaced = false;
    try {
        require(argc == 8, "Invalid update command.");
        size_t used = 0; auto pid = std::stoul(argv[2], &used);
        require(used == wcslen(argv[2]) && pid != 0, "Invalid parent process.");
        Handle parent{OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)};
        require(parent.value != nullptr, "The application is no longer running.");
        target = argv[4];
        std::wstring parentPath(32768, L'\0'); DWORD length = static_cast<DWORD>(parentPath.size());
        require(QueryFullProcessImageNameW(parent.value, 0, parentPath.data(), &length) != FALSE, "Could not identify the application.");
        parentPath.resize(length);
        require(std::filesystem::equivalent(target, parentPath), "The update target does not match the running application.");
        auto expected = parseVersion(argv[5]), current = parseVersion(AppVersion);
        require(expected && current && *expected > *current && validDigest(argv[6]), "Invalid upgrade version.");
        auto helper = executablePath(); payload = helper.parent_path() / L"payload.exe";
        require(helper.filename() == L"updater.exe" && helper.parent_path().filename().wstring().starts_with(L"MacroPulse-update-"), "Invalid update helper location.");
        used = 0; auto size = std::stoull(argv[7], &used); require(used == wcslen(argv[7]), "Invalid update size.");
        Release release{argv[5], {}, argv[6], size}; verifyPayload(payload, release);
        std::wstring name = argv[3]; require(name.starts_with(L"Local\\MacroPulse.Update."), "Invalid update event.");
        Handle ready{OpenEventW(EVENT_MODIFY_STATE, FALSE, name.c_str())};
        Handle commit{OpenEventW(SYNCHRONIZE, FALSE, (name + L".commit").c_str())};
        require(ready.value && commit.value, "The update was cancelled.");
        // Stage on the same volume and test write access before the app exits.
        staged = target.parent_path() / (L".MacroPulse-update-" + uniqueId() + L".exe");
        backup = target; backup += L".previous";
        require(CopyFileW(payload.c_str(), staged.c_str(), TRUE) != FALSE, "The application folder is not writable.");
        verifyPayload(staged, release);
        require(SetEvent(ready.value) != FALSE, "Could not finish the update handoff.");
        require(WaitForSingleObject(commit.value, 30000) == WAIT_OBJECT_0, "The update was cancelled.");
        require(WaitForSingleObject(parent.value, 60000) == WAIT_OBJECT_0, "MacroPulse did not close. The update was cancelled.");
        parentExited = true;
        for (int attempt = 0; attempt < 40; ++attempt) {
            if (replaceExecutable(target, staged, backup)) { replaced = true; break; }
            if (GetFileAttributesW(target.c_str()) == INVALID_FILE_ATTRIBUTES) restoreExecutable(target, backup);
            Sleep(250);
        }
        require(replaced, "Windows could not replace MacroPulse. The previous version has been kept.");
        require(restart(target, payload.parent_path()), "The updated application could not start. Restoring the previous version.");
        discardDownload(payload); return 0;
    } catch (const std::exception& ex) {
        if (parentExited) {
            if (replaced || GetFileAttributesW(target.c_str()) == INVALID_FILE_ATTRIBUTES) restoreExecutable(target, backup);
            restart(target);
            std::wstring message(ex.what(), ex.what() + strlen(ex.what()));
            pulse::messageBox(nullptr, message.c_str(), L"MacroPulse upgrade", MB_OK | MB_ICONWARNING);
        }
        if (!staged.empty()) { std::error_code ec; std::filesystem::remove(staged, ec); }
        discardDownload(payload); return 1;
    }
}
}
