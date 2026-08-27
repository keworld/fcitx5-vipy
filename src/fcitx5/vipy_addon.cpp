#include <Python.h>

#include <fcitx/action.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/menu.h>
#include <fcitx/statusarea.h>
#include <fcitx/userinterfacemanager.h>
#include <fcitx-config/iniparser.h>

#include "vipy/syllable_dict.hpp"
#include "vipy/utf8_helper.hpp"
#include "vipy/vipy-icon-resolver.hpp"
#include "vipy/vipy_config.hpp"

#include <functional>
#include <string>
#include <utility>

namespace vipy::fcitx_wrapper {
namespace {

class PythonEngine {
public:
    PythonEngine() {
        Py_Initialize();
        PyGILState_STATE state = PyGILState_Ensure();
        auto *path = PySys_GetObject("path");
        PyObject_CallMethod(path, "insert", "is", 0, VIPY_PYTHON_MODULE_DIR);
        module_ = PyImport_ImportModule("vietnamese_input_method");
        if (!module_) {
            PyErr_Print();
        } else {
            setSchema(InputMethod::Telex);
        }
        PyGILState_Release(state);
    }

    ~PythonEngine() {
        PyGILState_STATE state = PyGILState_Ensure();
        Py_XDECREF(engine_);
        Py_XDECREF(module_);
        PyGILState_Release(state);
    }

    PythonEngine(const PythonEngine &) = delete;
    PythonEngine &operator=(const PythonEngine &) = delete;

    void setSchema(InputMethod method) {
        PyGILState_STATE state = PyGILState_Ensure();
        Py_XDECREF(engine_);
        engine_ = nullptr;
        if (module_) {
            PyObject *phon = PyObject_GetAttrString(module_, "PHON");
            const char *schemaName = method == InputMethod::Telex
                ? "TelexSchema" : "VNISchema";
            PyObject *schemaClass = PyObject_GetAttrString(module_, schemaName);
            PyObject *schema = schemaClass ? PyObject_CallNoArgs(schemaClass) : nullptr;
            PyObject *engineClass = PyObject_GetAttrString(module_, "VietnameseEngine");
            if (phon && schema && engineClass) {
                engine_ = PyObject_CallFunctionObjArgs(engineClass, phon, schema, nullptr);
            }
            Py_XDECREF(phon);
            Py_XDECREF(schemaClass);
            Py_XDECREF(schema);
            Py_XDECREF(engineClass);
            if (!engine_) PyErr_Print();
        }
        PyGILState_Release(state);
    }

    std::string processWord(const std::string &word, char key) const {
        return callString("process_word", word, std::string(1, key));
    }

    bool isValidWord(const std::string &word) const {
        return callBool("is_valid_word", word);
    }

    bool hasMarks(const std::string &word) const {
        return callBool("has_vietnamese_marks", word);
    }

private:
    std::string callString(const char *name, const std::string &word,
                           const std::string &key = {}) const {
        PyGILState_STATE state = PyGILState_Ensure();
        std::string result;
        if (engine_) {
            PyObject *value = PyObject_CallMethod(engine_, name, "ss", word.c_str(), key.c_str());
            if (value && PyUnicode_Check(value)) {
                const char *text = PyUnicode_AsUTF8(value);
                if (text) result = text;
            } else if (!value) {
                PyErr_Print();
            }
            Py_XDECREF(value);
        }
        PyGILState_Release(state);
        return result;
    }

    bool callBool(const char *name, const std::string &word) const {
        PyGILState_STATE state = PyGILState_Ensure();
        bool result = false;
        if (engine_) {
            PyObject *value = PyObject_CallMethod(engine_, name, "s", word.c_str());
            if (value) result = PyObject_IsTrue(value) != 0;
            else PyErr_Print();
            Py_XDECREF(value);
        }
        PyGILState_Release(state);
        return result;
    }

    PyObject *module_ = nullptr;
    PyObject *engine_ = nullptr;
};

class ModeAction : public fcitx::Action {
public:
    ModeAction(std::string text, std::function<bool()> checked,
               std::function<void(fcitx::InputContext *)> activate)
        : text_(std::move(text)), checked_(std::move(checked)),
          activate_(std::move(activate)) { setCheckable(true); }
    std::string shortText(fcitx::InputContext *) const override { return text_; }
    std::string icon(fcitx::InputContext *) const override { return "fcitx-vipy"; }
    bool isChecked(fcitx::InputContext *) const override { return checked_(); }
    void activate(fcitx::InputContext *ic) override { activate_(ic); }
private:
    std::string text_;
    std::function<bool()> checked_;
    std::function<void(fcitx::InputContext *)> activate_;
};

class VietnameseInputMethodEngine : public fcitx::InputMethodEngineV2 {
public:
    explicit VietnameseInputMethodEngine(fcitx::Instance *instance)
        : telexAction_("Telex", [this] { return *config_.inputMethod == InputMethod::Telex; },
                       [this](auto *ic) { switchMode(InputMethod::Telex, ic); }),
          vniAction_("VNI", [this] { return *config_.inputMethod == InputMethod::Vni; },
                     [this](auto *ic) { switchMode(InputMethod::Vni, ic); }) {
        auto &ui = instance->userInterfaceManager();
        ui.registerAction("vipy-input-method", &modeAction_);
        modeMenu_.addAction(&telexAction_);
        modeMenu_.addAction(&vniAction_);
        modeAction_.setShortText("Input Method");
        modeAction_.setMenu(&modeMenu_);
        reloadConfig();
    }

