// directory.cc 
//	Routines to manage a directory of file names.
//
//	The directory is a table of fixed length entries; each
//	entry represents a single file, and contains the file name,
//	and the location of the file header on disk.  The fixed size
//	of each directory entry means that we have the restriction
//	of a fixed maximum size for file names.
//
//	The constructor initializes an empty directory of a certain size;
//	we use ReadFrom/WriteBack to fetch the contents of the directory
//	from disk, and to write back any modifications back to disk.
//
//	Also, this implementation has the restriction that the size
//	of the directory cannot expand.  In other words, once all the
//	entries in the directory are used, no more files can be created.
//	Fixing this is one of the parts to the assignment.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "utility.h"
#include "filehdr.h"
#include "directory.h"

#define RootSector 2
#define OFFSET(class, member) reinterpret_cast<int>(&(((class*)0)->member))

bool Serialize(DirectoryEntry2* ptr, OpenFile* file, int& at)
{
    const int s = OFFSET(DirectoryEntry2, parent);
    file->WriteAt((char*)ptr, s, at);
    at += s;
    file->WriteAt(ptr->name, ptr->namelen+1, at);
    at += ptr->namelen + 1;
    if(ptr->left)
        Serialize(ptr->left, file, at);
    if(ptr->right)
        Serialize(ptr->right, file, at);
    return TRUE;
}
DirectoryEntry2*
Deserialize(DirectoryEntry2* parent, OpenFile* file, int& at)
{
    const int s = OFFSET(DirectoryEntry2, parent);
    DirectoryEntry2 *ptr = new DirectoryEntry2;
    file->ReadAt((char*)ptr, s, at);
    at += s;
    ptr->name = new char[ptr->namelen + 1];
    file->ReadAt(ptr->name, ptr->namelen+1, at);
    at += ptr->namelen + 1;
    ptr->parent = parent;
    if(ptr->left)
        ptr->left = Deserialize(ptr, file, at);
    if(ptr->right)
        ptr->right = Deserialize(parent, file, at);
    return ptr;
}

//----------------------------------------------------------------------
// Directory::Directory
// 	Initialize a directory; initially, the directory is completely
//	empty.  If the disk is being formatted, an empty directory
//	is all we need, but otherwise, we need to call FetchFrom in order
//	to initialize it from disk.
//
//	"size" is the number of entries in the directory
//----------------------------------------------------------------------

Directory::Directory(int size)
{
/*
    table = new DirectoryEntry[size];
    tableSize = size;
    for (int i = 0; i < tableSize; i++)
	table[i].inUse = FALSE;
*/
    root = new DirectoryEntry2;
    cur = root;
    root->name = strdup("/");
    root->namelen = strlen(root->name);
    root->left = root->right = root->parent = NULL;
    root->isdir = TRUE;
    root->sector = RootSector;
}

//----------------------------------------------------------------------
// Directory::~Directory
// 	De-allocate directory data structure.
//----------------------------------------------------------------------

Directory::~Directory()
{ 
//    delete [] table;
    delete root;
} 

//----------------------------------------------------------------------
// Directory::FetchFrom
// 	Read the contents of the directory from disk.
//
//	"file" -- file containing the directory contents
//----------------------------------------------------------------------

void
Directory::FetchFrom(OpenFile *file)
{
//    (void) file->ReadAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
    delete root;
    int at = 0;
    root = Deserialize(NULL, file, at);
    cur = root;
}

//----------------------------------------------------------------------
// Directory::WriteBack
// 	Write any modifications to the directory back to disk
//
//	"file" -- file to contain the new directory contents
//----------------------------------------------------------------------

void
Directory::WriteBack(OpenFile *file)
{
//    (void) file->WriteAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
    int at = 0;
    Serialize(root, file, at);
}

//----------------------------------------------------------------------
// Directory::FindIndex
// 	Look up file name in directory, and return its location in the table of
//	directory entries.  Return -1 if the name isn't in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------

