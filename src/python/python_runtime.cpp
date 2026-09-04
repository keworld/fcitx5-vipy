#include "python_runtime.hpp"

#include "python_error.hpp"

#include <dlfcn.h>
#include <cstdlib>
#include <filesystem>

#include <iostream>

namespace vipy::python {
namespace {

std::filesystem::path userConfigRoot() {
    const char *home = std::getenv("HOME");
    const char *xdgConfigHome = std::getenv("XDG_CONFIG_HOME");
    const std::filesystem::path configHome =
        (xdgConfigHome && *xdgConfigHome)
            ? std::filesystem::path(xdgConfigHome)
            : (home && *home ? std::filesystem::path(home) / ".config"
                              : std::filesystem::path{});
    return configHome / "fcitx5-vipy";
}

void installUserDefaults(const std::filesystem::path &userRoot) {
    if (userRoot.empty() ||
        !std::filesystem::is_directory(VIPY_PYTHON_MODULE_DIR)) {
        return;
    }

    std::error_code error;
    std::filesystem::create_directories(userRoot / "script", error);
    if (error) {
        std::cerr << "Cannot create Vipy user script directory: "
                  << error.message() << '\n';
        return;
    }
    std::filesystem::create_directories(userRoot / "data", error);
    if (error) {
        std::cerr << "Cannot create Vipy user data directory: "
                  << error.message() << '\n';
        return;
    }

    for (const auto &entry :
         std::filesystem::directory_iterator(VIPY_PYTHON_MODULE_DIR, error)) {
        if (error) {
            std::cerr << "Cannot inspect Vipy system script directory: "
                      << error.message() << '\n';
            return;
        }
        if (entry.is_regular_file() && entry.path().extension() == ".py") {
            std::filesystem::copy_file(
                entry.path(), userRoot / "script" / entry.path().filename(),
                std::filesystem::copy_options::skip_existing, error);
            if (error) {
                std::cerr << "Cannot copy Vipy Python module "
                          << entry.path().filename() << ": "
                          << error.message() << '\n';
                error.clear();
            }
        }
    }

    for (const char *name : {"vietnamese.cm.dict", "vietnamese.macro"}) {
        std::filesystem::copy_file(
            std::filesystem::path(VIPY_DATA_DIR) / name,
            userRoot / "data" / name,
            std::filesystem::copy_options::skip_existing, error);
        if (error) {
            std::cerr << "Cannot copy Vipy data file " << name << ": "
                      << error.message() << '\n';
            error.clear();
        }
    }
}

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
    const auto userRoot = userConfigRoot();
    installUserDefaults(userRoot);
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
