/** @file sistema.s
 * @brief Parte Assembler del modulo sistema
 */
#include <libce.h>
#include <costanti.h>

//////////////////////////////////////////////////////////////////////////
// AVVIO                                                                //
//////////////////////////////////////////////////////////////////////////

.globl  _start, start
_start:				// entry point
start:
	// il boot loader ci ha passato in %rdi il puntatore alla struttura
	// multiboot_info. Questa, tra le altre cose, contiene l'indirizzo dei
	// moduli io e utente copiati in memoria dal bootloader stesso.
	// Salviamo temporaeamente l'indirizzo in un registro non-scratch per
	// poi passarlo a main.
	
	mov %rdi, %rbx
	// inizializziamo la IDT (funzione definita in questo file)
	call init_idt
	// settiamo il bit WP in CR0, in modo che le scritture nelle
	// pagine con R/W=0 siano vietate anche da livello sistema
	// (può aiutare a trovare qualche errore quando si svolgono gli
	// esercizi)
	mov %cr0, %rax
	or $(1<<16), %rax
	mov %rax, %cr0
	// chiamiamo eventuali costruttori di variabili globali
	call ctors
	// il resto dell'inizializzazione e' scritto in C++
	mov %rbx, %rdi
	call main
	// se arrivamo qui c'e' stato un errore, fermiamo la macchina
	.global halt
halt:
	hlt
	ret

//////////////////////////////////////////////////////////////////////////
// SALVATAGGIO/CARICAMENTO STATO PROCESSI                               //
//////////////////////////////////////////////////////////////////////////

// offset dei vari registri all'interno di des_proc
.set PUNT_NUCLEO, 8
.set CTX, 16
.set RAX, CTX+0
.set RCX, CTX+8
.set RDX, CTX+16
.set RBX, CTX+24
.set RSP, CTX+32
.set RBP, CTX+40
.set RSI, CTX+48
.set RDI, CTX+56
.set R8,  CTX+64
.set R9,  CTX+72
.set R10, CTX+80
.set R11, CTX+88
.set R12, CTX+96
.set R13, CTX+104
.set R14, CTX+112
.set R15, CTX+120
.set CR3, CTX+128

// copia lo stato dei registri generali nel des_proc del processo puntato da
// esecuzione.  Nessun registro viene sporcato.
salva_stato:
	// salviamo lo stato di un paio di registri in modo da poterli
	// temporaneamente riutilizzare. In particolare, useremo %rax come
	// registro di lavoro e %rbx come puntatore al des_proc.
	.cfi_startproc
	.cfi_def_cfa_offset 8
	pushq %rbx
	.cfi_adjust_cfa_offset 8
	.cfi_offset rbx, -16
	pushq %rax
	.cfi_adjust_cfa_offset 8
	.cfi_offset rax, -24

	movq esecuzione, %rbx
	movq %rbx, esecuzione_precedente

	// copiamo per primo il vecchio valore di %rax
	movq (%rsp), %rax
	movq %rax, RAX(%rbx)
	// usiamo %rax come appoggio per copiare il vecchio %rbx
	movq 8(%rsp), %rax
	movq %rax, RBX(%rbx)
	// copiamo gli altri registri
	movq %rcx, RCX(%rbx)
	movq %rdx, RDX(%rbx)
	// salviamo il valore che %rsp aveva prima della chiamata a salva stato
	// (valore corrente meno gli 8 byte che contengono l'indirizzo di
	// ritorno e i 16 byte dovuti alle due push che abbiamo fatto
	// all'inizio)
	movq %rsp, %rax
	addq $24, %rax
	movq %rax, RSP(%rbx)
	movq %rbp, RBP(%rbx)
	movq %rsi, RSI(%rbx)
	movq %rdi, RDI(%rbx)
	movq %r8,  R8 (%rbx)
	movq %r9,  R9 (%rbx)
	movq %r10, R10(%rbx)
	movq %r11, R11(%rbx)
	movq %r12, R12(%rbx)
	movq %r13, R13(%rbx)
	movq %r14, R14(%rbx)
	movq %r15, R15(%rbx)

	popq %rax
	.cfi_adjust_cfa_offset -8
	.cfi_restore rax
	popq %rbx
	.cfi_adjust_cfa_offset -8
	.cfi_restore rbx

	ret
	.cfi_endproc


