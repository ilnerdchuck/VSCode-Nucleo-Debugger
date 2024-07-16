import re
import sys
import gdb
import gdb.printing
import struct
import fcntl
import termios
from gdb.FrameDecorator import FrameDecorator

# cache some types
des_proc_type = gdb.lookup_type('des_proc')
des_proc_ptr_type = gdb.Type.pointer(des_proc_type)
richiesta_type = gdb.lookup_type('richiesta')
richiesta_ptr_type = gdb.Type.pointer(richiesta_type)
des_sem_type = gdb.lookup_type('des_sem')
des_sem_p = gdb.Type.pointer(des_sem_type)

# which des_proc fields we should show
des_proc_std_fields = [ None, 'id', 'cr3', 'contesto', 'livello', 'precedenza', 'puntatore', 'punt_nucleo', 'corpo', 'parametro' ]
toshow = [ f for f in des_proc_type.fields() if f.name not in des_proc_std_fields ]

# cache the vdf
vdf = gdb.parse_and_eval("vdf");

# cache some constants
max_liv  = int(gdb.parse_and_eval('$MAX_LIV'))
max_sem = int(gdb.parse_and_eval('$MAX_SEM'))
sc_desc  = int(gdb.parse_and_eval('$SEL_CODICE_SISTEMA'))
uc_desc  = int(gdb.parse_and_eval('$SEL_CODICE_UTENTE'))
ud_desc  = int(gdb.parse_and_eval('$SEL_DATI_UTENTE'))
max_proc  = int(gdb.parse_and_eval('$MAX_PROC'))
max_prio = int(gdb.parse_and_eval('$MAX_PRIORITY'))
min_prio = int(gdb.parse_and_eval('$MIN_PRIORITY'))
dummy_prio = int(gdb.parse_and_eval('DUMMY_PRIORITY'))
m_parts = [ 'sis_c', 'sis_p', 'mio_c', 'utn_c', 'utn_p' ]
m_ini = [ int(gdb.parse_and_eval('$I_' + x.upper())) for x in m_parts ]
m_names = []
for i, p in enumerate(m_parts):
    tr = { 'sis': 'sistema', 'mio': 'IO', 'utn': 'utente' }
    r, c = m_parts[i].split('_')
    m_names.append(tr[r] + "/" + ('condiviso' if c == 'c' else 'privato'))
m_ini.append(256)

def is_curproc(p):
    """true iff p is the current process"""
    return p['id'] == gdb.parse_and_eval('esecuzione->id')

def get_process(pid):
    """convert from pid to des_proc *"""
    p = gdb.parse_and_eval('proc_table[{}]'.format(pid))
    if p == gdb.Value(0):
        return None
    return p

N_M1 = 0
N_FRAME = 0
def get_frames():
    global N_M1, N_FRAME
    if not N_M1:
        N_M1 = int(gdb.parse_and_eval('N_M1'))
        N_FRAME = int(gdb.parse_and_eval('N_FRAME'))

def dump_corpo(proc):
    c = proc['corpo'];
    if not c:
        return ''
    return "{}:{}({})".format(*resolve_function(toi(c))[::-1], toi(proc['parametro']))

def process_dump(proc, indent=0, verbosity=3):
    write_key("livello", colorize('col_usermode', "utente") if proc['livello'] == gdb.Value(3) else colorize('col_sysmode', "sistema"), indent)
    write_key("corpo", dump_corpo(proc), indent)
    vstack = toi(proc['contesto'][4])
    stack = v2p(toi(proc['cr3']), vstack)
    if (verbosity > 2):
        gdb.write(colorize('col_proc_hdr', "-- pila sistema ({:016x} \u279e {:x}):\n".format(vstack, stack)), indent)
    rip = readfis(stack)
    rip_s = "{}".format(gdb.Value(rip).cast(void_ptr_type)).split()
    write_key("rip", "{:>18s} {}".format(rip_s[0], " ".join(rip_s[1:])), indent)
    if (verbosity > 2):
        write_key("cs",  dump_selector(readfis(stack + 8)), indent)
        write_key("rflags", dump_flags(readfis(stack + 16)), indent)
        write_key("rsp", "{:#18x}".format(readfis(stack + 24)), indent)
        write_key("ss",  dump_selector(readfis(stack + 32)), indent)
        gdb.write(colorize('col_proc_hdr', "-- contesto:\n"), indent)
        for i, r in enumerate(registers):
            write_key(r, hex(toi(proc['contesto'][i])), indent)
        cr3 = toi(proc['cr3'])
        write_key("cr3", vm_paddr_to_str(cr3), indent)
        gdb.write(colorize('col_proc_hdr', "-- prossima istruzione:\n"), indent)
        show_lines(gdb.find_pc_line(rip), indent)
    if len(toshow) > 0:
        if verbosity > 2:
            gdb.write("\x1b[33m-- campi aggiuntivi:\x1b[m\n", indent)
        for f in toshow:
            write_key(f.name, proc[f], indent)

