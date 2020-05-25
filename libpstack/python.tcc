#include <iostream>
#include <algorithm>
#include <stdlib.h>
#include <stddef.h>

#include "libpstack/dwarf.h"

// This reimplements PyCode_Addr2Line
template<int PyV> int
getLine(const Reader &proc, const PyCodeObject *code, const PyFrameObject *frame)
{
    PyVarObject lnotab;
    proc.readObj(Elf::Addr(code->co_lnotab), &lnotab);
    unsigned char linedata[lnotab.ob_size];
    proc.readObj(Elf::Addr(code->co_lnotab) + offsetof(PyBytesObject, ob_sval),
            &linedata[0], lnotab.ob_size);
    int line = code->co_firstlineno;
    int addr = 0;
    unsigned char *p = linedata;
    unsigned char *e = linedata + lnotab.ob_size;
    while (p < e) {
        addr += *p++;
        if (addr > frame->f_lasti) {
            break;
        }
        line += *p++;
    }
    return line;
}


template <int PyV> class HeapPrinter : public PythonTypePrinter<PyV> {
    Elf::Addr print(const PythonPrinter<PyV> *pc, const PyObject *, const PyTypeObject *pto, Elf::Addr remote) const override {
        pc->os << pc->proc.io->readString(Elf::Addr(pto->tp_name));
        if (pto->tp_dictoffset > 0) {
            pc->os << "\n";
            pc->depth++;
            PyObject *dictAddr;
            pc->proc.io->readObj(remote + pto->tp_dictoffset, &dictAddr);
            pc->print(Elf::Addr(dictAddr));
            pc->depth--;
            pc->os << "\n";
        }
        return 0;
    }
    const char *type() const override { return nullptr; }
    bool dupdetect() const override { return true; }
};

template <int PyV> class StringPrinter : public PythonTypePrinter<PyV> {
    Elf::Addr print(const PyObject *pyo, const PyTypeObject *, PythonPrinter<PyV> *pc, Elf::Addr) const override{
        auto *pso = (const PyBytesObject *)pyo;
        pc->os << "\"" << pso->ob_sval << "\"";
        return 0;
    }
    const char *type() const { return "PyString_Type"; }
    bool dupdetect() const { return false; }
};

template <int PyV> class FloatPrinter : public PythonTypePrinter<PyV> {
    Elf::Addr print(const PyObject *pyo, const PyTypeObject *, PythonPrinter<PyV> *pc, Elf::Addr) const override {
        auto *pfo = (const PyFloatObject *)pyo;
        pc->os << pfo->ob_fval;
        return 0;
    }
    const char *type() const { return "PyFloat_Type"; }
    bool dupdetect() const { return false; }
};


template <int PyV> class BoolPrint : public PythonTypePrinter<PyV> {
    Elf::Addr print(const PyObject *pyo, const PyTypeObject *, PythonPrinter<PyV> *pc, Elf::Addr) const override {
        auto pio = (const PyIntObject *)pyo;
        pc->os << (pio->ob_ival ? "True" : "False");
        return 0;
    }
    const char *type() { return "PyBool_Type"; }
    bool dupdetect() const override { return false; }
};

template<int PyV> class ModulePrinter : public PythonTypePrinter<PyV> {
    Elf::Addr print(const PyObject *, const PyTypeObject *, PythonPrinter<PyV> *pc, Elf::Addr) const override {
        pc->os << "<python module>";
        return 0;
    }
    const char *type() const override { return "PyModule_Type"; }
};


template<int PyV> class ListPrinter : public PythonTypePrinter<PyV> {
    Elf::Addr print(const PyObject *po, const PyTypeObject *, PythonPrinter<PyV> *pc, Elf::Addr) const override {
        auto plo = reinterpret_cast<const PyListObject *>(po);
        pc->os << "list: \n";
        auto size = std::min(plo->ob_size, Py_ssize_t(100));
        PyObject *objects[size];
        pc->proc.io->readObj(Elf::Addr(plo->ob_item), &objects[0], size);
        pc->depth++;
        for (auto addr : objects) {
          pc->os << pc->prefix();
          pc->print(Elf::Addr(addr));
          pc->os << "\n";
        }
        pc->depth--;

        pc->os << "\n";
        return 0;
    }
    const char *type() const override { return "PyList_Type"; }
    bool dupdetect() const override { return true; }
};


