#ifndef VIPY_STATE_HPP
#define VIPY_STATE_HPP

#include "python/python_engine.hpp"
#include "vipy/vipy_config.hpp"

#include <fcitx/event.h>
#include <fcitx/inputcontextproperty.h>

#include <string>

namespace vipy::fcitx_wrapper {

class VipyState final : public fcitx::InputContextProperty {
public:
    VipyState(python::PythonEngine *engine, VipyConfig *config,
              fcitx::InputContext *ic);

    void keyEvent(fcitx::KeyEvent &event);
    void reset();
    void commitAndReset(const std::string &suffix = {});

private:
    void updatePreedit();
    void backspace(fcitx::KeyEvent &event);

    python::PythonEngine &engine_;
    VipyConfig &config_;
    fcitx::InputContext *ic_;
    std::string current_;
    std::string raw_;
};

} // namespace vipy::fcitx_wrapper

#endif // VIPY_STATE_HPP
