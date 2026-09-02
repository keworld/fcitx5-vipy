#include <Python.h>

#include <dlfcn.h>

#include <fcitx/action.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/menu.h>
#include <fcitx/statusarea.h>
#include <fcitx/userinterfacemanager.h>
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/log.h>

#include "vipy/utf8_helper.hpp"
#include "vipy/vipy-icon-resolver.hpp"
#include "vipy/vipy_config.hpp"

#include <functional>
#include <string>
#include <utility>

namespace vipy::fcitx_wrapper {
namespace {

std::string lowercaseVietnamese(const std::string &value) {
    std::string result;
    result.reserve(value.size());
    for (size_t i = 0; i < value.size();) {
        const auto first = static_cast<unsigned char>(value[i]);
        if (first < 0x80) {
            result.push_back(first >= 'A' && first <= 'Z'
                                ? static_cast<char>(first - 'A' + 'a')
                                : static_cast<char>(first));
            ++i;
            continue;
        }

        size_t length = first < 0xE0 ? 2 : first < 0xF0 ? 3 : 4;
        if (i + length > value.size()) {
            result.append(value, i, std::string::npos);
            break;
        }
        char32_t codepoint = first & (length == 2 ? 0x1F : length == 3 ? 0x0F : 0x07);
        for (size_t j = 1; j < length; ++j) {
            const auto byte = static_cast<unsigned char>(value[i + j]);
            if ((byte & 0xC0) != 0x80) {
                result.push_back(value[i++]);
                codepoint = 0;
                break;
            }
            codepoint = (codepoint << 6) | (byte & 0x3F);
        }
        if (!codepoint) {
            continue;
        }
        if (codepoint == 0x0102 || codepoint == 0x00C2 ||
            codepoint == 0x00CA || codepoint == 0x00D4 ||
            codepoint == 0x0110 || codepoint == 0x01A0 ||
            codepoint == 0x01AF ||
            (codepoint >= 0x1EA0 && codepoint <= 0x1EF8 &&
             (codepoint & 1) == 0)) {
            ++codepoint;
        }
        utf8::encode(codepoint, result);
        i += length;
    }
    return result;
}

void logPythonError(const char *context) {
    if (!PyErr_Occurred()) {
        return;
    }
    PyObject *type = nullptr;
    PyObject *value = nullptr;
    PyObject *traceback = nullptr;
    PyErr_Fetch(&type, &value, &traceback);
    PyErr_NormalizeException(&type, &value, &traceback);
    std::string message = "unknown Python exception";
    if (value) {
        PyObject *text = PyObject_Str(value);
        if (text) {
            const char *utf8 = PyUnicode_AsUTF8(text);
            if (utf8) {
                message = utf8;
            }
            Py_DECREF(text);
        }
    }
    FCITX_ERROR() << context << ": " << message;
    Py_XDECREF(type);
    Py_XDECREF(value);
    Py_XDECREF(traceback);
}

void *makePythonSymbolsGlobal() {
    Dl_info info{};
    if (!dladdr(reinterpret_cast<void *>(&Py_Initialize), &info) ||
        !info.dli_fname) {
        FCITX_ERROR() << "Cannot locate the loaded libpython";
        return nullptr;
    }
    void *handle = dlopen(info.dli_fname, RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        FCITX_ERROR() << "Cannot promote libpython symbols from "
                      << info.dli_fname << ": " << dlerror();
        return nullptr;
    }
    return handle;
}

class PythonEngine {
public:
    PythonEngine() {
        pythonHandle_ = makePythonSymbolsGlobal();
        Py_Initialize();
        PyGILState_STATE state = PyGILState_Ensure();
        auto *path = PySys_GetObject("path");
        PyObject *insertResult = path
            ? PyObject_CallMethod(path, "insert", "is", 0,
                                  VIPY_PYTHON_MODULE_DIR)
            : nullptr;
        if (!insertResult) {
            if (path) {
                logPythonError("adding Python module path");
            } else {
                FCITX_ERROR() << "Python sys.path is unavailable";
            }
        }
        Py_XDECREF(insertResult);
        module_ = PyImport_ImportModule("vietnamese_input_method");
        if (!module_) {
            logPythonError("importing vietnamese_input_method");
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
        currentMethod_ = method;
        PyGILState_STATE state = PyGILState_Ensure();
        Py_XDECREF(engine_);
        engine_ = nullptr;
        const char *schemaName = method == InputMethod::Telex ? "telex" : "vni";
        if (module_) {
            PyObject *engineClass = PyObject_GetAttrString(module_, "VietnameseEngine");
            PyObject *schemaArg = PyUnicode_FromString(schemaName);
            if (engineClass && schemaArg) {
                engine_ = PyObject_CallFunctionObjArgs(engineClass, schemaArg, nullptr);
            }
            if (!engineClass || !schemaArg || !engine_) {
                logPythonError("creating Vietnamese engine");
            }
            Py_XDECREF(engineClass);
            Py_XDECREF(schemaArg);
        } else {
            FCITX_ERROR() << "Cannot select schema: Python module is unavailable";
        }
        PyGILState_Release(state);
    }

    std::string feedKey(const std::string &key) const {
        PyGILState_STATE state = PyGILState_Ensure();
        std::string result;
        if (engine_) {
            PyObject *value = PyObject_CallMethod(engine_, "feed", "s", key.c_str());
            if (!value) {
                logPythonError("feed");
            } else {
                Py_DECREF(value);
            }
            result = getWord();
        }
        PyGILState_Release(state);
        return result;
    }

    std::string getWord() const {
        return callString("get_word");
    }

    std::string commitCurrent() {
        return callString("commit");
    }

    void resetState() {
        setSchema(currentMethod_);
    }

private:
    std::string callString(const char *name, const std::string &word = {},
                           const std::string &key = {}) const {
        PyGILState_STATE state = PyGILState_Ensure();
        std::string result;
        if (engine_) {
            PyObject *value = nullptr;
            if (word.empty() && key.empty()) {
                value = PyObject_CallMethod(engine_, name, nullptr);
            } else if (key.empty()) {
                value = PyObject_CallMethod(engine_, name, "s", word.c_str());
            } else {
                value = PyObject_CallMethod(engine_, name, "ss", word.c_str(), key.c_str());
            }
            if (value && PyUnicode_Check(value)) {
                const char *text = PyUnicode_AsUTF8(value);
                if (text) result = text;
            } else if (!value) {
                logPythonError(name);
            } else {
                FCITX_ERROR() << name << " returned a non-string value";
            }
            Py_XDECREF(value);
        }
        PyGILState_Release(state);
        return result;
    }

    PyObject *module_ = nullptr;
    PyObject *engine_ = nullptr;
    InputMethod currentMethod_ = InputMethod::Telex;
    void *pythonHandle_ = nullptr;
};

class VipyState final : public fcitx::InputContextProperty {
public:
    VipyState(PythonEngine *engine, VipyConfig *config, fcitx::InputContext *ic)
        : engine_(*engine), config_(*config), ic_(ic) {}

