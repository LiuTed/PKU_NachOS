// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "openfile.h"
extern void StartProcess(char*);

int SwapPage(unsigned int vpn, int tid)
{
	char buf[32];
	TranslationEntry *entry = machine->InvPageTable;
	unsigned idx = machine->addrhash(vpn),
			 j = machine->rehash(tid),
			 pos = idx;
	int i = 0;
	do
	{
		if(!entry[idx].valid)
		{
			pos = idx;
			break;
		}
		else if(entry[idx].t < entry[pos].t)
		{
			pos = idx;
		}
		idx += j;
		idx %= NumPhysPages;
		i++;
	}while(i < NumPhysPages);
	if(!entry[pos].valid)
	{
		entry[pos].physicalPage = memmap->Find();
	}
	int pn = entry[pos].physicalPage;
	if(entry[pos].valid && entry[pos].dirty)
	{
		int ttid = entry[pos].tid;
		snprintf(buf, 32, "vm_%d", ttid);
		OpenFile *out = fileSystem->Open(buf);
		ASSERT(out);
		out->WriteAt(machine->mainMemory + pn*PageSize,
			PageSize, entry[pos].virtualPage*PageSize);
		delete out;
	}
	snprintf(buf, 32, "vm_%d", tid);
	OpenFile *in = fileSystem->Open(buf);
	ASSERT(in);
	in->ReadAt(machine->mainMemory + pn*PageSize,
		PageSize, vpn*PageSize);
	delete in;
	entry[pos].tid = tid;
	entry[pos].virtualPage = vpn;
	entry[pos].t = stats->totalTicks;
	entry[pos].tomb = entry[pos].dirty = entry[pos].readOnly =
	entry[pos].use = FALSE;
	entry[pos].valid = TRUE;
	stats->pageSwaps++;
	return pos;
}
//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

static void IncPC()
{
	machine->registers[PrevPCReg] = machine->registers[PCReg];
	machine->registers[PCReg] = machine->registers[NextPCReg];
	machine->registers[NextPCReg] += 4;
}
static void readytorun(int start)
{
	currentThread->space->RestoreState();
	machine->WriteRegister(PCReg, start);
	machine->WriteRegister(NextPCReg, start+4);
	machine->Run();
	ASSERT(FALSE);
}


