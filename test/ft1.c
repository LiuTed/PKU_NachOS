#include "syscall.h"
int main()
{
	const char* name = "test_file";
	int buf[32];
	int i;
	OpenFileId fd1, fd2;
	Create(name);
	fd1 = Open(name);
	fd2 = Open(name);
	for(i = 0; i < 32; i++)
	{
		buf[i] = i;
	}
	Write(buf, 6*sizeof(int), fd1);
	Write(buf+6, 3*sizeof(int), fd1);
	Read(buf, 3*sizeof(int), fd2);
	for(i = 0; i < 3; i++)
		PutInt(buf[i]);
	PutChar('\n');
	Read(buf+3, 6*sizeof(int), fd2);
	for(i = 3; i < 9; i++)
		PutInt(buf[i]);
	PutChar('\n');
	Exit(0);
}