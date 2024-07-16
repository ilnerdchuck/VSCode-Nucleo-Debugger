/*! @file sistema.cpp
 *  @brief Parte C++ del modulo sistema.
 *  @defgroup sistema Modulo sistema
 *  @{
 */
#include <costanti.h>
#include <libce.h>
#include <sys.h>
#include <sysio.h>
#include <boot.h>

/////////////////////////////////////////////////////////////////////////////////
/// @defgroup heap           Memoria Dinamica
///
/// Per la gestione della memoria dinamica del modulo sistema usiamo le fuzioni
/// fornite da libce e ci limitiamo a ridefinire gli operatori new/delete in
/// modo che le usino.  La zona di memoria usata come heap per il modulo
/// sistema è definita durante l'inizializzazione (si vedano la chiamata a
/// heap_init() in main() e in main_sistema()).
/// @{
/////////////////////////////////////////////////////////////////////////////////

/*! @brief Alloca nello heap.
 *  @param size	dimensione dell'oggetto da allocare
 *  @return puntatore all'oggetto (nullptr se heap pieno)
 */
void* operator new(size_t size)
{
	return alloc(size);
}

/*! @brief Alloca nello heap con allienamento.
 *  @param size dimensione dell'oggetto da allocare
 *  @param align allineamento richiesto
 *  @return indirizzo dell'oggetto (nullptr se heap pieno)
 */
void* operator new(size_t size, std::align_val_t align)
{
	return alloc_aligned(size, align);
}

/*! @brief Dealloca un oggetto restituendolo allo heap
 *  @param p puntatore all'oggetto da deallocare
 */
void operator delete(void* p)
{
	dealloc(p);
}
/// @}


/////////////////////////////////////////////////////////////////////////////////
/// @defgroup proc		Processi
///
/// Dispensa: <https://calcolatori.iet.unipi.it/resources/processi.pdf>
///
/// @{
/////////////////////////////////////////////////////////////////////////////////

/// Priorità del processo dummy
const natl DUMMY_PRIORITY = 0;
/// Numero di registri nel campo contesto del descrittore di processo
const int N_REG = 16;

/// @brief Descrittore di processo
struct des_proc {
	/// identificatore numerico del processo
	natw id;
	/// livello di privilegio (LIV_UTENTE o LIV_SISTEMA)
	natw livello;
	/// precedenza nelle code dei processi
	natl precedenza;
	/// indirizzo della base della pila sistema
	vaddr punt_nucleo;
	/// copia dei registri generali del processore
	natq contesto[N_REG];
	/// radice del TRIE del processo
	paddr cr3;

// ( ESAME 2024-01-29
/// @addtogroup esame
/// @{

	/// @brief barriera su cui il processo è bloccato
	///
	/// Se questo campo è diverso da 0xFFFFFFFF, vuol dire che il processo
	/// è bloccato sulla barriera con questo identificatore, ed è il primo
	/// processo ad essere arrivato su questa barriera dall'ultima chiusura.
	natl barrier_id;
/// @}
//   ESAME 2024-01-29 )

	/// prossimo processo in coda
	des_proc* puntatore;

	/// @name Informazioni utili per il per debugging
	/// @{

	/// parametro `f` passato alla `activate_p`/`_pe` che ha creato questo processo
	void (*corpo)(natq);
	/// parametro `a` passato alla `activate_p`/`_pe` che ha creato questo processo
	natq  parametro;
	/// @}
};

/// @brief Tabella che associa l'id di un processo al corrispondente des_proc.
///
/// I des_proc sono allocati dinamicamente nello heap del sistema (si veda
/// crea_processo).
des_proc* proc_table[MAX_PROC];

/// Numero di processi utente attivi.
natl processi;

/// @cond
// (forward) Distrugge il processo puntato da esecuzione.
extern "C" void c_abort_p(bool selfdump = true);

// (forward) Ferma tutto il sistema (in caso di bug nel sistema stesso)
extern "C" [[noreturn]] void panic(const char* msg);
/// @endcond

/// @brief Indici delle copie dei registri nell'array contesto
enum { I_RAX, I_RCX, I_RDX, I_RBX,
	I_RSP, I_RBP, I_RSI, I_RDI, I_R8, I_R9, I_R10,
	I_R11, I_R12, I_R13, I_R14, I_R15 };

/// Coda esecuzione (contiene sempre un solo elemento)
des_proc* esecuzione;

/// Coda pronti (vuota solo quando dummy è in @ref esecuzione)
des_proc* pronti;

/*! @brief Inserimento in lista ordinato (per priorità)
 *  @param p_lista	lista in cui inserire
 *  @param p_elem	elemento da inserire
 *  @note a parità di priorità favorisce i processi già in coda
 */
void inserimento_lista(des_proc*& p_lista, des_proc* p_elem)
{
// inserimento in una lista semplice ordinata
//   (tecnica dei due puntatori)
	des_proc *pp, *prevp;

	pp = p_lista;
	prevp = nullptr;
	while (pp && pp->precedenza >= p_elem->precedenza) {
		prevp = pp;
		pp = pp->puntatore;
	}

	p_elem->puntatore = pp;

	if (prevp)
		prevp->puntatore = p_elem;
	else
		p_lista = p_elem;

}

/*! @brief Estrazione del processo a maggiore priorità
 *  @param  p_lista lista da cui estrarre
 *  @return processo a più alta priorità
 */
des_proc* rimozione_lista(des_proc*& p_lista)
{
// estrazione dalla testa
	des_proc* p_elem = p_lista;  	// nullptr se la lista è vuota

	if (p_lista)
		p_lista = p_lista->puntatore;

	if (p_elem)
		p_elem->puntatore = nullptr;

	return p_elem;
}

/// @brief Inserisce @ref esecuzione in testa alla lista @ref pronti
extern "C" void inspronti()
{
	esecuzione->puntatore = pronti;
	pronti = esecuzione;
}

/*! @brief Sceglie il prossimo processo da mettere in esecuzione
 *  @note Modifica solo la variabile @ref esecuzione.
 *  Il processo andrà effettivamente in esecuzione solo alla prossima
 *  `call carica_stato; iretq`
 */
extern "C" void schedulatore(void)
{
// poiché la lista è già ordinata in base alla priorità,
// è sufficiente estrarre l'elemento in testa
	esecuzione = rimozione_lista(pronti);
}

/*! @brief Trova il descrittore di processo dato l'id.
 *
 *  Errore fatale se _id_ non corrisponde ad alcun processo.
 *
 *  @param id 	id del processo
 *  @return	descrittore di processo corrispondente
 */
extern "C" des_proc* des_p(natw id)
{
	if (id > MAX_PROC_ID)
		fpanic("id %hu non valido (max %lu)", id, MAX_PROC_ID);

	return proc_table[id];
}

/// @name Funzioni usate dal processo dummy
/// @{

/// @brief Esegue lo shutdown del sistema.
extern "C" [[noreturn]] void end_program();

/*! @brief Esegue l'istruzione `hlt`.
 *
 *  Mette in pausa il processore in attesa di una richiesta di interruzione
 *  esterna
 */
extern "C" void halt();
/// @}

/// @brief Corpo del processo dummy
void dummy(natq)
{
	while (processi)
		halt();
	flog(LOG_INFO, "Shutdown");
	end_program();
}


// si veda anche CREAZIONE E DISTRUZIONE DEI PROCESSI, più avanti
/// @}

/////////////////////////////////////////////////////////////////////////////////
/// @defgroup sem                   Semafori
///
/// Dispensa: <https://calcolatori.iet.unipi.it/resources/semafori.pdf>
/// @{
/////////////////////////////////////////////////////////////////////////////////

/// @brief Descrittore di semaforo
struct des_sem {
	/// se >= 0, numero di gettoni contenuti;
	/// se < 0, il valore assoluto è il numero di processi in coda
	int counter;
	/// coda di processi bloccati sul semaforo
	des_proc* pointer;
};

/// @brief Array dei descrittori di semaforo.
///
/// I primi MAX_SEM semafori di array_dess sono per il livello utente, gli altri
/// MAX_SEM sono per il livello sistema.
des_sem array_dess[MAX_SEM * 2];

/*! @brief Restituisce il livello a cui si trovava il processore al momento
 *  in cui è stata invocata la primitiva.
 *
 *  @warning funziona solo nelle routine di risposta ad una interruzione
 *  (INT, esterna o eccezione) se è stata chiamata `salva_stato`.
 */
int liv_chiamante()
{
	// salva_stato ha salvato il puntatore alla pila sistema
	// subito dopo l'invocazione della INT
	natq* pila = ptr_cast<natq>(esecuzione->contesto[I_RSP]);
	// la seconda parola dalla cima della pila contiene il livello
	// di privilegio che aveva il processore prima della INT
	return pila[1] == SEL_CODICE_SISTEMA ? LIV_SISTEMA : LIV_UTENTE;
}

/// Numero di semafori allocati per il livello utente
natl sem_allocati_utente  = 0;

/// Numero di semafori allocati per il livello sistema (moduli sistema e I/O)
natl sem_allocati_sistema = 0;

/*! @brief Alloca un nuovo semaforo.
 *
 *  @return id del nuovo semaforo (0xFFFFFFFF se esauriti)
 */
natl alloca_sem()
{
	// I semafori non vengono mai deallocati, quindi è possibile allocarli
	// sequenzialmente. Per far questo è sufficiente ricordare quanti ne
	// abbiamo già allocati (variabili sem_allocati_utente e
	// sem_allocati_sistema)

	int liv = liv_chiamante();
	natl i;
	if (liv == LIV_UTENTE) {
		if (sem_allocati_utente >= MAX_SEM)
			return 0xFFFFFFFF;
		i = sem_allocati_utente;
		sem_allocati_utente++;
	} else {
		if (sem_allocati_sistema >= MAX_SEM)
			return 0xFFFFFFFF;
		i = sem_allocati_sistema + MAX_SEM;
		sem_allocati_sistema++;
	}
	return i;
}

/*! @brief Verifica un id di semaforo
 *
 *  @param sem	id da verificare
 *  @return  true se sem è l'id di un semaforo allocato; false altrimenti
 */
