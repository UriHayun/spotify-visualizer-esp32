#include "buttons.hpp"

#include <Arduino.h>

#include <cstdint>

namespace {

constexpr std::uint8_t PIN_PREV = 0;
constexpr std::uint8_t PIN_NEXT = 14;
constexpr std::uint32_t DEBOUNCE_MS = 40;

struct Button {
    std::uint8_t pin;
    bool is_actuated;
    bool last_poll_actuation;
    std::uint32_t last_change_ms;
};

Button prev{PIN_PREV, false, false, 0};
Button next{PIN_NEXT, false, false, 0};

bool was_released(Button &btn) {
    bool poll_actuated = digitalRead(btn.pin) == LOW;
    std::uint32_t now_ms = millis();

    if (poll_actuated != btn.last_poll_actuation) {
        btn.last_poll_actuation = poll_actuated;
        btn.last_change_ms = now_ms;
    }

    bool settled = now_ms - btn.last_change_ms >= DEBOUNCE_MS;
    if (settled && poll_actuated != btn.is_actuated) {
        btn.is_actuated = poll_actuated;
        return !poll_actuated;
    }

    return false;
}

void sync_to_pin(Button &btn) {
    btn.is_actuated = digitalRead(btn.pin) == LOW;
    btn.last_poll_actuation = btn.is_actuated;
}

}  // namespace

namespace Buttons {

void init() {
    pinMode(PIN_PREV, INPUT_PULLUP);
    pinMode(PIN_NEXT, INPUT_PULLUP);

    sync_to_pin(prev);
    sync_to_pin(next);
}

Event poll_action() {
    if (was_released(prev)) {
        return Event::PrevActuated;
    }
    if (was_released(next)) {
        return Event::NextActuated;
    }
    return Event::None;
}

}  // namespace Buttons
