#pragma once

#include "stdafx.h"

// Critical section wrapper.
class Lock
{
public:
	Lock();

	virtual ~Lock();

	// Acquires a lock, should be paired with a relase when done.
	void Acquire();

	// Releases a lock.
	void Release();

private:
	// Lock handle.
	CRITICAL_SECTION m_Lock;
};