bool sem_valido(natl sem)
{
	// dal momento che i semafori non vengono mai deallocati,
	// un semaforo è valido se e solo se il suo indice è inferiore
	// al numero dei semafori allocati

	int liv = liv_chiamante();
	return sem < sem_allocati_utente ||
		(liv == LIV_SISTEMA && sem - MAX_SEM < sem_allocati_sistema);
}

/*! @brief Parte C++ della primitiva sem_ini().
 *  @param val	numero di gettoni iniziali
 */
extern "C" void c_sem_ini(int val)
{
	natl i = alloca_sem();

	if (i != 0xFFFFFFFF)
		array_dess[i].counter = val;

	esecuzione->contesto[I_RAX] = i;
}

/*! @brief Parte C++ della primitiva sem_wait().
 *  @param sem id di semaforo
 */
extern "C" void c_sem_wait(natl sem)
{
	// una primitiva non deve mai fidarsi dei parametri
	if (!sem_valido(sem)) {
		flog(LOG_WARN, "semaforo errato: %u", sem);
		c_abort_p();
		return;
	}

	des_sem* s = &array_dess[sem];
	s->counter--;

	if (s->counter < 0) {
		inserimento_lista(s->pointer, esecuzione);
		schedulatore();
	}
}

/*! @brief Parte C++ della primitiva sem_signal().
 *  @param sem id di semaforo
 */
extern "C" void c_sem_signal(natl sem)
{
	// una primitiva non deve mai fidarsi dei parametri
	if (!sem_valido(sem)) {
		flog(LOG_WARN, "semaforo errato: %u", sem);
		c_abort_p();
		return;
	}

	des_sem* s = &array_dess[sem];
	s->counter++;

	if (s->counter <= 0) {
		des_proc* lavoro = rimozione_lista(s->pointer);
		inspronti();	// preemption
		inserimento_lista(pronti, lavoro);
		schedulatore();	// preemption
	}
}
/// @}

/////////////////////////////////////////////////////////////////////////////////
/// @defgroup  timer 		Timer
///
/// Dispensa: <https://calcolatori.iet.unipi.it/resources/delay.pdf>
/// @{
/////////////////////////////////////////////////////////////////////////////////

/// Richiesta al timer
struct richiesta {
	/// tempo di attesa aggiuntivo rispetto alla richiesta precedente
	natl d_attesa;
	/// puntatore alla richiesta successiva
	richiesta* p_rich;
	/// descrittore del processo che ha effettuato la richiesta
	des_proc* pp;
};

/// Coda dei processi sospesi
richiesta* sospesi;

/*! @brief Inserisce un processo nella coda delle richieste al timer
 *  @param p richiesta da inserire
 */
void inserimento_lista_attesa(richiesta* p)
{
	richiesta *r, *precedente;

	r = sospesi;
	precedente = nullptr;

	while (r != nullptr && p->d_attesa > r->d_attesa) {
		p->d_attesa -= r->d_attesa;
		precedente = r;
		r = r->p_rich;
	}

	p->p_rich = r;
	if (precedente != nullptr)
		precedente->p_rich = p;
	else
		sospesi = p;

	if (r != nullptr)
		r->d_attesa -= p->d_attesa;
}

/*! @brief Parte C++ della primitiva delay.
 *  @param n	numero di intervalli di tempo
 */
extern "C" void c_delay(natl n)
{
	// caso particolare: se n è 0 non facciamo niente
	if (!n)
		return;

	richiesta* p = new richiesta;
	p->d_attesa = n;
	p->pp = esecuzione;

	inserimento_lista_attesa(p);
	schedulatore();
}

// ( ESAME 2024-01-29
/// @addtogroup esame
/// @{

/**
 * @brief Time out su una barriera.
 *
 * Chiamata dal driver del timer per i processi che devono essere risvegliati.
 * Se il processo era uno di quelli che si erano bloccati per primi su
 * una barriera, svolge le operazioni richieste dallo scatto del timeout.
 *
 * @param p	descrittore del processo che sta per essere risvegliato
 */
void check_barrier(des_proc *p);
/// @}
//   ESAME 2024-01-29 )

/// @brief Driver del timer
extern "C" void c_driver_td(void)
{
	inspronti();

	if (sospesi != nullptr) {
		sospesi->d_attesa--;
	}

	while (sospesi != nullptr && sospesi->d_attesa == 0) {
// ( ESAME 2024-01-29
/// @ingroup esame
/// @note Eseguiamo le azioni richieste da un eventuale timeout su una barriera. 

		check_barrier(sospesi->pp);
//   ESAME 2024-01-29 )
		inserimento_lista(pronti, sospesi->pp);
		richiesta* p = sospesi;
		sospesi = sospesi->p_rich;
		delete p;
	}

	schedulatore();
}
/// @}

/////////////////////////////////////////////////////////////////////////////////
/// @defgroup exc      		Eccezioni
/// @{
/////////////////////////////////////////////////////////////////////////////////

/// Primo indirizzo del codice di sistema
extern "C" natb start[];

/// Ultimo indirizzo del codice sistema (fornito dal collegatore)
extern "C" natb end[];

/// @cond
/// mostra sul log lo stato del processo (definita più avanti)
void process_dump(des_proc*, log_sev sev);
/// @endcond

/*! @brief Gestore generico di eccezioni.
 *
 *  Funzione chiamata da tutti i gestori di eccezioni in sistema.s, tranne
 *  quello per il Non-Maskable-Interrupt.
 *
 *  @param tipo tipo dell'eccezione
 *  @param errore eventuale codice di errore aggiuntivo
 *  @param rip istruction pointer salvato in pila
 */
extern "C" void gestore_eccezioni(int tipo, natq errore, vaddr rip)
{
	log_exception(tipo, errore, rip);

	if (tipo != 14 && (errore & SE_EXT)) {
		// la colpa dell'errore non si può attribuire al programma, ma al
		// sistema. Blocchiamo dunque l'esecuzione
		panic("ERRORE DI SISTEMA (EXT)");
	}

	if (tipo == 14 && (errore & PF_RES)) {
		// se la MMU ha rilevato un errore sui bit riservati vuol dire
		// che c'è qualche bug nel modulo sistema stesso, quindi
		// blocchiamo l'esecuzione
		panic("ERRORE NELLE TABELLE DI TRADUZIONE");
	}

	if (rip >= int_cast<vaddr>(start) && rip < int_cast<vaddr>(end)) {
		// l'indirizzo dell'istruzione che ha causato il fault è
		// all'interno del modulo sistema.
		panic("ERRORE DI SISTEMA");
	}

	// se siamo qui la colpa dell'errore è da imputare ai moduli utente o
	// I/O.  Inviamo sul log lo stato del processo al momento del sollevamento
	// dell'eccezione (come in questo momento si trova salvato in pila
	// sistema e descrittore di processo)
	process_dump(esecuzione, LOG_WARN);
	// abortiamo il processo (senza mostrare nuovamente il dump)
	c_abort_p(false /* no dump */);
}
/// @}

////////////////////////////////////////////////////////////////////////////////
/// @defgroup frame		Frame
/// @{
/////////////////////////////////////////////////////////////////////////////////

/// @brief Descrittore di frame
struct des_frame {
	union {
		/// numero di entrate valide (se il frame contiene una tabella)
		natw nvalide;
		/// prossimo frame libero (se il frame è libero)
		natl prossimo_libero;
	};
};

/// Numero totale di frame (M1 + M2)
natq const N_FRAME = MEM_TOT / DIM_PAGINA;

/// Numero di frame in M1
natq N_M1;

/// Numero di frame in M2
natq N_M2;

/// Array dei descrittori di frame
des_frame vdf[N_FRAME];

/// Testa della lista dei frame liberi
natq primo_frame_libero;

/// Numero di frame nella lista dei frame liberi
natq num_frame_liberi;

/*! @brief Inizializza la parte M2 e i descrittori di frame.
 *
 *  Funzione chiamata in fase di inizializzazione.
 */
void init_frame()
{
	// Tutta la memoria non ancora occupata viene usata per i frame.  La
	// funzione si preoccupa anche di inizializzare i descrittori dei frame
	// in modo da creare la lista dei frame liberi.  end è l'indirizzo del
	// primo byte non occupato dal modulo sistema (è calcolato dal
	// collegatore). La parte M2 della memoria fisica inizia al primo frame
	// dopo end.

	// primo frame di M2
	paddr fine_M1 = allinea(int_cast<paddr>(end), DIM_PAGINA);
	// numero di frame in M1 e indice di f in vdf
	N_M1 = fine_M1 / DIM_PAGINA;
	// numero di frame in M2
	N_M2 = N_FRAME - N_M1;

	if (!N_M2)
		return;

	// creiamo la lista dei frame liberi, che inizialmente contiene tutti i
	// frame di M2
	primo_frame_libero = N_M1;
/// @cond
#ifndef N_STEP
	// Alcuni esercizi definiscono N_STEP == 2 per creare mapping non
	// contigui in memoria virtale e controllare meglio alcuni possibili
	// bug
#define N_STEP 1
#endif
/// @endcond
	natq last;
	for (natq j = 0; j < N_STEP; j++) {
		for (natq i = j; i < N_M2; i += N_STEP) {
			vdf[primo_frame_libero + i].prossimo_libero =
				primo_frame_libero + i + N_STEP;
			num_frame_liberi++;
			last = i;
		}
		vdf[primo_frame_libero + last].prossimo_libero =
			primo_frame_libero + j + 1;
	}
	vdf[primo_frame_libero + last].prossimo_libero = 0;
}

/*! @brief Estrae un frame dalla lista dei frame liberi.
 *  @return indirizzo fisico del frame estratto, o 0 se la lista è vuota
 */
paddr alloca_frame()
{
	if (!num_frame_liberi) {
		flog(LOG_ERR, "out of memory");
		return 0;
	}
	natq j = primo_frame_libero;
	primo_frame_libero = vdf[primo_frame_libero].prossimo_libero;
	vdf[j].prossimo_libero = 0;
	num_frame_liberi--;
	return j * DIM_PAGINA;
}

/*! @brief Restiuisce un frame alla listsa dei frame liberi.
 *  @param f	indirizzo fisico del frame da restituire
 */
