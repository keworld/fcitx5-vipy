#ifndef VIPY_PYTHON_RUNTIME_HPP
#define VIPY_PYTHON_RUNTIME_HPP

#include <Python.h>

namespace vipy::python {

class GilGuard {
public:
    GilGuard() : state_(PyGILState_Ensure()) {}
    ~GilGuard() { PyGILState_Release(state_); }

    GilGuard(const GilGuard &) = delete;
    GilGuard &operator=(const GilGuard &) = delete;

private:
    PyGILState_STATE state_;
};

class PythonRuntime {
public:
    PythonRuntime();
    ~PythonRuntime() = default;

    PythonRuntime(const PythonRuntime &) = delete;
    PythonRuntime &operator=(const PythonRuntime &) = delete;

private:
    // Keep libpython loaded for the addon's process lifetime.
    void *handle_ = nullptr;
};

template <typename T = PyObject>
class PyObjectPtr {
public:
    PyObjectPtr() = default;
    explicit PyObjectPtr(T *value) : value_(value) {}
    ~PyObjectPtr() { Py_XDECREF(value_); }

    PyObjectPtr(const PyObjectPtr &) = delete;
    PyObjectPtr &operator=(const PyObjectPtr &) = delete;
    PyObjectPtr(PyObjectPtr &&other) noexcept : value_(other.release()) {}
    PyObjectPtr &operator=(PyObjectPtr &&other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    T *get() const { return value_; }
    explicit operator bool() const { return value_ != nullptr; }
    T *release() {
        T *value = value_;
        value_ = nullptr;
        return value;
    }
    void reset(T *value = nullptr) {
        Py_XDECREF(value_);
        value_ = value;
    }

private:
    T *value_ = nullptr;
};

} // namespace vipy::python

#endif // VIPY_PYTHON_RUNTIME_HPP
