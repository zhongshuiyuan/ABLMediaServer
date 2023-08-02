#ifndef _NetBaseThreadPool_H
#define _NetBaseThreadPool_H

#include <boost/lockfree/queue.hpp>
#include <condition_variable> 

#define   MaxNetHandleQueueCount    512 
typedef boost::unordered_map<NETHANDLE, NETHANDLE>   ClientProcessThreadMap;//�̶��ͻ��˵��߳���� 

class CNetBaseThreadPool
{
public:
	CNetBaseThreadPool(int nThreadCount);
   ~CNetBaseThreadPool();

   //��������ID 
   bool  InsertIntoTask(uint64_t nClientID);

private:
	volatile int nGetCurrentThreadOrder;
	void ProcessFunc();
	static void* OnProcessThread(void* lpVoid);

	volatile   uint64_t     nThreadProcessCount;
	std::mutex              threadLock;
	ClientProcessThreadMap  clientThreadMap;
    uint64_t              nTrueNetThreadPoolCount; 
    boost::lockfree::queue<uint64_t, boost::lockfree::capacity<2048>> m_NetHandleQueue[MaxNetHandleQueueCount];
    volatile bool         bExitProcessThreadFlag[MaxNetHandleQueueCount];
    volatile bool         bCreateThreadFlag;
#ifdef  OS_System_Windows
    HANDLE                hProcessHandle[MaxNetHandleQueueCount];
#else
	pthread_t             hProcessHandle[MaxNetHandleQueueCount];
#endif
	volatile  bool        bRunFlag;

	std::condition_variable  cv[MaxNetHandleQueueCount];
	std::mutex               mtx[MaxNetHandleQueueCount];
};

#endif