# I moduli usano libce, quindi dobbiamo sapere dove si trovano libce.h e
# libce64.a. La libreria installa un file libce.conf al cui interno sono
# definite alcune variabili, tra cui: CE_INCLUDE (path della directory che
# contiene libce.h) e CE_LIB64 (path della directory che contiene libce64.a).
#
# La variabile  di ambiente LIBCECONF deve contenere il path del file libce.conf.
# Se LIBCECONF non è già definita proviamo a cercare il file nei path più comuni.
# L'installazione di default di libce scrive il file in $HOME/CE/etc/libce.conf,
# ma il sito dell'autocorrezione usa altri path.
LIBCECONF ?= $(firstword 			\
	$(wildcard /usr/local/etc/libce.conf)	\
	$(wildcard /etc/libce.conf)		\
	$(wildcard $(HOME)/CE/etc/libce.conf))
# se LIBCECONF è rimasta vuota non possiamo proseguire
$(if $(LIBCECONF),,$(error libce non trovata))
# altrimenti includiamo libce.conf
include $(LIBCECONF)

# indirizzo di partenza del modulo sistema, fissato a 2MiB
START_SISTEMA=   0x0000000000200000
# START_IO e START_UTENTE dipendono dalla suddivisione della memoria virtuale
# definita in costanti.h. Invece di calcolarli a priori, compiliamo un piccolo
# programma (util/mkstart.cpp) che include costanti.h e scrive la definizione
# delle due variabili in util/start.mk, che includiamo qui.
-include util/start.mk

# alcuni esercizi definiscono ulteriori variabili di compilazione
-include conf/conf.mk

# compilatore e linker per i moduli sistema, io e utente
# (se non diversamente specificato, usiamo quelli selezionati da libce)
NCC ?= $(shell echo $(CE_CXX64))
NLD ?= $(shell echo $(CE_LD64))

# Opzioni per il compilatore:
#
# * -Wall:		massimo livello di warning
# * -Iinclude:		cerca file .h nella directory 'include'
# * -I$(CE_INCLUDE):	path della directory che contiene libce.h
#
# * disabilitiamo alcune funzionalità del C++ che richiedono un supporto
#   runtime che non abbiamo implementato:
# 	* -fno-exceptions:	eccezioni
# 	* -fno-rtti:		Run Time Type Information
#
# * disabilitiamo alcune protezioni del codice che o richiedono un supporto
#   runtime, o rendono il codice assembler più confuso
#   	* -fno-stack-protector:	stack canaries
#   	* -fcf-protection=none:	control-flow protection
#
# * disabilitiamo alcune ottimizzazioni che rendono il codice più difficile
#   da debuggare, oppure non possono essere usate in codice di livello sistema
#   	* -fno-omit-frame-pointer:	usa rbp per contenere il frame pointer
#   	* -mno-red-zone:		non usare la Red Zone
#   	* -mno-sse:			non usare le istruzioni SSE
#
# * -g3:		includi le informazioni per il debugger
#
# * -std=c++17		usa lo standard C++17 [per new(val_align_t)]
#
# * -ffreestanding	non assumere che esista la libreria standard
#
# La variabile di ambiente CFLAGS può essere usata per passare ulteriori argomenti.
NCFLAGS=			\
	-Wall 			\
	-Iinclude		\
	-I$(CE_INCLUDE)		\
	-fno-exceptions 	\
	-fno-rtti 		\
	-fno-stack-protector 	\
	-fcf-protection=none	\
	-fno-omit-frame-pointer \
	-mno-red-zone		\
	-mno-sse		\
	-g3			\
	-std=c++17		\
	-ffreestanding		\
	$(CFLAGS)

# se è definita la variabile di ambiente AUTOCORR definiamo la macro AUTOCORR,
# che permette di eseguire il codice in maniera non interattiva. In
# particolare, la funzione pause() viene trasformata in una NOP e i messaggi
# scritti sul video vengono inviati sul log, con livello USR.
ifdef AUTOCORR
	NCFLAGS+=-DAUTOCORR
endif

# Opzioni per il collegatore:
#
# * -nostdlib:		non collegare le librerie standard
# * -L$(LIB64):		path della directory che contiene libce64.a
#
# La variabile di ambiente LDFLAGS può essere usata per passare ulteriori argomenti.
NLDFLAGS=			\
        -nostdlib		\
	-L$(CE_LIB64)		\
	$(LDFLAGS)

# Librerie da collegare con i vari moduli:
#
# * - lce64:		libce64.a
#
# La variabile di ambiente LDLIBS può essere usata per collegare ulteriori librerie.
NLDLIBS=			\
	-lce64			\
	$(LDLIBS)

# target di default del Makefile
all: 				\
     build/sistema.strip 	\
     build/io.strip 		\
     build/utente.strip 	\
     .gdbinit

# regole per la costruzione dei vari moduli

build/sistema: sistema/sist_s.o sistema/sist_cpp.o
	$(NLD) $(NLDFLAGS) -o build/sistema -Ttext $(START_SISTEMA) sistema/sist_s.o sistema/sist_cpp.o $(NLDLIBS)