void rilascia_frame(paddr f)
{
	natq j = f / DIM_PAGINA;
	if (j < N_M1) {
		fpanic("tentativo di rilasciare il frame %lx di M1", f);
	}
	// dal momento che i frame di M2 sono tutti equivalenti, è
	// sufficiente inserire in testa
	vdf[j].prossimo_libero = primo_frame_libero;
	primo_frame_libero = j;
	num_frame_liberi++;
}
/// @}

/////////////////////////////////////////////////////////////////////////////////
/// @defgroup pg		Paginazione
///
/// Dispensa: <https://calcolatori.iet.unipi.it/resources/paginazione-implementazione.pdf>
///
/// @{
/////////////////////////////////////////////////////////////////////////////////
#include <vm.h>

/// @defgroup ranges Parti della memoria virtuale dei processi
///
/// Le parti hanno dimensioni multiple della dimensione della pagina di livello
/// massimo (@ref PART_SIZE), sono allineate naturalmente e non si
/// sovrappongono.  In questo modo possiamo definire le varie parti
/// semplicemente specificando un intervallo di entrate della tabella radice.
/// Per esempio, la parte sistema/condivisa usa @ref N_SIS_C entrate a partire da
/// @ref I_SIS_C e contiene tutti e soli gli indirizzi la cui traduzione passa da
/// queste entrate.
/// @{

/// Granularità delle parti della memoria virtuale.
static const natq PART_SIZE = dim_region(MAX_LIV - 1);

const vaddr ini_sis_c = norm(I_SIS_C * PART_SIZE); ///< base di sistema/condivisa
const vaddr ini_sis_p = norm(I_SIS_P * PART_SIZE); ///< base di sistema/privata
const vaddr ini_mio_c = norm(I_MIO_C * PART_SIZE); ///< base di modulo IO/condivisa
const vaddr ini_utn_c = norm(I_UTN_C * PART_SIZE); ///< base di utente/condivisa
const vaddr ini_utn_p = norm(I_UTN_P * PART_SIZE); ///< base di utente/privata

const vaddr fin_sis_c = ini_sis_c + PART_SIZE * N_SIS_C; ///< limite di sistema/condivisa
const vaddr fin_sis_p = ini_sis_p + PART_SIZE * N_SIS_P; ///< limite di sistema/privata
const vaddr fin_mio_c = ini_mio_c + PART_SIZE * N_MIO_C; ///< limite di modulo IO/condivisa
const vaddr fin_utn_c = ini_utn_c + PART_SIZE * N_UTN_C; ///< limite di utente/condivisa
const vaddr fin_utn_p = ini_utn_p + PART_SIZE * N_UTN_P; ///< limite di utente/privata
/// @}

/// @defgroup mapfuncs Funzioni necessarie per map() e unmap()
///
/// Le funzioni map() e unmap() di libce richiedono la definizione
/// di alcune funzioni per l'allocazione e la deallocazione delle
/// tabelle. Le definiamo qui, utilizzando i descrittori di frame.
/// In particolare, se un frame contiene una tabella, il campo
/// nvalide del suo descrittore (@ref des_frame) è il contatore
/// delle entrate valide della tabella (entrate della tabella con P == 1).
/// @{

/*! @brief Alloca un frame libero destinato a contenere una tabella.
 *
 *  Azzera tutta la tabella e il suo contatore di entrate di valide.
 *  @return indirizzo fisico della tabella
 */
paddr alloca_tab()
{
	paddr f = alloca_frame();
	if (f) {
		memset(voidptr_cast(f), 0, DIM_PAGINA);
		vdf[f / DIM_PAGINA].nvalide = 0;
	}
	return f;
}

/*! @brief Dealloca un frame che contiene una tabella
 *
 *  @warning La funzione controlla che la tabella non contenga entrate
 *  valide e causa un errore fatale in caso contrario.
 *
 *  @param f indirizzo fisico della tabella
 */
void rilascia_tab(paddr f)
{
	if (int n = get_ref(f)) {
		fpanic("tentativo di deallocare la tabella %lx con %d entrate valide", f, n);
	}
	rilascia_frame(f);
}

/*! @brief Incrementa il contatore delle entrate valide di una tabella
 *  @param f indirizzo fisico della tabella
 */
void inc_ref(paddr f)
{
	vdf[f / DIM_PAGINA].nvalide++;
}

/*! @brief Decrementa il contatore delle entrate valide di una tabella
 *  @param f indirizzo fisico della tabella
 */
void dec_ref(paddr f)
{
	vdf[f / DIM_PAGINA].nvalide--;
}

/*! @brief Legge il contatore delle entrate valide di una tabella
 *  @param f indirizzo fisico della tabella
 *  @return valore del contatore
 */
natl get_ref(paddr f)
{
	return vdf[f / DIM_PAGINA].nvalide;
}
/// @}

/*! @brief Controlla che un indirizzo appartenga alla zona utente/condivisa
 *  @param v indirizzo virtuale da controllare
 *  @return true sse _v_ appartiene alla parte utente/condivisa, false altrimenti
 */
bool in_utn_c(vaddr v)
{
	return v >= ini_utn_c && v < fin_utn_c;
}

/*! @brief Parte C++ della primitiva access()
 *
 *  Primitiva utilizzata dal modulo I/O per controllare che i buffer passati dal
 *  livello utente siano accessibili dal livello utente (problema del Cavallo di
 *  Troia) e non possano causare page fault nel modulo I/O (bit P tutti a 1 e
 *  scrittura permessa quando necessario).
 *
 *  @param begin	base dell'intervallo da controllare
 *  @param dim		dimensione dell'intervallo da controllare
 *  @param writeable	se true, l'intervallo deve essere anche scrivibile
 *  @param shared	se true, l'intevallo deve trovarsi in utente/condivisa
 *  @return		true se i vincoli sono rispettati, false altrimenti
 */
extern "C" bool c_access(vaddr begin, natq dim, bool writeable, bool shared = true)
{
	esecuzione->contesto[I_RAX] = false;

	if (!tab_iter::valid_interval(begin, dim))
		return false;

	if (shared && (!in_utn_c(begin) || (dim > 0 && !in_utn_c(begin + dim - 1))))
		return false;

	// usiamo un tab_iter per percorrere tutto il sottoalbero relativo
	// alla traduzione degli indirizzi nell'intervallo [begin, begin+dim).
	for (tab_iter it(esecuzione->cr3, begin, dim); it; it.next()) {
		tab_entry e = it.get_e();

		// interrompiamo il ciclo non appena troviamo qualcosa che non va
		if (!(e & BIT_P) || !(e & BIT_US) || (writeable && !(e & BIT_RW)))
			return false;
	}
	esecuzione->contesto[I_RAX] = true;
	return true;
}

/*! @brief Parte C++ della primitiva trasforma()
 *
 *  Traduce _ind_virt_ usando il TRIE del processo puntato
 *  da @ref esecuzione.
 *
 *  @param ind_virt indirizzo virtuale da tradurre
 */
extern "C" void c_trasforma(vaddr ind_virt)
{
	esecuzione->contesto[I_RAX] = trasforma(esecuzione->cr3, ind_virt);
}
/// @}

/////////////////////////////////////////////////////////////////////////////////
/// @addtogroup proc
/// @{
/// @defgroup create	Creazione e distruzione dei processi
/// @{
/////////////////////////////////////////////////////////////////////////////////

/// Associazione IRQ -> processo esterno che lo gestisce
des_proc* a_p[apic::MAX_IRQ];

/// @brief Valore da inserire in @ref a_p per gli IRQ che sono gestiti da driver
///
/// Nel nucleo base questo accade solo per l'IRQ del timer.
des_proc* const ESTERN_BUSY = reinterpret_cast<des_proc*>(1UL);

/// @name Funzioni di supporto alla creazione e distruzione dei processi
/// @{

/*! @brief Alloca un id di processo
 *  @param p	descrittore del processo a cui assegnare l'id
 *  @return	id del processo (0xFFFFFFFF se terminati)
 */
natl alloca_proc_id(des_proc* p)
{
	static natl next = 0;

	// La funzione inizia la ricerca partendo  dall'id successivo
	// all'ultimo restituito (salvato nella variable statica 'next'),
	// saltando quelli che risultano in uso.
	// In questo modo gli id tendono ad essere riutilizzati il più tardi possibile,
	// cosa che aiuta chi deve debuggare i propri programmi multiprocesso.
	natl scan = next, found = 0xFFFFFFFF;
	do {
		if (proc_table[scan] == nullptr) {
			found = scan;
			proc_table[found] = p;
		}
		scan = (scan + 1) % MAX_PROC;
	} while (found == 0xFFFFFFFF && scan != next);
	next = scan;
	return found;
}

/*! @brief Rilascia un id di processo non più utilizzato
 *  @param id	id da rilasciare
 */
void rilascia_proc_id(natw id)
{
	if (id > MAX_PROC_ID)
		fpanic("id %hu non valido (max %lu)", id, MAX_PROC_ID);

	if (proc_table[id] == nullptr)
		fpanic("tentativo di rilasciare id %d non allocato", id);

	proc_table[id] = nullptr;
}


/*! @brief Inizializza la tabella radice di un nuovo processo
 *  @param dest indirizzo fisico della tabella
 */
void init_root_tab(paddr dest)
{
	paddr pdir = esecuzione->cr3;

	// ci limitiamo a copiare dalla tabella radice corrente i puntatori
	// alle tabelle di livello inferiore per tutte le parti condivise
	// (sistema, utente e I/O). Quindi tutti i sottoalberi di traduzione
	// delle parti condivise saranno anch'essi condivisi. Questo, oltre a
	// semplificare l'inizializzazione di un processo, ci permette di
	// risparmiare un po' di memoria.
	copy_des(pdir, dest, I_SIS_C, N_SIS_C);
	copy_des(pdir, dest, I_MIO_C, N_MIO_C);
	copy_des(pdir, dest, I_UTN_C, N_UTN_C);
}

/*! @brief Ripulisce la tabella radice di un processo
 *  @param dest indirizzo fisico della tabella
 */
void clear_root_tab(paddr dest)
{
	// eliminiamo le entrate create da init_root_tab()
	set_des(dest, I_SIS_C, N_SIS_C, 0);
	set_des(dest, I_MIO_C, N_MIO_C, 0);
	set_des(dest, I_UTN_C, N_UTN_C, 0);
}