DirectoryEntry2*
Directory::FindIndex(char *name)
{
/*
    for (int i = 0; i < tableSize; i++)
        if (table[i].inUse && !strncmp(table[i].name, name, FileNameMaxLen))
	    return i;
    return -1;		// name not in directory
*/
    ASSERT(name[0]);//empty string is not acceptable
    int s, e;
    DirectoryEntry2 *ptr;
    if(!strncmp(name, "./", 2))
    {
        s = e = 2;
        ptr = cur;
    }
    else if(!strncmp(name, "/", 1))
    {
        s = e = 1;
        ptr = root;
    }
    else
    {
        s = e = 0;
        ptr = cur;
    }
    while(true)
    {
        if(e < s)
        {
            //do nothing;
        }
        else if(name[e] == '/')
        {
            if(!strncmp(name+s, "..", 2) && e-s == 2)
            {
                ptr = ptr->parent;
            }
            else
            {
                for(ptr = ptr->left; ptr; ptr = ptr->right)
                {
                    if(ptr->namelen == e-s
                        && !strncmp(ptr->name, name+s, e-s)
                        && ptr->isdir)
                    {
                        break;
                    }
                }
            }
            s = e + 1;
            if(!ptr)
            {
                DEBUG('f', "FindIndex fail, no such directory %*s\n",
                    e, name);
                return NULL;
            }
        }
        else if(name[e] == '\0')
        {
            for(ptr = ptr->left; ptr; ptr = ptr->right)
            {
                if(ptr->namelen == e-s
                    &&!strncmp(ptr->name, name+s, e-s))
                {
                    return ptr;
                }
            }
            DEBUG('f', "FindIndex fail, no such file %s\n",
                name);
            return ptr;
        }
        e++;
    }
}

//----------------------------------------------------------------------
// Directory::Find
// 	Look up file name in directory, and return the disk sector number
//	where the file's header is stored. Return -1 if the name isn't 
//	in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------

int
Directory::Find(char *name)
{
    DirectoryEntry2* ptr = FindIndex(name);

    if (ptr) return ptr->sector;
    return -1;
}

//----------------------------------------------------------------------
// Directory::Add
// 	Add a file into the directory.  Return TRUE if successful;
//	return FALSE if the file name is already in the directory, or if
//	the directory is completely full, and has no more space for
//	additional file names.
//
//	"name" -- the name of the file being added
//	"newSector" -- the disk sector containing the added file's header
//----------------------------------------------------------------------

bool
Directory::Add(char *name, int newSector, bool dir)
{ 
/*
    if (FindIndex(name) != -1)
	return FALSE;

    for (int i = 0; i < tableSize; i++)
        if (!table[i].inUse) {
            table[i].inUse = TRUE;
            strncpy(table[i].name, name, FileNameMaxLen); 
            table[i].sector = newSector;
        return TRUE;
	}
    return FALSE;	// no space.  Fix when we have extensible files.
*/
    ASSERT(name[0]);//empty string is not acceptable
    int s, e;
    DirectoryEntry2 *ptr;
    if(!strncmp(name, "./", 2))
    {
        s = e = 2;
        ptr = cur;
    }
    else if(!strncmp(name, "/", 1))
    {
        s = e = 1;
        ptr = root;
    }
    else
    {
        s = e = 0;
        ptr = cur;
    }
    while(true)
    {
        if(e < s)
        {
            //do nothing;
        }
        else if(name[e] == '/')
        {
            if(!strncmp(name+s, "..", 2) && e-s == 2)
            {
                ptr = ptr->parent;
            }
            else
            {
                for(ptr = ptr->left; ptr; ptr = ptr->right)
                {
                    if(ptr->namelen == e-s
                        && !strncmp(ptr->name, name+s, e-s)
                        && ptr->isdir)
                    {
                        break;
                    }
                }
            }
            s = e + 1;
            if(!ptr)
            {
                DEBUG('f', "Add fail, no such directory %*s\n",
                    e, name);
                return FALSE;
            }
        }
        else if(name[e] == '\0')
        {
            DirectoryEntry2* tmp;
            for(tmp = ptr->left; tmp; tmp = tmp->right)
            {
                if(tmp->namelen == e-s
                    &&!strncmp(tmp->name, name+s, e-s))
                {
                    DEBUG('f', "Add fail, file %s already exists\n",
                        name);
                    return FALSE;
                }
            }
            break;
        }
        e++;
    }
    DirectoryEntry2* nde = new DirectoryEntry2;
    nde->right = ptr->left;
    nde->parent = ptr;
    nde->left = NULL;
    nde->isdir = dir;
    nde->sector = newSector;
    nde->name = strdup(name+s);
    nde->namelen = strlen(nde->name);
    ptr->left = nde;
    return TRUE;
}

