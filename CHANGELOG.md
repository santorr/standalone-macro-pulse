# Changelog

## 1.5.1

- Release start and position-capture shortcuts while typing in supported external text fields. A bare letter such as G is no longer reserved by MacroPulse in those fields.
- Detect native edit controls, password fields and accessible editors through Windows focus events and background UI Automation, without reading text contents.
- Keep emergency stop registered, defer reactivation until held keys are released, and report conflicts acquired by other apps during the pause.
- Add tests with real Windows controls and shortcut registration, plus held-key regression coverage in the UI smoke test.
- Document the limits for games and custom controls without accessibility support.

## 1.5.0

- Translate all application controls, notifications, validation errors and Windows metadata into English. Preserve existing macro names and data.
- Check GitHub for a newer stable release in the background at startup; add Preferences → Check for updates.
- Offer Upgrade now / Later before downloading. Verify HTTPS origin, size, SHA-256, architecture and embedded version.
- Save the session, replace the executable with a backup and restart automatically. Preserve the current version on replacement failure and attempt rollback on launch failure.
- Add offline updater tests, including real replacement/restart of an isolated fixture.

## 1.4.0

- Integrated macro library, editing, duplication and automatic saving.
- Session restoration and migration of legacy `.mpulse` files.
- Configurable shortcuts, preferences and a native Windows interface.
- Portable Windows 10/11 x64 distribution with a static MSVC runtime.
- GitHub Debug/Release CI, versioned release packages and SHA-256 checksums.
