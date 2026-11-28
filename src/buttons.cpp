#include "buttons.hpp"

#include <Arduino.h>

#include <cstdint>

namespace {

constexpr std::uint8_t PIN_PREV = 0;
constexpr std::uint8_t PIN_NEXT = 14;
constexpr std::uint32_t DEBOUNCE_MS = 40;
constexpr std::uint32_t LONG_PRESS_MS = 600;

enum class Gesture { None, Tap, LongPress };

struct Button {
    std::uint8_t pin;
    bool is_actuated;
    bool last_poll_actuation;
    std::uint32_t last_change_ms;
    std::uint32_t actuated_at_ms = 0;
    bool long_press_actuated = false;
};

Button prev{PIN_PREV, false, false, 0};
Button next{PIN_NEXT, false, false, 0};

[[nodiscard]] Gesture poll_gesture(Button &btn) {
    bool poll_actuated = digitalRead(btn.pin) == LOW;
    std::uint32_t now_ms = millis();

    if (poll_actuated != btn.last_poll_actuation) {
        btn.last_poll_actuation = poll_actuated;
        btn.last_change_ms = now_ms;
    }

    bool settled = now_ms - btn.last_change_ms >= DEBOUNCE_MS;
    if (settled && poll_actuated != btn.is_actuated) {
        btn.is_actuated = poll_actuated;

        if (poll_actuated) {
            btn.actuated_at_ms = now_ms;
            btn.long_press_actuated = false;
            return Gesture::None;
        }

        return btn.long_press_actuated ? Gesture::None : Gesture::Tap;
    }

    if (btn.is_actuated && !btn.long_press_actuated &&
        now_ms - btn.actuated_at_ms >= LONG_PRESS_MS) {
        btn.long_press_actuated = true;
        return Gesture::LongPress;
    }

    return Gesture::None;
}

void sync_btn_state(Button &btn) {
    btn.is_actuated = digitalRead(btn.pin) == LOW;
    btn.last_poll_actuation = btn.is_actuated;
    btn.long_press_actuated = btn.is_actuated;
}

}  // namespace

namespace Buttons {

void init() {
    pinMode(PIN_PREV, INPUT_PULLUP);
    pinMode(PIN_NEXT, INPUT_PULLUP);

    sync_btn_state(prev);
    sync_btn_state(next);
}

Action poll_action() {
    switch (poll_gesture(prev)) {
    case Gesture::Tap:
        return Action::PrevTapped;

    case Gesture::LongPress:
        return Action::PlayPauseRequested;

    case Gesture::None:
        break;
    }

    switch (poll_gesture(next)) {
    case Gesture::Tap:
        return Action::NextTapped;

    case Gesture::LongPress:
        return Action::PlayPauseRequested;

    case Gesture::None:
        break;
    }

    return Action::None;
}

}  // namespace Buttons
