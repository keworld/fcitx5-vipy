#ifndef VIPY_CONFIG_HPP
#define VIPY_CONFIG_HPP

#include <fcitx-config/configuration.h>
#include <fcitx-config/enum.h>
#include <fcitx-config/option.h>

namespace vipy {

enum class InputMethod { Telex, Vni };
FCITX_CONFIG_ENUM_NAME(InputMethod, "Telex", "VNI");

FCITX_CONFIGURATION(
    VipyConfig,
    fcitx::Option<InputMethod> inputMethod{
        this, "InputMethod", "Input Method", InputMethod::Telex};
);

} // namespace vipy

#endif // VIPY_CONFIG_HPP