// carica nei registri del processore lo stato contenuto nel des_proc del
// processo puntato da esecuzione.  Questa funzione sporca tutti i registri.
carica_stato:
	.cfi_startproc
	.cfi_def_cfa_offset 8
	movq esecuzione, %rbx

	popq %rcx   //ind di ritorno, va messo nella nuova pila
	.cfi_adjust_cfa_offset -8
	.cfi_register rip, rcx

	// nuovo valore per cr3
	movq CR3(%rbx), %r10
	movq %cr3, %rax
	cmpq %rax, %r10
	je 1f			// evitiamo di invalidare il TLB
				// se cr3 non cambia
	movq %r10, %rax
	movq %rax, %cr3		// il TLB viene invalidato
1:

	// anche se abbiamo cambiato cr3 siamo sicuri che l'esecuzione prosegue
	// da qui, perché ci troviamo dentro la finestra FM che è comune a
	// tutti i processi
	movq RSP(%rbx), %rsp    //cambiamo pila
	pushq %rcx              //rimettiamo l'indirizzo di ritorno
	.cfi_adjust_cfa_offset 8
	.cfi_offset rip, -8

	// se il processo precedente era terminato o abortito la sua pila
	// sistema non era stata distrutta, in modo da permettere a noi di
	// continuare ad usarla. Ora che abbiamo cambiato pila possiamo
	// disfarci della precedente.
	cmpq $0, ultimo_terminato
	je 1f
	call distruggi_pila_precedente
1:

	// aggiorniamo il puntatore alla pila sistema usata dal meccanismo
	// delle interruzioni
	movq PUNT_NUCLEO(%rbx), %rcx
	movq tss_punt_nucleo, %rdi
	movq %rcx, (%rdi)

	movq RCX(%rbx), %rcx
	movq RDI(%rbx), %rdi
	movq RSI(%rbx), %rsi
	movq RBP(%rbx), %rbp
	movq RDX(%rbx), %rdx
	movq RAX(%rbx), %rax
	movq R8(%rbx), %r8
	movq R9(%rbx), %r9
	movq R10(%rbx), %r10
	movq R11(%rbx), %r11
	movq R12(%rbx), %r12
	movq R13(%rbx), %r13
	movq R14(%rbx), %r14
	movq R15(%rbx), %r15
	movq RBX(%rbx), %rbx


	retq
	.cfi_endproc

////////////////////////////////////////////////////////////////////////
//                 INIZIALIZZAZIONE IDT                               //
////////////////////////////////////////////////////////////////////////

// Carica un gate della IDT
// num: indice (a partire da 0) in IDT del gate da caricare
// routine: indirizzo della routine da associare al gate
// dpl: dpl del gate (LIV_SISTEMA o LIV_UTENTE)
.macro carica_gate num routine dpl
	movq $\num, %rdi
	call gate_present
	cmpq $0, %rax
	je 1f
	leaq idt_error(%rip), %rdi
	movq $\num, %rsi
	call _Z6fpanicPKcz
	call reboot
1:	movq $\num, %rdi
	movq $\routine, %rsi
	xorq %rdx, %rdx		// tipo interrupt
	movq $\dpl, %rcx
	call gate_init
.endm


