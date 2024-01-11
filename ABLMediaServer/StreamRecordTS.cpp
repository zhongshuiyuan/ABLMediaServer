/*
���ܣ�
    �������������Ϊmp4(ts��ʽ) 
	 
����    2023-11-04
����    �޼��ֵ�
QQ      79941308
E-Mail  79941308@qq.com
*/

#include "stdafx.h"
#include "StreamRecordTS.h"
#ifdef USE_BOOST
extern bool                                  DeleteNetRevcBaseClient(NETHANDLE CltHandle);
extern boost::shared_ptr<CMediaStreamSource> CreateMediaStreamSource(char* szUR, uint64_t nClient, MediaSourceType nSourceType, uint32_t nDuration, H265ConvertH264Struct  h265ConvertH264Struct);
extern boost::shared_ptr<CMediaStreamSource> GetMediaStreamSource(char* szURL, bool bNoticeStreamNoFound = false);
extern boost::shared_ptr<CMediaStreamSource> GetMediaStreamSourceNoLock(char* szURL);
extern bool                                  DeleteMediaStreamSource(char* szURL);
extern bool                                  DeleteClientMediaStreamSource(uint64_t nClient);
extern boost::shared_ptr<CNetRevcBase>       GetNetRevcBaseClient(NETHANDLE CltHandle);

extern CMediaSendThreadPool* pMediaSendThreadPool;
extern CMediaFifo                            pDisconnectBaseNetFifo; //�������ѵ����� 
extern char                                  ABL_MediaSeverRunPath[256]; //��ǰ·��
extern MediaServerPort                       ABL_MediaServerPort;
extern boost::shared_ptr<CNetRevcBase>       CreateNetRevcBaseClient(int netClientType, NETHANDLE serverHandle, NETHANDLE CltHandle, char* szIP, unsigned short nPort, char* szShareMediaURL);
extern boost::shared_ptr<CRecordFileSource>  GetRecordFileSource(char* szShareURL);
extern CMediaFifo                            pMessageNoticeFifo;          //��Ϣ֪ͨFIFO
#else
extern bool                                  DeleteNetRevcBaseClient(NETHANDLE CltHandle);
extern std::shared_ptr<CMediaStreamSource> CreateMediaStreamSource(char* szUR, uint64_t nClient, MediaSourceType nSourceType, uint32_t nDuration, H265ConvertH264Struct  h265ConvertH264Struct);
extern std::shared_ptr<CMediaStreamSource> GetMediaStreamSource(char* szURL, bool bNoticeStreamNoFound = false);
extern std::shared_ptr<CMediaStreamSource> GetMediaStreamSourceNoLock(char* szURL);
extern bool                                  DeleteMediaStreamSource(char* szURL);
extern bool                                  DeleteClientMediaStreamSource(uint64_t nClient);
extern std::shared_ptr<CNetRevcBase>       GetNetRevcBaseClient(NETHANDLE CltHandle);

extern CMediaSendThreadPool* pMediaSendThreadPool;
extern CMediaFifo                            pDisconnectBaseNetFifo; //�������ѵ����� 
extern char                                  ABL_MediaSeverRunPath[256]; //��ǰ·��
extern MediaServerPort                       ABL_MediaServerPort;
extern std::shared_ptr<CNetRevcBase>       CreateNetRevcBaseClient(int netClientType, NETHANDLE serverHandle, NETHANDLE CltHandle, char* szIP, unsigned short nPort, char* szShareMediaURL);
extern std::shared_ptr<CRecordFileSource>  GetRecordFileSource(char* szShareURL);
extern CMediaFifo                            pMessageNoticeFifo;          //��Ϣ֪ͨFIFO
#endif


static void* record_ts_alloc(void* param, size_t bytes)
{
	CStreamRecordTS* pThis = (CStreamRecordTS*)param;
	assert(bytes <= sizeof(pThis->s_bufferH264TS));
	return pThis->s_bufferH264TS;
}

static void record_ts_free(void* param, void* /*packet*/)
{
	return;
}