void
ExceptionHandler(ExceptionType which)
{
	int type = machine->ReadRegister(2);

	if (which == SyscallException)
	{
		switch(type)
		{
			case SC_Halt:
			{
			DEBUG('a', "Shutdown, initiated by user program.\n");
			interrupt->Halt();
			break;
			}
			
			case SC_Exit:
			{
			DEBUG('a', "program exited.\n");
			exit_code[currentThread->getTID()] =
				machine->ReadRegister(4);
			sem_list[currentThread->getTID()]->V();
			currentThread->Finish();
			break;
			}

			case SC_Yield:
			{
			currentThread->Yield();
			break;
			}

			case SC_Join:
			{
			int id = machine->ReadRegister(4);
			DEBUG('a', "waiting for thread %d\n", id);
			sem_list[id]->P();
			int code = exit_code[id];
			waitforreap[id] = false;
			sem_list[id]->V();
			DEBUG('a', "join thread %d with exit code %d", id, code);
			machine->WriteRegister(2, code);
			break;
			}

			case SC_Exec:
			{
			int nameaddr = machine->ReadRegister(4);
			char *name;
			machine->ReadMemStr(nameaddr, NULL, name);
			Thread *t = new Thread("exec");
			machine->WriteRegister(2, t->getTID());
			waitforreap[t->getTID()] = true;
			t->Fork(StartProcess, (int)name);
			break;
			}

			case SC_Fork:
			{
			int handler = machine->ReadRegister(4);
			Thread *t = new Thread("fork", currentThread->getPriority());
			t->space = currentThread->space;
			currentThread->space->ref++;
			for(std::set<int>::iterator i = currentThread->fds.begin();
				i != currentThread->fds.end();
				++i)
			{
				machine->fd_table[*i].cnt++;
				t->fds.insert(*i);
			}
			t->Fork(readytorun, (void*)handler);
			break;
			}

			case SC_Create:
			{
			DEBUG('a', "syscall: create\n");
			char *name;
			int nameaddr = machine->ReadRegister(4);
			machine->ReadMemStr(nameaddr, NULL, name);
			bool succ = fileSystem->Create(name);
			if(succ)
				DEBUG('a', "\tcreate succeed\n");
			else
				DEBUG('a', "\tcreate failed\n");
			delete[] name;
			break;
			}

			case SC_Open:
			{
			DEBUG('a', "syscall: open\n");
			char *name;
			int nameaddr = machine->ReadRegister(4);
			machine->ReadMemStr(nameaddr, NULL, name);
			int fd = -1;
			OpenFile *f = fileSystem->Open(name);
			if(!f)
			{
				DEBUG('a', "\topen failed\n");
			}
			for(int i = 2; i < NumFD; i++)
			{
				if(!machine->fd_table[i].file)
				{
					machine->fd_table[i].file = f;
					machine->fd_table[i].cnt = 1;
					fd = i;
					break;
				}
			}
			DEBUG('a', "\tresult fd:%d\n", fd);
			machine->WriteRegister(2, fd);
			currentThread->fds.insert(fd);
			delete[] name;
			break;
			}

			case SC_Write:
			{
			DEBUG('a', "syscall: write\n");
			int bufaddr = machine->ReadRegister(4);
			int size = machine->ReadRegister(5);
			int fd = machine->ReadRegister(6);
			char *buffer = new char[size];
			machine->ReadMemArr(bufaddr, size, buffer);
			ASSERT(fd >= 0 && fd < NumFD);
			OpenFile *f = machine->fd_table[fd].file;
			ASSERT(f);
			f->Write(buffer, size);
			delete[] buffer;
			break;
			}

			case SC_Read:
			{
			DEBUG('a', "syscall: read\n");
			int bufaddr = machine->ReadRegister(4);
			int size = machine->ReadRegister(5);
			int fd = machine->ReadRegister(6);
			char *buffer = new char[size];
			ASSERT(fd >= 0 && fd < NumFD);
			OpenFile *f = machine->fd_table[fd].file;
			ASSERT(f);
			int res = f->Read(buffer, size);
			machine->WriteRegister(2, res);
			machine->WriteMemArr(bufaddr, res, buffer);
			delete[] buffer;
			break;
			}

			case SC_Close:
			{
			DEBUG('a', "syscall: close\n");
			int fd = machine->ReadRegister(4);
			ASSERT(fd >= 0 && fd < NumFD);
			ASSERT(machine->fd_table[fd].file);
			int cnt = --machine->fd_table[fd].cnt;
			if(cnt == 0)
			{
				DEBUG('a', "\tfile closed\n");
				delete machine->fd_table[fd].file;
				machine->fd_table[fd].file = NULL;
			}
			currentThread->fds.erase(fd);
			break;
			}

			case SC_PutChar:
			{
			putchar(machine->ReadRegister(4));
			break;
			}

			case SC_PutInt:
			{
			printf("%d", machine->ReadRegister(4));
			break;
			}

			default:
			printf("unknown syscall\n");
			ASSERT(FALSE);
		}
		IncPC();
	}
	else if(which == PageFaultException)
	{
		int addr = machine->ReadRegister(BadVAddrReg);
    	//printf("page fault at virtual address %x\n" ,addr);
		if(machine->tlb == NULL)
		{
			SwapPage(addr / PageSize, currentThread->space->tid);
		}
		else
		{
			int vpn = (unsigned)addr / PageSize,
				idx = machine->find(vpn, currentThread->space->tid);

			if (vpn >= machine->pageTableSize)
			{
				DEBUG('a', "virtual page # %d too large for page table size %d!\n", 
					addr, machine->pageTableSize);
				machine->RaiseException(AddressErrorException, addr);
			}
			else if (idx >= NumPhysPages)
			{
				DEBUG('a', "virtual page # %d too large for page table size %d!\n", 
					addr, machine->pageTableSize);
				idx = SwapPage(vpn, currentThread->space->tid);
				ASSERT(idx < NumPhysPages);
			}

			int pos = 0;
			for(int i = 0; i < TLBSize; i++)
			{
				if(!machine->tlb[pos]) continue;
				if(machine->tlb[i] == machine->InvPageTable+idx)
				{
					pos = i;
					break;
				}
				if(!machine->tlb[i])
					pos = i;
				else if(machine->tlb[pos]->t > machine->tlb[i]->t)
					pos = i;
			}
			machine->tlb[pos] = machine->InvPageTable+idx;
			machine->tlb[pos]->t = stats->totalTicks;
		}
	}
	else {
		printf("Unexpected user mode exception %d %d\n", which, type);
		ASSERT(FALSE);
	}
}
