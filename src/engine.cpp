#include "engine.hpp"
#include <algorithm>
#include <stdexcept>

namespace pulse {
static constexpr ULONG_PTR InputTag = 0x4D50554C;
Engine::Engine(InputSink sink) : sink_(std::move(sink)) {
    if (!sink_) sink_ = [](UINT n, INPUT* inputs) { return SendInput(n, inputs, sizeof(INPUT)); };
    LARGE_INTEGER f{}; QueryPerformanceFrequency(&f); frequency_ = f.QuadPart;
    stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    timer_ = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!timer_) timer_ = CreateWaitableTimerW(nullptr, FALSE, nullptr);
}
Engine::~Engine() { stop(); if (timer_) CloseHandle(timer_); if (stopEvent_) CloseHandle(stopEvent_); }
int64_t Engine::now() const { LARGE_INTEGER n{}; QueryPerformanceCounter(&n); return n.QuadPart; }
int64_t Engine::ticks(uint32_t ms) const { return static_cast<int64_t>(ms) * frequency_ / 1000; }
void Engine::stop() {
    if (stopEvent_) SetEvent(stopEvent_);
    if (worker_.joinable()) worker_.join();
    state_ = RunState::Idle;
}
bool Engine::prepare(uint32_t delayMs, std::wstring& error) {
    if (!stopEvent_ || !timer_ || !frequency_) { error = L"Le moteur de temporisation Windows n'est pas disponible."; return false; }
    if (delayMs > 60000) { error = L"Le délai de départ est limité à 60 000 ms."; return false; }
    stop(); ResetEvent(stopEvent_);
    actions_ = 0; cycles_ = 0; step_ = 0; failed_ = false; ended_ = 0; started_ = now();
    held_.fill(false); mouseHeld_.fill(false);
    layout_ = GetKeyboardLayout(GetWindowThreadProcessId(GetForegroundWindow(), nullptr));
    state_ = RunState::Countdown;
    return true;
}
bool Engine::waitUntil(int64_t deadline) {
    HANDLE handles[] = {stopEvent_, timer_};
    for (;;) {
        if (WaitForSingleObject(stopEvent_, 0) != WAIT_TIMEOUT) return false;
        auto remaining = deadline - now();
        if (remaining <= 0) return true;
        LARGE_INTEGER due{};
        // Relative 100 ns intervals. No busy loop and no global timer-resolution change.
        due.QuadPart = -std::max<int64_t>(1, remaining / frequency_ * 10000000 + (remaining % frequency_) * 10000000 / frequency_);
        if (!SetWaitableTimer(timer_, &due, 0, nullptr, nullptr, FALSE)) { failed_ = true; return false; }
        auto result = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        if (result != WAIT_OBJECT_0 + 1) { if (result != WAIT_OBJECT_0) failed_ = true; return false; }
    }
}
bool Engine::send(INPUT* inputs, UINT count) {
    const UINT accepted = sink_(count, inputs);
    for (UINT i = 0; i < std::min(count, accepted); ++i) {
        const auto& in = inputs[i];
        if (in.type == INPUT_KEYBOARD && in.ki.wVk < held_.size()) {
            const auto vk = in.ki.wVk;
            held_[vk] = !(in.ki.dwFlags & KEYEVENTF_KEYUP);
            releases_[vk] = in; releases_[vk].ki.dwFlags |= KEYEVENTF_KEYUP;
        } else if (in.type == INPUT_MOUSE) {
            DWORD downs[] = {MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_MIDDLEDOWN};
            DWORD ups[] = {MOUSEEVENTF_LEFTUP, MOUSEEVENTF_RIGHTUP, MOUSEEVENTF_MIDDLEUP};
            for (int b = 0; b < 3; ++b) {
                if (in.mi.dwFlags & downs[b]) mouseHeld_[b] = true;
                if (in.mi.dwFlags & ups[b]) mouseHeld_[b] = false;
            }
        }
    }
    if (accepted != count) { failed_ = true; return false; }
    return true;
}
bool Engine::move(int x, int y) {
    int left = GetSystemMetrics(SM_XVIRTUALSCREEN), top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int width = GetSystemMetrics(SM_CXVIRTUALSCREEN), height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    INPUT in{}; in.type = INPUT_MOUSE;
    in.mi.dx = static_cast<LONG>(static_cast<int64_t>(std::clamp(x - left, 0, width - 1)) * 65535 / std::max(1, width - 1));
    in.mi.dy = static_cast<LONG>(static_cast<int64_t>(std::clamp(y - top, 0, height - 1)) * 65535 / std::max(1, height - 1));
    in.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    in.mi.dwExtraInfo = InputTag;
    return send(&in, 1);
}
bool Engine::click(Button button) {
    DWORD downs[] = {MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_MIDDLEDOWN};
    DWORD ups[] = {MOUSEEVENTF_LEFTUP, MOUSEEVENTF_RIGHTUP, MOUSEEVENTF_MIDDLEUP};
    INPUT inputs[2]{};
    for (auto& in : inputs) { in.type = INPUT_MOUSE; in.mi.dwExtraInfo = InputTag; }
    inputs[0].mi.dwFlags = downs[static_cast<int>(button)]; inputs[1].mi.dwFlags = ups[static_cast<int>(button)];
    return send(inputs, 2);
}
bool Engine::key(uint16_t vk, bool up) {
    if (up) { if (!held_[vk]) return true; auto release = releases_[vk]; return send(&release, 1); }
    if (held_[vk]) return true;
    auto scan = MapVirtualKeyExW(vk, MAPVK_VK_TO_VSC_EX, layout_);
    INPUT in{}; in.type = INPUT_KEYBOARD; in.ki.wVk = vk; in.ki.wScan = static_cast<WORD>(scan & 0xff);
    in.ki.dwFlags = scan ? KEYEVENTF_SCANCODE : 0;
    if ((scan & 0xff00) == 0xe000) in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    in.ki.dwExtraInfo = InputTag;
    return send(&in, 1);
}
bool Engine::perform(const Step& s) {
    switch (s.action) {
    case Action::Click: return (!s.fixed || move(s.x, s.y)) && click(s.button);
    case Action::Move: return move(s.x, s.y);
    case Action::Key: {
        const uint16_t mods[] = {VK_CONTROL, VK_MENU, VK_SHIFT, VK_LWIN};
        std::array<bool, 4> added{};
        for (int i = 0; i < 4; ++i) if ((s.modifiers & (1 << i)) && !held_[mods[i]] && s.key != mods[i]) {
            added[i] = true; if (!key(mods[i], false)) return false;
        }
        if (!held_[s.key] && (!key(s.key, false) || !key(s.key, true))) return false;
        for (int i = 3; i >= 0; --i) if (added[i] && !key(mods[i], true)) return false;
        return true;
    }
    case Action::KeyDown: return key(s.key, false);
    case Action::KeyUp: return key(s.key, true);
    case Action::Wait: return true;
    case Action::Scroll: {
        INPUT in{}; in.type = INPUT_MOUSE; in.mi.dwFlags = MOUSEEVENTF_WHEEL;
        in.mi.mouseData = static_cast<DWORD>(s.wheel * WHEEL_DELTA); in.mi.dwExtraInfo = InputTag;
        return send(&in, 1);
    }
    }
    return false;
}
void Engine::releaseAll() {
    for (uint16_t vk = 0; vk < 256; ++vk) if (held_[vk]) key(vk, true);
    DWORD ups[] = {MOUSEEVENTF_LEFTUP, MOUSEEVENTF_RIGHTUP, MOUSEEVENTF_MIDDLEUP};
    for (int b = 0; b < 3; ++b) if (mouseHeld_[b]) {
        INPUT in{}; in.type = INPUT_MOUSE; in.mi.dwFlags = ups[b]; in.mi.dwExtraInfo = InputTag; send(&in, 1);
    }
}
void Engine::finish() { releaseAll(); CancelWaitableTimer(timer_); ended_ = now(); state_ = RunState::Idle; }
bool Engine::startClicker(ClickConfig config, uint32_t delay, std::wstring& error) {
    if (!config.intervalMs || config.intervalMs > 60000 || config.count > 1000000 ||
        config.button < Button::Left || config.button > Button::Middle ||
        config.x < -100000 || config.x > 100000 || config.y < -100000 || config.y > 100000) {
        error = L"Intervalle : 1 à 60 000 ms. Nombre de clics : 0 à 1 000 000. Coordonnées : -100 000 à 100 000."; return false;
    }
    if (!prepare(delay, error)) return false;
    try { worker_ = std::thread([this, config, delay] {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
        if (!waitUntil(now() + ticks(delay))) { finish(); return; }
        state_ = RunState::Clicking;
        auto deadline = now();
        while (!config.count || actions_ < config.count) {
            if (!waitUntil(deadline) || (config.fixed && !move(config.x, config.y)) || !click(config.button)) break;
            ++actions_; ++cycles_;
            deadline += ticks(config.intervalMs);
            // Skip missed slots instead of generating a catch-up burst.
            const auto time = now();
            if (deadline < time) deadline += ((time - deadline) / ticks(config.intervalMs) + 1) * ticks(config.intervalMs);
        }
        finish();
    }); } catch (const std::exception&) { state_ = RunState::Idle; error = L"Impossible de démarrer le moteur."; return false; }
    return true;
}
bool Engine::startMacro(Macro macro, uint32_t delay, std::wstring& error) {
    if (!validMacro(macro, error) || !prepare(delay, error)) return false;
    try { worker_ = std::thread([this, macro = std::move(macro), delay] {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
        if (!waitUntil(now() + ticks(delay))) { finish(); return; }
        state_ = RunState::Macro;
        for (uint64_t cycle = 0; !macro.repeats || cycle < macro.repeats; ++cycle) {
            for (size_t i = 0; i < macro.steps.size(); ++i) {
                step_ = static_cast<uint32_t>(i + 1);
                // Each delay is a minimum before the action; pauses are never caught up.
                if (!waitUntil(now() + ticks(macro.steps[i].delayMs)) || !perform(macro.steps[i])) { finish(); return; }
                ++actions_;
            }
            releaseAll();
            if (failed_) break;
            ++cycles_;
        }
        finish();
    }); } catch (const std::exception&) { state_ = RunState::Idle; error = L"Impossible de démarrer le moteur."; return false; }
    return true;
}
Snapshot Engine::snapshot() const {
    auto start = started_.load(), end = ended_.load();
    return {state_.load(), actions_.load(), cycles_.load(), step_.load(), start ? static_cast<double>((end ? end : now()) - start) / frequency_ : 0.0, failed_.load()};
}
}
