# MacroPulse

[![CI](https://github.com/santorr/standalone-macro-pulse/actions/workflows/ci.yml/badge.svg)](https://github.com/santorr/standalone-macro-pulse/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/santorr/standalone-macro-pulse)](https://github.com/santorr/standalone-macro-pulse/releases/latest)

A portable **Windows 10/11 x64** desktop app for automated clicking and manually composed macros. Built in **C++20 / Win32**, with an English interface, a dark purple theme and no installer or third-party runtime.

## Download and run

Download `MacroPulse-X.Y.Z-windows-x64.exe` from [GitHub Releases](https://github.com/santorr/standalone-macro-pulse/releases/latest), or extract the ZIP that also includes the documentation. Keep the executable in a writable folder. No administrator permissions are required.

Only one instance can use the library in a Windows session. Launching MacroPulse again brings the existing window forward. Close an older version before replacing it manually.

Release executables are not Authenticode-signed. Compare `Get-FileHash -Algorithm SHA256 <file>` with `SHA256SUMS.txt` to verify a manual download.

## Automatic upgrades

Starting with **1.5.0**, MacroPulse checks the repository's latest stable release once at startup, in the background. A newer compatible version shows an **Upgrade now / Later** dialog when the app is idle and in the foreground. You can also use **Preferences → Check for updates**. Choosing Later keeps your current version; the app checks again on the next launch.

After you choose **Upgrade now**:

1. The app downloads the official Windows x64 executable over HTTPS.
2. It checks its size, GitHub-provided SHA-256 digest, executable architecture and embedded version.
3. It saves your settings and macro library, prepares the replacement, and closes.
4. A temporary helper replaces the executable at the same path and restarts it. The previous executable is kept alongside it as `<filename>.previous`.

The helper checks write access before closing the app. A failed replacement preserves the previous executable; a failed process launch attempts a rollback. This is not an application health check after launch. If necessary, close MacroPulse and restore the `.previous` file manually. Settings and macros remain in `%LOCALAPPDATA%\MacroPulse` throughout the upgrade.

Offline startup and GitHub rate limits do not block the interface or display an error popup. Manual checks show errors so you can retry. No token is embedded and no settings, macro content or telemetry are uploaded. Update checks contact `api.github.com`; downloads use GitHub's HTTPS release asset hosts. Certificate checks remain enabled. SHA-256 verifies the payload against metadata served by GitHub; it is not a publisher code signature.

**Version 1.4.0 and earlier need one manual upgrade to 1.5.0 or later**, because those executables do not contain an updater. Automatic downgrades, drafts, prereleases and non-x64 assets are rejected.

## Auto-clicker

1. Choose the left, right or middle mouse button.
2. Set the interval (1–60,000 ms) and click count (0 means continuous).
3. Follow the cursor or choose a fixed position. **F9** captures screen coordinates, including negative coordinates on other monitors.
4. Move to the target and press **F6**. The default start delay is 1,500 ms.
5. **F8** stops all actions, even when MacroPulse is in the background. F6/F7 also stop an active sequence.

The 5 / 10 / 50 clicks-per-second presets change the interval. The display shows elapsed time and the number of completed actions.

## Macro library and editor

**My macros** contains the built-in library. Select a macro, rename it, and click **Edit macro** (or double-click it). **New macro** creates an empty sequence; **Duplicate** creates an independent copy; **Delete** asks for confirmation. **My macros** in the editor returns to the library.

Choose an action, fill in its parameters, and click **Add**. Select a step to change it and click **Apply**. Use Duplicate, Delete and the arrow buttons to manage the sequence. Unapplied edits to a step are not part of the saved sequence.

Supported actions:

- Mouse click at the current or a fixed position.
- Absolute cursor movement.
- Key or key combination, such as `A`, `Enter`, `Ctrl+C` or `Ctrl+Shift+S`.
- Key down / key up to hold a single key across steps.
- Pause.
- Vertical scrolling in positive or negative notches.

Click the key button and press the desired combination. Tab, Enter and Esc are valid macro keys. Click elsewhere to cancel capture. For key down / key up, press and release one key; individual modifiers such as Ctrl or Shift are supported.

The delay is a **minimum wait before the action**. For Pause, it is the entire pause duration. Repetitions can be finite or continuous (`0`). An infinite loop must contain at least 1 ms of delay. Held keys are released when stopped and at the end of each loop.

**F7** runs the selected macro. Keys assigned to global shortcuts are reserved in macros, even with different modifiers, to prevent a macro from triggering its own controls. Change the shortcut in Preferences to free that key.

## Preferences and storage

In **Preferences**, click a shortcut and press a key or combination, then click **Apply shortcuts**. Esc, clicking elsewhere or leaving the window cancels capture. **Restore defaults** prepares F6/F7/F8/F9; click Apply to confirm.

Letters, numbers, function keys, navigation keys and numpad keys work alone or with Ctrl/Alt/Shift. Punctuation follows the active keyboard layout. F12, Alt+F4, Windows key combinations and modifier-only shortcuts are reserved. Duplicate shortcuts are rejected. If a shortcut conflicts with another application, the previous shortcuts stay active. An unavailable stop shortcut disables execution until it is reconfigured.

Shortcuts are suspended while capturing a combination or typing in MacroPulse, then restored after the keys are released. Start shortcuts do not execute actions while Preferences is in the foreground.

Since **1.5.1**, start and position-capture shortcuts are also temporarily **unregistered** when an editable field is detected in another application. For example, `G` assigned to the auto-clicker can be typed into a supported text field without starting it. The global stop shortcut (F8 by default) stays registered, including during an active macro. Leaving the field restores the shortcuts only after held keys have been released. A shortcut claimed by another app during that pause is reported as unavailable rather than silently assumed to work.

Detection uses native Windows focus events and UI Automation on a background thread. It checks control type and editability, including password fields, **without reading field contents, names, values or typed characters**. Launch shortcuts are also paused briefly while a new focus is being classified. This does not pause an already running macro or clicker; use the stop shortcut to stop it.

This is best-effort detection, not a guarantee for every app: games, custom-rendered chats and controls that do not expose accessibility information may remain undetectable. Focus notification latency is also possible. For those applications, use a function key such as F6 instead of a bare letter. Keep the stop shortcut on a non-text key such as F8, since it intentionally remains active while typing.

Settings are saved to `%LOCALAPPDATA%\MacroPulse\settings.dat`. Macros are stored in the adjacent `library.dat`. Names, repetitions, sequence edits and selection are saved automatically after 600 ms, on navigation and on close. Empty drafts are retained. Your library and selected macro return at startup without running automatically. Existing macro names, including names in French or other languages, are preserved.

Atomic replacement and a `.bak` file keep the previous valid library generation. If both copies are unreadable, they are preserved instead of overwritten. A failed save keeps edits in memory and blocks normal closing until the save succeeds. Limits: 1,000 macros, 10,000 steps per macro, 200,000 total steps and names of 1–80 Unicode characters.

The first launch with an old `.mpulse` session imports the previous macro when available, leaving its original file intact. `.mpulse` is now a migration format rather than an interactive import/export workflow.

## Build and test

Requirements: Windows, **Visual Studio 2022 with Desktop development with C++**, Windows SDK, CMake and **PowerShell 7**. Windows' own WinHTTP, BCrypt and WinRT JSON APIs support the updater; no additional package manager is needed. The MSVC runtime is linked statically.

```powershell
pwsh -File .\scripts\build.ps1 -Configuration Debug
pwsh -File .\scripts\build.ps1 -Configuration Release
.\build\Release\MacroPulse.exe
```

The build script configures x64 and runs CTest. `-BuildDirectory build-custom` selects an isolated build directory. `-SkipTests` is available for local development; CI always runs tests.

Equivalent commands with CMake on PATH:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure --no-tests=error
```

The eight CTest suites cover the model, timing engine, shortcut conflicts, storage recovery, library migration, session persistence, real UI controls, keyboard capture, the updater and external typing protection. Focus tests exercise actual native/UI Automation controls, real shortcut unregister/register behavior, emergency-stop preservation, conflicts on resume and held-key deferral. Updater tests reject malformed metadata, wrong repositories, invalid versions, corrupted downloads and incompatible executables. An isolated test host exercises the actual helper handoff, file replacement, backup and restart; it does not replace a user's application. CI tests make no network requests or synthetic mouse/keyboard inputs.

```powershell
.\build\Release\MacroPulse.exe --render-preview artifacts
.\build\Release\pulse_tests.exe --benchmark
```

The preview mode renders the app's own windows and controls, not other desktop windows. The benchmark uses simulated output and does not measure another application's ability to receive input. Generated previews stay outside Git. The icon is documented in `assets/README.md`.

## CI/CD and releases

The repository contains source, tests, assets, scripts and documentation. `.gitignore` excludes builds, local data, generated previews and credentials. Executables are distributed as release assets rather than committed to Git.

- **CI:** pushes to `main`, pull requests and manual runs build and test **Debug and Release x64** on Windows Server 2022 / Visual Studio 2022. Release packages remain available as workflow artifacts for 14 days.
- **Release:** pushing `vX.Y.Z` runs the same complete pipeline. The tag must match `VERSION`. After both configurations pass, the workflow verifies checksums, uploads assets to a draft and publishes it.
- Official actions are pinned to commit SHAs. Dependabot proposes weekly updates. Only the publication job receives `contents: write`; no personal token or extra secret is needed.

`VERSION` is the single source of truth for the app, Windows metadata, manifest and filenames. Use **MAJOR.MINOR.PATCH**: incompatible change, compatible feature, or fix. Components must be between 0 and 65535, with no leading zeroes. Only stable versions are supported.

To publish a future fix, update `VERSION` and `CHANGELOG.md`, commit/push to `main`, wait for CI, then create the matching tag:

```powershell
git switch main
git pull --ff-only
# VERSION and CHANGELOG.md must already be committed for 1.5.2.
git tag -a v1.5.2 -m "MacroPulse 1.5.2"
git push origin v1.5.2
```

Assets are `MacroPulse-1.5.2-windows-x64.exe`, `MacroPulse-1.5.2-windows-x64.zip` and `SHA256SUMS.txt`. Keep this filename convention and the GitHub asset digest: the updater relies on them. GitHub supplies source archives automatically.

To create packages locally after a successful Release build:

```powershell
pwsh -File .\scripts\package.ps1
```

Do not move published tags or overwrite releases. Publish a new version for fixes. A failed workflow can resume an unpublished draft; an already published release is protected against overwriting.

## Architecture and limitations

- `engine.*`: dedicated worker thread, `SendInput`, `QueryPerformanceCounter` and a high-resolution waitable timer, with a standard timer fallback. Stop interrupts waits immediately; missed clicks are skipped rather than replayed in a burst.
- `model.*`, `preferences.*`, `library.*`: validation, storage and transactional shortcut configuration.
- `main.cpp` and the `.inl` UI files: native Win32 controls, DPI scaling, dark rendering and global shortcuts. Statistics refresh at 10 Hz during execution; there is no continuous repaint while idle.
- `text_focus.*`: event-driven focus tracking and background UI Automation queries; temporarily releases launch/capture shortcuts while keeping emergency stop registered.
- `updater.*`: bounded HTTPS requests, numeric version comparison, SHA-256 verification and an isolated replacement helper. `updater_ui.inl` handles consent and background tasks.

There is no global keyboard/mouse hook, telemetry, driver or global timer-resolution change. The updater is the only network feature.

Windows is not a real-time operating system: a 1 ms interval does not guarantee 1,000 received clicks per second. Holding a key does not generate hardware keyboard repeat. Modifier keys physically held by the user can affect injected input.

`SendInput` respects Windows privilege levels. A normal application cannot inject into a target running as administrator. MacroPulse reports rejected input; even a successful injection does not guarantee acceptance by the target application.

References: [SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput), [waitable timers](https://learn.microsoft.com/en-us/windows/win32/sync/waitable-timer-objects), [GitHub release assets and digests](https://docs.github.com/en/rest/releases/assets).