template<int PyV> class ClassPrinter : public PythonTypePrinter<PyV> {
    Elf::Addr classPrint(const PyObject *po, const PyTypeObject *, PythonPrinter<PyV> *pc, Elf::Addr) const override {
        auto pco = reinterpret_cast<const PyClassObject *>(po);
        pc->os << "<class ";
        pc->print(Elf::Addr(pco->cl_name));
        pc->os << ">";
        return 0;
    };
    const char *type() const override { return "PyClass_Type"; }
    bool dupdetect() const override { return true; }
};


template<int PyV> class DictPrinter : public PythonTypePrinter<PyV> {
    Elf::Addr print(const PyObject *pyo, const PyTypeObject *, PythonPrinter<PyV> *pc, Elf::Addr) {
        PyDictObject *pdo = (PyDictObject *)pyo;
        if (pdo->ma_used == 0)
            return 0;
        for (Py_ssize_t i = 0; i < pdo->ma_mask && i < 50; ++i) {
            PyDictEntry pde = pc->proc.io->template readObj<PyDictEntry>(Elf::Addr(pdo->ma_table + i));
            if (pde.me_value == nullptr)
                continue;
            if (pde.me_key != nullptr) {
                pc->os << pc->prefix();
                pc->print(Elf::Addr(pde.me_key));
                pc->os << ": ";
                pc->print(Elf::Addr(pde.me_value));
                pc->os << "\n";
            }
        }
        return 0;
    }
    const char *type() const override { return "PyDict_Type"; }
    bool dupdetect() const override { return true; }
};

template <int PyV> class TypePrinter : public PythonTypePrinter<PyV> {
    Elf::Addr print(const PyObject *pyo, const PyTypeObject *, PythonPrinter<PyV> *pc, Elf::Addr) const override {
        auto pto = (const _typeobject *)pyo;
        pc->os << "type :\"" << pc->proc.io->readString(Elf::Addr(pto->tp_name)) << "\"";
        return 0;
    }
    const char *type() const override { return "PyType_Type"; }
    bool dupdetect() const override { return true; }
};

template <int PyV> class InstancePrinter : public PythonTypePrinter<PyV> {
    Elf::Addr instancePrint(const PyObject *pyo, const PyTypeObject *, PythonPrinter<PyV> *pc, Elf::Addr) const override {
        const auto pio = reinterpret_cast<const PyInstanceObject *>(pyo);
        pc->depth++;
        pc->os << "\n" << pc->prefix() << "class: ";
        pc->depth++;
        pc->print(Elf::Addr(pio->in_class));
        pc->depth--;
        pc->os << "\n" << pc->prefix() << "dict: \n";
        pc->depth++;
        pc->print(Elf::Addr(pio->in_dict));
        pc->depth--;
        pc->depth--;
        return 0;
    }
    const char *type() const override { return "PyInstance_Type"; }
    bool dupdetect() const override { return true; }
};

template <int PyV> class LongPrinter : public PythonTypePrinter<PyV> {
    Elf::Addr longPrint(const PyObject *pyo, const PyTypeObject *, PythonPrinter<PyV> *pc, Elf::Addr) {
        auto plo = (PyLongObject *)pyo;
        intmax_t value = 0;
        for (int i = 0; i < plo->ob_size; ++i) {
            value += intmax_t(plo->ob_digit[i]) << (PyLong_SHIFT * i) ;
        }
        pc->os << value;
        return 0;
    }
    const char *type() const {
        return "PyLong_Type";
    }
};

template <int PyV>
int
printTupleVars(PythonPrinter<PyV> *pc, Elf::Addr namesAddr, Elf::Addr valuesAddr, const char *type, Py_ssize_t maxvals = 1000000)
{
    const auto &names = pc->proc.io->template readObj<PyTupleObject>(namesAddr);

    maxvals = std::min(names.ob_size, maxvals);
    if (maxvals == 0)
        return 0;

    std::vector<PyObject *> varnames(maxvals);
    std::vector<PyObject *> varvals(maxvals);

    pc->proc.io->readObj(namesAddr + offsetof(PyTupleObject, ob_item), &varnames[0], maxvals);
    pc->proc.io->readObj(valuesAddr, &varvals[0], maxvals);

    pc->os << pc->prefix() << type <<":" << std::endl;
    pc->depth++;
    for (auto i = 0; i < maxvals; ++i) {
        pc->os << pc->prefix();
        pc->print(Elf::Addr(varnames[i]));
        pc->os << "=";
        pc->print(Elf::Addr(varvals[i]));
        pc->os << "\n";
    }
    pc->depth--;
    return maxvals;

}

