/*
 * Mailbox con memoria dinamica
 */

#include <all.h>

const int NMESG = 5;
const int MSG_SIZE = 100;

extern natl mailbox_piena;
extern natl mailbox_vuota;

extern natl scrittore1;

extern natl scrittore2;

extern natl lettore;
struct mess {
	int mittente;
	char corpo[MSG_SIZE];
};

mess* mailbox;

void pms(natq a)
{
	mess *ptr;
	for (int i = 0; i < NMESG; i++) {
		ptr = new mess;
		if (!ptr) {
			flog(LOG_WARN, "memoria esaurita");
			break;
		}
		ptr->mittente = a;
		snprintf(ptr->corpo, MSG_SIZE, "Messaggio numero %d", i);
		sem_wait(mailbox_vuota);
		mailbox = ptr;
		sem_signal(mailbox_piena);
		delay(20);
	}
	printf("fine scrittore\n");

	terminate_p();
}
void pml(natq a)
{
	mess *ptr;
	for (int i = 0; i < 2 * NMESG; i++) {
		sem_wait(mailbox_piena);
		ptr = mailbox;
		sem_signal(mailbox_vuota);
		printf("messaggio %d da %d: %s\n",
			i, ptr->mittente, ptr->corpo);
		delete ptr;
		ptr = 0;
	}
	printf("fine lettore\n");
	pause();

	terminate_p();
}
natl mailbox_piena;
natl mailbox_vuota;
natl scrittore1;
natl scrittore2;
natl lettore;

extern "C" void main()
{
	mailbox_piena = sem_ini(0);
	mailbox_vuota = sem_ini(1);
	scrittore1 = activate_p(pms, 1, 5, LIV_UTENTE);
	scrittore2 = activate_p(pms, 2, 5, LIV_UTENTE);
	lettore = activate_p(pml, 0, 5, LIV_UTENTE);

	terminate_p();
}