/*! @brief Crea una pila processo
 *
 *  @param root_tab	indirizzo fisico della radice del TRIE del processo
 *  @param bottom	indirizzo virtuale del bottom della pila
 *  @param size		dimensione della pila (in byte)
 *  @param liv		livello della pila (LIV_UTENTE o LIV_SISTEMA)
 *  @return		true se la creazione ha avuto successo, false altrimenti
 */
bool crea_pila(paddr root_tab, vaddr bottom, natq size, natl liv)
{
	vaddr v = map(root_tab,
		bottom - size,
		bottom,
		BIT_RW | (liv == LIV_UTENTE ? BIT_US : 0),
		[](vaddr) { return alloca_frame(); });
	if (v != bottom) {
		unmap(root_tab, bottom - size, v,
			[](vaddr, paddr p, int) { rilascia_frame(p); });
		return false;
	}
	return true;
}

/*! @brief Distrugge una pila processo
 *
 *  Funziona indifferentemente per pile utente o sistema.
 *
 *  @param root_tab	indirizzo fisico della radice del TRIE del processo
 *  @param bottom	indirizzo virtuale del bottom della pila
 *  @param size		dimensione della pila (in byte)
 */
void distruggi_pila(paddr root_tab, vaddr bottom, natq size)
{
	unmap(
		root_tab,
		bottom - size,
		bottom,
		[](vaddr, paddr p, int) { rilascia_frame(p); });
}

/*! @brief Funzione interna per la creazione di un processo.
 *
 *  Parte comune a activate_p() e activate_pe().  Alloca un id per il processo
 *  e crea e inizializza il descrittore di processo, la pila sistema e, per i
 *  processi di livello utente, la pila utente. Crea l'albero di traduzione
 *  completo per la memoria virtuale del processo.
 *
 *  @param f	corpo del processo
 *  @param a	parametro per il corpo del processo
 *  @param prio	priorità del processo
 *  @param liv	livello del processo (LIV_UTENTE o LIV_SISTEMA)
 *  @return	puntatore al nuovo descrittore di processo
 *  		(nullptr in caso di errore)
 */
des_proc* crea_processo(void f(natq), natq a, int prio, char liv)
{
	des_proc*	p;			// des_proc per il nuovo processo
	paddr		pila_sistema;		// pila_sistema del processo
	natq*		pl;			// pila sistema come array di natq
	natl		id;			// id del nuovo processo

	// allocazione (e azzeramento preventivo) di un des_proc
	p = new des_proc;
	if (!p)
		goto err_out;
	memset(p, 0, sizeof(des_proc));

	// rimpiamo i campi di cui conosciamo già i valori
	p->precedenza = prio;
	p->puntatore = nullptr;
	// il registro RDI deve contenere il parametro da passare alla
	// funzione f
	p->contesto[I_RDI] = a;

// ( ESAME 2024-01-29
/// @ingroup esame
/// @note Il processo non è inizialmente bloccato su nessuna barriera.

	p->barrier_id = 0xFFFFFFFF;
//   ESAME 2024-01-29 )

	// selezione di un identificatore
	id = alloca_proc_id(p);
	if (id == 0xFFFFFFFF)
		goto err_del_p;
	p->id = id;

	// creazione della tabella radice del processo
	p->cr3 = alloca_tab();
	if (p->cr3 == 0)
		goto err_rel_id;
	init_root_tab(p->cr3);

	// creazione della pila sistema
	if (!crea_pila(p->cr3, fin_sis_p, DIM_SYS_STACK, LIV_SISTEMA))
		goto err_rel_tab;
	// otteniamo un puntatore al fondo della pila appena creata.  Si noti
	// che non possiamo accedervi tramite l'indirizzo virtuale 'fin_sis_p',
	// che verrebbe tradotto seguendo l'albero del processo corrente, e non
	// di quello che stiamo creando.  Per questo motivo usiamo trasforma()
	// per ottenere il corrispondente indirizzo fisico. In questo modo
	// accediamo alla nuova pila tramite la finestra FM.
	pila_sistema = trasforma(p->cr3, fin_sis_p - DIM_PAGINA) + DIM_PAGINA;

	// convertiamo a puntatore a natq, per accedervi più comodamente
	pl = ptr_cast<natq>(pila_sistema);

	if (liv == LIV_UTENTE) {
		// inizializziamo la pila sistema.
		pl[-5] = int_cast<natq>(f);	    // RIP (codice utente)
		pl[-4] = SEL_CODICE_UTENTE;	    // CS (codice utente)
		pl[-3] = BIT_IF;	    	    // RFLAGS
		pl[-2] = fin_utn_p - sizeof(natq);  // RSP
		pl[-1] = SEL_DATI_UTENTE;	    // SS (pila utente)
		// eseguendo una IRET da questa situazione, il processo
		// passerà ad eseguire la prima istruzione della funzione f,
		// usando come pila la pila utente (al suo indirizzo virtuale)

		// creazione della pila utente
		if (!crea_pila(p->cr3, fin_utn_p, DIM_USR_STACK, LIV_UTENTE)) {
			flog(LOG_WARN, "creazione pila utente fallita");
			goto err_del_sstack;
		}

		// inizialmente, il processo si trova a livello sistema, come
		// se avesse eseguito una istruzione INT, con la pila sistema
		// che contiene le 5 parole lunghe preparate precedentemente
		p->contesto[I_RSP] = fin_sis_p - 5 * sizeof(natq);

		p->livello = LIV_UTENTE;

		// dal momento che usiamo traduzioni diverse per le parti sistema/private
		// di tutti i processi, possiamo inizializzare p->punt_nucleo con un
		// indirizzo (virtuale) uguale per tutti i processi
		p->punt_nucleo = fin_sis_p;

		//   tutti gli altri campi valgono 0
	} else {
		// processo di livello sistema
		// inizializzazione della pila sistema
		pl[-6] = int_cast<natq>(f);		// RIP (codice sistema)
		pl[-5] = SEL_CODICE_SISTEMA;            // CS (codice sistema)
		pl[-4] = BIT_IF;  	        	// RFLAGS
		pl[-3] = fin_sis_p - sizeof(natq);      // RSP
		pl[-2] = 0;			        // SS
		pl[-1] = 0;			        // ind. rit.
							//(non significativo)
		// i processi esterni lavorano esclusivamente a livello
		// sistema. Per questo motivo, prepariamo una sola pila (la
		// pila sistema)

		// inizializziamo il descrittore di processo
		p->contesto[I_RSP] = fin_sis_p - 6 * sizeof(natq);

		p->livello = LIV_SISTEMA;

		// tutti gli altri campi valgono 0
	}

	// informazioni di debug
	p->corpo = f;
	p->parametro = a;

	return p;

err_del_sstack:	distruggi_pila(p->cr3, fin_sis_p, DIM_SYS_STACK);
err_rel_tab:	clear_root_tab(p->cr3);
		rilascia_tab(p->cr3);
err_rel_id:	rilascia_proc_id(p->id);
err_del_p:	delete p;
err_out:	return nullptr;
}

/// @cond
// usate da distruggi processo e definite più avanti
extern paddr ultimo_terminato;
extern des_proc* esecuzione_precedente;
extern "C" void distruggi_pila_precedente();
/// @endcond

/*! @brief Dealloca tutte le risorse allocate da crea_processo()
 *
 *  Dealloca l'id, il descrittore di processo, l'eventuale pila utente,
 *  comprese le tabelle che la mappavano nella memoria virtuale del processo.
 *  Per la pila sistema si veda sopra @ref esecuzione_precedente.
 */
void distruggi_processo(des_proc* p)
{
	paddr root_tab = p->cr3;
	// la pila utente può essere distrutta subito, se presente
	if (p->livello == LIV_UTENTE)
		distruggi_pila(root_tab, fin_utn_p, DIM_USR_STACK);
	// se p == esecuzione_precedente rimandiamo la distruzione alla
	// carica_stato, altrimenti distruggiamo subito
	ultimo_terminato = root_tab;
	if (p != esecuzione_precedente) {
		// riporta anche ultimo_terminato a zero
		distruggi_pila_precedente();
	}
	rilascia_proc_id(p->id);
	delete p;
}

/*! @brief Carica un handler nella IDT.
 *
 *  Fa in modo che l'handler _irq_ -esimo sia associato alle richieste
 *  di interruzione provenienti dal piedino _irq_ dell'APIC. L'assozione
 *  avviene tramite l'entrata _tipo_ della IDT.
 *
 *  @param tipo 	tipo dell'interruzione a cui associare l'handler
 *  @param irq		piedino dell'APIC associato alla richiesta di interruzione
 */
extern "C" bool load_handler(natq tipo, natq irq);
/// @}

/// @name Distruzione della pila sistema corrente
///
/// Quando dobbiamo eliminare una pila sistema dobbiamo stare attenti a non
/// eliminare proprio quella che stiamo usando.  Questo succede durante una
/// terminate_p() o abort_p(), quando si tenta di distrugguere proprio il
/// processo che ha invocato la primitiva.
///
/// Per fortuna, se stiamo terminando il processo corrente, vuol dire anche che
/// stiamo per metterne in esecuzione un altro e possiamo dunque usare la pila
/// sistema di quest'ultimo. Operiamo dunque nel seguente modo:
///
/// - all'ingresso nel sistema (in salva_stato), salviamo il valore di @ref esecuzione
///   in @ref esecuzione_precedente; questo è il processo a cui appartiene la
///   pila sistema che stiamo usando;
/// - in @ref distruggi_processo(), se @ref esecuzione è uguale a @ref esecuzione_precedente
///   (stiamo distruggendo proprio il processo a cui appartiene la pila corrente),
///   _non_ distruggiamo la pila sistema e settiamo la variabile @ref ultimo_terminato;
/// - in carica_stato, dopo aver cambiato pila, se @ref ultimo_terminato è settato,
///   distruggiamo la pila sistema di @ref esecuzione_precedente.
///
/// @{

/// @brief Processo che era in esecuzione all'entrata nel modulo sistema
///
/// La salva_stato ricorda quale era il processo in esecuzione al momento
/// dell'entrata nel sistema e lo scrive in questa variabile.
des_proc* esecuzione_precedente;

/// @brief Se diverso da zero, indirizzo fisico della root_tab dell'ultimo processo
///        terminato o abortito.
///
/// La carica_stato legge questo indirizzo per sapere se deve distruggere la
/// pila del processo uscente, dopo aver effettuato il passaggio alla pila del
/// processo entrante.
paddr ultimo_terminato;

