#include <all.h>


extern natl hello;
const natq DIM_NOME = 80;
char nome[DIM_NOME + 1];
void hello_body(natq a)
{
	for (;;) {
		printf("Ciao, come ti chiami? ");
		if (readconsole(nome, DIM_NOME + 1) == DIM_NOME + 1) {
			printf("Troppo lungo! Max %lu caratteri\n", DIM_NOME);
			continue;
		}
		break;
	}
	printf("Ciao %s, piacere di conoscerti!\n", nome);
	pause();

	terminate_p();
}
natl hello;

extern "C" void main()
{
	hello = activate_p(hello_body, 0, 5, LIV_UTENTE);

	terminate_p();
}
