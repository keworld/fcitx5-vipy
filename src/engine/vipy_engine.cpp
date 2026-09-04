#include "vipy_engine.hpp"

#include <fcitx/addonmanager.h>
#include <fcitx/inputcontext.h>
#include <fcitx/statusarea.h>
#include <fcitx/userinterfacemanager.h>
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/log.h>

#include <cstdlib>
#include <filesystem>

namespace vipy::fcitx_wrapper {
namespace {

std::string macroFilePath() {
    const char *xdgConfigHome = std::getenv("XDG_CONFIG_HOME");
    const char *home = std::getenv("HOME");
    const std::filesystem::path configHome =
        (xdgConfigHome && *xdgConfigHome)
            ? std::filesystem::path(xdgConfigHome)
            : (home && *home ? std::filesystem::path(home) / ".config"
                              : std::filesystem::path{});
    const auto userMacro =
        configHome / "fcitx5-vipy" / "data" / "vietnamese.macro";
    if (std::filesystem::is_regular_file(userMacro)) {
        return userMacro.string();
    }
    return std::string(VIPY_DATA_DIR) + "/vietnamese.macro";
}

} // namespace

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
          [this](auto *ic) { switchMode(InputMethod::Vni, ic); }),
      loneWAction_("Lone w", [this] { return *config_.enableLoneW; },
                   [this] {
                       config_.enableLoneW.setValue(!*config_.enableLoneW);
                       setFeatureConfig("enable_lone_w", *config_.enableLoneW);
                   }),
      spellCheckAction_("Spell check", [this] { return *config_.enableSpellCheck; },
                        [this] {
                            config_.enableSpellCheck.setValue(
                                !*config_.enableSpellCheck);
                            setFeatureConfig("enable_spell_check",
                                             *config_.enableSpellCheck);
                        }),
      macroAction_("Macros", [this] { return *config_.enableMacro; }, [this] {
          config_.enableMacro.setValue(!*config_.enableMacro);
          setFeatureConfig("enable_macro", *config_.enableMacro);
      }),
      autoDecomposeAction_(
          "Automatic decomposition",
          [this] { return *config_.enableAutoDecompose; }, [this] {
              config_.enableAutoDecompose.setValue(
                  !*config_.enableAutoDecompose);
              setFeatureConfig("enable_auto_decompose",
                               *config_.enableAutoDecompose);
          }) {
    reloadConfig();
    registerProperties();
    setupActions();
    updateActions();
    initialized_ = true;
}

void VietnameseInputMethodEngine::registerProperties() {
    instance_->inputContextManager().registerProperty("VipyState", &stateFactory_);
}

void VietnameseInputMethodEngine::setupActions() {
    auto &ui = instance_->userInterfaceManager();
    if (!ui.registerAction("vipy-input-method", &modeAction_) ||
        !ui.registerAction("vipy-input-method-telex", &telexAction_) ||
        !ui.registerAction("vipy-input-method-vni", &vniAction_) ||
        !ui.registerAction("vipy-enable-lone-w", &loneWAction_) ||
        !ui.registerAction("vipy-enable-spell-check", &spellCheckAction_) ||
        !ui.registerAction("vipy-enable-macro", &macroAction_) ||
        !ui.registerAction("vipy-enable-auto-decompose",
                           &autoDecomposeAction_)) {
        FCITX_ERROR() << "Failed to register Vipy input method actions";
    }
    modeMenu_.addAction(&telexAction_);
    modeMenu_.addAction(&vniAction_);
    modeMenu_.addAction(&loneWAction_);
    modeMenu_.addAction(&spellCheckAction_);
    modeMenu_.addAction(&macroAction_);
    modeMenu_.addAction(&autoDecomposeAction_);
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
    syncPythonConfig();
    resetAllStates();
    saveConfig();
    updateActions();
}

void VietnameseInputMethodEngine::reloadConfig() {
    readAsIni(config_, fcitx::StandardPathsType::Config, "addon/vipy.conf");
    engine_.setSchema(*config_.inputMethod);
    syncPythonConfig();
    if (initialized_) {
        resetAllStates();
        updateActions();
    }
}

void VietnameseInputMethodEngine::syncPythonConfig() {
    engine_.setConfig("enable_lone_w", *config_.enableLoneW);
    engine_.setConfig("enable_spell_check", *config_.enableSpellCheck);
    engine_.setConfig("enable_macro", *config_.enableMacro);
    engine_.setConfig("enable_auto_decompose", *config_.enableAutoDecompose);
    engine_.setConfig("macro_file", macroFilePath());
}

void VietnameseInputMethodEngine::setFeatureConfig(const char *key, bool value) {
    engine_.setConfig(key, value);
    resetAllStates();
    saveConfig();
    updateActions();
}

void VietnameseInputMethodEngine::updateActions() {
    instance_->inputContextManager().foreach([this](fcitx::InputContext *ic) {
        modeAction_.update(ic);
        telexAction_.update(ic);
        vniAction_.update(ic);
        loneWAction_.update(ic);
        spellCheckAction_.update(ic);
        macroAction_.update(ic);
        autoDecomposeAction_.update(ic);
        return true;
    });
}

void VietnameseInputMethodEngine::saveConfig() {
    if (!safeSaveAsIni(config_, fcitx::StandardPathsType::Config,
                      "addon/vipy.conf")) {
        FCITX_ERROR() << "Failed to save Vipy configuration";
    }
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

void VietnameseInputMethodEngine::deactivate(
    const fcitx::InputMethodEntry &, fcitx::InputContextEvent &event) {
    if (auto *ic = event.inputContext()) {
        ic->propertyFor(&stateFactory_)->commitAndReset();
    }
    engine_.deactivate();
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
    syncPythonConfig();
    resetAllStates();
    safeSaveAsIni(config_, fcitx::StandardPathsType::Config,
                  "addon/vipy.conf");
    updateActions();
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
