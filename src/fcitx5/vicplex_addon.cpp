//
// Created by keworld on 8/26/26.
//
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputcontext.h>
#include <fcitx/event.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputpanel.h>
#include <fcitx/action.h>
#include <fcitx/instance.h>
#include <fcitx/menu.h>
#include <fcitx/statusarea.h>
#include <fcitx/userinterfacemanager.h>
#include <fcitx-config/iniparser.h>

#include "vicplex/telex_transformer.hpp"
#include "vicplex/vni_transformer.hpp"
#include "vicplex/vicplex_config.hpp"
#include "vicplex/utf8_helper.hpp"
#include "vicplex/vicplex-icon-resolver.hpp"

#include <functional>
#include <utility>

namespace vicplex::fcitx_wrapper {

namespace {
    class ModeAction : public fcitx::Action {
    public:
        ModeAction(std::string text, std::function<bool()> checked,
                   std::function<void(fcitx::InputContext *)> activate)
            : text_(std::move(text)), checked_(std::move(checked)),
              activate_(std::move(activate)) {
            setCheckable(true);
        }

        std::string shortText(fcitx::InputContext *) const override { return text_; }
        std::string icon(fcitx::InputContext *) const override {
            return "fcitx-vicplex";
        }
        bool isChecked(fcitx::InputContext *) const override { return checked_(); }
        void activate(fcitx::InputContext *ic) override { activate_(ic); }

    private:
        std::string text_;
        std::function<bool()> checked_;
        std::function<void(fcitx::InputContext *)> activate_;
    };

    class VietnameseInputMethodEngine : public fcitx::InputMethodEngineV2 {
    public:
        explicit VietnameseInputMethodEngine(fcitx::Instance *instance, SyllableDict &dict)
            : transformer_(dict),
              telexAction_("Telex", [this] { return *config_.inputMethod == InputMethod::Telex; },
                           [this](fcitx::InputContext *ic) { switchMode(InputMethod::Telex, ic); }),
              vniAction_("VNI", [this] { return *config_.inputMethod == InputMethod::Vni; },
                         [this](fcitx::InputContext *ic) { switchMode(InputMethod::Vni, ic); }) {
            auto &uiManager = instance->userInterfaceManager();
            uiManager.registerAction("vicplex-input-method", &modeAction_);
            uiManager.registerAction("vicplex-input-method-telex", &telexAction_);
            uiManager.registerAction("vicplex-input-method-vni", &vniAction_);
            modeMenu_.addAction(&telexAction_);
            modeMenu_.addAction(&vniAction_);
            modeAction_.setShortText("Input Method");
            modeAction_.setMenu(&modeMenu_);
            reloadConfig();
        }

        const fcitx::Configuration *getConfig() const override { return &config_; }

        void setConfig(const fcitx::RawConfig &config) override {
            const InputMethod oldMode = *config_.inputMethod;
            config_.load(config, true);
            if (*config_.inputMethod != oldMode) {
                resetState(nullptr);
            }
            safeSaveAsIni(config_, "conf/vicplex.conf");
            updateModeActions(nullptr);
        }

        void reloadConfig() override {
            const InputMethod oldMode = *config_.inputMethod;
            readAsIni(config_, "conf/vicplex.conf");
            if (*config_.inputMethod != oldMode) {
                resetState(nullptr);
            }
        }

        std::string subMode(const fcitx::InputMethodEntry &, fcitx::InputContext &) override {
            return InputMethodToString(*config_.inputMethod);
        }

        std::string subModeIconImpl(const fcitx::InputMethodEntry &,
                                    fcitx::InputContext &) override {
            static const IconSearchPaths paths{
                {
                    "/usr/share/icons/hicolor/22x22/status",
                    "/usr/share/icons/hicolor/24x24/status",
                    "/usr/share/icons/hicolor/scalable/status",
                    "/usr/share/icons/hicolor/scalable/apps",
                    "/usr/share/icons/hicolor/48x48/apps",
                },
                FCITX_VICPLEX_ICON_DIR,
            };
            return resolveIconPath({"fcitx-vicplex"}, paths);
        }

        void activate(const fcitx::InputMethodEntry &entry,
                      fcitx::InputContextEvent &event) override {
            FCITX_UNUSED(entry);
            auto *ic = event.inputContext();
            if (ic) {
                ic->statusArea().addAction(fcitx::StatusGroup::InputMethod, &modeAction_);
                telexAction_.update(ic);
                vniAction_.update(ic);
            }
        }

        void keyEvent(const fcitx::InputMethodEntry &, fcitx::KeyEvent &event) override {
            if (event.isRelease()) return;

            auto *ic = event.inputContext();
            const auto &key = event.key();

            if (key.states().test(fcitx::KeyState::Ctrl) ||
                key.states().test(fcitx::KeyState::Alt) ||
                key.states().test(fcitx::KeyState::Super)) {
                commitAndReset(ic);
                return;
            }

            if (key.sym() == FcitxKey_BackSpace) {
                handleBackspace(ic, event);
                return;
            }

            if (key.sym() == FcitxKey_space) {
                handleSpace(ic, event);
                return;
            }

            const uint32_t sym = key.sym();
            const bool isLetter = (sym >= 'a' && sym <= 'z') || (sym >= 'A' && sym <= 'Z');
            const bool isVniDigit = *config_.inputMethod == InputMethod::Vni &&
                sym >= '1' && sym <= '9';

            if ((!isLetter && !isVniDigit) || !ic) {
                commitAndReset(ic);
                return;
            }

            const char inputChar = static_cast<char>(sym);

            if (current_.full()) {
                commitAndReset(ic);
            }

            const bool consumed = *config_.inputMethod == InputMethod::Telex
                ? vicplex::TelexTransformer::processKey(current_, inputChar)
                : vicplex::VniTransformer::processKey(current_, inputChar);
            if (!consumed) {
                commitAndReset(ic);
                return;
            }
            raw_.push(static_cast<unsigned char>(inputChar));

            if (!isLikelyVietnamese() && hasVowelChar()) {
                current_.assignContentFrom(raw_);
            }

            updatePreedit(ic);
            event.filterAndAccept();
        }

