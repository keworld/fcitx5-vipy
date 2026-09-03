#include "mode_action.hpp"

#include <utility>

namespace vipy::fcitx_wrapper {

ModeAction::ModeAction(
    std::string text, std::function<bool()> checked,
    std::function<void(fcitx::InputContext *)> activate)
    : text_(std::move(text)), checked_(std::move(checked)),
      activate_(std::move(activate)) {
    setCheckable(true);
}

std::string ModeAction::shortText(fcitx::InputContext *) const {
    return text_;
}

std::string ModeAction::icon(fcitx::InputContext *) const {
    return "fcitx-vipy";
}

bool ModeAction::isChecked(fcitx::InputContext *) const {
    return checked_();
}

void ModeAction::activate(fcitx::InputContext *ic) {
    activate_(ic);
}

} // namespace vipy::fcitx_wrapper
