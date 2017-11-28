#include "copyright.h"
#include "synchconsole.h"

static void ReadAvailHandler(int arg)
{
	SynchConsole* sc = (SynchConsole*)arg;
	sc->ReadAvail();
}
static void WriteDoneHandler(int arg)
{
	SynchConsole* sc = (SynchConsole*)arg;
	sc->WriteDone();
}

SynchConsole::SynchConsole(char *readFile, char* writeFile)
{
	console = new Console(readFile, writeFile,
		ReadAvailHandler, WriteDoneHandler, (int)this);
	readlock = new Lock("synch console read lock");
	writelock = new Lock("synch console write lock");
	semRead = new Semaphore("synch console read", 0);
	semWrite = new Semaphore("synch console write", 0);
}
SynchConsole::~SynchConsole()
{
	delete semRead;
	delete semWrite;
	delete readlock;
	delete writelock;
	delete console;
}
int SynchConsole::GetChar()
{
	readlock->Acquire();
	semRead->P();
	int res = console->GetChar();
	readlock->Release();
	return res;
}
void SynchConsole::PutChar(char ch)
{
	writelock->Acquire();
	console->PutChar(ch);
	semWrite->P();
	writelock->Release();
}
void SynchConsole::ReadAvail()
{
	semRead->V();
}
void SynchConsole::WriteDone()
{
	semWrite->V();
}