// carica la idt
// le prime 32 entrate sono definite dall'Intel e corrispondono
// alle possibili eccezioni.
.global init_idt
init_idt:
	//		indice		routine			dpl
	// gestori eccezioni:
	carica_gate	0 		exc_div_error 	LIV_SISTEMA
	carica_gate	1 		exc_debug 	LIV_SISTEMA
	carica_gate	2 		exc_nmi 	LIV_SISTEMA
	carica_gate	3 		exc_breakpoint 	LIV_SISTEMA
	carica_gate	4 		exc_overflow 	LIV_SISTEMA
	carica_gate	5 		exc_bound_re 	LIV_SISTEMA
	carica_gate	6 		exc_inv_opcode	LIV_SISTEMA
	carica_gate	7 		exc_dev_na 	LIV_SISTEMA
	carica_gate	8 		exc_dbl_fault 	LIV_SISTEMA
	carica_gate	9 		exc_coproc_so 	LIV_SISTEMA
	carica_gate	10 		exc_inv_tss 	LIV_SISTEMA
	carica_gate	11 		exc_segm_fault 	LIV_SISTEMA
	carica_gate	12 		exc_stack_fault	LIV_SISTEMA
	carica_gate	13 		exc_prot_fault 	LIV_SISTEMA
	carica_gate	14 		exc_page_fault 	LIV_SISTEMA
	// il tipo 15 è riservato
	carica_gate	16 		exc_fp 		LIV_SISTEMA
	carica_gate	17 		exc_ac 		LIV_SISTEMA
	carica_gate	18 		exc_mc 		LIV_SISTEMA
	carica_gate	19 		exc_simd 	LIV_SISTEMA
	carica_gate	20		exc_virt	LIV_SISTEMA
	// tipi 21-29 riservati
	carica_gate	30		exc_sec		LIV_SISTEMA
	// tipo 31 riservato

	//primitive comuni (tipi 0x2-)
	carica_gate	TIPO_A		a_activate_p	LIV_UTENTE
	carica_gate	TIPO_T		a_terminate_p	LIV_UTENTE
	carica_gate	TIPO_SI		a_sem_ini	LIV_UTENTE
	carica_gate	TIPO_W		a_sem_wait	LIV_UTENTE
	carica_gate	TIPO_S		a_sem_signal	LIV_UTENTE
	carica_gate	TIPO_D		a_delay		LIV_UTENTE
	carica_gate	TIPO_L		a_do_log	LIV_UTENTE
	carica_gate	TIPO_GMI	a_getmeminfo	LIV_UTENTE
// ( ESAME 2024-01-29
	carica_gate	TIPO_BC		a_barrier_create		LIV_UTENTE
	carica_gate	TIPO_B		a_barrier			LIV_UTENTE
//   ESAME 2024-01-29 )

	// primitive per il livello I/O (tipi 0x3-)
	carica_gate	TIPO_APE	a_activate_pe	LIV_SISTEMA
	carica_gate	TIPO_WFI	a_wfi		LIV_SISTEMA
	carica_gate	TIPO_FG		a_fill_gate	LIV_SISTEMA
	carica_gate	TIPO_AB		a_abort_p	LIV_SISTEMA
	carica_gate	TIPO_IOP	a_io_panic	LIV_SISTEMA
	carica_gate	TIPO_TRA	a_trasforma	LIV_SISTEMA
	carica_gate	TIPO_ACC	a_access	LIV_SISTEMA

	// i tipi 0x4- verranno usati per le primitive fornite dal modulo I/O
	// (si veda fill_io_gates() in io.s)

	// i tipi da 0x50 a 0xFD verrano usati per gli handler
	// (si veda load_handler() più avanti)

	// la priorità massima è riservata al driver del timer di sistema
	carica_gate	INTR_TIPO_TIMER	driver_td	LIV_SISTEMA

	// idt_pointer è definito nella libce
	lidt idt_pointer
	ret

////////////////////////////////////////////////////////
// a_primitive                                        //
////////////////////////////////////////////////////////
        .extern c_activate_p
a_activate_p:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
        call c_activate_p
	call carica_stato
        iretq
	.cfi_endproc

        .extern c_terminate_p
a_terminate_p:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	movq $1, %rdi		// logmsg = true
        call c_terminate_p
	call carica_stato
	iretq
	.cfi_endproc

	.extern c_sem_ini
a_sem_ini:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call c_sem_ini
	call carica_stato
	iretq
	.cfi_endproc

	.extern c_sem_wait
a_sem_wait:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call c_sem_wait
	call carica_stato
	iretq
	.cfi_endproc

	.extern c_sem_signal
a_sem_signal:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call c_sem_signal
	call carica_stato
	iretq
	.cfi_endproc

	.extern c_delay
a_delay:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call c_delay
	call carica_stato
	iretq
	.cfi_endproc

	.extern c_log
a_do_log:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call c_do_log
	call carica_stato
	iretq
	.cfi_endproc

a_getmeminfo:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call c_getmeminfo
	call carica_stato
	iretq
	.cfi_endproc

// ( ESAME 2024-01-29
a_barrier_create:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call c_barrier_create
	call carica_stato
	iretq
	.cfi_endproc

