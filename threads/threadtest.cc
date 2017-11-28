// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create two threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustratethe inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "elevatortest.h"

// testnum is set in main.cc
int testnum = 1;

//----------------------------------------------------------------------
// SimpleThread
// 	Loop 5 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------

void
SimpleThread(int which)
{
    int num;
    
    for (num = 0; num < 3; num++) {
        printf("*** thread %d with priority %d looped %d times\n",
             which, currentThread->getPriority(), num);
        currentThread->Yield();
    }
}

//----------------------------------------------------------------------
// ForkAndLoop
//  Loop 5 times, fork a new thread and then yielding the CPU to 
//  another ready thread each iteration.
//
//  "which" is simply a number identifying the thread, for debugging
//  purposes.
//----------------------------------------------------------------------

void
ForkAndLoop(int which)
{
    int num;
    Thread* arr[3];
    for (num = 0; num < 3; num++) {
        printf("*** thread %d with priority %d forked and looped %d times\n",
             which, currentThread->getPriority(), num);
        arr[num] = new Thread("forked", 3 - num);
        arr[num]->Fork(SimpleThread, (void*)arr[num]->getTID());
        currentThread->Yield();
    }
}


//----------------------------------------------------------------------
//  ALongJob
//      Loop while switch on and off interrupt continously
//      to simulate the job that needs a long time to run
//
//  "which" is simply a number identifying the thread, for debugging
//  purposes.
//----------------------------------------------------------------------

void
ALongJob(int which)
{
    int num;
    for (num = 0; num < 30; num++) {
        printf("*** thread %d with priority %d looped %d times\n",
             which, currentThread->getPriority(), num);
        interrupt->SetLevel(IntOn);
        interrupt->SetLevel(IntOff);
    }
}
//----------------------------------------------------------------------
// ThreadTest1
// 	Set up a ping-pong between two threads, by forking a thread 
//	to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------

void
ThreadTest1()
{
    DEBUG('t', "Entering ThreadTest1");

    Thread *t = new Thread("forked thread");

    t->Fork(SimpleThread, (void*)1);
    SimpleThread(0);
}

//----------------------------------------------------------------------
// ThreadTest2
//	Fork threads until running out of tid.
//----------------------------------------------------------------------

void
ThreadTest2()
{
    DEBUG('t', "Entering ThreadTest2");
    
    Thread* ptr[130];
    for(int i=0; i<130; i++)
    {
        try
        {
            ptr[i] = new Thread("tmp");
        }
        catch(std::exception& e)
        {
            puts("could not create any more thread");
            ptr[i] = NULL;
        }
    }
    for(int i=10; i<130; i++)
    {
        delete ptr[i];
    }
    Thread* tmp = new Thread("forked");
    tmp -> Fork(SimpleThread, (void*)tmp->getTID());
    SimpleThread(0);
    TS();
}

//----------------------------------------------------------------------
// ThreadTest3
//  Test priority
//----------------------------------------------------------------------

void
ThreadTest3()
{
    DEBUG('t', "Entering ThreadTest3");
    Thread* t = new Thread("Forked", 1);
    t->Fork(SimpleThread, (void*)t->getTID());
    ForkAndLoop(0);
}


//----------------------------------------------------------------------
// ThreadTest4
//  Test multilevel feedback queue algo
//----------------------------------------------------------------------

void
ThreadTest4()
{
    DEBUG('t', "Entering ThreadTest4");
    Thread* t[2];
    t[0] = new Thread("LongJob", 1);
    t[0]->Fork(ALongJob, (void*)t[0]->getTID());
    t[1] = new Thread("Important&Fast", 0);
    t[1]->Fork(SimpleThread, (void*)t[1]->getTID());
    ForkAndLoop(0);
}

//----------------------------------------------------------------------
// Producer
//  Produce products and wait for customer
// Consumer
//  Wait for product and remove them
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// ThreadTest5
//  use lock and condition to present producer/consuer problem
//----------------------------------------------------------------------

static int productnum = 0;
static int maxproduct = 0;
Condition *cond;
Lock *lock;

