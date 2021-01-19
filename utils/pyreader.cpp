#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <fstream>
#include <filesystem>

#include <wx/init.h>
#include <wx/intl.h>

#include "reader.h"

static PyObject *reader_error;

static PyObject *pyreader_getlayout(PyObject *self, PyObject *args) {
    PyObject *obj_pdf_filename, *obj_code_filename;
    char *pdf_filename, *code_filename;
    if (!PyArg_ParseTuple(args, "O&O&", PyUnicode_FSConverter, &obj_pdf_filename, PyUnicode_FSConverter, &obj_code_filename)) {
        return nullptr;
    }

    pdf_filename = PyBytes_AsString(obj_pdf_filename);
    code_filename = PyBytes_AsString(obj_code_filename);

    Py_DECREF(obj_pdf_filename);
    Py_DECREF(obj_code_filename);

    try {
        pdf_document my_doc(pdf_filename);
        reader my_reader(my_doc);
        my_reader.exec_program(bytecode::read_from_file(code_filename));

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
        PyDict_SetItemString(ret, "warnings", values);
    }

    return ret;
}

static PyObject *pyreader_readpdf(PyObject *self, PyObject *args) {
    PyObject *obj_pdf_filename, *obj_code_filename;
    char *pdf_filename, *code_filename;
    if (!PyArg_ParseTuple(args, "O&O&", PyUnicode_FSConverter, &obj_pdf_filename, PyUnicode_FSConverter, &obj_code_filename)) {
        return nullptr;
    }

    pdf_filename = PyBytes_AsString(obj_pdf_filename);
    code_filename = PyBytes_AsString(obj_code_filename);

    Py_DECREF(obj_pdf_filename);
    Py_DECREF(obj_code_filename);

    try {
        pdf_document my_doc(pdf_filename);
        reader my_reader(my_doc);
        my_reader.exec_program(bytecode::read_from_file(code_filename));

        return to_pyoutput(my_reader.get_output());
    } catch (const std::exception &error) {
        PyErr_SetString(reader_error, error.what());
    }
    return nullptr;
}

static PyObject *pyreader_readpdf_autolayout(PyObject *self, PyObject *args) {
    PyObject *obj_pdf_filename, *obj_code_filename;
    char *pdf_filename, *code_filename;
    if (!PyArg_ParseTuple(args, "O&O&", PyUnicode_FSConverter, &obj_pdf_filename, PyUnicode_FSConverter, &obj_code_filename)) {
        return nullptr;
    }

    pdf_filename = PyBytes_AsString(obj_pdf_filename);
    code_filename = PyBytes_AsString(obj_code_filename);

    Py_DECREF(obj_pdf_filename);
    Py_DECREF(obj_code_filename);

    try {
        pdf_document my_doc(pdf_filename);
        reader my_reader(my_doc);
        my_reader.exec_program(bytecode::read_from_file(code_filename));

        const auto &my_output = my_reader.get_output();
        auto layout_it = my_output.globals.find("layout");
        if (layout_it != my_output.globals.end()) {
            auto layout_file = std::filesystem::path(code_filename).parent_path() / layout_it->second.str();

            my_reader.exec_program(bytecode::read_from_file(layout_file.string() + ".out"));

            return to_pyoutput(my_reader.get_output());
        } else {
            return to_pyoutput(my_output);
        }

        return to_pyoutput(my_reader.get_output());
    } catch (const std::exception &error) {
        PyErr_SetString(reader_error, error.what());
    }
    return nullptr;
}

static PyMethodDef reader_methods[] = {
    {"getlayout", pyreader_getlayout, METH_VARARGS, "Ottiene il layout"},
    {"readpdf", pyreader_readpdf, METH_VARARGS, "Esegue reader"},
    {"readpdf_autolayout", pyreader_readpdf_autolayout, METH_VARARGS, "Esegue reader con autolayout"},
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