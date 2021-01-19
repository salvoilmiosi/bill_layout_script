#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <fstream>
#include <wx/init.h>
#include <wx/intl.h>

#include "reader.h"

static PyObject *reader_error;

static PyObject *pyreader_getlayout(PyObject *self, PyObject *args) {
    const char *pdf_filename;
    const char *code_filename;
    PyArg_ParseTuple(args, "ss", &pdf_filename, &code_filename);

    try {
        pdf_document my_doc(pdf_filename);
        reader my_reader(my_doc);
        bytecode my_code;
        std::ifstream ifs(code_filename, std::ios::binary | std::ios::in);
        my_code.read_bytecode(ifs);
        my_reader.exec_program(my_code);

        const auto &my_output = my_reader.get_output();
        auto layout_it = my_output.globals.find("layout");
        if (layout_it != my_output.globals.end()) {
            return PyUnicode_FromString(layout_it->second.str().c_str());
        } else {
            Py_RETURN_NONE;
        }
    } catch (const std::exception &error) {
        PyErr_SetString(reader_error, error.what());
        return nullptr;
    }
}

static PyObject *pyreader_readpdf(PyObject *self, PyObject *args) {
    const char *pdf_filename;
    const char *code_filename;
    PyArg_ParseTuple(args, "ss", &pdf_filename, &code_filename);

    PyObject *ret = PyDict_New();

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

    try {
        pdf_document my_doc(pdf_filename);
        reader my_reader(my_doc);
        bytecode my_code;
        std::ifstream ifs(code_filename, std::ios::binary | std::ios::in);
        my_code.read_bytecode(ifs);
        my_reader.exec_program(my_code);

        const auto &my_output = my_reader.get_output();

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
            PyDict_SetItemString(ret, "warnings", values);
        }
    } catch (const std::exception &error) {
        PyErr_SetString(reader_error, error.what());
        PyMem_Free(ret);
        return nullptr;
    }

    return ret;
}

static PyMethodDef reader_methods[] = {
    {"getlayout", pyreader_getlayout, METH_VARARGS, "Ottiene il layout"},
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

    reader_error = PyErr_NewException("pyreader.error", nullptr, nullptr);
    Py_XINCREF(reader_error);
    if (PyModule_AddObject(m, "error", reader_error) < 0) {
        Py_XDECREF(reader_error);
        Py_CLEAR(reader_error);
        Py_DECREF(m);
        return nullptr;
    }

    wxEntryCleanup();

    return m;
}