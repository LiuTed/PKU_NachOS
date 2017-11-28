#ifndef RWLOCK_H
#define RWLOCK_H
#include "list.h"
class RWLock
{
public:
    RWLock(char *debugName);
    ~RWLock();
    char* getName() { return name; }    // debugging assist

    void AcquireReader();
    void ReleaseReader();
    void AcquireWriter();
    void ReleaseWriter();
    static const int free = 0, read = 1,
        write = 2;
    int GetStatus() const {return status;}
    int ref;

  private:
    char* name;
    int status;//0: free; 1: reading; 2: writing
    List *queue;
    List *rw;
    List *cur;
};
#endif