a_barrier:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call c_barrier
	call carica_stato
	iretq
	.cfi_endproc
//   ESAME 2024-01-29 )

//
// Interfaccia offerta al modulo di IO, inaccessibile dal livello utente
//

	.extern c_activate_pe
a_activate_pe:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
        call c_activate_pe
	call carica_stato
	iretq
	.cfi_endproc

a_wfi:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call apic_send_EOI
	call schedulatore
	call carica_stato
	iretq
	.cfi_endproc

a_fill_gate:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call c_fill_gate
	call carica_stato
	iretq
	.cfi_endproc

	.extern c_abort_p
a_abort_p:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	movq $1, %rdi		// selfdump = true
        call c_abort_p
	call carica_stato
	iretq
	.cfi_endproc

	.extern c_io_panic
a_io_panic:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
        call c_io_panic
	call carica_stato
	iretq
	.cfi_endproc

	.extern c_trasforma
a_trasforma:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call c_trasforma
	call carica_stato
	iretq
	.cfi_endproc

	.extern c_access
a_access:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call c_access
	call carica_stato
	iretq
	.cfi_endproc

////////////////////////////////////////////////////////////////
// gestori delle eccezioni				      //
////////////////////////////////////////////////////////////////
// Gestiamo tutte le eccezioni (tranne nmi) nello stesso modo: inviamo un
// messaggio al log e terminiamo forzatamente il processo che ha causato
// l'eccezione. Per questo motivo, ogni gestore chiama la funzione
// gestore_eccezioni() (definita in sistema.cpp), passandole il numero
// corrispondente al suo tipo di eccezione (primo argomento) e il %rip che si
// trova in cima alla pila (terzo argomento). Quest'ultimo da indicazioni sul
// punto del programma che ha causato l'eccezione.
//
// Il secondo parametro passato a gestore_eccezioni richiede qualche
// spiegazione in più.  Alcune eccezioni lasciano in pila un'ulteriore parola
// quadrupla, il cui significato dipende dal tipo di eccezione.  Per avere la
// pila sistema in uno stato uniforme prima di chiamare salva_stato, estraiamo
// questa ulteriore parola quadrupla copiandola nella variable globale
// exc_error. Il contenuto di exc_error viene poi passato come secondo
// parametro di gestore_eccezioni.  Le eccezioni che non prevedono questa
// ulteriore parola quadrupla si limitano a passare 0. L'uso della variabile
// globale exc_error non causa problemi, perché le interruzioni sono disabilitate.
//
exc_div_error:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	movq $0, %rdi
	movq $0, %rsi
	movq (%rsp), %rdx
	call gestore_eccezioni
	call carica_stato
	iretq
	.cfi_endproc

exc_debug:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	movq $1, %rdi
	movq $0, %rsi
	movq (%rsp), %rdx
	call gestore_eccezioni
	call carica_stato
	iretq
	.cfi_endproc

exc_nmi:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	// gestiamo nmi (non-maskable-interrupt) in modo speciale.  La funzione
	// c_nmi chiamerà panic(), la quale stamperà un po' di informazioni sullo
	// stato del sistema e spegnerà la macchina.
	call c_nmi
	call carica_stato
	iretq
	.cfi_endproc

exc_breakpoint:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	movq $3, %rdi
	movq $0, %rsi
	movq (%rsp), %rdx
	call gestore_eccezioni
	call carica_stato
	iretq
	.cfi_endproc

exc_overflow:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	movq $4, %rdi
	movq $0, %rsi
	movq (%rsp), %rdx
	call gestore_eccezioni
	call carica_stato
	iretq
	.cfi_endproc

exc_bound_re:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	movq $5, %rdi
	movq $0, %rsi
	movq (%rsp), %rdx
	call gestore_eccezioni
	call carica_stato
	iretq
	.cfi_endproc

exc_inv_opcode:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	movq $6, %rdi
	movq $0, %rsi
	movq (%rsp), %rdx
	call gestore_eccezioni
	call carica_stato
	iretq
	.cfi_endproc

exc_dev_na:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	movq $7, %rdi
	movq $0, %rsi
	movq (%rsp), %rdx
	call gestore_eccezioni
	call carica_stato
	iretq
	.cfi_endproc

