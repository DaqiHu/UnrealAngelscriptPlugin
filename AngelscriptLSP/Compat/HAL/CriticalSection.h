#pragma once

#include <mutex>

class FCriticalSection
{
public:
	void Lock()
	{
		Mutex.lock();
	}

	void Unlock()
	{
		Mutex.unlock();
	}

private:
	std::recursive_mutex Mutex;
};
