#include "syscall.h"
int main()
{
	const char *name="ft1";
	int i, tid, ec;
	tid = Exec(name);
	ec = Join(tid);
	for(i = 0; i < 10; i++)
	{
		PutInt(i);
		PutChar(' ');
	}
	Exit(ec);
}