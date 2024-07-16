#include <all.h>

#define dbg(s, ...) flog(LOG_DEBUG, "TEST %lu: " s, test_num, ## __VA_ARGS__)
#define msg(s, ...) printf("TEST %lu PROC %d: " s "\n", test_num, getpid(), ## __VA_ARGS__)
#define err(s, ...) msg("ERRORE: " s, ## __VA_ARGS__)
#define die(s, ...) do { err(s, ## __VA_ARGS__); goto error; } while (0)

#define new_proc(tn, pn)\
	t##tn##p##pn = activate_p(t##tn##p##pn##b, test_num, prio--, LIV_UTENTE);

natl end_test; // sync

#define end_subtest() do {\
	(void)&&error;\
error:\
	terminate_p();\
} while (0)

#define end_test() do {\
	(void)&&error;\
error:\
	sem_signal(end_test);\
	terminate_p();\
} while (0)

#define TCNT(n)	natl t##n##m0; natl t##n##n0;
#define testok(n) do {\
	sem_wait(t##n##m0);\
	t##n##n0++;\
	sem_signal(t##n##m0);\
} while (0)

static inline const char* bool2str(natq v)
{
	static char buf[100];
	switch(v) {
	case false:
		return "false";
	case true:
		return "true";
	default:
		snprintf(buf, 100, "%lu", v);
		return buf;
	}
}

#define ckbarrier(id_, er_) do {\
	bool r_ = barrier(id_);\
	if ((er_) != r_) {\
		err("ottenuto %s invece di %s",\
			bool2str(r_), bool2str(er_));\
		goto error;\
	}\
} while (0)
natq test_num;

///**********************************************************************
// *             test 00: errori vari                                   *
// **********************************************************************/

natl t00p0;

void t00p0b(natq test_num)
{
	barrier(10);
	err("accesso a barriera non esistente non ha causato abort");
	terminate_p();
}


///**********************************************************************
// *             test 01: nessun time out                               *
// **********************************************************************/

natl t01p0;
natl t01p1;
natl t01v0;
TCNT(01);

void t01p0b(natq test_num)
{
	t01v0 = barrier_create(2, 100);
	ckbarrier(t01v0, true);
	testok(01);
	end_test();
}

void t01p1b(natq test_num)
{
	ckbarrier(t01v0, true);
	testok(01);
	end_test();
}

///**********************************************************************
// *             test 02: nessun time out e riuso della barriera        *
// **********************************************************************/

natl t02p0;
natl t02p1;
natl t02v0;
TCNT(02);

void t02p0b(natq test_num)
{
	t02v0 = barrier_create(2, 100);
	ckbarrier(t02v0, true);
	ckbarrier(t02v0, true);
	testok(02);
	end_test();
}

void t02p1b(natq test_num)
{
	ckbarrier(t02v0, true);
	ckbarrier(t02v0, true);
	testok(02);
	end_test();
}

///**********************************************************************
// *             test 03: time out con un solo processo                 *
// **********************************************************************/

natl t03p0;
natl t03v0;
TCNT(03);

void t03p0b(natq test_num)
{
	t03v0 = barrier_create(2, 5);
	ckbarrier(t03v0, false);
	testok(03);
	end_test();
}

///**********************************************************************
// *             test 04: timeout con più processi                      *
// **********************************************************************/

natl t04p0;
natl t04p1;
natl t04v0;
TCNT(04);

void t04p0b(natq test_num)
{
	t04v0 = barrier_create(3, 5);
	ckbarrier(t04v0, false);
	testok(04);
	end_test();
}

void t04p1b(natq test_num)
{
	ckbarrier(t04v0, false);
	testok(04);
	end_test();
}

///**********************************************************************
// *             test 05: nessun timeout con processi scorrelati        */
// **********************************************************************/

natl t05p0;
natl t05p1;
natl t05p2;
natl t05v0;
TCNT(05);

void t05p0b(natq test_num)
{
	t05v0 = barrier_create(2, 8);
	ckbarrier(t05v0, true);
	testok(05);
	end_test();
}

void t05p1b(natq test_num)
{
	delay(4);
	ckbarrier(t05v0, true);
	testok(05);
	end_test();
}

void t05p2b(natq test_num)
{
	delay(12);
	testok(05);
	end_test();
}

///**********************************************************************
// *             test 06: time out e barriera aperta                    *
// **********************************************************************/

natl t06p0;
natl t06p1;
natl t06p2;
natl t06v0;
TCNT(06);

void t06p0b(natq test_num)
{
	t06v0 = barrier_create(3, 8);
	ckbarrier(t06v0, false);
	testok(06);
	end_test();
}

