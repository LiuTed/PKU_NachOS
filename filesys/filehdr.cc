// filehdr.cc 
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the 
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the 
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect 
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector, 
//
//      Unlike in a real system, we do not keep track of file permissions, 
//	ownership, last modification date, etc., in the file header. 
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "system.h"
#include "filehdr.h"

//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the bit map of free disk sectors
//----------------------------------------------------------------------

bool
FileHeader::Allocate(BitMap *freeMap, int fileSize, int type)
{ 
    DEBUG('f', "Allocating file with size %d, type %d\n",
        fileSize, type);
    numBytes = fileSize;
    numSectors  = divRoundUp(fileSize, SectorSize);
    filetype = type;
    int numSI = divRoundUp(numSectors, NumSecondIdx);
    if (freeMap->NumClear() < numSectors+numSI)
	    return FALSE;		// not enough space
    if(numSI > NumFirstIdx)
        return FALSE;
    int tmp[NumSecondIdx];
    for(int i = 0; i < numSI; i++)
    {
        FirstIdx[i] = freeMap->Find();
        synchDisk->ReadSector(FirstIdx[i], (char*)tmp);
        for(int j = 0; j < NumSecondIdx; j++)
        {
            if(i*NumSecondIdx+j < numSectors)
                tmp[j] = freeMap->Find();
            else
                tmp[j] = -1;
        }
        synchDisk->WriteSector(FirstIdx[i], (char*)tmp);
    }
    for(int i = numSI; i < NumFirstIdx; i++)
        FirstIdx[i] = -1;

    time(&creation);
    time(&lastaccess);
    time(&lastwrite);
    return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

void 
FileHeader::Deallocate(BitMap *freeMap)
{
    DEBUG('f', "Deallocating file\n");
    int numSI = divRoundUp(numSectors, NumSecondIdx);
    int tmp[NumSecondIdx];
    for(int i = 0; i < numSI; i++)
    {
        ASSERT(freeMap->Test(FirstIdx[i]));
//        DEBUG('f', "freeing first index at %d\n", FirstIdx[i]);
        synchDisk->ReadSector(FirstIdx[i], (char*)tmp);
        for(int j = 0; j < NumSecondIdx; j++)
        {
            if(i*NumSecondIdx+j < numSectors)
            {
                ASSERT(freeMap->Test(tmp[j]));
//        DEBUG('f', "freeing secondary index at %d\n", tmp[j]);
                freeMap->Clear(tmp[j]);
                tmp[j] = -1;
            }
        }
//        synchDisk->WriteSector(FirstIdx[i], (char*)tmp);
        freeMap->Clear(FirstIdx[i]);
        FirstIdx[i] = -1;
    }
    numBytes = numSectors = 0;
    freeMap->Print();
}

bool
FileHeader::Reallocate(BitMap *freeMap, int newSize)
{
    DEBUG('f', "Reallocating file to size %d\n", newSize);
    if(newSize <= 0)
    {
        Deallocate(freeMap);
        return TRUE;
    }
    else if(newSize > numBytes)
    {
        int nnumSectors = divRoundUp(newSize, SectorSize);
        int numSI = divRoundUp(numSectors, NumSecondIdx);
        int nnumSI = divRoundUp(nnumSectors, NumSecondIdx);
        if(freeMap->NumClear() <
            nnumSectors-numSectors + nnumSI-numSI)
            return FALSE;//no enough space
        if(nnumSectors > NumSectors)
            return FALSE;//no enough space
        if(nnumSI > NumFirstIdx)
            return FALSE;//no enough space
        int tmp[NumSecondIdx];
        for(int i = numSectors; i < nnumSectors;)
        {
            int fidx = divRoundDown(i, NumSecondIdx);
            if(FirstIdx[fidx] < 0)
            {
                FirstIdx[fidx] = freeMap->Find();
            }
            synchDisk->ReadSector(FirstIdx[fidx], (char*)tmp);
            for(int j = i-fidx*NumSecondIdx;
                j < NumSecondIdx; j++, i++)
            {
                if(fidx*NumSecondIdx+j < nnumSectors)
                {
                    tmp[j] = freeMap->Find();
                }
                else
                    tmp[j] = -1;
            }
            synchDisk->WriteSector(FirstIdx[fidx], (char*)tmp);
        }
        numBytes = newSize;
        numSectors = nnumSectors;
        return TRUE;
    }
    else if(newSize < numBytes)
    {
        int nnumSectors = divRoundUp(newSize, SectorSize);
        int numSI = divRoundUp(numSectors, NumSecondIdx);
        int nnumSI = divRoundUp(nnumSectors, NumSecondIdx);
        int tmp[NumSecondIdx];
        for(int i = numSectors - 1; i >= nnumSectors;)
        {
            int fidx = divRoundDown(i, NumSecondIdx);
            ASSERT(freeMap->Test(FirstIdx[fidx]));
            synchDisk->ReadSector(FirstIdx[fidx], (char*)tmp);
            for(int j = i-fidx*NumSecondIdx;
                j >= 0; j--, i--)
            {
                ASSERT(freeMap->Test(tmp[j]));
                freeMap->Clear(tmp[j]);
                tmp[j] = -1;
            }
            synchDisk->WriteSector(FirstIdx[fidx], (char*)tmp);
            if(i < fidx*NumSecondIdx)
                FirstIdx[fidx] = -1;
        }
        numBytes = newSize;
        numSectors = nnumSectors;
        return TRUE;
    }
    return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk. 
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void
FileHeader::FetchFrom(int sector)
{
    synchDisk->ReadSector(sector, (char *)this);
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk. 
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void
FileHeader::WriteBack(int sector)
{
    synchDisk->WriteSector(sector, (char *)this); 
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------

int
FileHeader::ByteToSector(int offset)
{
    int sec = divRoundDown(offset, SectorSize);
    int fidx = sec/NumSecondIdx;
    int tmp[NumSecondIdx];
    synchDisk->ReadSector(FirstIdx[fidx], (char*)tmp);
    return (tmp[sec % NumSecondIdx]);
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int
FileHeader::FileLength()
{
    return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

void
FileHeader::Print()
{
    int i, j, k;
    int numSI = divRoundUp(numSectors, SectorSize);
    char *data = new char[SectorSize];
    int tmp[NumSecondIdx];

    printf("FileHeader contents.  File size: %d, type: %d\n",
        numBytes, filetype);
    printf("creation: ");
    struct tm* _t = localtime(&creation);
    strftime(data, SectorSize, "%x %X", _t);
    puts(data);

    printf("last access: ");
    _t = localtime(&lastaccess);
    strftime(data, SectorSize, "%x %X", _t);
    puts(data);

    printf("last write: ");
    _t = localtime(&lastwrite);
    strftime(data, SectorSize, "%x %X", _t);
    puts(data);
    for (i = 0; i < numSI; i++)
    {
        printf("%d: ", FirstIdx[i]);
        synchDisk->ReadSector(FirstIdx[i], (char*)tmp);
        for(j = 0; j < NumSecondIdx; j++)
        {
            if(i * NumSecondIdx + j < numSectors)
                printf("%d, ", tmp[j]);
        }
        putchar('\n');
    }
    printf("File contents:\n");
/*
    for (i = k = 0; i < numSectors; i++) {
	synchDisk->ReadSector(dataSectors[i], data);
        for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
	    if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
		printf("%c", data[j]);
            else
		printf("\\%x", (unsigned char)data[j]);
	}
        printf("\n"); 
    }
*/
    int l = 0, m = 0;
    for (i = 0; i < numSI; i++)
    {
        synchDisk->ReadSector(FirstIdx[i], (char*)tmp);
        for(j = 0; j < NumSecondIdx; j++)
        {
            if(i * NumSecondIdx + j < numSectors)
            {
                synchDisk->ReadSector(tmp[j], data);
                for(k = 0; k < SectorSize && l < numBytes; k++, l++)
                {
                    printf("%02x ", (unsigned char)data[k]);
                    m++;
                    if(m==16)
                    {
                        putchar('\n');
                        m = 0;
                    }
                }
                printf("end sector %d\n", tmp[j]);
            }
            else break;
        }
    }
    putchar('\n');
    delete [] data;
}
