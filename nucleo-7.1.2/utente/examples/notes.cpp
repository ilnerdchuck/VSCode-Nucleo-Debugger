#include <all.h>

static const int MAX_NAME = 16;
static const int MAX_BLOCKS = 10;
static const int MAX_INPUT = 10;

int atoi(const char *in)
{
	int rv = 0;
	const char *sc = in;
	while ((sc - in) < MAX_INPUT && *sc != '\0' && *sc >= '0' && *sc <= '9') {
		rv = (rv * 10) + (*sc - '0');
		sc++;
	}
	return rv;
}


extern natl notes;
struct dir_entry {
	natl block;
	natl dim;
	char name[MAX_NAME];
};

char dir[DIM_BLOCK] alignas(alignof(dir_entry));
char buf[DIM_BLOCK * MAX_BLOCKS];
char c;
char input[MAX_INPUT];
void notes_body(natq a)
{
	readhd_n(dir, 0, 1);
	bool cont = true;
	natl max = 1;
	int sel;
	while (cont) {
		int i = 0;
		dir_entry *e = reinterpret_cast<dir_entry*>(dir);
		dir_entry *list = e;
		printf("List of notes:\n");
		while (e->block) {
			i++;
			printf("%d) %.*s\n", i, MAX_NAME, e->name);
			if (e->block + e->dim > max)
				max = e->block + e->dim;
			e++;
		}
		printf("c - create note; r - read note; w - write notes on disk; q - quit\n");
		printf("> ");
		readconsole(&c, 1);
		printf("\n");
		switch (c) {
		case 'c':
			printf("name? ");
			readconsole(e->name, MAX_NAME);
			e->block = max;
			printf("dim? ");
			readconsole(input, MAX_INPUT);
			e->dim = atoi(input);
			if (e->dim > MAX_BLOCKS) {
				printf("too large\n");
				e->block = 0;
				break;
			}
			printf("Write note contents. End with ENTER\n");
			readconsole(buf, e->dim * DIM_BLOCK);
			writehd_n(buf, e->block, e->dim);
			break;
		case 'r':
			printf("id? ");
			readconsole(input, MAX_INPUT);
			sel = atoi(input);
			if (sel > i) {
				printf("no such note\n");
				break;
			}
			printf("contents of note %d:\n", sel);
			readhd_n(buf, list[sel - 1].block, list[sel - 1].dim);
			printf("-------------------\n");
			printf("%s\n", buf);
			printf("-------------------\n");
			break;
		case 'w':
			writehd_n(dir, 0, 1);
			printf("OK\n");
			break;
		case 'q':
			cont = false;
			break;
		}
	}

	terminate_p();
}
natl notes;

extern "C" void main()
{
	notes = activate_p(notes_body, 0, 5, LIV_UTENTE);

	terminate_p();
}
