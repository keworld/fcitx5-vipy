#ifndef VIPY_CONFIG_HPP
#define VIPY_CONFIG_HPP

#include <fcitx-config/configuration.h>
#include <fcitx-config/enum.h>
#include <fcitx-config/option.h>

#include "input_method.hpp"

namespace vipy {

FCITX_CONFIG_ENUM_NAME(InputMethod, "Telex", "VNI");

FCITX_CONFIGURATION(
    VipyConfig,
    fcitx::Option<InputMethod> inputMethod{
        this, "InputMethod", "Input Method", InputMethod::Telex};
    fcitx::Option<bool> enableLoneW{
        this, "EnableLoneW", "Enable lone w", true};
    fcitx::Option<bool> enableSpellCheck{
        this, "EnableSpellCheck", "Enable spell checking", true};
    fcitx::Option<bool> enableMacro{
        this, "EnableMacro", "Enable macros", true};
    fcitx::Option<bool> enableAutoDecompose{
        this, "EnableAutoDecompose", "Enable automatic decomposition", true};
);

} // namespace vipy

#endif // VIPY_CONFIG_HPP
