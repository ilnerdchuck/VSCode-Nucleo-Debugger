/// @file io.h
/// @brief Primitive fornite dal modulo I/O.
///
/// Queste primitive possono essere usate dal modulo utente.  Sono eseguite a
/// livello sistema, ma con le interruzioni esterne mascherabili abilitate.

/**
 * @brief Inizializza la console (video e tastiera)
 *
 * Ripulisce il video (modalità testo) e setta il codice di attributo colore.
 *
 * @param cc		codice di attributo colore
 */
extern "C" void iniconsole(natb cc);

/**
 * @brief Lettura da tastiera.
 *
 * Legge una riga da tastiera, terminata da a-capo.  L'a-capo è sostituito con
 * il terminatore di stringa.
 *
 * @note La primitiva non legge mai più di _quanti_ caratteri, quindi può restituire
 * una stringa incompleta (non terminata) se l'a-capo non è ricevuto entro
 * i primi _quanti_ caratteri. Siccome il terminatore non è contato tra i
 * caratteri ricevuti, il caso di stringa incompleta può essere riconosciuto
 * perché è l'unico caso in cui il valore restituito è uguale a _quanti_.
 *
 * @param buff		buffer dove ricevere la riga
 * @param quanti	numero massimo di caratteri da leggere
 *
 * @return 		numero di caratteri effettivamente letti,
 * 			escluso l'a-capo
 */
extern "C" natq readconsole(char* buff, natq quanti);

/**
 * @brief Scrive caratteri sul video.
 *
 * La scrittura avviene a partire dalla posizione corrente del cursore.
 *
 * @param buff		buffer contenente i caratteri da scrivere
 * @param quanti	numero di caratteri da scrivere
 */
extern "C" void writeconsole(const char* buff, natq quanti);

/**
 * @brief Lettura di settori dall'hard disk.
 *
 * @param vetti		buffer destinato a ricevere i dati letti
 * @param primo		LBA del primo settore da leggere
 * @param quanti	numero di settori da leggere
 */
extern "C" void readhd_n(void* vetti, natl primo, natb quanti);

/**
 * @brief Scrittura di settori sull'hard disk.
 *
 * @param vetto		buffer contenente i dati da scrivere
 * @param primo		LBA del primo settore da scrivere
 * @param quanti	numero di settori da scrivere
 */
extern "C" void writehd_n(const void* vetto, natl primo, natb quanti);

/**
 * @brief Lettura di settori dall'hard disk (in DMA).
 *
 * @param vetti		buffer destinato a ricevere i dati letti
 * @param primo		LBA del primo settore da leggere
 * @param quanti	numero di settori da leggere
 */
extern "C" void dmareadhd_n(void* vetti, natl primo, natb quanti);

/**
 * @brief Scrittura di settori sull'hard disk (in DMA).
 *
 * @param vetto		buffer contenente i dati da scrivere
 * @param primo		LBA del primo settore da scrivere
 * @param quanti	numero di settori da scrivere
 */
extern "C" void dmawritehd_n(const void* vetto, natl primo, natb quanti);

/// @name Funzioni di supporto al debugging
/// @{

/**
 * @brief Informazioni di debug.
 *
 * Questa primitiva è usata in alcuni testi d'esame per eseguire dei controlli.
 *
 * @return quantià di byte liberi nello heap del modulo I/O
 */
extern "C" natq getiomeminfo();
/// @}