/*! @brief Distrugge la pila sistema del processo uscente e rilascia la sua tabella radice.
 *
 *  Chiamata da distruggi_processo() oppure da carica_stato se
 *  distruggi_processo() aveva rimandato la distruzione della pila.  Dealloca la
 *  pila sistema e le traduzioni corrispondenti nel TRIE di radice
 *  @ref ultimo_terminato. La distruggi_processo() aveva già eliminato tutte le
 *  altre traduzioni, quindi la funzione può anche deallocare la radice del
 *  TRIE.
 */
extern "C" void distruggi_pila_precedente() {
	distruggi_pila(ultimo_terminato, fin_sis_p, DIM_SYS_STACK);
	// ripuliamo la tabella radice (azione inversa di init_root_tab())
	// in modo da azzerare il contatore delle entrate valide e passare
	// il controllo in rilascia_tab()
	clear_root_tab(ultimo_terminato);
	rilascia_tab(ultimo_terminato);
	ultimo_terminato = 0;
}
/// @}

/// @name Primitive per la creazione e distruzione dei processi (parte C++)
/// @{

/*! @brief Parte C++ della primitiva activate_p()
 *  @param f	corpo del processo
 *  @param a	parametro per il corpo del processo
 *  @param prio	priorità del processo
 *  @param liv	livello del processo (LIV_UTENTE o LIV_SISTEMA)
 */
extern "C" void c_activate_p(void f(natq), natq a, natl prio, natl liv)
{
	des_proc* p;			// des_proc per il nuovo processo
	natl id = 0xFFFFFFFF;		// id da restituire in caso di fallimento

	// non possiamo accettare una priorità minore di quella di dummy
	// o maggiore di quella del processo chiamante
	if (prio < MIN_PRIORITY || prio > esecuzione->precedenza) {
		flog(LOG_WARN, "priorita' non valida: %u", prio);
		c_abort_p();
		return;
	}

	// controlliamo che 'liv' contenga un valore ammesso
	// [segnalazione di E. D'Urso]
	if (liv != LIV_UTENTE && liv != LIV_SISTEMA) {
		flog(LOG_WARN, "livello non valido: %u", liv);
		c_abort_p();
		return;
	}

	// non possiamo creare un processo di livello sistema mentre
	// siamo a livello utente
	if (liv == LIV_SISTEMA && liv_chiamante() == LIV_UTENTE) {
		flog(LOG_WARN, "errore di protezione");
		c_abort_p();
		return;
	}

	// accorpiamo le parti comuni tra c_activate_p e c_activate_pe
	// nella funzione ausiliare crea_processo
	p = crea_processo(f, a, prio, liv);

	if (p != nullptr) {
		inserimento_lista(pronti, p);
		processi++;
		id = p->id;			// id del processo creato
						// (allocato da crea_processo)
		flog(LOG_INFO, "proc=%u entry=%p(%lu) prio=%u liv=%u", id, f, a, prio, liv);
	}

	esecuzione->contesto[I_RAX] = id;
}

/// @brief Parte C++ della pritimiva terminate_p()
/// @param logmsg	set true invia un messaggio sul log
extern "C" void c_terminate_p(bool logmsg)
{
	des_proc* p = esecuzione;

	if (logmsg)
		flog(LOG_INFO, "Processo %u terminato", p->id);
	distruggi_processo(p);
	processi--;
	schedulatore();
}

/// @cond
/// funzioni usate da c_abort_p() per mostrare il backtrace del processo abortito.
/// (definite in sistema.s)
extern "C" void setup_self_dump();
extern "C" void cleanup_self_dump();
/// @endcond

/// @brief Parte C++ della primitiva abort_p()
///
/// Fuziona come la terminate_p(), ma invia anche un warning al log.  La
/// funzione va invocata quando si vuole terminare un processo segnalando che
/// c'è stato un errore.
///
/// @param selfdump	se true mostra sul log lo stato del processo
extern "C" void c_abort_p(bool selfdump)
{
	des_proc* p = esecuzione;

	if (selfdump) {
		setup_self_dump();
		process_dump(esecuzione, LOG_WARN);
		cleanup_self_dump();
	}
	flog(LOG_WARN, "Processo %u abortito", p->id);
	c_terminate_p(/* logmsg= */ false);
}

/*! @brief Parte C++ della primitiva activate_pe().
 *  @param f	corpo del processo
 *  @param a	parametro per il corpo del processo
 *  @param prio	priorità del processo
 *  @param liv	livello del processo (LIV_UTENTE o LIV_SISTEMA)
 *  @param irq	IRQ gestito dal processo
 */
extern "C" void c_activate_pe(void f(natq), natq a, natl prio, natl liv, natb irq)
{
	des_proc*	p;			// des_proc per il nuovo processo
	natw		tipo;			// entrata nella IDT

	esecuzione->contesto[I_RAX] = 0xFFFFFFFF;

	if (prio < MIN_EXT_PRIO || prio > MAX_EXT_PRIO) {
		flog(LOG_WARN, "priorita' non valida: %u", prio);
		return;
	}
	// controlliamo che 'liv' contenga un valore ammesso
	// [segnalazione di F. De Lucchini]
	if (liv != LIV_UTENTE && liv != LIV_SISTEMA) {
		flog(LOG_WARN, "livello non valido: %u", liv);
		return;
	}
	// controlliamo che 'irq' sia valido prima di usarlo per indicizzare
	// 'a_p'
	if (irq >= apic::MAX_IRQ) {
		flog(LOG_WARN, "irq %hhu non valido (max %u)", irq, apic::MAX_IRQ);
		return;
	}
	// se a_p è non-nullo, l'irq è già gestito da un altro processo
	// esterno o da un driver
	if (a_p[irq]) {
		flog(LOG_WARN, "irq %hhu occupato", irq);
		return;
	}
	// anche il tipo non deve essere già usato da qualcos'altro.
	// Controlliamo quindi che il gate corrispondente non sia marcato
	// come presente (bit P==1)
	tipo = prio - MIN_EXT_PRIO;
	if (gate_present(tipo)) {
		flog(LOG_WARN, "tipo %hx occupato", tipo);
		return;
	}

	p = crea_processo(f, a, prio, liv);
	if (p == 0)
		return;

	// creiamo il collegamento irq->tipo->handler->processo esterno
	// irq->tipo (tramite l'APIC)
	apic::set_VECT(irq, tipo);
	// associazione tipo->handler (tramite la IDT)
	// Nota: in sistema.s abbiamo creato un handler diverso per ogni
	// possibile irq. L'irq che passiamo a load_handler serve ad
	// identificare l'handler che ci serve.
	load_handler(tipo, irq);
	// associazione handler->processo esterno (tramite 'a_p')
	a_p[irq] = p;

	// ora che tutti i collegamenti sono stati creati possiamo
	// iniziare a ricevere interruzioni da irq. Smascheriamo
	// dunque le richieste irq nell'APIC
	apic::set_MIRQ(irq, false);

	flog(LOG_INFO, "estern=%u entry=%p(%lu) prio=%u (tipo=%2x) liv=%u irq=%hhu",
			p->id, f, a, prio, tipo, liv, irq);

	esecuzione->contesto[I_RAX] = p->id;
	return;
}

/// @}
/// @}
/// @}

///////////////////////////////////////////////////////////////////////////////////
/// @defgroup init		Inizializzazione
///
/// L'inizializzazione è in due parti:
///
/// 1. la funzione main() inizializza le strutture dati necessarie alla creazione
///    dei primi processi, quindi crea @ref dummy e @ref main_sistema, cedendo il
///    controllo a quest'ultimo (@ref salta_a_main);
///
/// 2. @ref main_sistema si occupa della seconda parte, che inizializza il modulo I/O e crea
///    il processo main utente, quindi termina cedendo il controllo a quest'ultimo.
///
/// @{
///////////////////////////////////////////////////////////////////////////////////

/// @name Costanti per lo heap di sistema
/// @{

/// Indirizzo base dello heap di sistema.
const natq HEAP_START = 1*MiB;

/// Dimensione dello heap di sistema.
const natq HEAP_SIZE = 1*MiB;
/// @}

/// Un primo des_proc, allocato staticamente, da usare durante l'inizializzazione.
des_proc init;

/*! @brief Crea il processo dummy
 *  @return id del processo
 */
natl crea_dummy()
{
	des_proc* di = crea_processo(dummy, 0, DUMMY_PRIORITY, LIV_SISTEMA);
	if (di == 0) {
		flog(LOG_ERR, "Impossibile creare il processo dummy");
		return 0xFFFFFFFF;
	}
	inserimento_lista(pronti, di);
	return di->id;
}

/*! @brief Corpo del processo main_sistema.
 *
 *  Si occupa della seconda parte dell'inizializzazione.
 */
void main_sistema(natq);

/*! @brief Crea il processo main_sistema
 *  @return id del processo
 */
natl crea_main_sistema()
{
	des_proc* m = crea_processo(main_sistema, 0, MAX_EXT_PRIO, LIV_SISTEMA);
	if (m == 0) {
		flog(LOG_ERR, "Impossibile creare il processo main_sistema");
		return 0xFFFFFFFF;
	}
	inserimento_lista(pronti, m);
	processi++;
	return m->id;
}

/// Periodo del timer di sistema.
const natl DELAY = 59659;

/// @brief Crea le parti utente/condivisa e io/condivisa
///
/// @note Setta le variabili @ref user_entry e @ref io_entry.
/// @param root_tab indirizzo fisico della tabella radice
/// @param mod informazioni sui moduli caricati dal boot loader
/// @return true in caso di successo, false altrimenti
bool crea_spazio_condiviso(paddr root_tab, boot64_modinfo* mod);

/// @brief Funzione di supporto pr avviare il primo processo (definita in sistema.s)
extern "C" void salta_a_main();

/// Indirizzo fisico del puntatore alla pila sistema nel segmento TSS.
extern paddr tss_punt_nucleo;

/*! @brief Parte C++ della primitiva fill_gate().
 *  @param tipo		tipo del gate da riempire
 *  @param routine	funzione da associare al gate
 *  @param liv		DPL del gate (LIV_UTENTE o LIV_SISTEMA)
 */
