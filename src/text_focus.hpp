#pragma once
#include <windows.h>
#include <objbase.h>
#include <UIAutomation.h>
#include <memory>

namespace pulse {
enum class TextFocus { Other, Editing, Unknown, Pending };
TextFocus nativeTextFocus(HWND control);
TextFocus automationTextFocus(IUIAutomationElement* element);

// Native focus events plus UI Automation focus events. No keyboard hook and no
// text/name/value reads. All cross-process accessibility queries run on an MTA.
class TextFocusMonitor {
public:
    static constexpr UINT ChangedMessage = WM_APP + 20;
    explicit TextFocusMonitor(HWND owner);
    ~TextFocusMonitor();
    TextFocusMonitor(const TextFocusMonitor&) = delete;
    TextFocusMonitor& operator=(const TextFocusMonitor&) = delete;
    bool pauseLaunchShortcuts() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
