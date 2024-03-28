#pragma once
#include <list>
#include <mutex>
#include <string.h>
struct MediaNode
{
	int nBegin;
	int nEnd;
	MediaNode()
	{
		nBegin = 0;
		nEnd = 0;
	}
};


typedef std::list<MediaNode> MediaFifoLengthList;

class CMediaFifo
{
public:
	CMediaFifo();
	~CMediaFifo();

	int    GetFifoLength();
	size_t GetSize();
	void  Reset();
	void  InitFifo(int nBufferLength);
	void  FreeFifo();
	int   GetFreeSpaceByte();
	bool  push(unsigned char* fifoBuffer, int nLength);

	unsigned char* pop(int* nLength);
	bool            pop_front();
	unsigned char* pMediaBuffer;

private:
	MediaFifoLengthList   LengthList;
	std::mutex            MediaFifoMutex;
	int                   nMediaBufferLength;
	int                   nFifoStart;
	int                   nFifoEnd;
};
