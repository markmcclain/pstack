#include "libpstack/elf.h"
#include "libpstack/proc.h"
template <int PyV> struct PythonPrinter;

struct _object;
struct _typeobject;


template <int V>
class PythonTypePrinter {
public:
    virtual Elf::Addr print(const PythonPrinter<V> *, const _object *, const _typeobject *, Elf::Addr addr) const = 0;
    virtual bool dupdetect() const { return true; }
    virtual const char * type() const = 0;
    PythonTypePrinter();
    ~PythonTypePrinter();
    static std::set<const PythonTypePrinter *> all;
};

template <int PyV>
using python_printfunc = Elf::Addr (*)(const _object *pyo, const _typeobject *, PythonPrinter<PyV> *pc, Elf::Addr);

template <int PyV>
struct PythonPrinter {
    void print(Elf::Addr remoteAddr) const;
    struct freetype {
        void operator()(_typeobject *to) {
            free(to);
        }
    };
    mutable std::map<Elf::Addr, std::unique_ptr<_typeobject, freetype>> types;

    PythonPrinter(Process &proc_, std::ostream &os_, const PstackOptions &);
    const char *prefix() const;
    void printStacks();
    Elf::Addr printThread(Elf::Addr);
    Elf::Addr printInterp(Elf::Addr);

    Process &proc;
    std::ostream &os;
    mutable std::set<Elf::Addr> visited;
    mutable int depth;
    Elf::Addr interp_head;
    Elf::Object::sptr libpython;
    Elf::Addr libpythonAddr;
    const PstackOptions &options;
    std::map<Elf::Addr, const PythonTypePrinter<PyV> *> printers;
    void findInterpreter();
};
bool pthreadTidOffset(const Process &proc, size_t *offsetp);