void t06p1b(natq test_num)
{
	delay(4);
	ckbarrier(t06v0, false);
	testok(06);
	end_test();
}

void t06p2b(natq test_num)
{
	delay(12);
	ckbarrier(t06v0, false);
	testok(06);
	end_test();
}

///**********************************************************************
// *             test 07: time out e riutilizzo della barriera          *
// **********************************************************************/

natl t07p0;
natl t07p1;
natl t07v0;
natl t07s0;
TCNT(07);

void t07p0b(natq test_num)
{
	t07v0 = barrier_create(2, 4);
	ckbarrier(t07v0, false);
	sem_wait(t07s0);
	ckbarrier(t07v0, true);
	testok(07);
	end_test();
}

void t07p1b(natq test_num)
{
	delay(12);
	ckbarrier(t07v0, false);
	sem_signal(t07s0);
	ckbarrier(t07v0, true);
	testok(07);
	end_test();
}

///**********************************************************************
// *             test 08: processi scorrelati in coda pronti            *
// **********************************************************************/

natl t08p0;
natl t08p1;
natl t08p2;
natl t08v0;
TCNT(08);

void t08p0b(natq test_num)
{
	delay(8);
	testok(08);
	end_test();
}

void t08p1b(natq test_num)
{
	t08v0 = barrier_create(3, 8);
	ckbarrier(t08v0, false);
	testok(08);
	end_test();
}

void t08p2b(natq test_num)
{
	delay(8);
	ckbarrier(t08v0, false);
	testok(08);
	end_test();
}


/**********************************************************************/


extern natl mainp;
void main_body(natq id)
{
	natl prio = 600;

	end_test = sem_ini(0);

	test_num = 0;
	dbg(">>>INIZIO<<<: errori vari");
	new_proc(00, 0);
	delay(1);
	dbg("=== FINE ===");

	test_num = 1;
	dbg(">>>INIZIO<<<: nessun time out");
	new_proc(01, 0);
	new_proc(01, 1);
	sem_wait(end_test);
	sem_wait(end_test);
	if (t01n0 == 2) msg("OK");
	dbg("=== FINE ===");

	test_num = 2;
	dbg(">>>INIZIO<<<: nessun time out e riuso della barriera");
	new_proc(02, 0);
	new_proc(02, 1);
	sem_wait(end_test);
	sem_wait(end_test);
	if (t02n0 == 2) msg("OK");
	dbg("=== FINE ===");

	test_num = 3;
	dbg(">>>INIZIO<<<: time out con un solo processo");
	new_proc(03, 0);
	sem_wait(end_test);
	if (t03n0 == 1) msg("OK");
	dbg("=== FINE ===");

	test_num = 4;
	dbg(">>>INIZIO<<<: time out con piu' processi");
	new_proc(04, 0);
	new_proc(04, 1);
	sem_wait(end_test);
	sem_wait(end_test);
	if (t04n0 == 2) msg("OK");
	dbg("=== FINE ===");

	test_num = 5;
	dbg(">>>INIZIO<<<: nessun timeout con processi scorrelati");
	new_proc(05, 0);
	new_proc(05, 1);
	new_proc(05, 2);
	sem_wait(end_test);
	sem_wait(end_test);
	sem_wait(end_test);
	if (t05n0 == 3) msg("OK");
	dbg("=== FINE ===");

	test_num = 6;
	dbg(">>>INIZIO<<<: timeout e barriera aperta");
	new_proc(06, 0);
	new_proc(06, 1);
	new_proc(06, 2);
	sem_wait(end_test);
	sem_wait(end_test);
	sem_wait(end_test);
	if (t06n0 == 3) msg("OK");
	dbg("=== FINE ===");

	test_num = 7;
	dbg(">>>INIZIO<<<: timeout e riutilizzo della barriera");
	t07s0 = sem_ini(0);
	new_proc(07, 0);
	new_proc(07, 1);
	sem_wait(end_test);
	sem_wait(end_test);
	if (t07n0 == 2) msg("OK");
	dbg("=== FINE ===");

	test_num = 8;
	dbg(">>>INIZIO<<<: processi scorrelati in coda pronti");
	new_proc(08, 0);
	new_proc(08, 1);
	new_proc(08, 2);
	sem_wait(end_test);
	sem_wait(end_test);
	sem_wait(end_test);
	if (t08n0 == 3) msg("OK");
	dbg("=== FINE ===");

	pause();

	terminate_p();
}
natl mainp;

extern "C" void main()
{
	mainp = activate_p(main_body, 0, 900, LIV_UTENTE);

	terminate_p();
}
