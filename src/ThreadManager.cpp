#include "ThreadManager.h"

using namespace std;

mutex ThreadManager::_m;
ThreadManager* ThreadManager::_instance = nullptr;

ThreadManager::ThreadManager(int threadCount) : _threadCount(threadCount)
{
    
}

ThreadManager* ThreadManager::GetInstance()
{
    { // 동시 접근을 막기 위해 락
        lock_guard<mutex> lock(_m);
        // aws 인스턴스에서 설정한 코어 수가 2개니 최대 4개까지 
        if(_instance == nullptr) _instance = new ThreadManager(4);
    }

    return _instance;
}

void ThreadManager::Release()
{
    if(_instance)
    {
         delete _instance;
         _instance = nullptr;
    }
}

void ThreadManager::AllJoin()
{
    _canRun= false;
    for(auto& th : _threads)
    {
        th.join();
    }
}

// func은 계속 돌아야하는지 묻는 bool의 매개변수를 필요로함 
void ThreadManager::Run(function<void(bool&)> func)
{
    for(int i=0; i<_threadCount; i++)
    {
        _threads.push_back(thread(func, ref(_canRun)));
    }
}
