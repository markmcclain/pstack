#include <python3.8/Python.h>
#include <python3.8/frameobject.h>
#include <python3.8/longintrepr.h>
#include <python3.8/longintrepr.h>
#include "libpstack/python.h"

template<> std::set<const PythonTypePrinter<3> *> PythonTypePrinter<3>::all = std::set<const PythonTypePrinter<3> *>();

class BoolPrinter : public PythonTypePrinter<3> {
    Elf::Addr print(const PythonPrinter<3> *pc, const PyObject *pyo, const PyTypeObject *, Elf::Addr) const override {
        auto pio = (const _longobject *)pyo;
        pc->os << (pio->ob_digit[0] ? "True" : "False");
        return 0;
    }
    const char *type() const override { return "PyBool_Type"; }
    bool dupdetect() const override { return false; }
};
static BoolPrinter boolPrinter;

template<>
ssize_t
PythonPrinter<3>::obsize(const PyObject *pyo) const {
    return reinterpret_cast<const PyVarObject *>(pyo)->ob_size;
}

template<>
ssize_t
PythonPrinter<3>::refcnt(const PyObject *pyo) const {
    return pyo->ob_refcnt;
}

template<>
Elf::Addr
PythonPrinter<3>::obtype(const PyObject *pyo) const {
    return (Elf::Addr)pyo->ob_type;
}


#include "libpstack/python.tcc"
template struct PythonPrinter<3>;
