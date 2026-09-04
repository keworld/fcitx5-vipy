#include "python_runtime.hpp"

#include "python_error.hpp"

#include <dlfcn.h>
#include <cstdlib>
#include <filesystem>

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
    const char *home = std::getenv("HOME");
    const char *xdgConfigHome = std::getenv("XDG_CONFIG_HOME");
    const std::filesystem::path configHome =
        (xdgConfigHome && *xdgConfigHome)
            ? std::filesystem::path(xdgConfigHome)
            : (home && *home ? std::filesystem::path(home) / ".config"
                              : std::filesystem::path{});
    const auto userRoot = configHome / "fcitx5-vipy";
    const auto userScript = userRoot / "script";
    const auto userDict = userRoot / "data" / "vietnamese.cm.dict";
    const std::string dictPath =
        std::filesystem::is_regular_file(userDict)
            ? userDict.string()
            : std::string(VIPY_DATA_DIR) + "/vietnamese.cm.dict";
    setenv("FCITX_TELEX_DICT", dictPath.c_str(), 1);
    Py_Initialize();
    GilGuard gil;
    PyObject *path = PySys_GetObject("path");
    PyObject *insertResult = nullptr;
    if (path) {
        insertResult =
            PyObject_CallMethod(path, "insert", "is", 0, VIPY_PYTHON_MODULE_DIR);
        if (insertResult && std::filesystem::is_directory(userScript)) {
            Py_DECREF(insertResult);
            insertResult =
                PyObject_CallMethod(path, "insert", "is", 0, userScript.c_str());
        }
    }
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
