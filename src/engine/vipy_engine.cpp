#include "vipy_engine.hpp"

#include <fcitx/addonmanager.h>
#include <fcitx/inputcontext.h>
#include <fcitx/statusarea.h>
#include <fcitx/userinterfacemanager.h>
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/log.h>

namespace vipy::fcitx_wrapper {

VietnameseInputMethodEngine::VietnameseInputMethodEngine(fcitx::Instance *instance)
    : instance_(instance), runtime_(), engine_(runtime_),
      stateFactory_([this](fcitx::InputContext &ic) {
          return new VipyState(&engine_, &config_, &ic);
      }),
      telexAction_(
          "Telex", [this] { return *config_.inputMethod == InputMethod::Telex; },
          [this](auto *ic) { switchMode(InputMethod::Telex, ic); }),
      vniAction_(
          "VNI", [this] { return *config_.inputMethod == InputMethod::Vni; },
          [this](auto *ic) { switchMode(InputMethod::Vni, ic); }) {
    registerProperties();
    setupActions();
    reloadConfig();
}

void VietnameseInputMethodEngine::registerProperties() {
    instance_->inputContextManager().registerProperty("VipyState", &stateFactory_);
}

void VietnameseInputMethodEngine::setupActions() {
    auto &ui = instance_->userInterfaceManager();
    if (!ui.registerAction("vipy-input-method", &modeAction_) ||
        !ui.registerAction("vipy-input-method-telex", &telexAction_) ||
        !ui.registerAction("vipy-input-method-vni", &vniAction_)) {
        FCITX_ERROR() << "Failed to register Vipy input method actions";
    }
    modeMenu_.addAction(&telexAction_);
    modeMenu_.addAction(&vniAction_);
    modeAction_.setShortText("Input Method");
    modeAction_.setIcon("fcitx-vipy");
    modeAction_.setMenu(&modeMenu_);
}

const fcitx::Configuration *VietnameseInputMethodEngine::getConfig() const {
    return &config_;
}

void VietnameseInputMethodEngine::setConfig(const fcitx::RawConfig &config) {
    config_.load(config, true);
    engine_.setSchema(*config_.inputMethod);
    resetAllStates();
    safeSaveAsIni(config_, "conf/vipy.conf");
}

void VietnameseInputMethodEngine::reloadConfig() {
    readAsIni(config_, "conf/vipy.conf");
    engine_.setSchema(*config_.inputMethod);
}

std::string VietnameseInputMethodEngine::subMode(const fcitx::InputMethodEntry &,
                                                 fcitx::InputContext &) {
    return InputMethodToString(*config_.inputMethod);
}

std::string VietnameseInputMethodEngine::subModeIconImpl(
    const fcitx::InputMethodEntry &, fcitx::InputContext &) {
    static const IconSearchPaths paths{
        {"/usr/share/icons/hicolor/22x22/status",
         "/usr/share/icons/hicolor/24x24/status",
         "/usr/share/icons/hicolor/scalable/status",
         "/usr/share/icons/hicolor/scalable/apps"},
        FCITX_VIPY_ICON_DIR};
    return resolveIconPath({"fcitx-vipy"}, paths);
}

void VietnameseInputMethodEngine::activate(const fcitx::InputMethodEntry &,
                                           fcitx::InputContextEvent &event) {
    if (auto *ic = event.inputContext()) {
        ic->statusArea().addAction(fcitx::StatusGroup::InputMethod,
                                   &modeAction_);
    }
}

void VietnameseInputMethodEngine::reset(const fcitx::InputMethodEntry &,
                                        fcitx::InputContextEvent &event) {
    resetState(event.inputContext());
}

void VietnameseInputMethodEngine::keyEvent(const fcitx::InputMethodEntry &,
                                           fcitx::KeyEvent &event) {
    auto *ic = event.inputContext();
    if (ic) {
        ic->propertyFor(&stateFactory_)->keyEvent(event);
    }
}

void VietnameseInputMethodEngine::switchMode(InputMethod mode,
                                             fcitx::InputContext *ic) {
    if (*config_.inputMethod == mode) {
        return;
    }
    if (ic) {
        ic->propertyFor(&stateFactory_)->commitAndReset();
    }
    config_.inputMethod.setValue(mode);
    engine_.setSchema(mode);
    safeSaveAsIni(config_, "conf/vipy.conf");
}

void VietnameseInputMethodEngine::resetState(fcitx::InputContext *ic) {
    if (ic) {
        ic->propertyFor(&stateFactory_)->reset();
    }
}

void VietnameseInputMethodEngine::resetAllStates() {
    instance_->inputContextManager().foreach([this](fcitx::InputContext *ic) {
        ic->propertyFor(&stateFactory_)->reset();
        return true;
    });
}

} // namespace vipy::fcitx_wrapper
