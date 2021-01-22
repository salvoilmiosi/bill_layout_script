#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <fstream>
#include <filesystem>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include <wx/init.h>
#include <wx/intl.h>

#include "reader.h"

using namespace std::chrono_literals;
static constexpr auto TIMEOUT = 5s;

static PyObject *reader_error;
static PyObject *reader_timeout;

static std::pair<char8_t *, char8_t *> get_filenames(PyObject *args) {
    PyObject *obj_pdf_filename, *obj_code_filename;
    char8_t *pdf_filename, *code_filename;
    if (!PyArg_ParseTuple(args, "O&O&", PyUnicode_FSConverter, &obj_pdf_filename, PyUnicode_FSConverter, &obj_code_filename)) {
        return {nullptr, nullptr};
    }

    pdf_filename = reinterpret_cast<char8_t *>(PyBytes_AsString(obj_pdf_filename));
    code_filename = reinterpret_cast<char8_t *>(PyBytes_AsString(obj_code_filename));

    Py_DECREF(obj_pdf_filename);
    Py_DECREF(obj_code_filename);
    return {pdf_filename, code_filename};
}

template<typename Function, typename TimeoutFunction = decltype([]{})>
static auto timeout_wrapper(Function fun, TimeoutFunction timeout_fun = TimeoutFunction()) -> decltype(fun()) {
    using return_type = decltype(fun());
    std::mutex m;
    std::condition_variable cv;
    std::atomic<bool> running = true;

    return_type ret;

    std::thread t([&]{
        ret = fun();
        if (running) {
            cv.notify_one();
        }
    });

    t.detach();

    {
        std::unique_lock l(m);
        if (cv.wait_for(l, TIMEOUT) == std::cv_status::timeout) {
            running = false;
            timeout_fun();
            PyErr_SetString(reader_timeout, "Timeout");
            if (std::is_pointer_v<return_type>) {
                return nullptr;
            }
        }
    }

    return ret;
}

static PyObject *to_pyoutput(const reader_output &my_output) {
    auto create_output_data = [](const variable_map &map) {
        PyObject *obj = PyDict_New();

        for (auto &[key, var] : map) {
            if (key.front() == '_') continue;

            auto py_key = PyUnicode_FromString(key.c_str());
            if (PyDict_Contains(obj, py_key)) {
                PyObject *var_list = PyDict_GetItem(obj, py_key);
                PyList_Append(var_list, PyUnicode_FromString(var.str().c_str()));
            } else {
                PyObject *var_list = PyList_New(1);
                PyList_SetItem(var_list, 0, PyUnicode_FromString(var.str().c_str()));
                PyDict_SetItem(obj, py_key, var_list);
            }
        }

        return obj;
    };

    PyObject *ret = PyDict_New();

    PyDict_SetItemString(ret, "globals", create_output_data(my_output.globals));

    PyObject *values = PyList_New(0);
    for (auto &map : my_output.values) {
        PyList_Append(values, create_output_data(map));
    }
    PyDict_SetItemString(ret, "values", values);

    if (!my_output.warnings.empty()) {
        PyObject *warnings = PyList_New(0);
        for (auto &str : my_output.warnings) {
            PyList_Append(warnings, PyUnicode_FromString(str.c_str()));
        }
        PyDict_SetItemString(ret, "warnings", warnings);
    }

    if (!my_output.layout_name.empty()) {
        PyDict_SetItemString(ret, "layout", PyUnicode_FromString(my_output.layout_name.c_str()));
    }

    return ret;
}

static PyObject *pyreader_readpdf(PyObject *self, PyObject *args) {
    auto [pdf_filename, code_filename] = get_filenames(args);

    reader my_reader;

    return timeout_wrapper([&]() -> PyObject* {
        try {
            pdf_document my_doc(pdf_filename);
            my_reader.set_document(my_doc);
            my_reader.add_layout(bill_layout_script::from_file(code_filename));
            my_reader.start();

            return to_pyoutput(my_reader.get_output());
        } catch (const std::exception &error) {
            PyErr_SetString(reader_error, error.what());
            return nullptr;
        }
    }, [&] {
        my_reader.halt();
    });
}

static PyMethodDef reader_methods[] = {
    {"readpdf", pyreader_readpdf, METH_VARARGS, "Esegue reader"},
    {nullptr, nullptr, 0, nullptr}
};

static struct PyModuleDef pyreader = {
    PyModuleDef_HEAD_INIT,
    "pyreader",
    nullptr,
    -1,
    reader_methods
};

PyMODINIT_FUNC PyInit_pyreader(void) {
    int argc = 1;
    const char *argv[] = {"pyreader"};
    wxEntryStart(argc, const_cast<char **>(argv));

    static wxLocale loc = wxLANGUAGE_DEFAULT;

    PyObject *m = PyModule_Create(&pyreader);
    if (!m) return nullptr;

    reader_error = PyErr_NewException("pyreader.Error", nullptr, nullptr);
    Py_XINCREF(reader_error);
    if (PyModule_AddObject(m, "Error", reader_error) < 0) {
        Py_XDECREF(reader_error);
        Py_CLEAR(reader_error);
        Py_DECREF(m);
        return nullptr;
    }

    reader_timeout = PyErr_NewException("pyreader.Timeout", nullptr, nullptr);
    Py_XINCREF(reader_timeout);
    if (PyModule_AddObject(m, "Timeout", reader_timeout) < 0) {
        Py_XDECREF(reader_timeout);
        Py_CLEAR(reader_timeout);
        Py_DECREF(m);
        return nullptr;
    }

    wxEntryCleanup();

    return m;
}