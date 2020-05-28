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
void PythonPrinter<3>::findInterpHeadFallback() {
    libpython = nullptr;
    auto addr = proc.findSymbolByName("_PyRuntime", false);
    for (auto &o : proc.objects) {
        std::string module = stringify(*o.second->io);
        if (module.find("python") == std::string::npos)
            continue;
        auto image = o.second;

        Elf::Sym sym;
        if (!image->findSymbolByName("_PyRuntime", sym, false))
            return;
        libpython = image;
        std::clog << "python library is " << *libpython->io << std::endl;
    }
}


#include "python.tcc"

template struct PythonPrinter<3>;