//----------------------------------------------------------------------
// Directory::Remove
// 	Remove a file name from the directory.  Return TRUE if successful;
//	return FALSE if the file isn't in the directory. 
//
//	"name" -- the file name to be removed
//----------------------------------------------------------------------

bool
Directory::Remove(char *name)
{
/* 
    int i = FindIndex(name);

    if (i == -1)
	return FALSE; 		// name not in directory
    table[i].inUse = FALSE;
    return TRUE;
*/	
    ASSERT(name[0]);//empty string is not acceptable
    int s, e;
    DirectoryEntry2 *ptr;
    if(!strncmp(name, "./", 2))
    {
        s = e = 2;
        ptr = cur;
    }
    else if(!strncmp(name, "/", 1))
    {
        s = e = 1;
        ptr = root;
    }
    else
    {
        s = e = 0;
        ptr = cur;
    }
    while(true)
    {
        if(e < s)
        {
            //do nothing;
        }
        else if(name[e] == '/')
        {
            if(!strncmp(name+s, "..", 2) && e-s == 2)
            {
                ptr = ptr->parent;
            }
            else
            {
                for(ptr = ptr->left; ptr; ptr = ptr->right)
                {
                    if(ptr->namelen == e-s
                        && !strncmp(ptr->name, name+s, e-s)
                        && ptr->isdir)
                    {
                        break;
                    }
                }
            }
            s = e + 1;
            if(!ptr)
            {
                DEBUG('f', "Remove fail, no such directory %*s\n",
                    e, name);
                return FALSE;
            }
        }
        else if(name[e] == '\0')
        {
            for(ptr = ptr->left; ptr; ptr = ptr->right)
            {
                if(ptr->namelen == e-s
                    &&!strncmp(ptr->name, name+s, e-s))
                {
                    break;
                }
            }
            if(!ptr)
            {
                DEBUG('f', "Remove fail, file %s doesn't exist\n",
                    name);
                return FALSE;
            }
            break;
        }
        e++;
    }
    if(ptr->left)
    {
        DEBUG('f', "Remove fail, dir is not empty\n");
        return FALSE;
    }
    DirectoryEntry2* prev;
    prev = ptr->parent;
    if(!prev)
    {
        DEBUG('f', "Remove fail, cannot remove root dir\n");
        return FALSE;
    }
    if(prev->left == ptr)
    {
        prev->left = ptr->right;
    }
    else
    {
        prev = prev->left;
        while(prev && prev->right != ptr) prev = prev->right;
        if(!prev) ASSERT(false);
        prev->right = ptr->right;
    }
    ptr->left = ptr->right = ptr->parent = NULL;
    delete ptr;
    return TRUE;
}

void printDE(DirectoryEntry2* ptr, int indent)
{
    for(int i = 0; i < indent; i++)
        putchar('\t');
    printf("File: %s, Sector: %d, isdir: %d\n",
        ptr->name, ptr->sector, (int)(ptr->isdir));
    if(ptr->left)
        printDE(ptr->left, indent+1);
    if(ptr->right)
        printDE(ptr->right, indent);
}

//----------------------------------------------------------------------
// Directory::List
// 	List all the file names in the directory. 
//----------------------------------------------------------------------

void
Directory::List()
{
/*
   for (int i = 0; i < tableSize; i++)
	if (table[i].inUse)
	    printf("%s\n", table[i].name);
*/
    printDE(root, 0);
}

//----------------------------------------------------------------------
// Directory::Print
// 	List all the file names in the directory, their FileHeader locations,
//	and the contents of each file.  For debugging.
//----------------------------------------------------------------------

DirectoryEntry2* succ(DirectoryEntry2* ptr)
{
    if(ptr->left) return ptr->left;
    do
    {
        if(ptr->right) return ptr->right;
        ptr = ptr->parent;
    }while(ptr);
    return NULL;
}

void
Directory::Print()
{ 
    FileHeader *hdr = new FileHeader;

    printf("Directory contents:\n");
    for (DirectoryEntry2* p = root; p; p = succ(p))
	{
	    printf("Name: %s, Sector: %d\n", p->name, p->sector);
	    hdr->FetchFrom(p->sector);
	    hdr->Print();
	}
    printf("\n");
    delete hdr;
}
