#include <python2.7/Python.h>
#include <python2.7/frameobject.h>
#include <python2.7/longintrepr.h>

template <int PyV> struct PythonPrinter;

template <int PyV>
using python_printfunc = Elf::Addr (*)(const PyObject *pyo, const PyTypeObject *, PythonPrinter<PyV> *pc, Elf::Addr);

template <int PyV>
struct PyPrinterEntry {
    python_printfunc<PyV> printer;
    bool dupdetect;
    PyPrinterEntry(python_printfunc<PyV>, bool dupdetect);
};

template <int PyV>
struct PythonPrinter {
    void addPrinter(const char *symbol, python_printfunc<PyV> func, bool dupDetect);
    void print(Elf::Addr remoteAddr);
    std::map<Elf::Addr, PyTypeObject> types;

    PythonPrinter(Process &proc_, std::ostream &os_, const PstackOptions &);
    const char *prefix() const;
    void printStacks();
    Elf::Addr printThread(Elf::Addr);
    Elf::Addr printInterp(Elf::Addr);

    Process &proc;
    std::ostream &os;
    std::set<Elf::Addr> visited;
    mutable int depth;
    Elf::Addr interp_head;
    Elf::Object::sptr libpython;
    Elf::Addr libpythonAddr;
    std::map<Elf::Addr, PyPrinterEntry<PyV>> printers;
    PyPrinterEntry<PyV> *heapPrinter;
    const PstackOptions &options;
};
