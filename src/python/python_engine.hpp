#ifndef VIPY_PYTHON_ENGINE_HPP
#define VIPY_PYTHON_ENGINE_HPP

#include "python_runtime.hpp"
#include "input_method.hpp"

#include <string>
#include <unordered_map>

namespace vipy::python {

class PythonEngine {
public:
    struct ProcessResult {
        bool consumed = false;
        std::string commit;
        std::string preedit;
        int cursor = 0;
    };

    explicit PythonEngine(PythonRuntime &runtime);
    ~PythonEngine();

    PythonEngine(const PythonEngine &) = delete;
    PythonEngine &operator=(const PythonEngine &) = delete;

    void setSchema(vipy::InputMethod method);
    ProcessResult processKey(const std::string &key, int modifiers,
                             bool isRelease) const;
    ProcessResult processKey(const void *context, const std::string &key,
                             int modifiers, bool isRelease) const;
    std::string commitText() const;
    std::string commitText(const void *context) const;
    void setConfig(const char *key, bool value) const;
    void setConfig(const char *key, const std::string &value) const;
    void setSurroundingText(const std::string &text, int cursor) const;
    void deactivate() const;
    void deactivate(const void *context) const;
    void reset() const;
    void resetState() const;
    void resetState(const void *context) const;
    void removeContext(const void *context) const;

private:
    PyObject *engineFor(const void *context) const;
    PyObject *createEngine() const;
    std::string callString(const char *name, const std::string &word = {},
                           const std::string &key = {}) const;
    void applyConfig(const char *key, PyObject *value) const;
    void applyConfig(PyObject *engine, const char *key, PyObject *value) const;

    PythonRuntime &runtime_;
    PyObjectPtr<> module_;
    mutable std::unordered_map<const void *, PyObjectPtr<>> engines_;
    vipy::InputMethod currentMethod_ = vipy::InputMethod::Telex;
    mutable bool enableLoneW_ = true;
    mutable bool enableSpellCheck_ = true;
    mutable bool enableMacro_ = true;
    mutable bool enableAutoDecompose_ = true;
    mutable std::string macroFile_;
};

} // namespace vipy::python

#endif // VIPY_PYTHON_ENGINE_HPP
