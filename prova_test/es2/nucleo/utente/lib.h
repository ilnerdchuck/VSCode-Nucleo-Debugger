/// @file lib.h
/// @brief Funzioni di libreria per il modulo utente
/// @addtogroup usr Modulo utente
/// @{

/// @addtogroup usrutil Funzioni di utilità generale
/// @{

/// @brief Formatta un messaggio e lo scrive sul video
///
/// Si possono usare gli stessi formati della `printf(3)` della
/// libreria standard del C, con l'esclusione di quelli relativi
/// ai tipi `float` e `double`.
///
/// @param fmt	stringa di formato
/// @param ... argomenti richiesti da _fmt_
/// @return	numero di caratteri scritti
int printf(const char *fmt, ...) __attribute__ ((format(printf, 1, 2)));

/// @brief Attende la pressione di un carattere
void pause();

/// @brief Id del processo corrente
/// @return id del processo corrente
natl getpid();
/// @}
/// @}
