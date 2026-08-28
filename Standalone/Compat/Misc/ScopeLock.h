#pragma once

#include "HAL/CriticalSection.h"

class FScopeLock
{
public:
	explicit FScopeLock(FCriticalSection* InCriticalSection)
		: CriticalSection(InCriticalSection)
	{
		if (CriticalSection != nullptr)
		{
			CriticalSection->Lock();
		}
	}

	~FScopeLock()
	{
		Unlock();
	}

	void Unlock()
	{
		if (CriticalSection != nullptr)
		{
			CriticalSection->Unlock();
			CriticalSection = nullptr;
		}
	}

	FScopeLock(const FScopeLock&) = delete;
	FScopeLock& operator=(const FScopeLock&) = delete;

private:
	FCriticalSection* CriticalSection;
};
