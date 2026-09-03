#ifndef VIPY_PYTHON_ENGINE_HPP
#define VIPY_PYTHON_ENGINE_HPP

#include "python_runtime.hpp"
#include "input_method.hpp"

#include <string>

namespace vipy::python {

class PythonEngine {
public:
    explicit PythonEngine(PythonRuntime &runtime);
    ~PythonEngine();

    PythonEngine(const PythonEngine &) = delete;
    PythonEngine &operator=(const PythonEngine &) = delete;

    void setSchema(vipy::InputMethod method);
    std::string feedKey(const std::string &key) const;
    std::string getWord() const;
    std::string commitCurrent();
    void resetState();

private:
    std::string callString(const char *name, const std::string &word = {},
                           const std::string &key = {}) const;

    PythonRuntime &runtime_;
    PyObjectPtr<> module_;
    PyObjectPtr<> engine_;
    vipy::InputMethod currentMethod_ = vipy::InputMethod::Telex;
};

} // namespace vipy::python

#endif // VIPY_PYTHON_ENGINE_HPP
