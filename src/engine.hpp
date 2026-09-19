#pragma once
#include "model.hpp"
#include <array>
#include <atomic>
#include <functional>
#include <thread>

namespace pulse {
struct ClickConfig {
    uint32_t intervalMs = 100;
    uint32_t count = 0;
    Button button = Button::Left;
    bool fixed = false;
    int x = 0, y = 0;
};
enum class RunState { Idle, Countdown, Clicking, Macro };
struct Snapshot {
    RunState state;
    uint64_t actions, cycles;
    uint32_t step;
    double elapsedSeconds;
    bool inputFailed;
};
class Engine {
public:
    using InputSink = std::function<UINT(UINT, INPUT*)>;
    explicit Engine(InputSink sink = {});
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    bool startClicker(ClickConfig config, uint32_t startDelayMs, std::wstring& error);
    bool startMacro(Macro macro, uint32_t startDelayMs, std::wstring& error);
    void stop();
    Snapshot snapshot() const;
private:
    InputSink sink_;
    HANDLE stopEvent_ = nullptr, timer_ = nullptr;
    std::thread worker_;
    std::atomic<RunState> state_{RunState::Idle};
    std::atomic<uint64_t> actions_{0}, cycles_{0};
    std::atomic<uint32_t> step_{0};
    std::atomic<int64_t> started_{0}, ended_{0};
    std::atomic<bool> failed_{false};
    int64_t frequency_ = 0;
    std::array<bool, 256> held_{};
    std::array<INPUT, 256> releases_{};
    std::array<bool, 3> mouseHeld_{};
    HKL layout_ = nullptr;
    int64_t now() const;
    int64_t ticks(uint32_t ms) const;
    bool prepare(uint32_t delayMs, std::wstring& error);
    bool waitUntil(int64_t deadline);
    bool send(INPUT* input, UINT count);
    bool move(int x, int y);
    bool click(Button button);
    bool key(uint16_t vk, bool up);
    bool perform(const Step& step);
    void releaseAll();
    void finish();
};
}
