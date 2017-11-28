// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"
#ifdef HOST_SPARC
#include <strings.h>
#endif
static inline int min(int a, int b)
{
    if(a < b) return a;
    return b;
}

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable)
{
    NoffHeader noffH;
    unsigned int i, size;

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

//    ASSERT(numPages <= NumPhysPages);		// check we're not trying
						// to run anything too big --
						// at least until we have
						// virtual memory

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
					numPages, size);
// first, set up the translation 
/*    pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++) {
	pageTable[i].virtualPage = i;	// for now, virtual page # = phys page #
	int pn = memmap->Find();
    if(pn < 0)
        throw std::overflow_error("running out of mainMemory");
    pageTable[i].physicalPage = pn;
	pageTable[i].valid = TRUE;
	pageTable[i].use = FALSE;
	pageTable[i].dirty = FALSE;
	pageTable[i].readOnly = FALSE;  // if the code segment was entirely on 
					// a separate page, we could set its 
					// pages to be read-only
    bzero(machine->mainMemory+pn*PageSize, PageSize);
    }*/

/*    tid = currentThread->getTID();
    for(i = 0; i < numPages; i++)
    {
        unsigned int idx = machine->getnew(i, tid);
        if(idx >= NumPhysPages)
            throw std::overflow_error("running out of mainMemory");
        machine->InvPageTable[idx].tid = currentThread->getTID();
        machine->InvPageTable[idx].valid = TRUE;
        machine->InvPageTable[idx].readOnly = FALSE;
        machine->InvPageTable[idx].use = FALSE;
        machine->InvPageTable[idx].dirty = FALSE;
        machine->InvPageTable[idx].tomb = FALSE;
        machine->InvPageTable[idx].t = stats->totalTicks;
        machine->InvPageTable[idx].virtualPage = i;
        machine->InvPageTable[idx].physicalPage = memmap->Find();
    } 
*/  
// zero out the entire address space, to zero the unitialized data segment 
// and the stack segment
//    bzero(machine->mainMemory, size);

    char buf[PageSize];
    char name[32];
    snprintf(name, 32, "vm_%d", currentThread->getTID());
    fileSystem->Create(name, size);
    OpenFile *vm = fileSystem->Open(name);
    ASSERT(vm);
// then, copy in the code and data segments into memory
    if (noffH.code.size > 0) {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
			noffH.code.virtualAddr, noffH.code.size);
//        executable->ReadAt(&(machine->mainMemory[noffH.code.virtualAddr]),
//			noffH.code.size, noffH.code.inFileAddr);
        int codesize = noffH.code.size,
            ifaddr = noffH.code.inFileAddr,
            codevaddr = noffH.code.virtualAddr;
        while(codesize > 0)
        {
            int vpn = codevaddr / PageSize,
                off = codevaddr % PageSize,
                idx = machine->find(vpn, tid),
                pn = machine->InvPageTable[idx].physicalPage,
                readsize = min(codesize, PageSize-off);
//            executable->ReadAt(machine->mainMemory + pn*PageSize + off,
//                readsize, ifaddr);
            executable->ReadAt(buf + off, readsize, ifaddr);
            vm->WriteAt(buf + off, PageSize, codevaddr);
            ifaddr += readsize;
            codesize -= readsize;
            codevaddr += readsize;
        }
    }
    if (noffH.initData.size > 0) {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
			noffH.initData.virtualAddr, noffH.initData.size);
//       executable->ReadAt(&(machine->mainMemory[noffH.initData.virtualAddr]),
//			noffH.initData.size, noffH.initData.inFileAddr);
        int datasize = noffH.initData.size,
            ifaddr = noffH.initData.inFileAddr,
            datavaddr = noffH.initData.virtualAddr;
        while(datasize > 0)
        {
            int vpn = datavaddr / PageSize,
                off = datavaddr % PageSize,
                idx = machine->find(vpn, tid),
                pn = machine->InvPageTable[idx].physicalPage,
                readsize = min(datasize, PageSize-off);
//            executable->ReadAt(machine->mainMemory + pn*PageSize + off,
//                readsize, ifaddr);
            executable->ReadAt(buf + off, readsize, ifaddr);
            vm->WriteAt(buf + off, PageSize, datavaddr);
            ifaddr += readsize;
            datasize -= readsize;
            datavaddr += readsize;
        }
    }
    delete vm;

}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
    for(int i = 0; i < numPages; i++)
    {
        int idx = machine->find(i, tid);
        if(idx >= NumPhysPages) continue;
        machine->InvPageTable[idx].valid = FALSE;
        machine->InvPageTable[idx].tomb = TRUE;
        memmap->Clear(machine->InvPageTable[idx].physicalPage);
    }
//    delete pageTable;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
//    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}
