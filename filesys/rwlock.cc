#include "rwlock.h"
#include "system.h"
RWLock::RWLock(char *debugName)
{
    name = debugName;
    queue = new List;
    rw = new List;
    cur = new List;
    ref = 0;
    status = free;
}
RWLock::~RWLock()
{
    delete queue;
    delete rw;
    delete cur;
}
void RWLock::AcquireReader()
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    if(status == write || (status == read && !queue->IsEmpty()))
    {
        queue->Append(currentThread);
        rw->Append((void*)read);
        currentThread->Sleep();
    }
    cur->Append(currentThread);
    status = read;
    interrupt->SetLevel(oldLevel);
}
void RWLock::ReleaseReader()
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    cur->Remove(currentThread);
    if(cur->IsEmpty())
    {
        if(!queue->IsEmpty())
        {
            int will = (int)rw->Remove();
            Thread *ptr = queue->Remove();
            ASSERT(will == write);
            scheduler->ReadyToRun(ptr);
            status = write;
        }
        else status = free;
    }
    interrupt->SetLevel(oldLevel);
}
void RWLock::AcquireWriter()
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    if(status != free)
    {
        queue->Append(currentThread);
        rw->Append((void*)write);
        currentThread->Sleep();
    }
    cur->Append(currentThread);
    status = write;
    interrupt->SetLevel(oldLevel);
}
void RWLock::ReleaseWriter()
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    cur->Remove(currentThread);
    ASSERT(cur->IsEmpty());
    if(!queue->IsEmpty())
    {
        int will = rw->Remove();
        Thread *ptr = queue->Remove();
        status = will;
        scheduler->ReadyToRun(ptr);
        if(will == read)
        {
            while(true)
            {
                will = rw->Remove();
                ptr = queue->Remove();
                if(!ptr) break;
                if(will != read)
                {
                    rw->Prepend((void*)will);
                    queue->Prepend((void*)ptr);
                    break;
                }
                scheduler->ReadyToRun(ptr);
            }
        }
    }
    else status = free;
    interrupt->SetLevel(oldLevel);
}