def process_list(t='all'):
    for pid in range(max_proc):
        p = get_process(pid)
        if p is None:
            continue
        proc = p.dereference()
        if t == "user" and proc['livello'] != gdb.Value(3):
            continue
        if t == "system" and proc['livello'] == gdb.Value(3):
            continue
        yield (pid, proc)

def parse_process(a):
        if not a:
            a = 'esecuzione->id'
        _pid = gdb.parse_and_eval(a)
        pid = 0
        try:
            pid = int(_pid)
        except:
            pass
        if pid != 0xFFFFFFFF:
            p = get_process(pid)
            if p is None:
                return None
            p = p.dereference()
        elif _pid.type == des_proc_ptr_type:
            p = pid.dereference()
        elif _pid.type == des_proc_type:
            p = pid
        else:
            raise TypeError("expression must be a (pointer to) des_proc or a process id")
        return p

class Process(gdb.Command):
    """info about processes"""

    def __init__(self):
        super(Process, self).__init__("process", gdb.COMMAND_DATA, prefix=True)

class ProcessDump(gdb.Command):
    """show information from the des_proc of a process.
The argument can be any expression returning a process id or a des_proc*.
If no arguments are given, 'esecuzione->id' is assumed."""
    def __init__(self):
        super(ProcessDump, self).__init__("process dump", gdb.COMMAND_DATA, gdb.COMPLETE_EXPRESSION)

    def invoke(self, arg, from_tty):
        p = parse_process(arg)
        if not p:
            raise gdb.GdbError("no such process")
        process_dump(p)

