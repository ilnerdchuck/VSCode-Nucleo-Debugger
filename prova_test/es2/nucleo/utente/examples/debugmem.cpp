#include <all.h>


extern natl debugmem;
void p1(natq a)
{
	meminfo m = getmeminfo();
	printf("p1:                  heap %d, frame %d\n",
		m.heap_libero, m.num_frame_liberi);
	terminate_p();
}

void debugmem_body(natq a)
{
	meminfo m1, m2;

	m1 = getmeminfo();
	printf("prima di activate_p: heap %d, frame %d\n",
		m1.heap_libero, m1.num_frame_liberi);
	activate_p(p1, 0, 4, LIV_UTENTE);
	delay(10);
	m2 = getmeminfo();
	printf("dopo terminazione:   heap %d, frame %d\n",
		m2.heap_libero, m2.num_frame_liberi);
	pause();

	terminate_p();
}
natl debugmem;

extern "C" void main()
{
	debugmem = activate_p(debugmem_body, 0, 5, LIV_UTENTE);

	terminate_p();
}
