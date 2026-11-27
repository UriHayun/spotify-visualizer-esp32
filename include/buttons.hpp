#pragma once

namespace Buttons {

enum class Action { None, PrevTapped, NextTapped, PlayPauseRequested };

void init();
[[nodiscard]] Action poll_action();

}  // namespace Buttons
