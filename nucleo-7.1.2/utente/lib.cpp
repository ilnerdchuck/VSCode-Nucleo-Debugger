/// @file lib.cpp
/// @brief Libreria collegata con i programmi utente
/// @addtogroup usr Modulo utente
/// @{
#include <all.h>

/// @addtogroup usrutil Funzioni di utilità generale
/// @{
/*!
 * Non possiamo usare la funzione printf di libce, perché non
 * abbiamo accesso diretto alla memoria video.
 * Invece, formattiamo il messaggio in un buffer e poi lo
 * scriviamo tramite @ref writeconsole().
 */
int printf(const char* fmt, ...)
{
	static const natq PRINTF_BUF = 1024;
	va_list ap;
	char buf[PRINTF_BUF];
	int l;

	va_start(ap, fmt);
	l = vsnprintf(buf, PRINTF_BUF, fmt, ap);
	va_end(ap);

	writeconsole(buf, l);

	return l;
}

/// buffer della funzione pause()
char pause_buf[1];

/*!
 * Non possiamo usare la funzione pause() di libce, perché
 * non abbiamo accesso diretto ai registri della tastiera.
 * Invece, usiamo @ref readconsole() per leggere un singolo
 * carattere.
 */
void pause()
{
	printf("Premere un tasto per continuare");
	readconsole(pause_buf, 1);
}

/*!
 * Per implementare getpid() usiamo @ref getmeminfo().
 */
natl getpid()
{
	return getmeminfo().pid;
}
/// @}

/// @defgroup usrheap Memoria dinamica
///
/// Dal momento che il modulo utente è eseguito con le interruzioni
/// esterne mascherabili abilitate, dobbiamo proteggere lo heap con
/// un semaforo di mutua esclusione.
///
/// @{

/// Semaforo di mutua esclusione per lo heap utente.
natl userheap_mutex;

/*! @brief alloca un oggetto nello heap utente
 * @param s	dimensione dell'oggetto
 * @return	puntatore all'oggetto (nullptr se heap esaurito)
 */
void* operator new(size_t s)
{
	void* p;

	sem_wait(userheap_mutex);
	p = alloc(s);
	sem_signal(userheap_mutex);

	return p;
}

/*! @brief dealloca un oggetto restituendolo allo heap utente.
 * @param p puntatore all'oggetto
 */
void operator delete(void* p)
{
	sem_wait(userheap_mutex);
	dealloc(p);
	sem_signal(userheap_mutex);
}
/// @}

/// @defgroup usrerr Gestione errori
/// @{

/*! @brief Termina il processo corrente.
 *
 * Gli errori nel modulo utente non sono mai fatali.
 */
extern "C" void panic(const char* msg)
{
	flog(LOG_WARN, "%s", msg);
	terminate_p();
}
/// @}

/// ultimo indirizzo usato dal modulo utente (fornito dal collegatore)
extern "C" natb end[];

/// @defgroup usrinit	Inizializzazione
/// @{

/// @brief Inizializza la libreria utente
///
/// In particolare, alloca il semaforo di mutua esclusione dello
/// heap utente e inizializza lo heap stesso.
///
/// @note La stringa `__attribute__((constructor))` è una estensione di gcc.
/// Dice al compilatore di aggiungere un puntatore a questa funzione alla
/// tabella init_array del file ELF. Nel nostro caso, queste funzioni vengono
/// poi chiamate da start (usando la funzione ctors() di libce) prima di invocare main().
void lib_init() __attribute__((constructor));
/// @cond
void lib_init()
{
	userheap_mutex = sem_ini(1);
	if (userheap_mutex == 0xFFFFFFFF) {
		panic("Impossibile creare il mutex per lo heap utente");
	}
	heap_init(allinea_ptr(end, DIM_PAGINA), DIM_USR_HEAP);
	flog(LOG_INFO, "Heap del modulo utente: %llxB [%p, %p)", DIM_USR_HEAP,
			end, end + DIM_USR_HEAP);
}
/// @endcond
/// @}
/// @}
