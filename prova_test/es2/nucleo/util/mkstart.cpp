#include <fstream>
#include <iomanip>
#include <costanti.h>
#include <libce.h>
#include <vm.h>

using namespace std;

int main()
{
	natq start_io = norm(((natq)I_MIO_C << 39UL));
	natq start_utente = norm(((natq)I_UTN_C << 39UL));

	ofstream startmk("util/start.mk");
	startmk << "MEM=" << MEM_TOT/MiB << endl;
	startmk << hex;
	startmk << "START_IO=0x"      << start_io << endl;
	startmk << "START_UTENTE=0x"  << start_utente << endl;
	startmk.close();

	ofstream startgdb("util/tmp.gdb");
	startgdb << "set $MAX_LIV="      	<< MAX_LIV << endl;
	startgdb << "set $MAX_SEM="      	<< MAX_SEM << endl;
	startgdb << "set $SEL_CODICE_SISTEMA="  << SEL_CODICE_SISTEMA << endl;
	startgdb << "set $SEL_CODICE_UTENTE="   << SEL_CODICE_UTENTE << endl;
	startgdb << "set $SEL_DATI_UTENTE="     << SEL_DATI_UTENTE << endl;
	startgdb << "set $MAX_PROC="     	<< MAX_PROC << endl;
	startgdb << "set $MAX_PRIORITY="     	<< MAX_PRIORITY << endl;
	startgdb << "set $MIN_PRIORITY="     	<< MIN_PRIORITY << endl;
	startgdb << "set $I_SIS_C="		<< I_SIS_C << endl;
	startgdb << "set $I_SIS_P="		<< I_SIS_P << endl;
	startgdb << "set $I_MIO_C="		<< I_MIO_C << endl;
	startgdb << "set $I_UTN_C="		<< I_UTN_C << endl;
	startgdb << "set $I_UTN_P="		<< I_UTN_P << endl;
	startgdb.close();

	ofstream startpl("util/start.pl");
	startpl << hex;
	startpl << "$START_IO='"      << setw(16) << setfill('0') << start_io << "';" << endl;
	startpl << "$START_UTENTE='"  << setw(16) << setfill('0') << start_utente << "';" << endl;
	startpl << "$CE_ADDR2LINE='" << CE_ADDR2LINE << "';" << endl;
	startpl << "1;" << endl;
	startpl.close();

	return 0;
}