void
Producer(int which)
{
    DEBUG('t', "Entering Producer");

    for(int i = 0; i < 10; i++)
    {
        lock->Acquire();
        while(productnum >= maxproduct)
        {
            printf("full!(%d)\n", which);
            cond->Wait(lock);
            printf("back to producing(%d)\n", which);
        }
        productnum++;
        printf("producer %d produced one product. cur num=%d\n",
             which, productnum);
        if(productnum == 1)
            cond->Broadcast(lock);
        lock->Release();
    }
}
void
Consumer(int which)
{
    DEBUG('t', "Entering Consumer");
    for(int i = 0; i < 8; i++)
    {
        lock->Acquire();
        while(productnum <= 0)
        {
            printf("empty!(%d)\n", which);
            cond->Wait(lock);
            printf("product available(%d)\n", which);
        }
        productnum--;
        printf("consumer %d buy one product. cur num=%d\n",
            which, productnum);
        if(productnum == maxproduct - 1)
            cond->Broadcast(lock);
        lock->Release();
    }
}

void
ThreadTest5()
{
    DEBUG('t', "Entering ThreadTest5");
    cond = new Condition("condition");
    lock = new Lock("lock");
    maxproduct = 7;
    Thread *p[2], *c[2];
    for(int i = 0; i < 2; i++)
    {
        p[i] = new Thread("producer", 3);
        p[i]->Fork(Producer, (void*)i);
    }
    for(int i = 0; i < 2; i++)
    {
        c[i] = new Thread("consumer", 3);
        c[i]->Fork(Consumer, (void*)i);
    }
}

//----------------------------------------------------------------------
// ThreadTest6
//  use semaphore to present producer/consuer problem
//----------------------------------------------------------------------

Semaphore *rest, *avail;
Semaphore *mutex;
void
Producer_sem(int which)
{
    DEBUG('t', "Entering Producer");

    for(int i = 0; i < 10; i++)
    {
        rest->P();
        mutex->P();
        productnum++;
        printf("producer %d produced a product. cur num=%d\n",
            which, productnum);
        mutex->V();
        avail->V();
    }
}
void
Consumer_sem(int which)
{
    DEBUG('t', "Entering Consumer");
    for(int i = 0; i < 8; i++)
    {
        avail->P();
        mutex->P();
        productnum--;
        printf("consumer %d buy one product. cur num=%d\n",
            which, productnum);
        mutex->V();
        rest->V();
    }
}

void
ThreadTest6()
{
    DEBUG('t', "Entering ThreadTest6");
    rest = new Semaphore("restpos", 7);
    avail = new Semaphore("availnum", 0);
    mutex = new Semaphore("mutex", 1);
    Thread *p[2], *c[2];
    for(int i = 0; i < 2; i++)
    {
        p[i] = new Thread("producer", 3);
        p[i]->Fork(Producer_sem, (void*)i);
    }
    for(int i = 0; i < 2; i++)
    {
        c[i] = new Thread("consumer", 3);
        c[i]->Fork(Consumer_sem, (void*)i);
    }
}

//----------------------------------------------------------------------
// ThreadTest7
//  Test Barrier
//----------------------------------------------------------------------
Barrier* barrier;

void BarrierTest(int which)
{
    printf("Thread %d face barrier\n", which);
    barrier->Wait();
    printf("Now Running thread %d\n", which);
}

void
ThreadTest7()
{
    DEBUG('t', "Entering ThreadTest7");
    barrier = new Barrier("barrier", 3);
    Thread *p[4];
    for(int i = 0; i < 4; i++)
    {
        p[i] = new Thread("test barrier", 3);
        p[i]->Fork(BarrierTest, (void*)i);
    }
}
#ifdef USER_PROGRAM
//----------------------------------------------------------------------
// ThreadTest8
//  Test User Program
//----------------------------------------------------------------------

extern void StartProcess(char*);

void RunSort(int which)
{
    StartProcess("../test/sort");
}
void RunMatMult(int which)
{
    StartProcess("../test/matmult");
}

void
ThreadTest8()
{
    DEBUG('t', "Entering ThreadTest8");
    Thread *p[2];
    p[0] = new Thread("thread 1", 3);
    p[1] = new Thread("thread 2", 3);
    p[0]->Fork(RunSort, (void*)1);
    p[1]->Fork(RunMatMult, (void*)2);
}
#endif

//----------------------------------------------------------------------
// ThreadTest
// 	Invoke a test routine.
//----------------------------------------------------------------------
#define TT(N) case N:\
ThreadTest##N();\
break;
void
ThreadTest()
{
    switch (testnum) {
        TT(1)
        TT(2)
        TT(3)
        TT(4)
        TT(5)
        TT(6)
        TT(7)
        #ifdef USER_PROGRAM
        TT(8)
        #endif
    default:
       printf("No test specified.\n");
       break;
    }
}
#undef TT