template <int PyV> class FramePrinter : public PythonTypePrinter<PyV> {
    Elf::Addr print(const PyObject *pyo, const PyTypeObject *, PythonPrinter<PyV> *pc, Elf::Addr remoteAddr) {
        auto pfo = (const PyFrameObject *)pyo;
        if (pfo->f_code != 0) {
            const auto &code = pc->proc.io->template readObj<PyCodeObject>(Elf::Addr(pfo->f_code));
            auto lineNo = getLine<PyV>(*pc->proc.io, &code, pfo);
            auto func = pc->proc.io->readString(Elf::Addr(code.co_name) + offsetof(PyStringObject, ob_sval));
            auto file = pc->proc.io->readString(Elf::Addr(code.co_filename) + offsetof(PyStringObject, ob_sval));
            pc->os << pc->prefix() << func << " in " << file << ":" << lineNo << "\n";

            if (pc->options[PstackOption::doargs]) {

                Elf::Addr flocals = remoteAddr + offsetof(PyFrameObject, f_localsplus);

                pc->depth++;

                printTupleVars(pc, Elf::Addr(code.co_varnames), flocals, "fastlocals", code.co_nlocals);
                flocals += code.co_nlocals * sizeof (PyObject *);

                auto cellcount = printTupleVars(pc, Elf::Addr(code.co_cellvars), flocals, "cells");
                flocals += cellcount * sizeof (PyObject *);

                printTupleVars(pc, Elf::Addr(code.co_freevars), flocals, "freevars");

                --pc->depth;
            }
        }

        if (pc->options[PstackOption::doargs] && pfo->f_locals != 0) {
            pc->depth++;
            pc->os << pc->prefix() << "locals: " << std::endl;
            pc->print(Elf::Addr(pfo->f_locals));
            pc->depth--;
        }

        return Elf::Addr(pfo->f_back);
    }
    const char *type() { return "PyFrame_Type"; }
};

template <int PyV>
const char *
PythonPrinter<PyV>::prefix() const
{
    static const char spaces[] =
        "                                           "
        "                                           "
        "                                           "
        "                                           "
        "                                           "
        "                                           ";

    return spaces + sizeof spaces - 1 - depth * 4;
}

template<int PyV>
PythonTypePrinter<PyV>::PythonTypePrinter()
{
    all.insert(this);
}

template<int PyV>
PythonTypePrinter<PyV>::~PythonTypePrinter()
{
    all.erase(this);
}


template<int PyV>
void
PythonPrinter<PyV>::printStacks()
{
    Elf::Addr ptr;
    for (proc.io->readObj(interp_head, &ptr); ptr; )
        ptr = printInterp(ptr);
}


template <int PyV> HeapPrinter<PyV> heapPrinter;
template <int PyV> StringPrinter<PyV> stringPrinter;
template <int PyV> FloatPrinter<PyV> floatPrinter;
template <int PyV> BoolPrint<PyV> boolPrinter;
template <int PyV> ModulePrinter<PyV> modulePrinter;
template <int PyV> ListPrinter<PyV> listPrinter;
template <int PyV> ClassPrinter<PyV> classPrinter;
template <int PyV> DictPrinter<PyV> dictPrinter;
template <int PyV> TypePrinter<PyV> typePrinter;
template <int PyV> InstancePrinter<PyV> instancePrinter;
template <int PyV> LongPrinter<PyV> longPrinter;
template <int PyV> FramePrinter<PyV> framePrinter;

template <int PyV>
PythonPrinter<PyV>::PythonPrinter(Process &proc_, std::ostream &os_, const PstackOptions &options_)
    : proc(proc_)
    , os(os_)
    , depth(0)
    , libpython(nullptr)
    , options(options_)
{
    // First search the ELF symbol table.
    try {
       auto interp_headp = proc.findSymbolByName("Py_interp_headp", false,
                [this](Elf::Addr loadAddr, const Elf::Object::sptr &o) {
                    libpython = o;
                    libpythonAddr = loadAddr;
                    auto name = stringify(*o->io);
                    return name.find("python") != std::string::npos;
                });
       if (verbose)
          *debug << "found interp_headp in ELF syms" << std::endl;
       proc.io->readObj(interp_headp, &interp_head);
    }
    catch (...) {
       libpython = nullptr;
       for (auto &o : proc.objects) {
           std::string module = stringify(*o.second->io);
           if (module.find("python") == std::string::npos)
               continue;
           auto dwarf = proc.imageCache.getDwarf(o.second);
           if (!dwarf)
               continue;
           for (auto u : dwarf->getUnits()) {
               // For each unit
               const auto &compile = u->root();
               if (compile.tag() != Dwarf::DW_TAG_compile_unit)
                   continue;
               // Do we have a global variable called interp_head?
               for (auto var : compile.children()) {
                   if (var.tag() == Dwarf::DW_TAG_variable && (var.name() == "interp_head" || var.name() == "Py_interp_head")) {
                       Dwarf::ExpressionStack evalStack;
                       auto location = var.attribute(Dwarf::DW_AT_location);
                       if (!location.valid())
                               throw Exception() << "no DW_AT_location for interpreter";
                       interp_head = evalStack.eval(proc, location, 0, o.first);
                       libpython = o.second;
                       libpythonAddr = o.first;
                       break;
                   }
               }
           }
       }
       if (libpython == nullptr)
           throw Exception() << "No libpython found";
       std::clog << "python library is " << *libpython->io << std::endl;
    }
}

