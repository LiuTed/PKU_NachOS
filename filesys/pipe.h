#ifndef PIPE_H
#define PIPE_H
#include "filesys.h"
#include "openfile.h"
#include "synch.h"
class Pipe
{
public:
	Pipe(int size = SectorSize);
	~Pipe();
	int Read(char* buf, int n);
	int Write(char* buf, int n);
	bool good(){return file;}
private:
	static int cnt;
	OpenFile *file;
	int r, w, s;
	char *name;
	Lock *lock;
};

#endif
