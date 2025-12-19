#pragma once

#include "cemu_hooks.h"

class RumbleManager {
public:
    RumbleManager(XrSession session, XrAction haptic_action, XrPath subaction_path = XR_NULL_PATH) : m_session(session), m_haptic_action(haptic_action), m_subaction_path(subaction_path) {
        m_update_thread = std::thread(&RumbleManager::update_thread, this);
    }

    ~RumbleManager() {
        m_shutdown.store(true);
        if (m_update_thread.joinable()) {
            m_update_thread.join();
        }
    }

    // pattern: uint8_t* rumble pattern
    // length: length in bits
    void controlMotor(uint8_t* pattern, uint8_t length) {
        length = std::min<uint8_t>(length, 120);

        if (length == 0) {
            stopMotor();
            return;
        }

        push_rumble(pattern, length);
    }

    void stopMotor() {
        std::scoped_lock lock(m_rumble_mutex);
        while (!m_rumble_queue.empty()) {
            m_rumble_queue.pop();
        }
        m_parser = 0;
        m_current_rumbling = false;
        stop_haptic();
    }

    void startSimpleRumble(double duration, float frequency, float amplitude) {
        // duration is in seconds
        XrHapticVibration vibration = {};
        vibration.type = XR_TYPE_HAPTIC_VIBRATION;
        vibration.next = nullptr;
        vibration.duration = (XrDuration)(duration * 1e9);;
        vibration.frequency = frequency;
        vibration.amplitude = amplitude;

        XrHapticActionInfo haptic_info = {};
        haptic_info.type = XR_TYPE_HAPTIC_ACTION_INFO;
        haptic_info.next = nullptr;
        haptic_info.action = m_haptic_action;
        haptic_info.subactionPath = m_subaction_path;

        checkXRResult(xrApplyHapticFeedback(m_session, &haptic_info, (const XrHapticBaseHeader*)&vibration), "Failed to start rumble");
    }

private:
    bool push_rumble(uint8_t* pattern, uint8_t length) {
        if (pattern == nullptr || length == 0) {
            stopMotor();
            return true;
        }

        std::scoped_lock lock(m_rumble_mutex);
        if (m_rumble_queue.size() >= 5) {
            return false;
        }

        std::vector<bool> bitset;
        int len = length;
        int byte_idx = 0;
        while (len > 0) {
            uint8_t p = pattern[byte_idx];
            for (int j = 0; j < 8 && j < len; j += 2) {
                bool set = (p & (3 << j)) != 0;
                bitset.push_back(set);
            }
            ++byte_idx;
            len -= 8;
        }

        m_rumble_queue.emplace(std::move(bitset));
        return true;
    }

    void update_thread() {
        using clock = std::chrono::steady_clock;
        const auto period = std::chrono::milliseconds(1000 / 60);

        auto next_tick = clock::now();
        while (!m_shutdown.load(std::memory_order_relaxed)) {
            {
                std::unique_lock lock(m_rumble_mutex);
                if (m_rumble_queue.empty()) {
                    if (m_current_rumbling) {
                        stop_haptic();
                        m_current_rumbling = false;
                    }
                    m_parser = 0;
                }
                else {
                    const auto& current_pattern = m_rumble_queue.front();
                    bool should_rumble = current_pattern[m_parser];
                    if (should_rumble != m_current_rumbling) {
                        if (should_rumble) {
                            apply_haptic_infinite();
                        }
                        else {
                            stop_haptic();
                        }
                        m_current_rumbling = should_rumble;
                    }
                    ++m_parser;
                    if (m_parser >= current_pattern.size()) {
                        m_rumble_queue.pop();
                        m_parser = 0;
                    }
                }
            }
            next_tick += period;
            std::this_thread::sleep_until(next_tick);
        }
    }

void apply_haptic_infinite() {
        XrHapticVibration vibration = {};
        vibration.type = XR_TYPE_HAPTIC_VIBRATION;
        vibration.next = nullptr;
        vibration.duration = XR_INFINITE_DURATION;
        vibration.frequency = XR_FREQUENCY_UNSPECIFIED;
        vibration.amplitude = 1.0f;

        XrHapticActionInfo haptic_info = {};
        haptic_info.type = XR_TYPE_HAPTIC_ACTION_INFO;
        haptic_info.next = nullptr;
        haptic_info.action = m_haptic_action;
        haptic_info.subactionPath = m_subaction_path;

        checkXRResult(xrApplyHapticFeedback(m_session, &haptic_info, (const XrHapticBaseHeader*)&vibration), "Failed to start rumble");

        m_haptic_start_time = std::chrono::steady_clock::now();
        m_haptic_active = true;
    }

    void stop_haptic() {
        XrHapticActionInfo haptic_info = {};
        haptic_info.type = XR_TYPE_HAPTIC_ACTION_INFO;
        haptic_info.next = nullptr;
        haptic_info.action = m_haptic_action;
        haptic_info.subactionPath = m_subaction_path;

        checkXRResult(xrStopHapticFeedback(m_session, &haptic_info), "Failed to stop rumble");

        if (m_haptic_active) {
            auto end_time = std::chrono::steady_clock::now();
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - m_haptic_start_time).count();
            //Log::print("Stopping controller (haptic duration: {} ms)", duration_ms);
            m_haptic_active = false;
        }
    }

    XrSession m_session;
    XrAction m_haptic_action;
    XrPath m_subaction_path;

    std::queue<std::vector<bool>> m_rumble_queue;
    std::mutex m_rumble_mutex;
    size_t m_parser = 0;
    bool m_current_rumbling = false;
    std::atomic<bool> m_shutdown{ false };
    std::thread m_update_thread;

    std::chrono::steady_clock::time_point m_haptic_start_time{};
    bool m_haptic_active = false;
};