exc_dbl_fault:
	.cfi_startproc
	.cfi_def_cfa_offset 48
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	popq exc_error
	call salva_stato
	movq $8, %rdi
	movq exc_error, %rsi
	movq (%rsp), %rdx
	call gestore_eccezioni
	call carica_stato
	iretq
	.cfi_endproc

exc_coproc_so:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	movq $9, %rdi
	movq $0, %rsi
	movq (%rsp), %rdx
	call gestore_eccezioni
	call carica_stato
	iretq
	.cfi_endproc

exc_inv_tss:
	.cfi_startproc
	.cfi_def_cfa_offset 48
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	pop exc_error
	.cfi_adjust_cfa_offset -8
	call salva_stato
	movq $10, %rdi
	movq exc_error, %rsi
	movq (%rsp), %rdx
	call gestore_eccezioni
	call carica_stato
	iretq
	.cfi_endproc

exc_segm_fault:
	.cfi_startproc
	.cfi_def_cfa_offset 48
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	pop exc_error
	.cfi_adjust_cfa_offset -8
	call salva_stato
	movq $11, %rdi
	movq exc_error, %rsi
	movq (%rsp), %rdx
	call gestore_eccezioni
	call carica_stato
	iretq
	.cfi_endproc

exc_stack_fault:
	.cfi_startproc
	.cfi_def_cfa_offset 48
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	pop exc_error
	.cfi_adjust_cfa_offset -8
	call salva_stato
	movq $12, %rdi
	movq exc_error, %rsi
	movq (%rsp), %rdx
	call gestore_eccezioni
	call carica_stato
	iretq
	.cfi_endproc

exc_prot_fault:
	.cfi_startproc
	.cfi_def_cfa_offset 48
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	pop exc_error
	.cfi_adjust_cfa_offset -8
	call salva_stato
	movq $13, %rdi
	movq exc_error, %rsi
	movq (%rsp), %rdx
	call gestore_eccezioni
	call carica_stato
	iretq
	.cfi_endproc

exc_page_fault:
	.cfi_startproc
	.cfi_def_cfa_offset 48
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	pop exc_error
	.cfi_adjust_cfa_offset -8
	call salva_stato
	movq $14, %rdi
	movq exc_error, %rsi
	movq (%rsp), %rdx
	call gestore_eccezioni
	call carica_stato
	iretq
	.cfi_endproc

exc_fp:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	movq $16, %rdi
	movq $0, %rsi
	movq (%rsp), %rdx
	call gestore_eccezioni
	call carica_stato
	iretq
	.cfi_endproc

exc_ac:
	.cfi_startproc
	.cfi_def_cfa_offset 48
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	pop exc_error
	.cfi_adjust_cfa_offset -8
	call salva_stato
	movq $13, %rdi
	movq exc_error, %rsi
	movq (%rsp), %rdx
	call gestore_eccezioni
	call carica_stato
	iretq
	.cfi_endproc

exc_mc:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	movq $18, %rdi
	movq $0, %rsi
	movq (%rsp), %rdx
	call gestore_eccezioni
	call carica_stato
	iretq
	.cfi_endproc

exc_simd:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	movq $19, %rdi
	movq $0, %rsi
	movq (%rsp), %rdx
	call gestore_eccezioni
	call carica_stato
	iretq
	.cfi_endproc

exc_virt:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	movq $20, %rdi
	movq $0, %rsi
	movq (%rsp), %rdx
	call gestore_eccezioni
	call carica_stato
	iretq
	.cfi_endproc

exc_sec:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	movq $31, %rdi
	movq $0, %rsi
	movq (%rsp), %rdx
	call gestore_eccezioni
	call carica_stato
	iretq
	.cfi_endproc

////////////////////////////////////////////////////////
// handler/driver                                     //
////////////////////////////////////////////////////////

// driver del timer
	.extern c_driver_td
driver_td:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call c_driver_td
	call apic_send_EOI
	call carica_stato
	iretq
	.cfi_endproc

// predisponiamo tutti i possibili handler, ma non li inseriamo ancora nella
// IDT perché non sappiamo la priorità.  Sarà la activate_pe() a predisporre
// opportunamente i gate della IDT invocando la funzione load_handler()
// definita più avanti.

