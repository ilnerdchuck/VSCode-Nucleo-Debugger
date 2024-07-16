/// @file costanti.h
/// @brief File incluso da tutti i moduli, sia nella parte C++ che nella parte assembler.
/// @defgroup esame ESAME

/// @name limiti modificabili
///
/// Queste costanti definiscono alcune dimensioni o valori che possono essere modificati
/// liberamente, entro limiti ragionevoli.
/// @{

/// massimo numero di semafori per livello
#define MAX_SEM			1024UL
/// massimo numero di processi
#define MAX_PROC 		1024UL
/// priorità minima dei processi esterni
#define MIN_EXT_PRIO		1024UL
/// dimensione della memoria fisica
#define MEM_TOT			(32*MiB)
/// dimensione dello heap utente
#define DIM_USR_HEAP		(1*MiB)
/// dimensione degli stack utente
#define DIM_USR_STACK		(64*KiB)
/// dimensione dello heap del modulo I/O
#define DIM_IO_HEAP		(1*MiB)
/// dimensione degli stack sistema
#define DIM_SYS_STACK		(4*KiB)
/// numero massimo di PRD usati da dmaread/dmawrite
#define MAX_PRD			16
// ( ESAME 2024-01-29
/// @addtogroup esame
/// @{

/// Massimo numero di barriere
#define MAX_BARRIERS		100
/// @}
//   ESAME 2024-01-29 )
/// @}

/// @name Tipi delle primitive
/// @{

/// @name Primitive comuni
///
/// Tipi delle primitive dichiarate in `sys.h`
/// @{
#define TIPO_A			0x20	///< activate_p()
#define TIPO_T			0x21	///< terminate_p()
#define TIPO_SI			0x22	///< sem_ini()
#define TIPO_W			0x23	///< sem_wait()
#define TIPO_S			0x24	///< sem_signal()
#define TIPO_D			0x25	///< delay()
#define TIPO_L			0x26	///< do_log()
#define TIPO_GMI		0x27	///< getmeminfo()
// ( ESAME 2024-01-29
/// @addtogroup esame
/// @{

#define TIPO_BC			0x2a	///< barrier_create()
#define TIPO_B			0x2c	///< barrier()
/// @}
//   ESAME 2024-01-29 )
/// @}

/// @name Primitive riservate per il modulo I/O
///
/// Tipi delle primitive dichiarate in `sysio.h`
/// @{
#define TIPO_APE		0x30	///< activate_pe()
#define TIPO_WFI		0x31	///< wfi()
#define TIPO_FG			0x32	///< fill_gate()
#define TIPO_AB			0x33	///< abort_p()
#define TIPO_IOP		0x34	///< io_panic()
#define TIPO_TRA		0x35	///< trasforma()
#define TIPO_ACC		0x36	///< access()
/// @}

/// @name Primitive fornite dal modulo I/O
///
/// Tipi delle primitive dichiarate in `io.h`
/// @{
#define IO_TIPO_HDR		0x40	///< readhd_n()
#define IO_TIPO_HDW		0x41	///< writehd_n()
#define IO_TIPO_DMAHDR		0x42	///< dmareadhd_n()
#define IO_TIPO_DMAHDW		0x43	///< dmawritehd_n()
#define IO_TIPO_RCON		0x44	///< readconsole()
#define IO_TIPO_WCON		0x45	///< writeconsole()
#define IO_TIPO_INIC		0x46	///< iniconsole()
#define IO_TIPO_GMI		0x47	///< getiomeminfo()
/// @}
/// @}

/// @name Tipi delle interruzioni esterne
/// @{
#define INTR_TIPO_KBD		0x50	///< tastiera
#define INTR_TIPO_HD		0x60	///< hard disk
#define INTR_TIPO_TIMER		0xFE	///< timer (prio massima)
/// @}

/// @name Suddivisione della memoria virtuale.
///
/// I nomi di queste costanti seguono lo schema
/// {I,N}_{SIS,MIO,UTN}_{C,P}, dove:
///
///   - I    = Indice della prima entrata in root_tab
///   - N    = Numero di entrate in root_tab
///   - SIS  = SIStema
///   - MIO  = Modulo IO
///   - UTN  = modulo UTeNte
///   - C    = Condiviso
///   - P    = Privato
///
/// @{
#define I_SIS_C		0 		///< prima entrata sistema/condivisa
#define I_SIS_P		1		///< prima entrata sistema/privata
#define I_MIO_C		2		///< prima entrata modulo IO/condivisa
#define I_UTN_C       256		///< prima entrata utente/condivisa
#define I_UTN_P	      384		///< prima entrata utente/privata

#define N_SIS_C		1		///< numero entrate sistema/condivisa
#define N_SIS_P		1		///< numero entrate sistema/privata
#define N_MIO_C		1		///< numero entrate modulo IO/condivisa
#define N_UTN_C	      128		///< numero entrate utente/convidisa
#define N_UTN_P	      128		///< numero entrate utente/privata
/// @}

/// @} // costanti modificabili

/// @name Costanti non modificabili
///
/// Non modificare la defizione di queste costanti.
/// @{
#define MIN_PROC_ID		0			///< minimo id di processo
#define MAX_PROC_ID		(MAX_PROC - 1)		///< massimo id di processo
#define MAX_PRIORITY		(MIN_EXT_PRIO - 1)	///< priorità massima dei processi (non esterni)
#define MIN_PRIORITY		0x1			///< priorità minima dei processi
#define MAX_EXT_PRIO		(MIN_EXT_PRIO + 0xFE)	///< priorità massima dei processi esterni
/// @}
