#include "vipy_state.hpp"

#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/userinterfacemanager.h>

namespace vipy::fcitx_wrapper {

VipyState::VipyState(python::PythonEngine *engine, VipyConfig *config,
                     fcitx::InputContext *ic)
    : engine_(*engine), config_(*config), ic_(ic) {}

void VipyState::keyEvent(fcitx::KeyEvent &event) {
    if (event.isRelease()) {
        return;
    }
    const auto sym = event.key().sym();
    if (event.key().states().test(fcitx::KeyState::Ctrl) ||
        event.key().states().test(fcitx::KeyState::Alt) ||
        event.key().states().test(fcitx::KeyState::Super)) {
        commitAndReset();
        return;
    }
    if (sym == FcitxKey_BackSpace) {
        backspace(event);
        return;
    }
    if (sym == FcitxKey_space) {
        if (!current_.empty()) {
            commitAndReset(" ");
            event.filterAndAccept();
        }
        return;
    }

    const bool letter = (sym >= 'a' && sym <= 'z') ||
                        (sym >= 'A' && sym <= 'Z');
    const bool digit = *config_.inputMethod == InputMethod::Vni &&
                       sym >= '1' && sym <= '9';
    if (!letter && !digit) {
        commitAndReset();
        return;
    }
    if (current_.size() >= 32) {
        commitAndReset();
    }

    const char key = static_cast<char>(sym);
    current_ = engine_.feedKey(std::string(1, key));
    if (current_.empty()) {
        commitAndReset();
        return;
    }
    raw_.push_back(key);
    updatePreedit();
    event.filterAndAccept();
}

void VipyState::reset() {
    engine_.resetState();
    current_.clear();
    raw_.clear();
    updatePreedit();
}

void VipyState::commitAndReset(const std::string &suffix) {
    if (ic_ && !current_.empty()) {
        const std::string committed = engine_.commitCurrent();
        if (!committed.empty()) {
            ic_->commitString(committed + suffix);
        }
    }
    reset();
}

void VipyState::updatePreedit() {
    if (!ic_) {
        return;
    }
    auto &inputPanel = ic_->inputPanel();
    inputPanel.reset();
    if (current_.empty()) {
        ic_->updatePreedit();
        ic_->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
        return;
    }
    const bool useClientPreedit =
        ic_->capabilityFlags().test(fcitx::CapabilityFlag::Preedit);
    fcitx::TextFormatFlags formatFlags{fcitx::TextFormatFlag::Underline};
    fcitx::Text text(current_, formatFlags);
    text.setCursor(static_cast<int>(current_.size()));
    if (useClientPreedit) {
        inputPanel.setPreedit(fcitx::Text{});
        inputPanel.setClientPreedit(text);
    } else {
        inputPanel.setClientPreedit(fcitx::Text{});
        inputPanel.setPreedit(text);
    }
    ic_->updatePreedit();
    ic_->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
}

void VipyState::backspace(fcitx::KeyEvent &event) {
    if (raw_.empty()) {
        return;
    }
    raw_.pop_back();
    current_ = engine_.feedKey("\b");
    updatePreedit();
    event.filterAndAccept();
}

} // namespace vipy::fcitx_wrapper