handler_0:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call inspronti

	mov a_p+0*8, %rax
	movq %rax, esecuzione

	call carica_stato
	iretq
	.cfi_endproc

handler_1:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call inspronti

	mov a_p+1*8, %rax
	movq %rax, esecuzione

	call carica_stato
	iretq
	.cfi_endproc

handler_2:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call inspronti

	mov a_p+2*8, %rax
	movq %rax, esecuzione

	call carica_stato
	iretq
	.cfi_endproc

handler_3:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call inspronti

	mov a_p+3*8, %rax
	movq %rax, esecuzione

	call carica_stato
	iretq
	.cfi_endproc

handler_4:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call inspronti

	mov a_p+4*8, %rax
	movq %rax, esecuzione

	call carica_stato
	iretq
	.cfi_endproc

handler_5:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call inspronti

	mov a_p+5*8, %rax
	movq %rax, esecuzione

	call carica_stato
	iretq
	.cfi_endproc

handler_6:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call inspronti

	mov a_p+6*8, %rax
	movq %rax, esecuzione

	call carica_stato
	iretq
	.cfi_endproc

handler_7:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call inspronti

	mov a_p+7*8, %rax
	movq %rax, esecuzione

	call carica_stato
	iretq
	.cfi_endproc

handler_8:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call inspronti

	mov a_p+8*8, %rax
	movq %rax, esecuzione

	call carica_stato
	iretq
	.cfi_endproc

handler_9:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call inspronti

	mov a_p+9*8, %rax
	movq %rax, esecuzione

	call carica_stato
	iretq
	.cfi_endproc

handler_10:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call inspronti

	mov a_p+10*8, %rax
	movq %rax, esecuzione

	call carica_stato
	iretq
	.cfi_endproc

handler_11:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call inspronti

	mov a_p+11*8, %rax
	movq %rax, esecuzione

	call carica_stato
	iretq
	.cfi_endproc

handler_12:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call inspronti

	mov a_p+12*8, %rax
	movq %rax, esecuzione

	call carica_stato
	iretq
	.cfi_endproc

handler_13:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call inspronti

	mov a_p+13*8, %rax
	movq %rax, esecuzione

	call carica_stato
	iretq
	.cfi_endproc

handler_14:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call inspronti

	mov a_p+14*8, %rax
	movq %rax, esecuzione

	call carica_stato
	iretq
	.cfi_endproc

handler_15:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call inspronti

	mov a_p+15*8, %rax
	movq %rax, esecuzione

	call carica_stato
	iretq
	.cfi_endproc

handler_16:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call inspronti

	mov a_p+16*8, %rax
	movq %rax, esecuzione

	call carica_stato
	iretq
	.cfi_endproc

handler_17:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call inspronti

	mov a_p+17*8, %rax
	movq %rax, esecuzione

	call carica_stato
	iretq
	.cfi_endproc

handler_18:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call inspronti

	mov a_p+18*8, %rax
	movq %rax, esecuzione

	call carica_stato
	iretq
	.cfi_endproc

handler_19:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call inspronti

	mov a_p+19*8, %rax
	movq %rax, esecuzione

	call carica_stato
	iretq
	.cfi_endproc

handler_20:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call inspronti

	mov a_p+20*8, %rax
	movq %rax, esecuzione

	call carica_stato
	iretq
	.cfi_endproc

handler_21:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call inspronti

	mov a_p+21*8, %rax
	movq %rax, esecuzione

	call carica_stato
	iretq
	.cfi_endproc

handler_22:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call inspronti

	mov a_p+22*8, %rax
	movq %rax, esecuzione

	call carica_stato
	iretq
	.cfi_endproc

handler_23:
	.cfi_startproc
	.cfi_def_cfa_offset 40
	.cfi_offset rip, -40
	.cfi_offset rsp, -16
	call salva_stato
	call inspronti

	mov a_p+23*8, %rax
	movq %rax, esecuzione

	call carica_stato
	iretq
	.cfi_endproc

	.global load_handler
	// La funzione si aspetta un <tipo> in %rdi e un <irq> in %rsi.
	// Provvede quindi a caricare il gate <tipo> della ITD in modo che
	// punti a handler_<irq>.
