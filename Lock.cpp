#include "Lock.h"

Lock::Lock()
{
	InitializeCriticalSection( &m_Lock );
}

Lock::~Lock()
{
	DeleteCriticalSection( &m_Lock );
}

void Lock::Acquire()
{
	if ( FALSE == TryEnterCriticalSection( &m_Lock ) ) {
		EnterCriticalSection( &m_Lock );
	}
}

void Lock::Release()
{
	LeaveCriticalSection( &m_Lock );
}