class ProcessList(gdb.Command):
    """list existing processes
The command accepts an optional argument which may be 'system'
(show only system processes), 'user' (show only user processes)
or 'all' (default, show all processes)."""

    def __init__(self):
        super(ProcessList, self).__init__("process list", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        for pid, proc in process_list(arg):
            gdb.write("==> Processo {}\n".format(pid))
            process_dump(proc, indent=4, verbosity=0)

    def complete(self, text, word):
        return [ w for w in [ 'all', 'user', 'system'] if w.startswith(word) ]

Process()
ProcessDump()
ProcessList()

class DesProc(gdb.Function):
    """return a pointer to the des_proc, given the process id"""

    def __init__(self):
        super(DesProc, self).__init__("des_p")

    def invoke(self, pid):
        p = get_process(pid)
        if p is None:
            return gdb.Value(0)
        return p

DesProc()

def sem_list(cond='all'):
    sem = gdb.parse_and_eval("sem_allocati_utente")
    for i in range(sem):
        s = gdb.parse_and_eval("array_dess[{}]".format(i))
        if cond == 'waiting' and s['pointer'] == gdb.Value(0):
            continue
        yield (i, s)
    sem = gdb.parse_and_eval("sem_allocati_sistema")
    for i in range(sem):
        s = gdb.parse_and_eval("array_dess[{}]".format(i + max_sem))
        if cond == 'waiting' and s['pointer'] == gdb.Value(0):
            continue
        yield (i + max_sem, s)

class Semaphore(gdb.Command):
    """show the status of semaphores.
By default, show the status of all allocated semaphores.
With 'waiting' as argument, show only the semaphores with a non-empty waiting queue."""

    def __init__(self):
        super(Semaphore, self).__init__("semaphore", gdb.COMMAND_DATA, prefix=True)

    def invoke(self, arg, from_tty):
        for i, s in sem_list(arg):
            gdb.write(colorize('col_var', "sem[") +
                      colorize('col_index', format(i, '5d')) +
                      colorize('col_var', "]: ") +
                      str(s) + "\n")

    def complete(self, text, word):
        return 'waiting' if 'waiting'.startswith(word) else None

Semaphore()

class NucleoFrameIterator():

    def __init__(self, frame_iter):
        self.frame_iter = frame_iter

    def __iter__(self):
        return self

    def __next__(self):
        f = self.frame_iter.__next__()
        if f.address() == 0:
            raise StopIteration
        return f

class NucleoFrameFilter:

    def __init__(self):
        self.name = "nucleo"
        self.enabled = True
        self.priority = 100
        gdb.frame_filters[self.name] = self

    def filter(self, frame_iter):
        return NucleoFrameIterator(frame_iter)

NucleoFrameFilter()

code_proc = []
class Coda_esecuzione:
    def __init__(self):
        code_proc.append(self)

    def show_waiting(self):
        gdb.write(colorize('col_var', "esecuzione: ") + show_list(gdb.parse_and_eval("esecuzione"), 'puntatore', nmax=1, vis=proc_elem) + "\n")
Coda_esecuzione()

class Coda_pronti:
    def __init__(self):
        code_proc.append(self)

    def show_waiting(self):
        gdb.write(colorize('col_var', "pronti:     ") + show_list(gdb.parse_and_eval("pronti"), 'puntatore', vis=proc_elem) + "\n")
Coda_pronti()

class Code_semafori:
    def __init__(self):
        code_proc.append(self)

    def show_waiting(self):
        gdb.execute("semaphore waiting")
Code_semafori()

class Coda_sospesi:
    def __init__(self):
        code_proc.append(self)

    def show_waiting(self):
        gdb.write(colorize('col_var', "sospesi:  ") + show_list(gdb.parse_and_eval("sospesi"), 'p_rich') + "\n")
Coda_sospesi()

def context_code():
    print_hdr("code processi")
    gdb.write(colorize('col_var', 'processi:   {:d}\n'.format(int(gdb.parse_and_eval('processi')))))
    for c in code_proc:
        c.show_waiting()

def context_esecuzione():
    esecuzione = gdb.parse_and_eval('esecuzione')
    if esecuzione:
        print_hdr("esecuzione")
        gdb.write('{}\n'.format(esecuzione))

def print_context():
    get_frames()
    get_row_col()
    context_backtrace()
    context_sorgente()
    context_code()
    context_esecuzione()
    context_protezione()
    print_footer()

def show_list(head, link, nmax=20, trunk='...', vis=None):
    elems, p = [], head
    count = 0
    seen = set()
    if vis == None:
        vis = str
    while p != gdb.Value(0):
        if count >= nmax:
            elems.append(trunk)
            break
        elem = p.dereference()
        p = elem[link]
        elems.append(vis(elem))
        pp = toi(p)
        if pp in seen:
            elems.append('LOOP!')
            break
        seen.add(pp)
        count += 1
    return ' \u279e '.join(elems)

def proc_elem(proc):
    """Convert a proc to a string suitable for show_list"""
    prio = int(proc['precedenza'])
    if prio == max_prio:
        prio_str = 'MAX_PRIO'
    elif prio == min_prio:
        prio_str = 'MIN_PRIO'
    elif prio == dummy_prio:
        prio_str = 'DUMMY'
    else:
        prio_str = str(prio)
    i = toi(proc['id'])
    if i == 0xFFFF:
        id_str = '0xFFFF'
    else:
        id_str = str(i)
    return colorize('col_proc_elem', "[{}, {}]".format(id_str, prio_str))

class richiestaPrinter:
    """Print a richiesta list"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        return "{{{}, {}}}".format(toi(self.val['d_attesa']), show_list(self.val['pp'], 'puntatore', nmax=1, vis=proc_elem))

def richiestaLookup(val):
    if val.type == richiesta_type:
        return richiestaPrinter(val)
    elif val.type == richiesta_ptr_type:
        return ptrPrinter(val)
    return None

gdb.pretty_printers.append(richiestaLookup)

class des_semPrinter:
    """Print a des_sem"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        return "{{ {}, {} }}".format(self.val['counter'], show_list(self.val['pointer'], 'puntatore', vis=proc_elem))

def des_semLookup(val):
    if val.type == des_sem_type:
        return des_semPrinter(val)
    elif val.type == des_sem_p:
        return ptrPrinter(val)
    return None

gdb.pretty_printers.append(des_semLookup)

class paddrPrinter:
    """print a paddr"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        return vm_paddr_to_str(self.val)

def paddrLookup(val):
    if str(val.type.unqualified()) == 'paddr':
        return paddrPrinter(toi(val))
    return None

gdb.pretty_printers.append(paddrLookup)

class des_procPrinter:
    """print a des_proc *"""

    recurse_level = 0

    def __init__(self, val):
        self.val = val

    def to_string(self):
        self.__class__.recurse_level += 1
        s = ''
        try:
            s = self._to_string()
        except:
            pass
        self.__class__.recurse_level -= 1
        return s

    def _to_string(self):
        if self.val == gdb.Value(0):
            return 'null'
        try:
            proc = self.val.dereference()
        except:
            return '{:x} invalid'.format(toi(self.val))
        if self.__class__.recurse_level > 2:
            return '{:x} ...'.format(toi(self.val))
        s = 'id: {}, corpo: "{}", prec: {}, rax: {}'.format(proc['id'], dump_corpo(proc), proc['precedenza'], proc['contesto'][0])
        for f in toshow:
            s += ', {}: {}'.format(f.name, proc[f])
        return '{:x} \u279e {{ {} }}'.format(toi(self.val), s)

def des_procLookup(val):
    if val.type == des_proc_ptr_type:
        return des_procPrinter(val)
    return None

gdb.pretty_printers.append(des_procLookup)

class A_p(gdb.Command):
    """show the contents of the a_p array"""

    def __init__(self):
        super(A_p, self).__init__("a_p", gdb.COMMAND_DATA, prefix=False)

    def invoke(self, arg, from_tty):
        a_p = gdb.parse_and_eval('a_p')
        for i in range(24):
            if not a_p[i]:
                continue
            s = 'DRIVER'
            if a_p[i] != 1:
                s = "proc {}".format(a_p[i])
            gdb.write("[{:2d}] {}\n".format(i, s))
A_p()
