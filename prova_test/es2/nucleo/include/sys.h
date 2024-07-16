/// @file sys.h
/// @brief Primitive comuni definite dal modulo sistema
///
/// Queste primitive possono essere usate sia dal modulo utente che
/// dal modulo I/O. Alcune sono usate anche dal modulo sistema stesso.


/**
 * @brief Crea un nuovo processo
 *
 * Il nuovo processo eseguirà _f(a)_ con priorità _prio_ e a livello _liv_.
 *
 * Un processo non può usare questa primitiva per creare un processo a priorità
 * o livello maggiori dei propri.
 *
 * @param f	corpo del processo
 * @param a	parametro per il corpo del processo 
 * @param prio	priorità del processo 
 * @param liv	livello del processo (LIV_UTENTE o LIV_SISTEMA)
 *
 * @return 	id del nuovo processo, o 0xFFFFFFFF in caso di errore
 */
extern "C" natl activate_p(void f(natq), natq a, natl prio, natl liv);

/**
 * @brief Termina il processo corrente.
 *
 * I processi devono invocare questa primitiva per poter terminare.
 */
extern "C" void terminate_p();

/**
 * @brief Crea un nuovo semaforo.
 *
 * @param val	numero di gettoni iniziali
 *
 * @return 	id del nuovo semaforo, o 0xFFFFFFFF in caso di errore
 */
extern "C" natl sem_ini(int val);

/**
 * @brief Estrae un gettone da un semaforo.
 *
 * @param sem id del semaforo.
 */
extern "C" void sem_wait(natl sem);

/**
 * @brief Inserisce un gettone in un semaforo.
 *
 * @param sem	id del semaforo
 */
extern "C" void sem_signal(natl sem);

/**
 * @brief Sospende il processo corrente.
 *
 * @param n	numero di intervalli di tempo
 */
extern "C" void delay(natl n);

/// @name Funzioni di supporto al debugging
/// @{

/**
 * @brief Invia un messaggio al log.
 *
 * Questa primitiva è usata dai moduli I/O e utente per inviare i propri
 * messaggi al log di sistema.
 *
 * @param sev		severità del messaggio
 * @param buf		buffer contenente il messaggio
 * @param quanti	lunghezza del messaggio
 */
extern "C" void do_log(log_sev sev, const char* buf, natl quanti);

/**
 * @brief Informazioni di debug
 *
 * Questa struttura contiene delle informazioni che sono usate nei testi d'esame
 * per eseguire alcuni controlli.
 */
struct meminfo {
	/// numero di byte liberi nello heap di sistema
	natl heap_libero;
	/// numero di frame liberi in M2
	natl num_frame_liberi;
	/// id del processo corrente
	natl pid;
};

/**
 * @brief Estrae informazioni di debug.
 *
 * @return struttura contenente le informazioni
 */
extern "C" meminfo getmeminfo();
/// @}

// ( ESAME 2024-01-29
/// @addtogroup esame
/// @{

/**
 * @brief Crea una nuova barriera.
 *
 * @param nproc	numero di processi da sincronizzare
 * @param to	timeout per la sincronizzazione
 *
 * @return 	id della barrirea, o 0xFFFFFFFF in caso di errore
 */
extern "C" natl barrier_create(natl nproc, natl to);

/**
 * @brief Attraversa una barriera.
 *
 * @param id	id della barriera
 * @pre 	La barriera deve esistere.
 *
 * @return 	false se la barriera è stata attraversata nello stato erroneo,
 * 		true se è stata attraversata normalmente
 */
extern "C" bool barrier(natl id);
/// @}
//   ESAME 2024-01-29 )
