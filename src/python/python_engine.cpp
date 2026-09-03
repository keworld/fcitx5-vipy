#include "python_engine.hpp"

#include "python_error.hpp"

#include <iostream>
namespace vipy::python {

PythonEngine::PythonEngine(PythonRuntime &runtime) : runtime_(runtime) {
    GilGuard gil;
    module_.reset(PyImport_ImportModule("vietnamese_input_method"));
    if (!module_) {
        logPythonError("importing vietnamese_input_method");
    }
}

PythonEngine::~PythonEngine() {
    GilGuard gil;
    engine_.reset();
    module_.reset();
}

void PythonEngine::setSchema(vipy::InputMethod method) {
    currentMethod_ = method;
    GilGuard gil;
    engine_.reset();
    const char *schemaName = method == vipy::InputMethod::Telex ? "telex" : "vni";
    if (!module_) {
        std::cerr << "Cannot select schema: Python module is unavailable\n";
        return;
    }
    PyObjectPtr<> engineClass(PyObject_GetAttrString(module_.get(), "VietnameseEngine"));
    PyObjectPtr<> schemaArg(PyUnicode_FromString(schemaName));
    if (engineClass && schemaArg) {
        engine_.reset(
            PyObject_CallFunctionObjArgs(engineClass.get(), schemaArg.get(), nullptr));
    }
    if (!engine_) {
        logPythonError("creating Vietnamese engine");
    }
}

PythonEngine::ProcessResult PythonEngine::processKey(
    int keysym, int modifiers, bool isRelease) const {
    ProcessResult result;
    GilGuard gil;
    if (!engine_) {
        return result;
    }
    PyObjectPtr<> value(PyObject_CallMethod(
        engine_.get(), "process_key", "iii", keysym, modifiers,
        isRelease ? 1 : 0));
    if (!value) {
        logPythonError("process_key");
        return result;
    }
    if (!PyDict_Check(value.get())) {
        std::cerr << "process_key returned a non-dict value\n";
        return result;
    }
    auto getString = [&](const char *key) {
        PyObject *item = PyDict_GetItemString(value.get(), key);
        if (!item || !PyUnicode_Check(item)) return std::string{};
        const char *text = PyUnicode_AsUTF8(item);
        return text ? std::string(text) : std::string{};
    };
    PyObject *consumed = PyDict_GetItemString(value.get(), "consumed");
    PyObject *cursor = PyDict_GetItemString(value.get(), "cursor");
    result.consumed = consumed && PyObject_IsTrue(consumed);
    result.commit = getString("commit");
    result.preedit = getString("preedit");
    if (cursor && PyLong_Check(cursor)) {
        result.cursor = static_cast<int>(PyLong_AsLong(cursor));
    }
    return result;
}

std::string PythonEngine::commitText() const {
    GilGuard gil;
    if (!engine_) return {};
    PyObjectPtr<> value(
        PyObject_CallMethod(engine_.get(), "get_commit_text", nullptr));
    if (!value) {
        logPythonError("get_commit_text");
        return {};
    }
    PyObject *text = PyTuple_Check(value.get()) ? PyTuple_GetItem(value.get(), 0)
                                                : value.get();
    if (!text || !PyUnicode_Check(text)) {
        std::cerr << "get_commit_text returned an invalid value\n";
        return {};
    }
    const char *utf8 = PyUnicode_AsUTF8(text);
    return utf8 ? std::string(utf8) : std::string{};
}

void PythonEngine::applyConfig(const char *key, PyObject *value) const {
    GilGuard gil;
    if (!engine_ || !value) return;
    PyObjectPtr<> result(
        PyObject_CallMethod(engine_.get(), "set_config", "sO", key, value));
    if (!result) logPythonError("set_config");
}

void PythonEngine::setConfig(const char *key, bool value) const {
    PyObjectPtr<> pythonValue(PyBool_FromLong(value ? 1 : 0));
    applyConfig(key, pythonValue.get());
}

void PythonEngine::setConfig(const char *key, const std::string &value) const {
    PyObjectPtr<> pythonValue(PyUnicode_FromString(value.c_str()));
    applyConfig(key, pythonValue.get());
}

void PythonEngine::setSurroundingText(const std::string &text, int cursor) const {
    GilGuard gil;
    if (!engine_) return;
    PyObjectPtr<> result(PyObject_CallMethod(
        engine_.get(), "set_surrounding_text", "si", text.c_str(), cursor));
    if (!result) logPythonError("set_surrounding_text");
}

void PythonEngine::deactivate() const {
    GilGuard gil;
    if (!engine_) return;
    PyObjectPtr<> result(PyObject_CallMethod(engine_.get(), "deactivate", nullptr));
    if (!result) logPythonError("deactivate");
}

void PythonEngine::reset() const {
    GilGuard gil;
    if (!engine_) return;
    PyObjectPtr<> result(PyObject_CallMethod(engine_.get(), "reset", nullptr));
    if (!result) logPythonError("reset");
}

void PythonEngine::resetState() {
    reset();
}

std::string PythonEngine::callString(const char *name, const std::string &word,
                                     const std::string &key) const {
    GilGuard gil;
    if (!engine_) {
        return {};
    }
    PyObject *rawValue = nullptr;
    if (word.empty() && key.empty()) {
        rawValue = PyObject_CallMethod(engine_.get(), name, nullptr);
    } else if (key.empty()) {
        rawValue = PyObject_CallMethod(engine_.get(), name, "s", word.c_str());
    } else {
        rawValue = PyObject_CallMethod(engine_.get(), name, "ss", word.c_str(),
                                       key.c_str());
    }
    PyObjectPtr<> value(rawValue);
    if (value && PyUnicode_Check(value.get())) {
        const char *text = PyUnicode_AsUTF8(value.get());
        if (text) {
            return text;
        }
    } else if (!value) {
        logPythonError(name);
    } else {
        std::cerr << name << " returned a non-string value\n";
    }
    return {};
}

} // namespace vipy::python
