
#include <mutex>
#include <functional>
#include <vector>
#include <thread>

class ThreadManager
{
private:
    ThreadManager(int threadCount);
public:
    static ThreadManager* GetInstance();

    static void Release();
public:
    void Run(std::function<void(bool&)> func);
    void AllJoin();
public:
    const int _threadCount;
    bool _canRun = true;
private:
    std::vector<std::thread> _threads;

private:
    static std::mutex _m;
    static ThreadManager* _instance; 
};

