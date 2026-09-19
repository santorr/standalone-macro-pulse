#include "text_focus.hpp"
#include <wrl.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <string>
#include <algorithm>
#include <cwctype>

namespace pulse {
using Microsoft::WRL::ComPtr;
TextFocus nativeTextFocus(HWND control) {
    if (!control || !IsWindow(control)) return TextFocus::Unknown;
    if (!IsWindowEnabled(control)) return TextFocus::Other;
    wchar_t name[256]{}; GetClassNameW(control, name, 256);
    std::wstring klass(name); std::transform(klass.begin(), klass.end(), klass.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
    if (klass == L"edit" || klass.starts_with(L"richedit"))
        return (GetWindowLongPtrW(control, GWL_STYLE) & ES_READONLY) ? TextFocus::Other : TextFocus::Editing;
    // Classic terminals accept typed commands without an Edit child.
    if (klass == L"consolewindowclass") return TextFocus::Editing;
    return TextFocus::Unknown;
}
TextFocus automationTextFocus(IUIAutomationElement* element) {
    if (!element) return TextFocus::Unknown;
    BOOL enabled = TRUE, password = FALSE; CONTROLTYPEID type = 0;
    if (FAILED(element->get_CurrentControlType(&type))) return TextFocus::Unknown;
    if (SUCCEEDED(element->get_CurrentIsEnabled(&enabled)) && !enabled) return TextFocus::Other;
    if (SUCCEEDED(element->get_CurrentIsPassword(&password)) && password) return TextFocus::Editing;
    if (type != UIA_EditControlTypeId && type != UIA_DocumentControlTypeId && type != UIA_CustomControlTypeId &&
        type != UIA_PaneControlTypeId && type != UIA_ComboBoxControlTypeId) return TextFocus::Other;
    ComPtr<IUIAutomationValuePattern> value;
    if (SUCCEEDED(element->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&value))) && value) {
        BOOL readOnly = TRUE;
        if (SUCCEEDED(value->get_CurrentIsReadOnly(&readOnly))) return readOnly ? TextFocus::Other : TextFocus::Editing;
    }
    ComPtr<IUIAutomationTextPattern> text;
    if (SUCCEEDED(element->GetCurrentPatternAs(UIA_TextPatternId, IID_PPV_ARGS(&text))) && text) {
        ComPtr<IUIAutomationTextRange> range;
        if (SUCCEEDED(text->get_DocumentRange(&range)) && range) {
            VARIANT readOnly; VariantInit(&readOnly);
            auto hr = range->GetAttributeValue(UIA_IsReadOnlyAttributeId, &readOnly);
            auto result = SUCCEEDED(hr) && readOnly.vt == VT_BOOL
                ? (readOnly.boolVal ? TextFocus::Other : TextFocus::Editing) : TextFocus::Unknown;
            VariantClear(&readOnly); if (result != TextFocus::Unknown) return result;
        }
    }
    VARIANT editable; VariantInit(&editable);
    auto hr = element->GetCurrentPropertyValue(UIA_IsTextEditPatternAvailablePropertyId, &editable);
    bool hasTextEdit = SUCCEEDED(hr) && editable.vt == VT_BOOL && editable.boolVal;
    VariantClear(&editable);
    if (hasTextEdit || type == UIA_EditControlTypeId) return TextFocus::Editing;
    return TextFocus::Unknown;
}
namespace {
struct FocusState {
    std::atomic<HWND> owner{nullptr};
    HANDLE changed = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    HANDLE stopping = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    std::mutex mutex;
    uint64_t generation = 0;
    HWND foreground = nullptr;
    TextFocus result = TextFocus::Pending;
    ~FocusState() { if (changed) CloseHandle(changed); if (stopping) CloseHandle(stopping); }
    void request() {
        auto window = owner.load(); if (!window) return;
        {
            std::lock_guard lock(mutex); ++generation;
            foreground = GetForegroundWindow(); result = TextFocus::Pending;
        }
        SetEvent(changed); PostMessageW(window, TextFocusMonitor::ChangedMessage, 0, 0);
    }
};
thread_local FocusState* nativeState = nullptr;
void CALLBACK focusEvent(HWINEVENTHOOK, DWORD event, HWND source, LONG, LONG, DWORD, DWORD) {
    if (event != EVENT_SYSTEM_FOREGROUND && source && GetAncestor(source, GA_ROOT) != GetForegroundWindow()) return;
    if (nativeState) {
        nativeState->request();
        // Out-of-context callbacks run on the hook owner's UI thread. Release
        // bindings now, rather than waiting for another posted message.
        if (auto owner = nativeState->owner.load()) SendMessageW(owner, TextFocusMonitor::ChangedMessage, 0, 0);
    }
}
class FocusHandler final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IUIAutomationFocusChangedEventHandler> {
public:
    explicit FocusHandler(std::shared_ptr<FocusState> state) : state_(std::move(state)) {}
    HRESULT STDMETHODCALLTYPE HandleFocusChangedEvent(IUIAutomationElement*) override {
        // Never call a remote provider from an event callback.
        state_->request(); return S_OK;
    }
private:
    std::shared_ptr<FocusState> state_;
};
TextFocus inspect(HWND foreground, HWND owner, IUIAutomation* automation) {
    if (!foreground || foreground == owner) return TextFocus::Other;
    DWORD process = 0; DWORD thread = GetWindowThreadProcessId(foreground, &process);
    if (process == GetCurrentProcessId()) return TextFocus::Other;
    GUITHREADINFO info{sizeof(info)};
    if (GetGUIThreadInfo(thread, &info)) {
        auto native = nativeTextFocus(info.hwndFocus);
        if (native != TextFocus::Unknown) return native;
    }
    if (automation) {
        ComPtr<IUIAutomationElement> element;
        if (SUCCEEDED(automation->GetFocusedElement(&element)) && element) return automationTextFocus(element.Get());
    }
    return TextFocus::Unknown;
}
}
struct TextFocusMonitor::Impl {
    std::shared_ptr<FocusState> state = std::make_shared<FocusState>();
    HWINEVENTHOOK focus = nullptr, foreground = nullptr;
    std::thread worker;
    explicit Impl(HWND owner) {
        state->owner = owner;
        if (!state->changed || !state->stopping) return;
        worker = std::thread([shared = state] {
            const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            ComPtr<IUIAutomation> automation;
            ComPtr<FocusHandler> handler;
            if (SUCCEEDED(initialized)) {
                CoCreateInstance(CLSID_CUIAutomation8, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&automation));
                if (!automation) CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&automation));
                if (automation) {
                    ComPtr<IUIAutomation2> options;
                    if (SUCCEEDED(automation.As(&options))) { options->put_ConnectionTimeout(500); options->put_TransactionTimeout(500); }
                    handler = Microsoft::WRL::Make<FocusHandler>(shared);
                    automation->AddFocusChangedEventHandler(nullptr, handler.Get());
                }
            }
            HANDLE events[]{shared->stopping, shared->changed};
            while (WaitForMultipleObjects(2, events, FALSE, INFINITE) == WAIT_OBJECT_0 + 1) {
                uint64_t generation; HWND foreground;
                { std::lock_guard lock(shared->mutex); generation = shared->generation; foreground = shared->foreground; }
                auto result = inspect(foreground, shared->owner.load(), automation.Get());
                {
                    std::lock_guard lock(shared->mutex);
                    if (generation != shared->generation || foreground != GetForegroundWindow()) continue;
                    shared->result = result;
                }
                if (auto owner = shared->owner.load()) PostMessageW(owner, ChangedMessage, 0, 0);
            }
            if (automation && handler) automation->RemoveFocusChangedEventHandler(handler.Get());
            handler.Reset(); automation.Reset(); if (SUCCEEDED(initialized)) CoUninitialize();
        });
        nativeState = state.get();
        focus = SetWinEventHook(EVENT_OBJECT_FOCUS, EVENT_OBJECT_FOCUS, nullptr, focusEvent, 0, 0, WINEVENT_OUTOFCONTEXT);
        foreground = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, focusEvent, 0, 0, WINEVENT_OUTOFCONTEXT);
        state->request();
    }
    ~Impl() {
        state->owner = nullptr; nativeState = nullptr;
        if (focus) UnhookWinEvent(focus); if (foreground) UnhookWinEvent(foreground);
        SetEvent(state->stopping); if (worker.joinable()) worker.join();
    }
};
TextFocusMonitor::TextFocusMonitor(HWND owner) : impl_(std::make_unique<Impl>(owner)) {}
TextFocusMonitor::~TextFocusMonitor() = default;
bool TextFocusMonitor::pauseLaunchShortcuts() const {
    auto foreground = GetForegroundWindow(); DWORD process = 0;
    DWORD thread = foreground ? GetWindowThreadProcessId(foreground, &process) : 0;
    if (!foreground || process == GetCurrentProcessId()) return false;
    GUITHREADINFO info{sizeof(info)};
    if (thread && GetGUIThreadInfo(thread, &info)) {
        auto native = nativeTextFocus(info.hwndFocus);
        if (native != TextFocus::Unknown) return native == TextFocus::Editing;
    }
    if (!impl_->worker.joinable()) return false;
    std::lock_guard lock(impl_->state->mutex);
    // Release immediately while a new focus is being classified. Ignore stale
    // results, and never issue a UI Automation call on the UI thread.
    if (impl_->state->foreground != foreground) { impl_->state->foreground = foreground; ++impl_->state->generation; impl_->state->result = TextFocus::Pending; SetEvent(impl_->state->changed); return true; }
    return impl_->state->result == TextFocus::Editing || impl_->state->result == TextFocus::Pending;
}
}