    void keyEvent(fcitx::KeyEvent &event) {
        if (event.isRelease()) return;
        const auto sym = event.key().sym();
        if (event.key().states().test(fcitx::KeyState::Ctrl) ||
            event.key().states().test(fcitx::KeyState::Alt) ||
            event.key().states().test(fcitx::KeyState::Super)) {
            commitAndReset();
            return;
        }
        if (sym == FcitxKey_BackSpace) {
            backspace(event);
            return;
        }
        if (sym == FcitxKey_space) {
            if (!current_.empty()) {
                commitAndReset(" ");
                event.filterAndAccept();
            }
            return;
        }

        const bool letter = (sym >= 'a' && sym <= 'z') ||
                            (sym >= 'A' && sym <= 'Z');
        const bool digit = *config_.inputMethod == InputMethod::Vni &&
                           sym >= '1' && sym <= '9';
        if (!letter && !digit) {
            commitAndReset();
            return;
        }
        if (current_.size() >= 32) {
            commitAndReset();
        }

        const char key = static_cast<char>(sym);
        current_ = engine_.feedKey(std::string(1, key));
        if (current_.empty()) {
            commitAndReset();
            return;
        }
        raw_.push_back(key);
        updatePreedit();
        event.filterAndAccept();
    }

    void reset() {
        engine_.resetState();
        current_.clear();
        raw_.clear();
        updatePreedit();
    }