static int record_ts_write(void* param, const void* packet, size_t bytes)
{
	if (param != NULL)
	{
		CStreamRecordTS* handle = (CStreamRecordTS*)param;
		if (handle != NULL)
		{
			bool  bUpdateFlag = false;

			if (handle->fTSFileWrite == NULL)
			{
#ifdef OS_System_Windows
				SYSTEMTIME st;
				GetLocalTime(&st);
				sprintf(handle->szFileName, "%s%04d%02d%02d%02d%02d%02d.mp4", handle->szRecordPath, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
				sprintf(handle->szFileNameOrder, "%04d%02d%02d%02d%02d%02d.mp4", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);;
#else
				time_t now;
				time(&now);
				struct tm *local;
				local = localtime(&now);
				sprintf(handle->szFileName, "%s%04d%02d%02d%02d%02d%02d.mp4", handle->szRecordPath, local->tm_year + 1900, local->tm_mon + 1, local->tm_mday, local->tm_hour, local->tm_min, local->tm_sec);
				sprintf(handle->szFileNameOrder, "%04d%02d%02d%02d%02d%02d.mp4", local->tm_year + 1900, local->tm_mon + 1, local->tm_mday, local->tm_hour, local->tm_min, local->tm_sec);;
#endif
				auto pRecord = GetRecordFileSource(handle->m_szShareMediaURL);
				if (pRecord)
				{
					bUpdateFlag = pRecord->UpdateExpireRecordFile(handle->szFileName);
					if (bUpdateFlag)
					{
						handle->fTSFileWrite = fopen(handle->szFileName, "r+b");
						if (handle->fTSFileWrite)
							fseek(handle->fTSFileWrite, 0, SEEK_SET);
					}
					else
						handle->fTSFileWrite = fopen(handle->szFileName, "wb");

					if (handle->fTSFileWrite)
					{
						pRecord->AddRecordFile(handle->szFileNameOrder);
						WriteLog(Log_Debug, "CStreamRecordFMP4 = %X %s ����¼���ļ� nClient = %llu ,nMediaClient = %llu szFileNameOrder %s ", handle, handle->m_szShareMediaURL, handle->nClient, handle->nMediaClient, handle->szFileNameOrder);
					}
				}
			}

			if (handle->fTSFileWrite != NULL)
			{
				handle->fTSFileWriteByteCount += bytes;
				return 1 == fwrite(packet, bytes, 1, (FILE*)handle->fTSFileWrite) ? 0 : ferror((FILE*)handle->fTSFileWrite);
			}
			else
				return 0;
		}
	}
	else
		return 0;
}

int CStreamRecordTS::ts_stream(void* ts, int codecid)
{
	std::map<int, int>::const_iterator it = streamsTS.find(codecid);
	if (streamsTS.end() != it)
		return it->second;

	int i = mpeg_ts_add_stream(ts, codecid, NULL, 0);
	streamsTS[codecid] = i;
	return i;
}

//H264��Ƭ
bool  CStreamRecordTS::H264H265FrameToTSFile(unsigned char* szVideo, int nLength)
{
	if (szVideo == NULL || nLength <= 0)
		return false;

	if (strcmp(mediaCodecInfo.szVideoName, "H264") == 0)
		avtype = PSI_STREAM_H264;
	else if (strcmp(mediaCodecInfo.szVideoName, "H265") == 0)
		avtype = PSI_STREAM_H265;

	if (CheckVideoIsIFrame(mediaCodecInfo.szVideoName, szVideo, nLength) == true)
		flags = 1;
	else
		flags = 0;

	ptsVideo = videoDts * 90;
	if (strcmp(mediaCodecInfo.szVideoName, "H264") == 0 || strcmp(mediaCodecInfo.szVideoName, "H265") == 0)
		mpeg_ts_write(tsPacketHandle, ts_stream(tsPacketHandle, avtype), flags, ptsVideo, ptsVideo, szVideo, nLength);
	else
	{

	}
	nVideoOrder ++;

	if (nVideoOrder % (ABL_MediaServerPort.fileSecond * 25) == 0)
	{//1����Ƭ1��
		fflush(fTSFileWrite);

		fclose(fTSFileWrite);
		fTSFileWrite = NULL;

		//���һ��fmp4��Ƭ�ļ�֪ͨ 
		if (ABL_MediaServerPort.hook_enable == 1 && ABL_MediaServerPort.nClientRecordMp4 > 0)
		{
			MessageNoticeStruct msgNotice;
			msgNotice.nClient = ABL_MediaServerPort.nClientRecordMp4;
			sprintf(msgNotice.szMsg, "{\"app\":\"%s\",\"stream\":\"%s\",\"mediaServerId\":\"%s\",\"networkType\":%d,\"fileName\":\"%s\",\"currentFileDuration\":%llu}", app, stream, ABL_MediaServerPort.mediaServerID, netBaseNetType, szFileNameOrder, (nCurrentVideoFrames / mediaCodecInfo.nVideoFrameRate));
			pMessageNoticeFifo.push((unsigned char*)&msgNotice, sizeof(MessageNoticeStruct));
		}
 		nCurrentVideoFrames = 0;
	}
	return true;
}

CStreamRecordTS::CStreamRecordTS(NETHANDLE hServer, NETHANDLE hClient, char* szIP, unsigned short nPort,char* szShareMediaURL)
{
	nVideoOrder = 0;
	fTSFileWrite = NULL;
	fTSFileWriteByteCount = 0;
	tsPacketHandle = NULL;

	memset((char*)&avc, 0x00, sizeof(avc));
	memset((char*)&hevc, 0x00, sizeof(hevc));

	nCurrentVideoFrames = 0;//��ǰ��Ƶ֡��
	nTotalVideoFrames = 0;//¼����Ƶ��֡��

	strcpy(m_szShareMediaURL,szShareMediaURL);
 	netBaseNetType = NetBaseNetType_RecordFile_TS;
	nMediaClient = 0;
  
	nClient = hClient;
	hls_init_segmentFlag = false;
	audioDts = 0;
	videoDts = 0;
  
	strcpy(szClientIP, szIP);
	nClientPort = nPort;

 	m_videoFifo.InitFifo(MaxLiveingVideoFifoBufferLength);
	m_audioFifo.InitFifo(MaxLiveingAudioFifoBufferLength);

 	nCreateDateTime = GetTickCount64();

	if (tsPacketHandle == NULL)
	{
		tshandler.alloc = record_ts_alloc;
		tshandler.write = record_ts_write;
		tshandler.free = record_ts_free;
  
		tsPacketHandle = mpeg_ts_create(&tshandler, (void*)this);
 	}

	WriteLog(Log_Debug, "CStreamRecordTS ���� = %X nClient = %llu ", this, nClient);
}

CStreamRecordTS::~CStreamRecordTS()
{
	bRunFlag = false ;
	std::lock_guard<std::mutex> lock(mediaMP4MapLock);

	if (tsPacketHandle != NULL)
	{
		mpeg_ts_destroy(tsPacketHandle);
		tsPacketHandle = NULL;
	}
 
	m_videoFifo.FreeFifo();
	m_audioFifo.FreeFifo();
 
	if (fTSFileWrite)
	{
		fflush(fTSFileWrite);
 		fclose(fTSFileWrite);
		fTSFileWrite = NULL;

		//���һ��fmp4��Ƭ�ļ�֪ͨ 
		if (ABL_MediaServerPort.hook_enable == 1 && ABL_MediaServerPort.nClientRecordMp4 > 0)
		{
			MessageNoticeStruct msgNotice;
			msgNotice.nClient = ABL_MediaServerPort.nClientRecordMp4;
			sprintf(msgNotice.szMsg, "{\"key\":%llu,\"app\":\"%s\",\"stream\":\"%s\",\"mediaServerId\":\"%s\",\"networkType\":%d,\"fileName\":\"%s\",\"currentFileDuration\":%llu}", key, app, stream, ABL_MediaServerPort.mediaServerID, netBaseNetType, szFileNameOrder, (nCurrentVideoFrames / mediaCodecInfo.nVideoFrameRate));
			pMessageNoticeFifo.push((unsigned char*)&msgNotice, sizeof(MessageNoticeStruct));
		}
	}

	WriteLog(Log_Debug, "CStreamRecordTS ���� = %X nClient = %llu ,nMediaClient = %llu\r\n", this, nClient, nMediaClient);
	malloc_trim(0);
}

int CStreamRecordTS::PushVideo(uint8_t* pVideoData, uint32_t nDataLength, char* szVideoCodec)
{
	std::lock_guard<std::mutex> lock(mediaMP4MapLock);
	if (!bRunFlag)
		return -1;
	nRecvDataTimerBySecond = 0;
	nCurrentVideoFrames ++;//��ǰ��Ƶ֡��
	nTotalVideoFrames ++ ;//¼����Ƶ��֡��

	m_videoFifo.push(pVideoData, nDataLength);

	if (ABL_MediaServerPort.hook_enable == 1 && ABL_MediaServerPort.nClientRecordProgress > 0 && (GetTickCount64() - nCreateDateTime ) >= 1000 )
	{
		MessageNoticeStruct msgNotice;
		msgNotice.nClient = ABL_MediaServerPort.nClientRecordProgress;
		sprintf(msgNotice.szMsg, "{\"app\":\"%s\",\"stream\":\"%s\",\"mediaServerId\":\"%s\",\"networkType\":%d,\"key\":%d,\"fileName\":\"%s\",\"currentFileDuration\":%llu,\"TotalVideoDuration\":%llu}", app, stream, ABL_MediaServerPort.mediaServerID, netBaseNetType,key, szFileNameOrder, (nCurrentVideoFrames / mediaCodecInfo.nVideoFrameRate), (nTotalVideoFrames / mediaCodecInfo.nVideoFrameRate));
		pMessageNoticeFifo.push((unsigned char*)&msgNotice, sizeof(MessageNoticeStruct));
		nCreateDateTime = GetTickCount64();
	}
	return 0;
}

int CStreamRecordTS::PushAudio(uint8_t* pAudioData, uint32_t nDataLength, char* szAudioCodec, int nChannels, int SampleRate)
{
	std::lock_guard<std::mutex> lock(mediaMP4MapLock);
	if (ABL_MediaServerPort.nEnableAudio == 0 || !bRunFlag)
		return -1;

	m_audioFifo.push(pAudioData, nDataLength);

	return 0;
}

int CStreamRecordTS::SendVideo()
{
 	std::lock_guard<std::mutex> lock(mediaMP4MapLock);

	nRecvDataTimerBySecond = 0;

	nVideoStampAdd = 1000 / mediaCodecInfo.nVideoFrameRate;

	videoDts += nVideoStampAdd;

	unsigned char* pData = NULL;
	int            nLength = 0;
	if ((pData = m_videoFifo.pop(&nLength)) != NULL)
	{
		 if (tsPacketHandle != NULL )
 		  H264H265FrameToTSFile(pData, nLength);

		m_videoFifo.pop_front();
	}

	return 0;
}

int CStreamRecordTS::SendAudio()
{
	std::lock_guard<std::mutex> lock(mediaMP4MapLock);
 	if (ABL_MediaServerPort.nEnableAudio == 0)
		return 0 ;
   
 	unsigned char* pData = NULL;
	int            nLength = 0;
	int            nRet;
	if ((pData = m_audioFifo.pop(&nLength)) != NULL && tsPacketHandle != NULL )
	{

		if (strcmp(mediaCodecInfo.szAudioName, "AAC") == 0)
			audioType = PSI_STREAM_AAC;
		else if (strcmp(mediaCodecInfo.szAudioName, "G711_A") == 0)
			audioType = PSI_STREAM_AUDIO_G711A;
		else if (strcmp(mediaCodecInfo.szAudioName, "G711_U") == 0)
			audioType = PSI_STREAM_AUDIO_G711U;

		nRet = mpeg_ts_write(tsPacketHandle, ts_stream(tsPacketHandle, audioType), 0, audioDts * 90, audioDts * 90, pData, nLength);
		
		if (strcmp(mediaCodecInfo.szAudioName, "AAC") == 0)
			audioDts += mediaCodecInfo.nBaseAddAudioTimeStamp;
		else if (strcmp(mediaCodecInfo.szAudioName, "G711_A") == 0 || strcmp(mediaCodecInfo.szAudioName, "G711_U") == 0)
			audioDts += nLength ;

		m_audioFifo.pop_front();
	}
	return 0;
}

int CStreamRecordTS::InputNetData(NETHANDLE nServerHandle, NETHANDLE nClientHandle, uint8_t* pData, uint32_t nDataLength, void* address)
{
	
	return 0;
}

int CStreamRecordTS::ProcessNetData()
{
	return 0;
}

//���͵�һ������
int CStreamRecordTS::SendFirstRequst()
{

	 return 0;
}

//����m3u8�ļ�
bool  CStreamRecordTS::RequestM3u8File()
{
	return true;
}