extern "C" void c_fill_gate(natb tipo, void routine(), int liv)
{
	esecuzione->contesto[I_RAX] = false;

	if ( (tipo & 0xF0) != 0x40) {
		flog(LOG_WARN, "tipo non valido %#02x (deve essere 0x4*)", tipo);
		return;
	}

	if (gate_present(tipo)) {
		flog(LOG_WARN, "gate %#02x occupato", tipo);
		return;
	}

	if (liv != LIV_UTENTE && liv != LIV_SISTEMA) {
		flog(LOG_WARN, "livello %d non valido", liv);
		return;
	}

	gate_init(tipo, routine, true /* tipo trap */, liv);

	esecuzione->contesto[I_RAX] = true;
}

/*! @brief Prima parte dell'inizializazione; crea i primi processi.
 *  @param info informazioni passata dal boot loader di libce
 */
extern "C" void main(boot64_info* info)
{
	natl mid, dummy_id;

	// anche se il primo processo non è completamente inizializzato,
	// gli diamo un identificatore, in modo che compaia nei log
	init.id = 0xFFFF;
	init.precedenza = MAX_PRIORITY;
	init.cr3 = readCR3();
	esecuzione = &init;
	esecuzione_precedente = esecuzione;

	flog(LOG_INFO, "Nucleo di Calcolatori Elettronici, v7.1.1");

	tss_punt_nucleo = info->tss_punt_nucleo;
	// Usiamo come heap la parte di memoria nel primo MiB non occupata dalle
	// strutture dati allocate dal boot loader. Il boot loader ci ha passato
	// in info->memlibera l'indirizzo del primo chunk libero.
	// (In realtà solo fino ai primi 640KiB perché in [640KiB, 1MiB) sono
	// mappate altre cose, tra cui la memoria video).
	heap_init(0, 0, info->memlibera);
	flog(LOG_INFO, "Heap del modulo sistema: [%llx, %llx)", DIM_PAGINA, 640*KiB);

	// iizializziamo la parte M2
	init_frame();
	flog(LOG_INFO, "Numero di frame: %lu (M1) %lu (M2)", N_M1, N_M2);

	flog(LOG_INFO, "Suddivisione della memoria virtuale:");
	flog(LOG_INFO, "- sis/cond [%16lx, %16lx)", ini_sis_c, fin_sis_c);
	flog(LOG_INFO, "- sis/priv [%16lx, %16lx)", ini_sis_p, fin_sis_p);
	flog(LOG_INFO, "- io /cond [%16lx, %16lx)", ini_mio_c, fin_mio_c);
	flog(LOG_INFO, "- usr/cond [%16lx, %16lx)", ini_utn_c, fin_utn_c);
	flog(LOG_INFO, "- usr/priv [%16lx, %16lx)", ini_utn_p, fin_utn_p);

	// Le parti sis/priv e usr/priv verranno create da crea_processo() ogni
	// volta che si attiva un nuovo processo.  La parte sis/cond contiene
	// la finestra FM, creata dal boot loader.  Le parti io/cond e usr/cond
	// devono contenere i segmenti ELF dei moduli I/O e utente,
	// rispettivamente. In questo momento le copie di questi due file ELF
	// si trovano in memoria nel secondo MiB (caricate dal primo boot
	// loader).  Estraiamo i loro segmenti codice e dati e li copiamo in
	// frame di M2, creando contestualmente le necessarie traduzioni.
	// Creiamo queste traduzioni una sola volta all'avvio (adesso) e poi le
	// condividiamo tra tutti i processi.
	if (!crea_spazio_condiviso(init.cr3, info->mod))
		goto error;
	flog(LOG_INFO, "Frame liberi: %lu (M2)", num_frame_liberi);

	// creazione del processo dummy
	dummy_id = crea_dummy();
	if (dummy_id == 0xFFFFFFFF)
		goto error;
	flog(LOG_INFO, "Creato il processo dummy (id = %u)", dummy_id);

	// creazione del processo main_sistema
	mid = crea_main_sistema();
	if (mid == 0xFFFFFFFF)
		goto error;
	flog(LOG_INFO, "Creato il processo main_sistema (id = %u)", mid);

	// il resto dell'inizializzazione prosegue nel processo main_sistema(),
	// che può sospendersi e quindi è più comodo da usare. Fino ad ora le
	// richieste di interruzione esterne sono rimaste disabilitate (IF=0 in
	// RFLAGS), ma nel momento in cui passeremo a main_sistema() verranno
	// abilitate, quindi dobbiamo preoccuparci di inizializzare l'APIC.
	flog(LOG_INFO, "Inizializzo l'APIC");
	apic::init(); // in libce

	flog(LOG_INFO, "Cedo il controllo al processo main sistema...");
	// ora possiamo passare a main_sistema(), che in questo momento è
	// in testa alla coda pronti.
	// Lo selezioniamo:
	schedulatore();
	// e poi carichiamo il suo stato. La funzione salta_a_main(), definita
	// in sistema.s, contiene solo 'call carica_stato; iretq'.
	salta_a_main();

error:
	panic("Errore di inizializzazione");
}

/// @brief Entry point del modulo IO
///
/// @note Inizializzato da crea_spazio_condiviso().
void (*io_entry)(natq);

/// @brief Entry point del modulo utente
///
/// @note Inizializzato da crea_spazio_condiviso().
void (*user_entry)(natq);

/// @brief Corpo del processo main_sistema
void main_sistema(natq)
{
	// ora che abbiamo una nuova pila possiamo riutilizzare la memoria
	// occupata dal boot loader e dalle copie originali dei 3 moduli
	// Aggiungiamo questa memoria allo heap di sistema.
	heap_init(voidptr_cast(HEAP_START), HEAP_SIZE);
	flog(LOG_INFO, "Heap del modulo sistema: aggiunto [%lx, %lx)",
			HEAP_START, HEAP_START + HEAP_SIZE);

	// attiviamo il timer, in modo che i processi di inizializzazione
	// possano usare anche delay(), se ne hanno bisogno.
	// occupiamo a_p[2] (in modo che non possa essere sovrascritta
	// per errore tramite activate_pe()) e smascheriamo il piedino
	// 2 dell'APIC
	flog(LOG_INFO, "Attivo il timer (DELAY=%u)", DELAY);
	a_p[2] = ESTERN_BUSY;
	apic::set_VECT(2, INTR_TIPO_TIMER);
	apic::set_MIRQ(2, false);
	timer::start0(DELAY);

	// inizializzazione del modulo di io
	// Creiamo un processo che esegua la procedura start del modulo I/O.
	// Usiamo un semaforo di sincronizzazione per sapere quando
	// l'inizializzazione è terminata.
	flog(LOG_INFO, "Creo il processo main I/O");
	natl id;
	natl sync_io = sem_ini(0);
	if (sync_io == 0xFFFFFFFF) {
		flog(LOG_ERR, "Impossibile allocare il semaforo di sincr per l'IO");
		goto error;
	}
	id = activate_p(io_entry, sync_io, MAX_EXT_PRIO, LIV_SISTEMA);
	if (id == 0xFFFFFFFF) {
		flog(LOG_ERR, "impossibile creare il processo main I/O");
		goto error;
	}
	flog(LOG_INFO, "Attendo inizializzazione modulo I/O...");
	sem_wait(sync_io);

	// creazione del processo main utente
	flog(LOG_INFO, "Creo il processo main utente");
	id = activate_p(user_entry, 0, MAX_PRIORITY, LIV_UTENTE);
	if (id == 0xFFFFFFFF) {
		flog(LOG_ERR, "impossibile creare il processo main utente");
		goto error;
	}

	// terminazione
	flog(LOG_INFO, "Cedo il controllo al processo main utente...");
	terminate_p();

error:
	panic("Errore di inizializzazione");
}

///////////////////////////////////////////////////////////////////////////////////
/// @defgroup mod	Caricamento dei moduli I/O e utente
///
/// All'avvio troviamo il contenuto dei file `sistema`, `io` e `utente` già
/// copiati in memoria dal primo boot loader (quello realizzato da QEMU stesso).
/// Il secondo boot loader (quello della libce) ha caricato nella posizione
/// finale solo il modulo sistema. Ora il modulo sistema deve rendere operativi
/// anche i moduli IO e utente, interpretando i file e creando le necessarie
/// traduzioni nella memoria virtuale.
/// Per farlo dobbiamo esaminare le righe di tipo PT_LOAD delle tabelle di programma
/// dei due file. Ciascuna di queste righe ci indica:
///
/// - una parte _S_ del file da mappare in memoria, espressa come offset
///   dall'inizio del file e dimensone in byte (_S_ è un segmento ELF);
///
/// - l'indirizzo virtuale _V_ a cui mappare _S_ e il numero di byte di memoria virtuale da
///   occupare (può essere più grande della dimensione di _S_, e in quel caso la parte
///   eccedente deve essere azzerata)
///
/// - i diritti di accesso (lettura, scrittura, esecuzione) da garantire.
///
/// Per poter creare le traduzioni da _V_ a _S_ dobbiamo copiare _S_ da dove si
/// trova ora in dei frame di M2, per almeno due motivi: la copia attuale di _S_
/// potrebbe non essere allineata correttamente; inoltre, potremmo non avere
/// spazio per azzerare l'eventuale parte eccedente.
///
/// @{
///////////////////////////////////////////////////////////////////////////////////
#include <elf64.h>

/// @brief Oggetto da usare con map() per caricare un segmento ELF in memoria virtuale.
struct copy_segment {
	// Il segmento si trova in memoria agli indirizzi (fisici) [mod_beg, mod_end)
	// e deve essere visibile in memoria virtuale a partire dall'indirizzo
	// virt_beg. Il segmento verrà copiato (una pagina alla volta) in
	// frame liberi di M2. La memoria precedentemente occupata dal modulo
	// sarà poi riutilizzata per lo heap di sistema.

	/// base del segmento in memoria fisica
	paddr mod_beg;
	/// limite del segmento in memoria fisica
	paddr mod_end;
	/// indirizzo virtuale della base del segmento
	vaddr virt_beg;

	paddr operator()(vaddr);
};

/*! @brief Funzione chiamata da map().
 *
 *  Copia il prossimo frame di un segmento in un frame di M2.
 *  @param v indirizzo virtuale da mappare
 *  @return indirizzo fisico del frame di M2
 */
