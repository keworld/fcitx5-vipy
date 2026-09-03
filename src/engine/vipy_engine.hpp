#ifndef VIPY_ENGINE_HPP
#define VIPY_ENGINE_HPP

#include "mode_action.hpp"
#include "python/python_engine.hpp"
#include "python/python_runtime.hpp"
#include "vipy_state.hpp"
#include "vipy_config.hpp"
#include "../fcitx5/vipy-icon-resolver.hpp"

#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include <fcitx/menu.h>

namespace vipy::fcitx_wrapper {

class VietnameseInputMethodEngine final : public fcitx::InputMethodEngineV2 {
public:
    explicit VietnameseInputMethodEngine(fcitx::Instance *instance);

    const fcitx::Configuration *getConfig() const override;
    void setConfig(const fcitx::RawConfig &config) override;
    void reloadConfig() override;
    std::string subMode(const fcitx::InputMethodEntry &,
                        fcitx::InputContext &) override;
    std::string subModeIconImpl(const fcitx::InputMethodEntry &,
                                fcitx::InputContext &) override;
    void activate(const fcitx::InputMethodEntry &,
                  fcitx::InputContextEvent &event) override;
    void deactivate(const fcitx::InputMethodEntry &,
                    fcitx::InputContextEvent &event) override;
    void reset(const fcitx::InputMethodEntry &,
               fcitx::InputContextEvent &event) override;
    void keyEvent(const fcitx::InputMethodEntry &,
                  fcitx::KeyEvent &event) override;

private:
    void setupActions();
    void registerProperties();
    void switchMode(InputMethod mode, fcitx::InputContext *ic);
    void syncPythonConfig();
    void updateActions();
    void saveConfig();
    void resetState(fcitx::InputContext *ic);
    void resetAllStates();

    fcitx::Instance *instance_;
    python::PythonRuntime runtime_;
    python::PythonEngine engine_;
    VipyConfig config_;
    fcitx::FactoryFor<VipyState> stateFactory_;
    fcitx::Menu modeMenu_;
    fcitx::SimpleAction modeAction_;
    ModeAction telexAction_;
    ModeAction vniAction_;
    ToggleAction loneWAction_;
    ToggleAction spellCheckAction_;
    ToggleAction macroAction_;
    ToggleAction autoDecomposeAction_;
};

} // namespace vipy::fcitx_wrapper

#endif // VIPY_ENGINE_HPP
