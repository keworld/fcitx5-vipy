#include "python_error.hpp"

#include <Python.h>

#include <iostream>
#include <string>

namespace vipy::python {

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
    std::cerr << context << ": " << message << '\n';
    Py_XDECREF(type);
    Py_XDECREF(value);
    Py_XDECREF(traceback);
}

} // namespace vipy::python