paddr copy_segment::operator()(vaddr v)
{
	// allochiamo un frame libero in cui copiare la pagina
	paddr dst = alloca_frame();
	if (dst == 0)
		return 0;

	// offset della pagina all'interno del segmento
	natq offset = v - virt_beg;
	// indirizzo della pagina all'interno del modulo
	paddr src = mod_beg + offset;

	// il segmento in memoria può essere più grande di quello nel modulo.
	// La parte eccedente deve essere azzerata.
	natq tocopy = DIM_PAGINA;
	if (src > mod_end)
		tocopy = 0;
	else if (mod_end - src < DIM_PAGINA)
		tocopy =  mod_end - src;
	if (tocopy > 0)
		memcpy(voidptr_cast(dst), voidptr_cast(src), tocopy);
	if (tocopy < DIM_PAGINA)
		memset(voidptr_cast(dst + tocopy), 0, DIM_PAGINA - tocopy);
	return dst;
}

/*! @brief Carica un modulo in M2.
 *
 *  Copia il modulo in M2, lo mappa al suo indirizzo virtuale e
 *  aggiunge lo heap dopo l'ultimo indirizzo virtuale usato.
 *
 *  @param mod	informazioni sul modulo caricato dal boot loader
 *  @param root_tab indirizzo fisico della radice del TRIE
 *  @param flags	BIT_US per rendere il modulo accessibile da livello utente,
 *  		altrimenti 0
 *  @param heap_size dimensione dello heap (in byte)
 *  @return indirizzo virtuale dell'entry point del modulo, o zero
 *  	   in caso di errore
 */
vaddr carica_modulo(boot64_modinfo* mod, paddr root_tab, natq flags, natq heap_size)
{
	// puntatore all'intestazione ELF
	Elf64_Ehdr* elf_h  = ptr_cast<Elf64_Ehdr>(mod->mod_start);
	// indirizzo fisico della tabella dei segmenti
	paddr ph_addr = mod->mod_start + elf_h->e_phoff;
	// ultimo indirizzo virtuale usato
	vaddr last_vaddr = 0;
	// esaminiamo tutta la tabella dei segmenti
	for (int i = 0; i < elf_h->e_phnum; i++) {
		Elf64_Phdr* elf_ph = ptr_cast<Elf64_Phdr>(ph_addr);

		// ci interessano solo i segmenti di tipo PT_LOAD
		if (elf_ph->p_type != PT_LOAD)
			continue;

		// i byte che si trovano ora in memoria agli indirizzi (fisici)
		// [mod_beg, mod_end) devono diventare visibili nell'intervallo
		// di indirizzi virtuali [virt_beg, virt_end).
		vaddr	virt_beg = elf_ph->p_vaddr,
			virt_end = virt_beg + elf_ph->p_memsz;
		paddr	mod_beg  = mod->mod_start + elf_ph->p_offset,
			mod_end  = mod_beg + elf_ph->p_filesz;

		// se necessario, allineiamo alla pagina gli indirizzi di
		// partenza e di fine
		natq page_offset = virt_beg & (DIM_PAGINA - 1);
		virt_beg -= page_offset;
		mod_beg  -= page_offset;
		virt_end = allinea(virt_end, DIM_PAGINA);

		// aggiorniamo l'ultimo indirizzo virtuale usato
		if (virt_end > last_vaddr)
			last_vaddr = virt_end;

		// settiamo BIT_RW nella traduzione solo se il segmento è
		// scrivibile
		if (elf_ph->p_flags & PF_W)
			flags |= BIT_RW;

		// mappiamo il segmento
		if (map(root_tab,
			virt_beg,
			virt_end,
			flags,
			copy_segment{mod_beg, mod_end, virt_beg}) != virt_end)
			return 0;

		flog(LOG_INFO, " - segmento %s %s mappato a [%16lx, %16lx)",
				(flags & BIT_US) ? "utente " : "sistema",
				(flags & BIT_RW) ? "read/write" : "read-only ",
				virt_beg, virt_end);

		// passiamo alla prossima entrata della tabella dei segmenti
		ph_addr += elf_h->e_phentsize;
	}
	// dopo aver mappato tutti i segmenti, mappiamo lo spazio destinato
	// allo heap del modulo. I frame corrispondenti verranno allocati da
	// alloca_frame()
	if (map(root_tab,
		last_vaddr,
		last_vaddr + heap_size,
		flags | BIT_RW,
		[](vaddr) { return alloca_frame(); }) != last_vaddr + heap_size)
		return 0;
	flog(LOG_INFO, " - heap:                                 [%16lx, %16lx)",
				last_vaddr, last_vaddr + heap_size);
	flog(LOG_INFO, " - entry point: 0x%lx", elf_h->e_entry);
	return elf_h->e_entry;
}

/*! @brief Mappa il modulo I/O.
 *  @param mod informazioni sul modulo caricato dal boot loader
 *  @param root_tab indirizzo fisico della radice del TRIE
 *  @return indirrizzo virtuale dell'entry point del modulo I/O,
 *  	   o zero in caso di errore
 */
vaddr carica_IO(boot64_modinfo* mod, paddr root_tab)
{
	flog(LOG_INFO, "mappo il modulo I/O:");
	return carica_modulo(mod, root_tab, 0, DIM_IO_HEAP);
}

/*! @brief Mappa il modulo utente.
 *  @param mod informazioni sul modulo caricato dal boot loader
 *  @param root_tab indirizzo fisico della radice del TRIE
 *  @return indirrizzo virtuale dell'entry point del modulo utente,
 *  	   o zero in caso di errore
 */
vaddr carica_utente(boot64_modinfo* mod, paddr root_tab)
{
	flog(LOG_INFO, "mappo il modulo utente:");
	return carica_modulo(mod, root_tab, BIT_US, DIM_USR_HEAP);
}

/// @cond
/// sezioni exception-handler dei moduli
///
/// Utilizzate dalle funzioni di stack-unwinding in libce, per implementare il
/// backtrace.
///
vaddr sis_eh_frame;
natq  sis_eh_frame_len;
vaddr mio_eh_frame;
natq  mio_eh_frame_len;
vaddr utn_eh_frame;
natq  utn_eh_frame_len;
/// @endcond

/// @cond
bool crea_spazio_condiviso(paddr root_tab, boot64_modinfo* mod)
{
	io_entry = ptr_cast<void(natq)>(carica_IO(&mod[1], root_tab));
	if (!io_entry)
		return false;
	user_entry = ptr_cast<void(natq)>(carica_utente(&mod[2], root_tab));
	if (!user_entry)
		return false;

	// per il supporto al backtrace
	find_eh_frame(mod[0].mod_start, sis_eh_frame, sis_eh_frame_len);
	find_eh_frame(mod[1].mod_start, mio_eh_frame, mio_eh_frame_len);
	find_eh_frame(mod[2].mod_start, utn_eh_frame, utn_eh_frame_len);

	return true;
}
/// @endcond
/// @}
/// @}

///////////////////////////////////////////////////////////////////////////////////
/// @defgroup err 	Gestione errori
/// @{
///////////////////////////////////////////////////////////////////////////////////

/*! @brief Ferma il sistema e stampa lo stato di tutti i processi
 *  @param msg messaggio da inviare al log (severità LOG_ERR)
 */
extern "C" void panic(const char* msg)
{
	static int in_panic = 0;

	if (in_panic) {
		flog(LOG_ERR, "panic ricorsivo. STOP");
		end_program();
	}
	in_panic = 1;

	flog(LOG_ERR, "PANIC: %s", msg);
	if (esecuzione_precedente) {
		flog(LOG_ERR, "  processi: %u", processi);
		flog(LOG_ERR, "------------------------------ PROCESSO IN ESECUZIONE -------------------------------");
		setup_self_dump();
		process_dump(esecuzione_precedente, LOG_ERR);
		cleanup_self_dump();
		flog(LOG_ERR, "---------------------------------- ALTRI PROCESSI -----------------------------------");
		for (natl id = 0; id < MAX_PROC; id ++) {
			if (proc_table[id] && proc_table[id] != esecuzione_precedente)
				process_dump(proc_table[id], LOG_ERR);
		}
	}
	end_program();
}

/*! @brief Parte C++ della primitiva io_panic()
 *
 *  Il modulo I/O può usare questa primitiva per segnalare un errore fatale.
 */
extern "C" void c_io_panic()
{
	panic("errore fatale nel modulo I/O");
}

/*! @brief Routine di risposta a un non-maskable-interrupt
 *
 *  La routine ferma il sistema e stampa lo stato di tutti i processi.
 *  @note Il sito dell'autocorrezione invia un nmi se il programma da
 *  testare non termina entro il tempo prestabilito.
 */
extern "C" void c_nmi()
{
	panic("INTERRUZIONE FORZATA");
}

#ifdef AUTOCORR
int MAX_LOG = 4;
#else
/// Massimo livello ammesso per la severità dei messaggi del log
int MAX_LOG = 5;
#endif

/*! @brief Parte C++ della primitiva do_log().
 *
 *  @param sev severità del messaggio
 *  @param buf buffer che contiene il messaggio
 *  @param quanti lunghezza del messaggio in byte
 */
extern "C" void c_do_log(log_sev sev, const char* buf, natl quanti)
{
	if (liv_chiamante() == LIV_UTENTE &&
			!c_access(int_cast<vaddr>(buf), quanti, false, false)) {
		flog(LOG_WARN, "log: parametri non validi");
		c_abort_p();
		return;
	}
	if (sev > MAX_LOG) {
		flog(LOG_WARN, "log: livello di warning errato");
		c_abort_p();
		return;
	}
	do_log(sev, buf, quanti);
}

/// @brief Parte C++ della primitiva getmeminfo().
extern "C" void c_getmeminfo()
{
	meminfo m;

	// byte liberi nello heap di sistema
	m.heap_libero = disponibile();
	// numero di frame nella lista dei frame liberi
	m.num_frame_liberi = num_frame_liberi;
	// id del processo in esecuzione
	m.pid = esecuzione->id;

	memcpy(&esecuzione->contesto[I_RAX], &m, sizeof(natq));
	memcpy(&esecuzione->contesto[I_RDX], &m.pid, sizeof(natq));
}

