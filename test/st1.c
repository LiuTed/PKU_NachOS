#include "syscall.h"
void forked()
{
	int i;
	for(i = 0; i < 10; i++)
	{
		PutInt(i);
		PutChar(' ');
		Yield();
	}
	Exit(1);
}
int main()
{
	int i;
	Fork(forked);
	for(i = 0; i < 10; i++)
	{
		PutInt(i);
		PutChar(' ');
		if(i&1) Yield();
	}
	Exit(0);
}