        void reset(const fcitx::InputMethodEntry &, fcitx::InputContextEvent &event) override {
            resetState(event.inputContext());
        }

    private:
        void switchMode(InputMethod mode, fcitx::InputContext *ic) {
            if (*config_.inputMethod == mode) return;
            commitAndReset(ic);
            config_.inputMethod.setValue(mode);
            safeSaveAsIni(config_, "conf/vicplex.conf");
            updateModeActions(ic);
            if (ic) {
                ic->updateUserInterface(fcitx::UserInterfaceComponent::StatusArea);
            }
        }

        void updateModeActions(fcitx::InputContext *ic) {
            telexAction_.update(ic);
            vniAction_.update(ic);
            modeAction_.update(ic);
        }

        bool isLikelyVietnamese() const {
            return vicplex::TelexTransformer::isLikelyVietnamese(current_);
        }

        bool inDictionary() const {
            return vicplex::TelexTransformer::inDictionary(current_);
        }

        bool hasVowelChar() const {
            return vicplex::TelexTransformer::hasVowelChar(current_);
        }

        TelexTransformer transformer_;
        CharBuffer current_;
        CharBuffer raw_;
        VicplexConfig config_;
        fcitx::Menu modeMenu_;
        fcitx::SimpleAction modeAction_;
        ModeAction telexAction_;
        ModeAction vniAction_;

        void updatePreedit(fcitx::InputContext *ic) const {
            if (!ic) return;
            auto &panel = ic->inputPanel();
            if (current_.empty()) {
                panel.reset();
                ic->updatePreedit();
                return;
            }
            std::string preedit;
            utf8::encodeAll(current_.data(), current_.size(), preedit);

            fcitx::Text clientPreedit(
                preedit,
                fcitx::TextFormatFlags{
                    fcitx::TextFormatFlag::Underline,
                    fcitx::TextFormatFlag::DontCommit});

            clientPreedit.setCursor(static_cast<int>(preedit.size()));
            panel.setClientPreedit(clientPreedit);

            ic->updatePreedit();
        }

        void handleBackspace(fcitx::InputContext *ic, fcitx::KeyEvent &event) {
            if (raw_.empty()) return;

            raw_.pop();

            if (raw_.empty()) {
                resetState(ic);
            } else {
                CharBuffer tempRaw = raw_;
                current_.clear();
                raw_.clear();

                for (size_t i = 0; i < tempRaw.size(); ++i) {
                    const char c = static_cast<char>(tempRaw[i]);
                    if (*config_.inputMethod == InputMethod::Telex) {
                        vicplex::TelexTransformer::processKey(current_, c);
                    } else {
                        vicplex::VniTransformer::processKey(current_, c);
                    }
                    raw_.push(static_cast<unsigned char>(c));
                }

                if (!isLikelyVietnamese() && hasVowelChar()) {
                    current_.assignContentFrom(raw_);
                }

                updatePreedit(ic);
            }

            event.filterAndAccept();
        }

        void commitAndReset(fcitx::InputContext *ic) {
            if (ic && !current_.empty()) {
                std::string out;
                const bool valid = isLikelyVietnamese() &&
                    (*config_.inputMethod == InputMethod::Vni || inDictionary());
                const CharBuffer &src = valid ? current_ : raw_;
                utf8::encodeAll(src.data(), src.size(), out);
                ic->commitString(out);
            }
            resetState(ic);
        }

        void resetState(fcitx::InputContext *ic) {
            current_.clear();
            raw_.clear();
            updatePreedit(ic);
        }

        void handleSpace(fcitx::InputContext *ic, fcitx::KeyEvent &event) {
            std::string out;
            if (!current_.empty()) {
                const bool valid = isLikelyVietnamese() &&
                    (*config_.inputMethod == InputMethod::Vni || inDictionary());
                const CharBuffer &src = valid ? current_ : raw_;
                utf8::encodeAll(src.data(), src.size(), out);
                ic->commitString(out);
            } else if (!raw_.empty()) {
                utf8::encodeAll(raw_.data(), raw_.size(), out);
                ic->commitString(out);
            }
            resetState(ic);
        }
    };
}

namespace {
    class ViCplexAddonFactory : public fcitx::AddonFactory {
    public:
        fcitx::AddonInstance *create(fcitx::AddonManager *manager) override {
            static SyllableDict sharedDict;
            return new VietnameseInputMethodEngine(manager->instance(), sharedDict);
        }
    };
}

} // namespace vicplex::fcitx_wrapper

FCITX_ADDON_FACTORY(vicplex::fcitx_wrapper::ViCplexAddonFactory)