build/io: io/io_s.o io/io_cpp.o
	$(NLD) $(NLDFLAGS) -o build/io -Ttext $(START_IO) io/io_s.o io/io_cpp.o $(NLDLIBS)

build/utente: utente/uten_s.o utente/lib.o utente/uten_cpp.o
	$(NLD) $(NLDFLAGS) -o build/utente -Ttext $(START_UTENTE) utente/uten_cpp.o utente/uten_s.o utente/lib.o $(NLDLIBS)

build/%.strip: build/%
	$(CE_STRIP) -s $^ -o $@

# compilazione di sistema.s e sistema.cpp
sistema/sist_s.o: sistema/sistema.s include/costanti.h
	$(NCC) $(NCFLAGS) -x assembler-with-cpp -c sistema/sistema.s -o sistema/sist_s.o

sistema/sist_cpp.o: sistema/sistema.cpp include/costanti.h include/sys.h include/sysio.h
	$(NCC) $(NCFLAGS) -c sistema/sistema.cpp -o sistema/sist_cpp.o

# compilazione di io.s e io.cpp
io/io_s.o: io/io.s include/costanti.h
	$(NCC) $(NCFLAGS) -x assembler-with-cpp -c io/io.s -o io/io_s.o

io/io_cpp.o: io/io.cpp include/costanti.h include/sysio.h include/sys.h include/io.h
	$(NCC) $(NCFLAGS) -fpic -c io/io.cpp -o io/io_cpp.o

# compilazione di utente.s e utente.cpp
utente/uten_s.o: utente/utente.s include/costanti.h
	$(NCC) $(NCFLAGS) -x assembler-with-cpp -c utente/utente.s -o utente/uten_s.o

utente/uten_cpp.o: utente/utente.cpp include/costanti.h include/sys.h include/io.h utente/lib.h
	$(NCC) $(NCFLAGS) -fpic -Iutente -c utente/utente.cpp -o utente/uten_cpp.o

utente/lib.o: utente/lib.cpp include/costanti.h include/sys.h include/io.h utente/lib.h
	$(NCC) $(NCFLAGS) -fpic -Iutente -c utente/lib.cpp -o utente/lib.o

# file di supporto
build/mkstart: include/costanti.h util/mkstart.cpp
	g++ -std=c++17 -fno-exceptions -g -Iinclude -I$(CE_INCLUDE) -DCE_UTILS -DCE_ADDR2LINE='"$(CE_ADDR2LINE)"' -o build/mkstart util/mkstart.cpp

util/start.mk util/start.pl util/tmp.gdb &: build/mkstart
	build/mkstart

.gdbinit: util/tmp.gdb debug/gdbinit
	cp util/tmp.gdb $@
	sed 's|@CE_QEMU_BOOT@|$(CE_QEMU_BOOT)|g;s|@CE_LIB64@|$(CE_LIB64)|g' debug/gdbinit >> .gdbinit

# pulizia
.PHONY: clean reset doc
clean:
	rm -f sistema/*.o io/*.o utente/*.o util/*.o
	rm -f util/start.mk util/start.pl util/tmp.gdb

reset: clean
	rm -rf build/* doc/html doc/latex .gdbinit

# creazione automatica di alcune directory, nel caso fossero assenti
build:
	mkdir -p $@

tags: $(wildcard */*.cpp */*.h)
	ctags --recurse=yes --exclude=build/ --exclude=.git --exclude=\*.o --exclude=\*.patch --exclude=\*.rej --exclude=\*.orig \
		--exclude=Makefile --exclude=run --exclude=\*.mk --exclude=\*.py --exclude=\*.pl --exclude=\*.gdb \
		. $(CE_INCLUDE) $(if $(CE_SOURCES),$(addprefix $(CE_SOURCES)/,*/*.cpp include/* as64/*.s))

ifeq ($(CE_ESAME),)
doc: doc/html/index.html 
	@true

doc/html/index.html: $(wildcard */*.cpp */*.s */*.h)
	PROJECT_NUMBER=$$(git describe --tags HEAD) doxygen
else
doc: doc/html/index.html 
	@true

doc/html/index.html: question.dox $(wildcard */*.cpp */*.s */*.h)
	PROJECT_NUMBER=$(CE_ESAME) doxygen
	sed -i -e 's|\([a-z>")]\)\.\([A-Z<]\)|\1\.<p>\2|g;s|$$\([^$$]*\)\$$|<i>\1</i>|g;s|\\math[a-z]*{\([^}]*\)}|\1|g' doc/html/group__esame.html

question.dox: util/question.md
		( echo '/// @ingroup esame'; \
		  d=$$(LANG=it_IT.utf8 date +'%-e %B %Y' -d $(CE_ESAME)); \
		  case "$$d" in \
		  [18]*) h="dell'$$d";; \
		  *)     h="del $$d";; \
		  esac; \
		  echo "/// @page esame Prova pratica $$h"; sed 's|^|/// |' $^) > $@

util/question.md: util/question.tex
	pandoc -o $@ $^
endif
