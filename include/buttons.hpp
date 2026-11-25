#pragma once

namespace Buttons {

enum class Event { None, PrevActuated, NextActuated };

void init();
Event poll_action();

}  // namespace Buttons
