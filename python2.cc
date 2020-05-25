#include <python2.7/Python.h>
#include <python2.7/frameobject.h>
#include <python2.7/longintrepr.h>
#include "libpstack/python.h"
template<> std::set<const PythonTypePrinter<2> *> PythonTypePrinter<2>::all = std::set<const PythonTypePrinter<2> *>();

class IntPrint : public PythonTypePrinter<2> {
    Elf::Addr print(const PythonPrinter<2> *pc, const PyObject *pyo, const PyTypeObject *, Elf::Addr) const override {
        auto pio = (const PyIntObject *)pyo;
        pc->os << pio->ob_ival;
        return 0;
    }
    const char *type() const override { return "PyInt_Type"; }
    bool dupdetect() const override { return false; }

};
static IntPrint intPrinter;

#include "libpstack/python.tcc"

template struct PythonPrinter<2>;