template <int PyV>
void
PythonPrinter<PyV>::print(Elf::Addr remoteAddr) const {
    if (depth > 10000) {
        os << "too deep" << std::endl;
        return;
    }
    depth++;
    PyVarObject baseObj;
    try {
        while (remoteAddr) {
            proc.io->readObj<PyVarObject> (remoteAddr, &baseObj);
            if (baseObj.ob_refcnt == 0) {
                os << "(dead object)";
            }

            const PythonTypePrinter<PyV> *printer = printers.at(Elf::Addr(baseObj.ob_type));

            bool isNew = (types.find(Elf::Addr(baseObj.ob_type)) == types.end());
            auto &pto = types[Elf::Addr(baseObj.ob_type)];
            if (isNew) {
                proc.io->readObj(Elf::Addr(Elf::Addr(baseObj.ob_type)), &pto);
            }

            if (printer == 0) {
                std::string tn;
                tn = proc.io->readString(Elf::Addr(pto->tp_name));
                if (tn == "NoneType") {
                    os << "None";
                    break;
                } else if (printer == 0 && (pto->tp_flags & Py_TPFLAGS_HEAPTYPE)) {
                    static HeapPrinter<2> heapPrinter;
                    printer = &heapPrinter;
                } else {
                    os <<  remoteAddr << " unprintable-type-" << tn << "@"<< Elf::Addr(baseObj.ob_type);
                    break;
                }
            }

            if (printer->dupdetect() && visited.find(remoteAddr ) != visited.end()) {
                os << "(already seen)";
                break;
            }

            if (printer->dupdetect())
                visited.insert(remoteAddr);

            size_t size = pto->tp_basicsize;
            size_t itemsize = pto->tp_itemsize;
            ssize_t fullSize;
            if (itemsize != 0) {
                // object is a variable length object:
                if (baseObj.ob_size > 65536 || baseObj.ob_size < 0) {
                    os << "(skip massive object " << baseObj.ob_size << ")";
                    break;
                }
                fullSize = size + itemsize * baseObj.ob_size;
            } else {
                fullSize = size;
            }
            char buf[fullSize];
            proc.io->readObj(remoteAddr, buf, fullSize);
            remoteAddr = printer->print(this, (const PyObject *)buf, pto, remoteAddr);
        }
    }
    catch (...) {
        os <<  "(print failed)";
    }
    --depth;
}

/*
 * process one python thread in an interpreter, at remote addr "ptr". 
 * returns the address of the next thread on the list.
 */
template <int PyV>
Elf::Addr
PythonPrinter<PyV>::printThread(Elf::Addr ptr)
{
    PyThreadState thread;
    proc.io->readObj(ptr, &thread);
    size_t toff;
    if (thread.thread_id && pthreadTidOffset(proc, &toff)) {
        Elf::Addr tidptr = thread.thread_id + toff;
        pid_t tid;
        proc.io->readObj(tidptr, &tid);
        os << "pthread: 0x" << std::hex << thread.thread_id << std::dec << ", lwp " << tid;
    } else {
        os << "anonymous thread";
    }
    os << "\n";
    print(Elf::Addr(thread.frame));
    return Elf::Addr(thread.next);
}

/*
 * Process one python interpreter in the process at remote address ptr
 * Returns the address of the next interpreter on on the process's list.
 */
template <int PyV>
Elf::Addr
PythonPrinter<PyV>::printInterp(Elf::Addr ptr)
{
    PyInterpreterState state;
    proc.io->readObj(ptr, &state);
    os << "---- interpreter @" << std::hex << ptr << std::dec << " -----" << std::endl ;
    for (Elf::Addr tsp = reinterpret_cast<Elf::Addr>(state.tstate_head); tsp; ) {
        tsp = printThread(tsp);
        os << std::endl;
    }
    return reinterpret_cast<Elf::Addr>(state.next);
}
