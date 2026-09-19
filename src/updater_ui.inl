// UI-thread orchestration. Network and hashing run in a background task.
void App::checkUpdates(bool manual) {
    if (smoke || updateTask.valid() || updateDialog) return;
    manualUpdate = manual;
    availableUpdate.reset();
    notice = manual ? L"Checking for updates…" : notice;
    updateTask = std::async(std::launch::async, [stop = updateStop.get_token()] {
        UpdateResult result;
        try { result.release = updates::checkLatest(stop); }
        catch (const std::exception& ex) { result.error.assign(ex.what(), ex.what() + strlen(ex.what())); }
        catch (...) { result.error = L"Could not read GitHub release information. Please try again later."; }
        return result;
    });
    editorState(); InvalidateRect(hwnd, nullptr, FALSE);
}
void App::pollUpdates() {
    if (smoke || updateDialog) return;
    if (updateTask.valid() && updateTask.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        auto result = updateTask.get();
        if (downloadingUpdate) {
            downloadingUpdate = false;
            const auto release = *availableUpdate;
            availableUpdate.reset();
            if (!result.error.empty()) { notice = result.error; error(notice); }
            else {
                // Save again: the user may have edited macros during the download.
                updateDialog = true;
                if (persistLibrary() && persistPreferences()) {
                    std::wstring problem;
                    if (updates::launchInstaller(result.payload, release, problem)) {
                        DestroyWindow(hwnd); return;
                    }
                    notice = problem;
                }
                updates::discardDownload(result.payload);
                updateDialog = false; error(notice);
            }
            availableUpdate.reset();
        } else if (!result.error.empty()) {
            if (manualUpdate) { notice = result.error; error(notice); }
        } else if (result.release) availableUpdate = std::move(result.release);
        else if (manualUpdate) {
            notice = L"MacroPulse " + std::wstring(AppVersion) + L" is up to date.";
            pulse::messageBox(hwnd, notice.c_str(), L"MacroPulse updates", MB_OK | MB_ICONINFORMATION);
        }
        editorState(); InvalidateRect(hwnd, nullptr, FALSE);
    }
    // Never interrupt active automation, key capture, typing or another window.
    if (!availableUpdate || updateTask.valid() || downloadingUpdate || keyCaptureTarget || textInput ||
        engine.snapshot().state != RunState::Idle || GetForegroundWindow() != hwnd) return;
    updateDialog = true;
    const auto release = *availableUpdate;
    std::wstring title = L"MacroPulse " + release.version + L" is available";
    std::wstring content = L"Installed version: " + std::wstring(AppVersion) + L"\n\nDownload and install the upgrade now? MacroPulse will save your settings and macros, then restart automatically.";
    TASKDIALOG_BUTTON buttons[]{{100, L"Upgrade now"}, {101, L"Later"}};
    TASKDIALOGCONFIG dialog{}; dialog.cbSize = sizeof(dialog); dialog.hwndParent = hwnd;
    dialog.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
    dialog.pszWindowTitle = L"MacroPulse upgrade"; dialog.pszMainInstruction = title.c_str();
    dialog.pszContent = content.c_str(); dialog.pszMainIcon = TD_INFORMATION_ICON;
    dialog.cButtons = 2; dialog.pButtons = buttons; dialog.nDefaultButton = 101;
    int choice = 101;
    auto status = TaskDialogIndirect(&dialog, &choice, nullptr, nullptr);
    updateDialog = false;
    if (FAILED(status) || choice != 100) { availableUpdate.reset(); return; }
    downloadingUpdate = true; notice = L"Downloading and verifying the upgrade…";
    updateTask = std::async(std::launch::async, [release, stop = updateStop.get_token()] {
        UpdateResult result;
        try { result.payload = updates::download(release, stop); }
        catch (const std::exception& ex) { result.error.assign(ex.what(), ex.what() + strlen(ex.what())); }
        catch (...) { result.error = L"Could not download the upgrade. Your current version has not been changed."; }
        return result;
    });
    editorState(); InvalidateRect(hwnd, nullptr, FALSE);
}