    const fcitx::Configuration *getConfig() const override { return &config_; }
    void setConfig(const fcitx::RawConfig &config) override {
        config_.load(config, true);
        engine_.setSchema(*config_.inputMethod);
        resetState(nullptr);
        safeSaveAsIni(config_, "conf/vipy.conf");
    }
    void reloadConfig() override {
        readAsIni(config_, "conf/vipy.conf");
        engine_.setSchema(*config_.inputMethod);
    }
    std::string subMode(const fcitx::InputMethodEntry &, fcitx::InputContext &) override {
        return InputMethodToString(*config_.inputMethod);
    }
    std::string subModeIconImpl(const fcitx::InputMethodEntry &, fcitx::InputContext &) override {
        static const IconSearchPaths paths{{"/usr/share/icons/hicolor/22x22/status",
            "/usr/share/icons/hicolor/24x24/status", "/usr/share/icons/hicolor/scalable/status",
            "/usr/share/icons/hicolor/scalable/apps"}, FCITX_VIPY_ICON_DIR};
        return resolveIconPath({"fcitx-vipy"}, paths);
    }
    void activate(const fcitx::InputMethodEntry &, fcitx::InputContextEvent &event) override {
        if (auto *ic = event.inputContext()) ic->statusArea().addAction(
            fcitx::StatusGroup::InputMethod, &modeAction_);
    }
    void reset(const fcitx::InputMethodEntry &, fcitx::InputContextEvent &event) override {
        resetState(event.inputContext());
    }
    void keyEvent(const fcitx::InputMethodEntry &, fcitx::KeyEvent &event) override {
        if (event.isRelease()) return;
        auto *ic = event.inputContext();
        const auto sym = event.key().sym();
        if (event.key().states().test(fcitx::KeyState::Ctrl) ||
            event.key().states().test(fcitx::KeyState::Alt) ||
            event.key().states().test(fcitx::KeyState::Super)) {
            commitAndReset(ic); return;
        }
        if (sym == FcitxKey_BackSpace) { backspace(ic, event); return; }
        if (sym == FcitxKey_space) { commitAndReset(ic); event.filterAndAccept(); return; }
        const bool letter = (sym >= 'a' && sym <= 'z') || (sym >= 'A' && sym <= 'Z');
        const bool digit = *config_.inputMethod == InputMethod::Vni && sym >= '1' && sym <= '9';
        if (!ic || (!letter && !digit)) { commitAndReset(ic); return; }
        if (current_.size() >= 32) commitAndReset(ic);
        const char key = static_cast<char>(sym);
        const std::string next = engine_.processWord(current_, key);
        if (next.empty()) { commitAndReset(ic); return; }
        current_ = next;
        raw_.push_back(key);
        updatePreedit(ic);
        event.filterAndAccept();
    }

private:
    void switchMode(InputMethod mode, fcitx::InputContext *ic) {
        if (*config_.inputMethod == mode) return;
        commitAndReset(ic);
        config_.inputMethod.setValue(mode);
        engine_.setSchema(mode);
        safeSaveAsIni(config_, "conf/vipy.conf");
    }
    void updatePreedit(fcitx::InputContext *ic) const {
        if (!ic) return;
        auto &panel = ic->inputPanel();
        if (current_.empty()) { panel.reset(); ic->updatePreedit(); return; }
        fcitx::Text text(current_, fcitx::TextFormatFlags{
            fcitx::TextFormatFlag::Underline, fcitx::TextFormatFlag::DontCommit});
        text.setCursor(static_cast<int>(current_.size()));
        panel.setClientPreedit(text);
        ic->updatePreedit();
    }
    void backspace(fcitx::InputContext *ic, fcitx::KeyEvent &event) {
        if (raw_.empty()) return;
        raw_.pop_back();
        current_.clear();
        for (char key : raw_) current_ = engine_.processWord(current_, key);
        updatePreedit(ic);
        event.filterAndAccept();
    }
    void commitAndReset(fcitx::InputContext *ic) {
        if (ic && !current_.empty()) {
            const bool valid = engine_.hasMarks(current_) && engine_.isValidWord(current_);
            const bool dictionaryMatch = *config_.inputMethod == InputMethod::Vni ||
                SyllableDict::contains(current_);
            const std::string &out = valid && dictionaryMatch ? current_ : raw_;
            ic->commitString(out);
        }
        resetState(ic);
    }
    void resetState(fcitx::InputContext *ic) {
        current_.clear(); raw_.clear(); updatePreedit(ic);
    }

    PythonEngine engine_;
    std::string current_;
    std::string raw_;
    VipyConfig config_;
    fcitx::Menu modeMenu_;
    fcitx::SimpleAction modeAction_;
    ModeAction telexAction_;
    ModeAction vniAction_;
};

class VipyAddonFactory : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance *create(fcitx::AddonManager *manager) override {
        return new VietnameseInputMethodEngine(manager->instance());
    }
};

} // namespace
} // namespace vipy::fcitx_wrapper

FCITX_ADDON_FACTORY(vipy::fcitx_wrapper::VipyAddonFactory)
