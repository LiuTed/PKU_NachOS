//synchconsole.h
//	Data structures to export a synchronous interface to the
//	raw console simulator

#ifndef SYNCHCONSOLE_H
#define SYNCHCONSOLE_H

#include "copyright.h"
#include "console.h"
#include "synch.h"

class SynchConsole
{
public:
	SynchConsole(char* readFile, char* writeFile);
	~SynchConsole();
	int GetChar();
	void PutChar(char);
	void ReadAvail();
	void WriteDone();
private:
	Console *console;
	Lock *readlock, *writelock;
	Semaphore *semRead, *semWrite;
};

#endif
