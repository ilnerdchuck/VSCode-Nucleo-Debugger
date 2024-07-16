/// @file sysio.h
/// @brief Primitive realizzate dal modulo sistema e riservate al modulo I/O.

/**
 * @brief Crea un processo esterno.
 *
 * La primitiva crea un processo esterno e lo associa ad una particolare richiesta
 * di interruzione (IRQ).
 *
 * Il processo eseguirà _f(a)_ con priorità _prio_ a livello _liv_.
 *
 * Il tipo da associare alla richiesta _irq_ è passato indirettamente tramite
 * il parametro _prio_. In particolare, il tipo è ottenuto da _prio_ sottraendo
 * @ref MIN_EXT_PRIO.
 *
 * @param f		corpo del processo
 * @param a		parametro per il corpo del processo
 * @param prio		priorità del processo
 * @param liv		livello del processo (LIV_UTENTE o LIV_SISTEMA)
 * @param irq		IRQ gestito dal processo
 *
 * @return		id del nuovo processo, o 0xFFFFFFFF in caso di errore
 */
extern "C" natl activate_pe(void f(int), int a, natl prio, natl liv, natb irq);

/**
 * @brief Attende la prossima richiesta di interruzione.
 */
extern "C" void wfi();

/**
 * @brief Abortisce il processo corrente.
 *
 * Usata dalle primitive di I/O quando rilevano un errore nei parametri ricevuti
 * dall'utente.
 */
extern "C" void abort_p();

/**
 * @brief Errore fatale nel modulo I/O.
 *
 * Invocando questa primitiva il modulo I/O informa il modulo sistema di aver
 * rilevato un errore fatale (per esempio, non è riuscito ad inizializzare
 * la console).
 */
extern "C" void io_panic();

/**
 * @brief Traduzione da indirizzo virtuale a fisico.
 *
 * Usa il TRIE del processo corrente.
 *
 * @param ff		indirizzo virtuale da tradurre
 *
 * @return 		indirizzo fisico corrispondente (0 se non mappato)
 */
extern "C" paddr trasforma(void* ff);

/**
 * @brief Verifica dei problemi di Cavallo di Troia
 *
 * La primitiva controlla che tutti gli indirizzi in [_start_, _start_ + _dim_)
 * siano accessibili da livello utente. In particolare, tutti gli indirizzi
 * dell'intervallo devono essere mappati.
 *
 * @param start		base dell'intervallo da controllare
 * @param dim		dimensione in byte dell'intervallo da controllare
 * @param writeable	se true, l'intervallo deve essere anche scrivibile
 * @param shared	se true, l'intervallo deve trovarsi nella parte utente/condivisa
 *
 * @return		true se i vincoli sono rispettati, false altrimenti
 */
extern "C" bool access(const void* start, natq dim, bool writeable, bool shared = true);

/**
 * @brief Riempi un gate della IDT.
 *
 * Il gate sarà in ogni caso di tipo trap.  Il gate non deve essere già
 * occupato. Il tipo deve essere compreso tra 0x40 e 0x4F (inclusi).
 *
 * @param tipo		tipo del gate da riempire
 * @param f		funzione da associare al gate
 *
 * @return 		false in caso di errore, true altrimenti
 */
extern "C" bool fill_gate(natl tipo, vaddr f);