    void commitAndReset(const std::string &suffix = {}) {
        if (ic_ && !current_.empty()) {
            const std::string committed = engine_.commitCurrent();
            if (!committed.empty()) {
                ic_->commitString(committed + suffix);
            }
        }
        reset();
    }

private:
    void updatePreedit() {
        if (!ic_) return;

        auto &panel = ic_->inputPanel();

        // Không có nội dung preedit -> xóa sạch và return sớm
        if (current_.empty()) {
            panel.reset();
            ic_->updatePreedit();
            return;
        }

        // App có hỗ trợ tự vẽ preedit trong editor không?
        // (VD: GTK/Qt thường có, PyCharm/Java thì không)
        const bool useClientPreedit =
            ic_->capabilityFlags().test(fcitx::CapabilityFlag::Preedit);

        // Preedit có format: gạch chân, không commit nguyên văn
        fcitx::Text text(current_, fcitx::TextFormatFlags{
            fcitx::TextFormatFlag::Underline,
            fcitx::TextFormatFlag::DontCommit});
        text.setCursor(static_cast<int>(current_.size()));

        if (useClientPreedit) {
            // App tự vẽ preedit -> xóa preedit phía server
            panel.setPreedit(fcitx::Text{});
            panel.setClientPreedit(text);
        } else {
            // App không hỗ trợ (VD: PyCharm)
            // -> fcitx tự vẽ khung preedit nổi
            panel.setClientPreedit(fcitx::Text{});
            panel.setPreedit(text);
        }

        ic_->updatePreedit();
    }

    void backspace(fcitx::KeyEvent &event) {
        if (raw_.empty()) return;
        raw_.pop_back();
        current_ = engine_.feedKey("\b");
        updatePreedit();
        event.filterAndAccept();
    }

    PythonEngine &engine_;
    VipyConfig &config_;
    fcitx::InputContext *ic_;
    std::string current_;
    std::string raw_;
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
        : instance_(instance),
          stateFactory_([this](fcitx::InputContext &ic) {
              return new VipyState(&engine_, &config_, &ic);
          }),
          telexAction_("Telex", [this] { return *config_.inputMethod == InputMethod::Telex; },
                       [this](auto *ic) { switchMode(InputMethod::Telex, ic); }),
          vniAction_("VNI", [this] { return *config_.inputMethod == InputMethod::Vni; },
                     [this](auto *ic) { switchMode(InputMethod::Vni, ic); }) {
        instance->inputContextManager().registerProperty("VipyState",
                                                         &stateFactory_);
        auto &ui = instance->userInterfaceManager();
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
        reloadConfig();
    }

    const fcitx::Configuration *getConfig() const override { return &config_; }
    void setConfig(const fcitx::RawConfig &config) override {
        config_.load(config, true);
        engine_.setSchema(*config_.inputMethod);
        resetAllStates();
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
        if (auto *ic = event.inputContext()) {
            ic->statusArea().addAction(
                fcitx::StatusGroup::InputMethod, &modeAction_);
        }
    }
    void reset(const fcitx::InputMethodEntry &, fcitx::InputContextEvent &event) override {
        resetState(event.inputContext());
    }
    void keyEvent(const fcitx::InputMethodEntry &, fcitx::KeyEvent &event) override {
        auto *ic = event.inputContext();
        if (!ic) return;
        ic->propertyFor(&stateFactory_)->keyEvent(event);
    }

private:
    void switchMode(InputMethod mode, fcitx::InputContext *ic) {
        if (*config_.inputMethod == mode) return;
        if (ic) ic->propertyFor(&stateFactory_)->commitAndReset();
        config_.inputMethod.setValue(mode);
        engine_.setSchema(mode);
        safeSaveAsIni(config_, "conf/vipy.conf");
    }
    void resetState(fcitx::InputContext *ic) {
        if (ic) ic->propertyFor(&stateFactory_)->reset();
    }
    void resetAllStates() {
        instance_->inputContextManager().foreach([this](fcitx::InputContext *ic) {
            ic->propertyFor(&stateFactory_)->reset();
            return true;
        });
    }

    fcitx::Instance *instance_;
    PythonEngine engine_;
    VipyConfig config_;
    fcitx::FactoryFor<VipyState> stateFactory_;
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
