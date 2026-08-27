#include <Python.h>

#include <dlfcn.h>

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
#include <fcitx-utils/log.h>

#include "vipy/syllable_dict.hpp"
#include "vipy/utf8_helper.hpp"
#include "vipy/vipy-icon-resolver.hpp"
#include "vipy/vipy_config.hpp"

#include <functional>
#include <string>
#include <utility>

namespace vipy::fcitx_wrapper {
namespace {

std::string lowercaseVietnamese(std::string value) {
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
    FCITX_INFO() << "Promoted libpython symbols globally from "
                 << info.dli_fname;
    return handle;
}

class PythonEngine {
public:
    PythonEngine() {
        pythonHandle_ = makePythonSymbolsGlobal();
        Py_Initialize();
        FCITX_INFO() << "Python initialized; module path: "
                     << VIPY_PYTHON_MODULE_DIR;
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
        } else {
            FCITX_INFO() << "Imported vietnamese_input_method successfully";
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
        const char *schemaName = method == InputMethod::Telex
            ? "TelexSchema" : "VNISchema";
        FCITX_INFO() << "Selecting schema " << schemaName;
        if (module_) {
            PyObject *phon = PyObject_GetAttrString(module_, "PHON");
            PyObject *schemaClass = PyObject_GetAttrString(module_, schemaName);
            PyObject *schema = schemaClass ? PyObject_CallNoArgs(schemaClass) : nullptr;
            PyObject *engineClass = PyObject_GetAttrString(module_, "VietnameseEngine");
            if (phon && schema && engineClass) {
                engine_ = PyObject_CallFunctionObjArgs(engineClass, phon, schema, nullptr);
            }
            if (!phon || !schemaClass || !schema || !engineClass || !engine_) {
                logPythonError("creating Vietnamese engine");
            } else {
                FCITX_INFO() << "Schema " << schemaName << " is ready";
            }
            Py_XDECREF(phon);
            Py_XDECREF(schemaClass);
            Py_XDECREF(schema);
            Py_XDECREF(engineClass);
        } else {
            FCITX_ERROR() << "Cannot select schema: Python module is unavailable";
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
        if (!engine_) {
            FCITX_ERROR() << "Cannot call Python " << name
                          << ": engine is unavailable";
        } else {
            PyObject *value = PyObject_CallMethod(engine_, name, "ss", word.c_str(), key.c_str());
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

    bool callBool(const char *name, const std::string &word) const {
        PyGILState_STATE state = PyGILState_Ensure();
        bool result = false;
        if (!engine_) {
            FCITX_ERROR() << "Cannot call Python " << name
                          << ": engine is unavailable";
        } else {
            PyObject *value = PyObject_CallMethod(engine_, name, "s", word.c_str());
            if (value) result = PyObject_IsTrue(value) != 0;
            else logPythonError(name);
            Py_XDECREF(value);
        }
        PyGILState_Release(state);
        return result;
    }

    PyObject *module_ = nullptr;
    PyObject *engine_ = nullptr;
    void *pythonHandle_ = nullptr;
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
        const bool parentRegistered =
            ui.registerAction("vipy-input-method", &modeAction_);
        const bool telexRegistered =
            ui.registerAction("vipy-input-method-telex", &telexAction_);
        const bool vniRegistered =
            ui.registerAction("vipy-input-method-vni", &vniAction_);
        FCITX_INFO() << "Action registration: parent=" << parentRegistered
                     << " telex=" << telexRegistered << " vni=" << vniRegistered
                     << " ids=" << modeAction_.id() << "/" << telexAction_.id()
                     << "/" << vniAction_.id();
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
        resetState(nullptr);
        safeSaveAsIni(config_, "conf/vipy.conf");
    }
    void reloadConfig() override {
        readAsIni(config_, "conf/vipy.conf");
        FCITX_INFO() << "Loaded input method configuration: "
                     << (*config_.inputMethod == InputMethod::Telex
                             ? "Telex" : "VNI");
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
            FCITX_DEBUG() << "Activated for input context; status action id="
                          << modeAction_.id() << " menu actions="
                          << modeMenu_.actions().size();
        } else {
            FCITX_WARN() << "Activate called without an input context";
        }
    }
    void reset(const fcitx::InputMethodEntry &, fcitx::InputContextEvent &event) override {
        resetState(event.inputContext());
    }
    void keyEvent(const fcitx::InputMethodEntry &, fcitx::KeyEvent &event) override {
        if (event.isRelease()) return;
        auto *ic = event.inputContext();
        const auto sym = event.key().sym();
        FCITX_DEBUG() << "Key event: " << event.key() << " sym=" << sym
                      << " current=" << current_ << " raw=" << raw_;
        if (event.key().states().test(fcitx::KeyState::Ctrl) ||
            event.key().states().test(fcitx::KeyState::Alt) ||
            event.key().states().test(fcitx::KeyState::Super)) {
            commitAndReset(ic); return;
        }
        if (sym == FcitxKey_BackSpace) { backspace(ic, event); return; }
        if (sym == FcitxKey_space) {
            if (!current_.empty()) {
                commitAndReset(ic, " ");
                event.filterAndAccept();
            }
            return;
        }
        const bool letter = (sym >= 'a' && sym <= 'z') || (sym >= 'A' && sym <= 'Z');
        const bool digit = *config_.inputMethod == InputMethod::Vni && sym >= '1' && sym <= '9';
        if (!ic || (!letter && !digit)) { commitAndReset(ic); return; }
        if (current_.size() >= 32) commitAndReset(ic);
        const char key = static_cast<char>(sym);
        const std::string next = engine_.processWord(current_, key);
        if (next.empty()) {
            FCITX_WARN() << "Python returned an empty result for key " << key
                         << "; committing raw buffer";
            commitAndReset(ic);
            return;
        }
        FCITX_DEBUG() << "Processed key " << key << " => " << next;
        current_ = next;
        raw_.push_back(key);
        updatePreedit(ic);
        event.filterAndAccept();
    }

private:
    void switchMode(InputMethod mode, fcitx::InputContext *ic) {
        if (*config_.inputMethod == mode) return;
        FCITX_INFO() << "Switching input method to "
                     << (mode == InputMethod::Telex ? "Telex" : "VNI");
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
    void commitAndReset(fcitx::InputContext *ic, const std::string &suffix = {}) {
        if (ic && !current_.empty()) {
            const bool valid = engine_.hasMarks(current_) && engine_.isValidWord(current_);
            const std::string normalized = lowercaseVietnamese(current_);
            const bool dictionaryMatch = *config_.inputMethod == InputMethod::Vni ||
                SyllableDict::contains(normalized);
            const std::string &out = valid && dictionaryMatch ? current_ : raw_;
            FCITX_INFO() << "Commit: current=" << current_ << " raw=" << raw_
                         << " normalized=" << normalized << " valid=" << valid
                         << " dictionary=" << dictionaryMatch << " output="
                         << out << suffix;
            ic->commitString(out + suffix);
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
