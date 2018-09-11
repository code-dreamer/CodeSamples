#pragma once

// usage:
//SCOPE_EXIT { 
//	logic();	
//};
//
//ScopeExit e = [&](){ logic(); };
//e.DoTask();

class ScopeExit
{
	typedef std::function<void(void)> Callback;

public:
	ScopeExit(const Callback& callback)
		: callback_(callback)
		, taskFinished_(false)
	{
		G_ASSERT(callback_ != nullptr);
	}

	~ScopeExit()
	{ 
		if (!taskFinished_) {
			DoTask();
		}
	}

public:
	void DoTask() 
	{
		if (callback_ != nullptr) {
			callback_(); 
			taskFinished_ = true;
		}
	}

private:
	ScopeExit();
	ScopeExit(const ScopeExit&);
	ScopeExit& operator =(const ScopeExit&);

private:
	std::function<void(void)> callback_;
	bool taskFinished_;
};

#define EXIT_SCOPE_CREATE_UNIQ_NAME2(line) scopeExit_E1FB5B84_B1FC_4A16_8DA4_EE4542AF5109##line
#define EXIT_SCOPE_CREATE_UNIQ_NAME(line) EXIT_SCOPE_CREATE_UNIQ_NAME2(line)
#define SCOPE_EXIT ScopeExit EXIT_SCOPE_CREATE_UNIQ_NAME(__LINE__) = [&]()
