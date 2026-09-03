#include "python_runtime.hpp"

#include "python_error.hpp"

#include <dlfcn.h>

#include <iostream>

namespace vipy::python {
namespace {

void *makePythonSymbolsGlobal() {
    Dl_info info{};
    if (!dladdr(reinterpret_cast<void *>(&Py_Initialize), &info) ||
        !info.dli_fname) {
        std::cerr << "Cannot locate the loaded libpython\n";
        return nullptr;
    }
    void *handle = dlopen(info.dli_fname, RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        std::cerr << "Cannot promote libpython symbols from " << info.dli_fname
                  << ": " << dlerror() << '\n';
        return nullptr;
    }
    return handle;
}

} // namespace

PythonRuntime::PythonRuntime() : handle_(makePythonSymbolsGlobal()) {
    Py_Initialize();
    GilGuard gil;
    PyObject *path = PySys_GetObject("path");
    PyObject *insertResult =
        path ? PyObject_CallMethod(path, "insert", "is", 0,
                                   VIPY_PYTHON_MODULE_DIR)
             : nullptr;
    if (!insertResult) {
        if (path) {
            logPythonError("adding Python module path");
        } else {
            std::cerr << "Python sys.path is unavailable\n";
        }
    }
    Py_XDECREF(insertResult);
}

} // namespace vipy::python
