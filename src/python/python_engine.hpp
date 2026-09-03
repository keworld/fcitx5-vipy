#ifndef VIPY_PYTHON_ENGINE_HPP
#define VIPY_PYTHON_ENGINE_HPP

#include "python_runtime.hpp"
#include "input_method.hpp"

#include <string>

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
    std::string commitText() const;
    void setConfig(const char *key, bool value) const;
    void setConfig(const char *key, const std::string &value) const;
    void setSurroundingText(const std::string &text, int cursor) const;
    void deactivate() const;
    void reset() const;
    void resetState();

private:
    std::string callString(const char *name, const std::string &word = {},
                           const std::string &key = {}) const;
    void applyConfig(const char *key, PyObject *value) const;

    PythonRuntime &runtime_;
    PyObjectPtr<> module_;
    PyObjectPtr<> engine_;
    vipy::InputMethod currentMethod_ = vipy::InputMethod::Telex;
};

} // namespace vipy::python

#endif // VIPY_PYTHON_ENGINE_HPP