/// @name Funzioni di supporto per il backtrace
/// @{
#include <cfi.h>

/*! @brief Callback invocata dalla funzione cfi_backstep() per leggere
 *          dalla pila di un qualunque processo.
 *  @param token 	(opaco) descrittore del processo di cui si
 *  			sta producendo il backtrace
 *  @param v		indirizzo virtuale da leggere
 *  @return		natq letto da _v_ nella memoria virtuale del
 *  			processo (0 se non mappato)
 */
natq read_mem(void* token, vaddr v)
{
	des_proc* p = static_cast<des_proc*>(token);
	paddr pa = trasforma(p->cr3, v);
	natq rv = 0;
	if (pa) {
		memcpy(&rv, voidptr_cast(pa), sizeof(rv));
	}
	return rv;
}

/*! @brief Invia sul log il backtrace di un processo.
 *
 *  @param p	descrittore del processo
 *  @param sev	severità dei messaggi da inviare al log
 *  @param msg	primo messaggio da inviare (intestazione)
 */
void backtrace(des_proc* p, log_sev sev, const char* msg)
{
	cfi_d cfi;

	cfi.regs[CFI::RAX] = p->contesto[I_RAX];
	cfi.regs[CFI::RCX] = p->contesto[I_RCX];
	cfi.regs[CFI::RDX] = p->contesto[I_RDX];
	cfi.regs[CFI::RBX] = p->contesto[I_RBX];
	cfi.regs[CFI::RSP] = read_mem(p, p->contesto[I_RSP] + 24);
	cfi.regs[CFI::RBP] = p->contesto[I_RBP];
	cfi.regs[CFI::RSI] = p->contesto[I_RSI];
	cfi.regs[CFI::RDI] = p->contesto[I_RDI];
	cfi.regs[CFI::R8]  = p->contesto[I_R8];
	cfi.regs[CFI::R9]  = p->contesto[I_R9];
	cfi.regs[CFI::R10] = p->contesto[I_R10];
	cfi.regs[CFI::R11] = p->contesto[I_R11];
	cfi.regs[CFI::R12] = p->contesto[I_R12];
	cfi.regs[CFI::R13] = p->contesto[I_R13];
	cfi.regs[CFI::R14] = p->contesto[I_R14];
	cfi.regs[CFI::R15] = p->contesto[I_R15];

	cfi.token = p;
	cfi.read_stack = read_mem;

	vaddr rip = read_mem(p, p->contesto[I_RSP]) - 1;
	do {
		if (rip >= ini_sis_c && rip < fin_sis_c) {
			cfi.eh_frame = sis_eh_frame;
			cfi.eh_frame_len = sis_eh_frame_len;
		} else if (rip >= ini_mio_c && rip < fin_mio_c) {
			cfi.eh_frame = mio_eh_frame;
			cfi.eh_frame_len = mio_eh_frame_len;
		} else if (rip >= ini_utn_c && rip < fin_utn_c) {
			cfi.eh_frame = utn_eh_frame;
			cfi.eh_frame_len = utn_eh_frame_len;
		} else {
			cfi.eh_frame = 0;
			cfi.eh_frame_len = 0;
		}

		if (!cfi.eh_frame)
			break;

		if (!cfi_backstep(cfi, rip))
			break;

		rip = cfi.regs[CFI::RA];

		if (!rip)
			break;

		rip--;

		flog(sev, "%s0x%lx", msg, rip);
	} while (true);
}

/*! @brief Invia sul log lo stato di un processo
 *
 *  @param p	descrittore del processo
 *  @param sev	severità dei messaggi da inviare al log
 */
void process_dump(des_proc* p, log_sev sev)
{
	natq* pila = ptr_cast<natq>(trasforma(p->cr3, p->contesto[I_RSP]));

	flog(sev, "proc %u: corpo %p(%lu), livello %s, precedenza %u", p->id, p->corpo, p->parametro,
			p->livello == LIV_UTENTE ? "UTENTE" : "SISTEMA", p->precedenza);
	if (pila) {
		flog(sev, "  RIP=0x%lx CPL=%s", pila[0], pila[1] == SEL_CODICE_UTENTE ? "LIV_UTENTE" : "LIV_SISTEMA");
		natq rflags = pila[2];
		flog(sev, "  RFLAGS=%lx [%s %s %s %s %s %s %s %s %s %s, IOPL=%s]",
			rflags,
			(rflags & 1U << 14) ? "NT" : "--",
			(rflags & 1U << 11) ? "OF" : "--",
			(rflags & 1U << 10) ? "DF" : "--",
			(rflags & 1U << 9)  ? "IF" : "--",
			(rflags & 1U << 8)  ? "TF" : "--",
			(rflags & 1U << 7)  ? "SF" : "--",
			(rflags & 1U << 6)  ? "ZF" : "--",
			(rflags & 1U << 4)  ? "AF" : "--",
			(rflags & 1U << 2)  ? "PF" : "--",
			(rflags & 1U << 0)  ? "CF" : "--",
			(rflags & 0x3000) == 0x3000 ? "UTENTE" : "SISTEMA");
	} else {
		flog(sev, "  impossibile leggere la pila del processo");
	}
	flog(sev, "  RAX=%16lx RBX=%16lx RCX=%16lx RDX=%16lx",
			p->contesto[I_RAX],
			p->contesto[I_RBX],
			p->contesto[I_RCX],
			p->contesto[I_RDX]);
	flog(sev, "  RDI=%16lx RSI=%16lx RBP=%16lx RSP=%16lx",
			p->contesto[I_RDI],
			p->contesto[I_RSI],
			p->contesto[I_RBP],
			pila ? pila[3] : 0);
	flog(sev, "  R8 =%16lx R9 =%16lx R10=%16lx R11=%16lx",
			p->contesto[I_R8],
			p->contesto[I_R9],
			p->contesto[I_R10],
			p->contesto[I_R11]);
	flog(sev, "  R12=%16lx R13=%16lx R14=%16lx R15=%16lx",
			p->contesto[I_R12],
			p->contesto[I_R13],
			p->contesto[I_R14],
			p->contesto[I_R15]);
	if (pila) {
		flog(sev, "  backtrace:");
		backtrace(p, sev, "  > ");
	}
}
/// @}
/// @}
/// @}

// ( ESAME 2024-01-29
/// @addtogroup esame
/// @{

/**
 * @brief Riumove un processo dalla coda del timer.
 *
 * @param p	descrittore del processo da rimuovere
 */
void rimozione_lista_attesa(des_proc* p)
{
	richiesta** r;

	for (r = &sospesi; *r && (*r)->pp != p; r = &(*r)->p_rich)
		;
	if (richiesta* t = *r) {
		if ( (*r = t->p_rich) )
			(*r)->d_attesa += t->d_attesa;
		delete t;
	}
}

/// Descrittore di barriera
struct barrier_t {
	/// Numero di processi che devono sincronizzarsi
	natl nproc;
	/// Numero di processi già arrivati
	natl narrived;
	/// Timeout richiesto per questa barriera
	natl timeout;
	/// True se la barriera è nello stato erroneo
	bool bad;
	/// Coda dei processi che attendono l'apertura della barriera
	des_proc *waiting;
	/// Puntatore al primo processo arrivato alla barriera
	des_proc *first;
};

/// Array dei descrittori di barriera
barrier_t barriers[MAX_BARRIERS];
/// Numero di descrittori di barriera utilizzati
natl barrier_nextid = 0;

/// Parte C++ della primitiva barrier_create()
extern "C" void c_barrier_create(natl nproc, natl to)
{
	if (!to  || !nproc) {
		flog(LOG_WARN, "parametri non validi");
		c_abort_p();
		return;
	}

	if (barrier_nextid >= MAX_BARRIERS) {
		flog(LOG_WARN, "create troppe barriere");
		esecuzione->contesto[I_RAX] = 0xFFFFFFFF;
		return;
	}

	esecuzione->contesto[I_RAX] = barrier_nextid;
	barrier_t *b = &barriers[barrier_nextid];
	barrier_nextid++;

	b->nproc = nproc;
	b->narrived = 0;
	b->timeout = to;
	b->bad = false;
	b->first = nullptr;
	b->waiting = nullptr;
}

void check_barrier(des_proc *p)
{
	if (p->barrier_id == 0xFFFFFFFF)
		return;


	barrier_t *b = &barriers[p->barrier_id];
	p->barrier_id = 0xFFFFFFFF;

	while (b->waiting) {
		des_proc *work = rimozione_lista(b->waiting);
		work->contesto[I_RAX] = false;
		// il processo corrente verrà inserito in lista
		// pronti dal driver del timer, quindi dobbiamo
		// stare attendi a non inserirlo due volte
		if (work != p)
			inserimento_lista(pronti, work);
	}
	if (b->narrived == b->nproc) {
		b->first = nullptr;
		b->narrived = 0;
	} else {
		b->bad = true;
	}
//   SOLUZIONE 2024-01-29 )
}

/// Parte C++ della primitiva barrier()
extern "C" void c_barrier(natl id)
{
/// @todo Scrivere il corpo della funzione.
// ( SOLUZIONE 2024-01-29
if (id >= barrier_nextid) {
		flog(LOG_WARN, "id non valido: %d", id);
		c_abort_p();
		return;
	}

	barrier_t *b = &barriers[id];

	b->narrived++;

	if (b->bad) {
		esecuzione->contesto[I_RAX] = false;
		if (b->narrived == b->nproc) {
			b->bad = false;
			b->narrived = 0;
		}
		return;
	}

	if (!b->first) {
		b->first = esecuzione;
		esecuzione->barrier_id = id;
		richiesta* p = new richiesta;
		p->d_attesa = b->timeout;
		p->pp = esecuzione;
		inserimento_lista_attesa(p);
	}

	inserimento_lista(b->waiting, esecuzione);
	if (b->narrived == b->nproc) {
		rimozione_lista_attesa(b->first);
		b->first->barrier_id = 0xFFFFFFFF;
		b->first = nullptr;
		while (b->waiting) {
			des_proc *work = rimozione_lista(b->waiting);
			work->contesto[I_RAX] = true;
			inserimento_lista(pronti, work);
		}
		b->narrived = 0;
	}
	schedulatore();
//   SOLUZIONE 2024-01-29 )
}
/// @}
//   ESAME 2024-01-29 )