load_handler:
	.cfi_startproc
	movq %rsi, %rax
	// visto che gli handler sono tutti della stessa dimensione,
	// calcoliamo l'indirizzo dell'handler che ci interessa usando
	// la formula
	//
	//	handler_0 + <dim_handler> * <irq>
	//
	// dove <dim_handler> si può ottenere sottraendo gli indirizzi
	// di due handler consecutivi qualunque.
	movq $(handler_1 - handler_0), %rcx
	mulq %rcx
	movq $handler_0, %rsi
	addq %rax, %rsi
	// ora %rsi contiene l'indirizzo dell'handler, mentre %rdi
	// contiene ancora il tipo
	xorq %rdx, %rdx	// tipo interrupt
	movq $LIV_SISTEMA, %rcx
	call gate_init
	ret
	.cfi_endproc

//////////////////////////////////////////////////////////
// primitive richiamate dal nucleo stesso	        //
//////////////////////////////////////////////////////////
	.global sem_ini
sem_ini:
	.cfi_startproc
	int $TIPO_SI
	ret
	.cfi_endproc

	.global sem_wait
sem_wait:
	.cfi_startproc
	int $TIPO_W
	ret
	.cfi_endproc

	.global activate_p
activate_p:
	.cfi_startproc
	int $TIPO_A
	ret
	.cfi_endproc

	.global terminate_p
terminate_p:
	.cfi_startproc
	int $TIPO_T
	ret
	.cfi_endproc

	.global salta_a_main
salta_a_main:
	.cfi_startproc
	call carica_stato		// carichiamo, in particolare, tss_punt_nucleo
	iretq				// torniamo al chiamante "trasformati" in processo
	.cfi_endproc

// setup_self_dump: chiamata da panic e abort_p.  Il backtrace parte normalmente
// dall'ultimo stato salvato di ogni processo, ma per il processo corrente
// vorremmo farla partire dallo stato attuale del processore, in modo da
// conoscere il motivo per cui sono state invocata panic o abort_p. La funzione self_dump
// modifica la pila sistema in modo che si trovi in uno stato simile a quello
// successivo ad una interruzione e chiama salva stato per aggiornare il
// descrittore di processo con lo stato attuale del processore. In questo modo
// il bactrace del processo corrente partirà dalla funzione panic/abort_p stessa,
// passando poi al suo chiamante e così via.
	.global setup_self_dump
setup_self_dump:
	.cfi_startproc
	pushq %rax	// parola lunga non significativa
	.cfi_adjust_cfa_offset 8
	pushq %rsp	// %rsp del chiamante - 16 (per via della call e della push %rax)
	.cfi_adjust_cfa_offset 8
	pushf		// salviamo i flag prima di fare la add
	.cfi_adjust_cfa_offset 8
	addq $16, 8(%rsp) // aggiustiamo l'%rsp salvato
	pushq $SEL_CODICE_SISTEMA // cpl
	.cfi_adjust_cfa_offset 8
	pushq 32(%rsp)	// %rip del chiamante
	.cfi_adjust_cfa_offset 8
	// per completare l'interruzione simulata, chiamiamo salva_stato
	call salva_stato
	// duplichiamo il %rip salvato, perché una copia serve alla backtrace
	pushq (%rsp)
	.cfi_adjust_cfa_offset 8
	ret
	.cfi_endproc

// cleanup_self_dump: ripulisce la pila da quanto vi aveva inserito
// la setup_self_dump, in modo che sia possibile proseguire con la
// normale esecuzione, nel caso process_dump() fosse stata chiamata
// da c_abort_p() e non da panic().
	.global cleanup_self_dump
cleanup_self_dump:
	.cfi_startproc
	popq %rax
	.cfi_adjust_cfa_offset -8
	.cfi_register rip, rax
	addq $40, %rsp
	.cfi_def_cfa_offset 0
	pushq %rax
	.cfi_adjust_cfa_offset 8
	.cfi_offset rip, -8
	ret
	.cfi_endproc

	.global end_program
end_program:
	call reboot
	cli
	hlt

////////////////////////////////////////////////////////////////
// sezione dati                                               //
////////////////////////////////////////////////////////////////
.data
idt_error:
	.asciz "Errore nel caricamento del gate %#02x (duplicato?)"
.bss
exc_error:
	.space 8, 0
.global tss_punt_nucleo
tss_punt_nucleo:
	.quad 0
