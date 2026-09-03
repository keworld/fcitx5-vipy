#include "vipy_engine.hpp"

#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>

namespace vipy::fcitx_wrapper {

class VipyAddonFactory final : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance *create(fcitx::AddonManager *manager) override {
        return new VietnameseInputMethodEngine(manager->instance());
    }
};

} // namespace vipy::fcitx_wrapper

FCITX_ADDON_FACTORY(vipy::fcitx_wrapper::VipyAddonFactory)
