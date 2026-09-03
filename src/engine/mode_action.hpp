#ifndef VIPY_MODE_ACTION_HPP
#define VIPY_MODE_ACTION_HPP

#include <fcitx/action.h>

#include <functional>
#include <string>

namespace vipy::fcitx_wrapper {

class ModeAction final : public fcitx::Action {
public:
    ModeAction(std::string text, std::function<bool()> checked,
               std::function<void(fcitx::InputContext *)> activate);
    std::string shortText(fcitx::InputContext *) const override;
    std::string icon(fcitx::InputContext *) const override;
    bool isChecked(fcitx::InputContext *) const override;
    void activate(fcitx::InputContext *ic) override;

private:
    std::string text_;
    std::function<bool()> checked_;
    std::function<void(fcitx::InputContext *)> activate_;
};

class ToggleAction final : public fcitx::Action {
public:
    ToggleAction(std::string text, std::function<bool()> checked,
                 std::function<void()> toggle);
    std::string shortText(fcitx::InputContext *) const override;
    std::string icon(fcitx::InputContext *) const override;
    bool isChecked(fcitx::InputContext *) const override;
    void activate(fcitx::InputContext *) override;

private:
    std::string text_;
    std::function<bool()> checked_;
    std::function<void()> toggle_;
};

} // namespace vipy::fcitx_wrapper

#endif // VIPY_MODE_ACTION_HPP
