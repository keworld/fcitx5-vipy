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

std::string PythonEngine::feedKey(const std::string &key) const {
    GilGuard gil;
    if (!engine_) {
        return {};
    }
    PyObjectPtr<> value(
        PyObject_CallMethod(engine_.get(), "feed", "s", key.c_str()));
    if (!value) {
        logPythonError("feed");
        return {};
    }
    return getWord();
}

std::string PythonEngine::getWord() const {
    return callString("get_word");
}

std::string PythonEngine::commitCurrent() {
    return callString("commit");
}

void PythonEngine::resetState() {
    setSchema(currentMethod_);
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
