#pragma once

#pragma warning(push)
#pragma warning(disable : 4265 4625  4626 4640)

#include <thread>
#include <mutex>

template<typename T>
class Singleton
{
public:
	virtual ~Singleton()
	{
		isDestructed_ = true;
	}

	static T& Get()	
	{
		DCHECK(!isDestructed_);
		
		(void)isDestructed_; // prevent removing isDestructed_ in Release configuration

		std::lock_guard<std::mutex> lock(GetMutex());
		static T instance;
		return instance;
	}

protected:
	Singleton() {}

private:
	static bool isDestructed_;

	static std::mutex& GetMutex()	
	{
		static std::mutex mutex;
		return mutex;
	}
};

// force creating mutex before main() is called
template<typename T>
bool Singleton<T>::isDestructed_ = (Singleton<T>::GetMutex(), false);


#pragma warning(pop)
