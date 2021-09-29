#define MAX_SEM_COUNT 100

#include "NandSemaphore.h"

void __InitSemaphore(NandSemaphore *sem,int val)
{
}

void __DeinitSemaphore(NandSemaphore *sem)
{
}

void __Semaphore_wait(NandSemaphore *sem)
{
}

int __Semaphore_waittimeout(NandSemaphore *sem,long jiffies)
{
	return 0;
}

void __Semaphore_signal(NandSemaphore *sem)
{
}

void __InitNandMutex(NandMutex *mutex)
{
}

void __DeinitNandMutex(NandMutex *mutex)
{
}

void __NandMutex_Lock(NandMutex *mutex)
{
}

void __NandMutex_Unlock(NandMutex* mutex)
{
}

int __NandMutex_TryLock(NandMutex *mutex)
{
	return 0;
}

void __InitNandSpinLock(NandSpinLock *lock)
{
}

void __DeInitNandSpinLock(NandSpinLock *lock)
{
}

void __NandSpinLock_Lock(NandSpinLock *lock)
{
}

void __NandSpinLock_Unlock(NandSpinLock *lock)
{
}
