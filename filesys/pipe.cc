#include "pipe.h"
#include "stdio.h"
#include "system.h"
#include "utility.h"
#include "synch.h"
int Pipe::cnt = 0;
Pipe::Pipe(int size)
{
	size++;
	name = new char[128];
	snprintf(name, 127, "__pipe_file_%d_%d__", size, cnt++);
	fileSystem->Create(name, size);
	file = fileSystem->Open(name);
	r = w = 0;
	s = size;
	lock = new Lock("pipe lock");
}
Pipe::~Pipe()
{
	delete file;
	fileSystem->Remove(name);
	delete[] name;
	cnt--;
	delete lock;
}
int Pipe::Read(char* buf, int n)
{
	if(r==w) return 0;
	lock->Acquire();
	if(r>w) w+=s;
	int tmp = r+n;
	if(tmp>w) tmp = w;
	int total = file->ReadAt(buf, (tmp>s?s-r:tmp-r), r);
	if(tmp>s)
	{
		total += file->ReadAt(buf+total, tmp-s, 0);
	}
	r += total;
	r %= s;
	w %= s;
	lock->Release();
	return total;
}
int Pipe::Write(char* buf, int n)
{
	if((w+1)%s==r) return 0;
	lock->Acquire();
	if(w>=r) r+=s;
	int tmp = w+n;
	if(tmp>r-1) tmp = r-1;
	int total = file->WriteAt(buf, (tmp>s?s-w:tmp-w), w);
	if(tmp>s)
	{
		total += file->WriteAt(buf+total, tmp-s, 0);
	}
	w += total;
	r %= s;
	w %= s;
	lock->Release();
	return total;
}
