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
    int modifiers = 0;
    if (event.key().states().test(fcitx::KeyState::Shift)) modifiers |= 1;
    if (event.key().states().test(fcitx::KeyState::Ctrl)) modifiers |= 2;
    if (event.key().states().test(fcitx::KeyState::Alt)) modifiers |= 4;
    const auto result =
        engine_.processKey(event.key().sym(), modifiers, event.isRelease());
    if (!result.commit.empty() && ic_) ic_->commitString(result.commit);
    current_ = result.preedit;
    cursor_ = result.cursor;
    updatePreedit();
    if (result.consumed) event.filterAndAccept();
}

void VipyState::reset() {
    engine_.resetState();
    current_.clear();
    cursor_ = 0;
    updatePreedit();
}

void VipyState::commitAndReset(const std::string &suffix) {
    if (ic_ && !current_.empty()) {
        const std::string committed = engine_.commitText();
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
    text.setCursor(cursor_);
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

} // namespace vipy::fcitx_wrapper
