/*
���ܣ�
       ʵ��
	   1 ��RTSP�������˵Ľ���������
	   2 ��RTSP�������� TCP ��ʽ���� rtp 
	   3������֧��UDP��ʽ���� rtp ���� 2023-04-14
����    2021-03-29
����    �޼��ֵ�
QQ      79941308
E-Mail  79941308@qq.com
*/

#include "stdafx.h"
#include "NetRtspServer.h"
#include "LCbase64.h"

#include "netBase64.h"
#include "Base64.hh"

uint64_t                                     CNetRtspServer::Session = 1000;
extern bool                                  DeleteNetRevcBaseClient(NETHANDLE CltHandle);
extern boost::shared_ptr<CMediaStreamSource> CreateMediaStreamSource(char* szURL, uint64_t nClient, MediaSourceType nSourceType, uint32_t nDuration, H265ConvertH264Struct  h265ConvertH264Struct);
extern boost::shared_ptr<CMediaStreamSource> GetMediaStreamSource(char* szURL);
extern bool                                  DeleteMediaStreamSource(char* szURL);
extern bool                                  DeleteClientMediaStreamSource(uint64_t nClient);
extern CMediaFifo                            pDisconnectBaseNetFifo; //�������ѵ����� 
extern CMediaSendThreadPool*                 pMediaSendThreadPool;
extern size_t base64_decode(void* target, const char *source, size_t bytes);
extern MediaServerPort                       ABL_MediaServerPort;
extern boost::shared_ptr<CNetRevcBase>       CreateNetRevcBaseClient(int netClientType, NETHANDLE serverHandle, NETHANDLE CltHandle, char* szIP, unsigned short nPort, char* szShareMediaURL);
extern boost::shared_ptr<CNetRevcBase>       GetNetRevcBaseClient(NETHANDLE CltHandle);
unsigned short                               CNetRtspServer::nRtpPort = 55000 ;
extern void LIBNET_CALLMETHOD                onread(NETHANDLE srvhandle, NETHANDLE clihandle, uint8_t* data, uint32_t datasize, void* address);

//AAC����Ƶ�����
int avpriv_mpeg4audio_sample_rates[] = {
	96000, 88200, 64000, 48000, 44100, 32000,
	24000, 22050, 16000, 12000, 11025, 8000, 7350
};

//rtp����
void RTP_DEPACKET_CALL_METHOD rtppacket_callback(_rtp_depacket_cb* cb)
{
	//	int nGet = 5; ABLRtspChan
	CNetRtspServer* pUserHandle = (CNetRtspServer*)cb->userdata;

	if (!pUserHandle->bRunFlag)
		return;

	if (pUserHandle != NULL )
	{
 		if (cb->handle == pUserHandle->hRtpHandle[0])
		{
			if (pUserHandle->netBaseNetType == NetBaseNetType_RtspServerRecvPush && pUserHandle->pMediaSource != NULL)
			{
				if (pUserHandle->m_bHaveSPSPPSFlag && pUserHandle->m_nSpsPPSLength > 0)
				{
					if (pUserHandle->CheckVideoIsIFrame(pUserHandle->szVideoName, cb->data, cb->datasize))
					{
						memmove(cb->data + pUserHandle->m_nSpsPPSLength, cb->data, cb->datasize);
						memcpy(cb->data, pUserHandle->m_pSpsPPSBuffer, pUserHandle->m_nSpsPPSLength);
						pUserHandle->pMediaSource->PushVideo(cb->data, cb->datasize + pUserHandle->m_nSpsPPSLength, pUserHandle->szVideoName);

						//WriteLog(Log_Debug, "CNetRtspServer=%X ,nClient = %llu, rtppacket_callback() �ص�I֡  ", pUserHandle, pUserHandle->nClient);
					}else
						pUserHandle->pMediaSource->PushVideo(cb->data, cb->datasize, pUserHandle->szVideoName);
				}else 
 				  pUserHandle->pMediaSource->PushVideo(cb->data, cb->datasize, pUserHandle->szVideoName);

				 // WriteLog(Log_Debug, "CNetRtspServer=%X ,nClient = %llu, rtp ����ص� %02X %02X %02X %02X %02X, timeStamp = %d ,datasize = %d ", pUserHandle, pUserHandle->nClient, cb->data[0], cb->data[1], cb->data[2], cb->data[3], cb->data[4],cb->timestamp,cb->datasize);

#ifdef WriteRtpDepacketFileFlag
				  if (pUserHandle->CheckVideoIsIFrame(pUserHandle->szVideoName, cb->data, cb->datasize))
				  {
					  fwrite(pUserHandle->m_pSpsPPSBuffer, 1, pUserHandle->m_nSpsPPSLength, pUserHandle->fWriteRtpVideo);
					  fflush(pUserHandle->fWriteRtpVideo);
					  pUserHandle->bStartWriteFlag = true;
				  }

				if (pUserHandle->bStartWriteFlag)
				{
					fwrite(cb->data, 1, cb->datasize, pUserHandle->fWriteRtpVideo);
					fflush(pUserHandle->fWriteRtpVideo);
				}
#endif  	
			}
 		}
		else if (cb->handle == pUserHandle->hRtpHandle[1])
		{
			if (strcmp(pUserHandle->szAudioName, "AAC") == 0)
			{//aac
 				pUserHandle->SplitterRtpAACData(cb->data, cb->datasize);
 			}
			else
			{// G711A ��G711U
				pUserHandle->pMediaSource->PushAudio(cb->data, cb->datasize, pUserHandle->szAudioName, pUserHandle->nChannels, pUserHandle->nSampleRate);
 			}
		   //WriteLog(Log_Debug, "CNetRtspServer=%X ,nClient = %llu, rtp Audio ����ص� %02X %02X %02X %02X %02X, timeStamp = %llu ,datasize = %d ", pUserHandle, pUserHandle->nClient, cb->data[0], cb->data[1], cb->data[2], cb->data[3], cb->data[4],cb->timestamp,cb->datasize);

		}
	}
}

//��AAC��rtp�������и�
void  CNetRtspServer::SplitterRtpAACData(unsigned char* rtpAAC, int nLength)
{
	au_header_length = (rtpAAC[0] << 8) + rtpAAC[1];
	au_header_length = (au_header_length + 7) / 8;  
	ptr = rtpAAC;

	au_size = 2;  
	au_numbers = au_header_length / au_size;

	//����5֡����������Щ��Ƶ�Ҵ��
    if (au_numbers > 16 )
 	 	return ;
 
	ptr += 2;  
	pau = ptr + au_header_length;  

	for (int i = 0; i < au_numbers; i++)
	{
		SplitterSize[i] = (ptr[0] << 8) | (ptr[1] & 0xF8);
		SplitterSize[i] = SplitterSize[i] >> 3; 

 		if (SplitterSize[i] > nLength || SplitterSize[i] <= 0 )
		{
			WriteLog(Log_Debug, "CNetRtspServer=%X ,nClient = %llu, rtp �и�� ���� ", this, nClient);

			pDisconnectBaseNetFifo.push((unsigned char*)&nClient, sizeof(nClient));
			return;
		}
	 
		 AddADTSHeadToAAC((unsigned char*)pau, SplitterSize[i]);

		if (netBaseNetType == NetBaseNetType_RtspServerRecvPush && pMediaSource != NULL)
		{
		   pMediaSource->PushAudio(aacData, SplitterSize[i] +7, szAudioName, nChannels,nSampleRate);

#ifdef WriteRtpDepacketFileFlag //����д���AAC��Ƶ�ļ���ֱ�Ӳ��� 
		   fwrite(aacData, 1, SplitterSize[i] + 7, fWriteRtpAudio);
		   fflush(fWriteRtpAudio);
#endif
		}
 
		ptr += au_size;
		pau += SplitterSize[i];
	}
}

//׷��adts��Ϣͷ
void  CNetRtspServer::AddADTSHeadToAAC(unsigned char* szData, int nAACLength)
{
	//int profile = 1;   //AAC LC��MediaCodecInfo.CodecProfileLevel.AACObjectLC;

	/*int avpriv_mpeg4audio_sample_rates[] = {
	96000, 88200, 64000, 48000, 44100, 32000,
	24000, 22050, 16000, 12000, 11025, 8000, 7350
	};
	channel_configuration: ��ʾ������chanCfg
	0: Defined in AOT Specifc Config
	1: 1 channel: front-center
	2: 2 channels: front-left, front-right
	3: 3 channels: front-center, front-left, front-right
	4: 4 channels: front-center, front-left, front-right, back-center
	5: 5 channels: front-center, front-left, front-right, back-left, back-right
	6: 6 channels: front-center, front-left, front-right, back-left, back-right, LFE-channel
	7: 8 channels: front-center, front-left, front-right, side-left, side-right, back-left, back-right, LFE-channel
	8-15: Reserved
	*/

    /*// fill in ADTS data
	aacData[0] = (BYTE)0xFF;
	aacData[1] = (BYTE)0xF1;
	aacData[2] = (BYTE)0x40 | (sample_index << 2) | (AdtsChannel >> 2);
	aacData[3] = (BYTE)((AdtsChannel & 0x3) << 6) | (nAACLength >> 11);
	aacData[4] = (BYTE)(nAACLength >> 3) & 0xff;
	aacData[5] = (BYTE)((nAACLength << 5) & 0xff) | 0x1f;
	aacData[6] = (BYTE)0xFC;*/

	int len = nAACLength + 7;
	uint8_t profile = 2;
	uint8_t sampling_frequency_index = sample_index;
	uint8_t channel_configuration = nChannels;
	aacData[0] = 0xFF; /* 12-syncword */
	aacData[1] = 0xF0 /* 12-syncword */ | (0 << 3)/*1-ID*/ | (0x00 << 2) /*2-layer*/ | 0x01 /*1-protection_absent*/;
	aacData[2] = ((profile - 1) << 6) | ((sampling_frequency_index & 0x0F) << 2) | ((channel_configuration >> 2) & 0x01);
	aacData[3] = ((channel_configuration & 0x03) << 6) | ((len >> 11) & 0x03); /*0-original_copy*/ /*0-home*/ /*0-copyright_identification_bit*/ /*0-copyright_identification_start*/
	aacData[4] = (uint8_t)(len >> 3);
	aacData[5] = ((len & 0x07) << 5) | 0x1F;
	aacData[6] = 0xFC | ((len / 1024) & 0x03);

	memcpy(aacData + 7, szData, nAACLength);
}

CNetRtspServer::CNetRtspServer(NETHANDLE hServer, NETHANDLE hClient, char* szIP, unsigned short nPort,char* szShareMediaURL)
{
	currentSession = Session;
	Session ++;

	nServerVideoUDP[0] = nServerVideoUDP[1] = nServerAudioUDP[0] = nServerAudioUDP[1] = 0;
	for (int i = 0; i < MaxRtpHandleCount; i++) 
	{
		memset((char*)&addrClientVideo[i], 0x00, sizeof(sockaddr_in));
		addrClientVideo[i].sin_family = AF_INET;
		addrClientVideo[i].sin_addr.s_addr = inet_addr(szIP);
		memset((char*)&addrClientAudio[i], 0x00, sizeof(sockaddr_in));
		addrClientAudio[i].sin_family = AF_INET;
		addrClientAudio[i].sin_addr.s_addr = inet_addr(szIP);
	}

	nSetupOrder = 0;
	m_RtspNetworkType = RtspNetworkType_Unknow;
	nRtspPlayCount = 0;
	nServer = hServer;
	nClient = hClient;
	hRtpVideo = 0;
	hRtpAudio = 0;
	nSendRtpVideoMediaBufferLength = nSendRtpAudioMediaBufferLength = 0;
	nCalcAudioFrameCount = 0;
	nStartVideoTimestamp = VideoStartTimestampFlag; //��Ƶ��ʼʱ��� 
	nSendRtpFailCount = 0;//�ۼƷ���rtp��ʧ�ܴ��� 
	strcpy(m_szShareMediaURL, szShareMediaURL);

	strcpy(szClientIP, szIP);
	nClientPort = nPort;
	nPrintCount = 0;
	pMediaSource = NULL;
	bRunFlag = true;
	bIsInvalidConnectFlag = false;

	netDataCacheLength = 0;//�������ݻ����С
	nNetStart = nNetEnd = 0; //����������ʼλ��\����λ��
	MaxNetDataCacheCount = MaxNetDataCacheBufferLength ;
	data_Length = 0;

	nRecvLength = 0;
	memset((char*)szHttpHeadEndFlag, 0x00, sizeof(szHttpHeadEndFlag));
	strcpy((char*)szHttpHeadEndFlag, "\r\n\r\n");
	nHttpHeadEndLength = 0;
	nContentLength = 0;
	memset(szResponseHttpHead, 0x00, sizeof(szResponseHttpHead));

	m_bHaveSPSPPSFlag = false;
	m_nSpsPPSLength = 0;
	memset(s_extra_data,0x00,sizeof(s_extra_data));
	extra_data_size = 0;

	RtspProtectArrayOrder = 0;
	for (int i = 0; i < MaxRtspProtectCount; i++)
		memset((char*)&RtspProtectArray[i], 0x00, sizeof(RtspProtectArray[i]));

	for (int i = 0; i < MaxRtpHandleCount; i++)
  		hRtpHandle[i] = 0 ;
 
	for (int i = 0; i < 3; i++)
		bExitProcessFlagArray[i] = true;

	m_nSpsPPSLength = 0;
	netBaseNetType = NetBaseNetType_Unknown;

#ifdef WriteRtpDepacketFileFlag
	fWriteRtpVideo = fopen("d:\\rtspRecv.264", "wb");
 	fWriteRtpAudio = fopen("d:\\rtspRecv.aac", "wb");
	bStartWriteFlag = false;
#endif
#ifdef WriteVideoDataFlag 
	fWriteVideoFile =	fopen("D:\\rtspServer.264","wb");
#endif
	bRunFlag = true;
	WriteLog(Log_Debug, "CNetRtspServer ���� nClient = %llu ", nClient);
}

CNetRtspServer::~CNetRtspServer()
{
	WriteLog(Log_Debug, "CNetRtspServer �ȴ������˳� nTime = %llu, nClient = %llu ",GetTickCount64(), nClient);
	std::lock_guard<std::mutex> lock(netDataLock);

	bRunFlag = false;
	
	for (int i = 0; i < 3; i++)
	{
		while (!bExitProcessFlagArray[i])
			Sleep(5);
	}
	WriteLog(Log_Debug, "CNetRtspServer �����˳���� nTime = %llu, nClient = %llu ", GetTickCount64(), nClient);

	for (int i = 0; i < MaxRtpHandleCount; i++)
	{
		if(hRtpHandle[i] > 0 )
		  rtp_depacket_stop(hRtpHandle[i]);

		if (nServerVideoUDP[i] > 0)
			XHNetSDK_DestoryUdp(nServerVideoUDP[i]);

		if (nServerAudioUDP[i] > 0)
			XHNetSDK_DestoryUdp(nServerAudioUDP[i]);
	}
 
#ifdef WriteRtpDepacketFileFlag
	if(fWriteRtpVideo)
	  fclose(fWriteRtpVideo);
	if(fWriteRtpAudio)
	  fclose(fWriteRtpAudio);
#endif
#ifdef WriteVideoDataFlag 
	if(fWriteVideoFile != NULL)
	  fclose(fWriteVideoFile );
#endif
	if (hRtpVideo != 0)
	{
		rtp_packet_stop(hRtpVideo);
		hRtpVideo = 0;
	}
	if (hRtpAudio != 0)
	{
		rtp_packet_stop(hRtpAudio);
		hRtpAudio = 0;
	}
	m_videoFifo.FreeFifo();
	m_audioFifo.FreeFifo();

	if (bPushMediaSuccessFlag && netBaseNetType == NetBaseNetType_RtspServerRecvPush && pMediaSource != NULL )
		DeleteMediaStreamSource(szMediaSourceURL);

	malloc_trim(0);

	WriteLog(Log_Debug, "CNetRtspServer ���� nClient = %llu \r\n", nClient);
}
int CNetRtspServer::PushVideo(uint8_t* pVideoData, uint32_t nDataLength, char* szVideoCodec)
{
	if (strlen(mediaCodecInfo.szVideoName) == 0)
		strcpy(mediaCodecInfo.szVideoName, szVideoCodec);

	m_videoFifo.push(pVideoData, nDataLength);

#ifdef WriteVideoDataFlag 
	if (fWriteVideoFile != NULL)
	{
		fwrite(pVideoData, 1, nDataLength, fWriteVideoFile);
	    fflush(fWriteVideoFile);
	}
#endif 
#if  0
	if (extra_data_size == 0 && strcmp(mediaCodecInfo.szVideoName,"H264") == 0)
	{
		int vcl = 0;
		int update = 0;
		unsigned char * s_buffer = new unsigned char[1024 * 1024 * 2];
		int n = h264_annexbtomp4(&avc, pVideoData, nDataLength, s_buffer, 1024 * 1024 * 2, &vcl, &update);
		extra_data_size = mpeg4_avc_decoder_configuration_record_save(&avc, s_extra_data, sizeof(s_extra_data));
		delete [] s_buffer;
	}
#endif 

	return 0;
}

int CNetRtspServer::PushAudio(uint8_t* pVideoData, uint32_t nDataLength, char* szAudioCodec, int nChannels, int SampleRate)
{
	if (strlen(mediaCodecInfo.szAudioName) == 0)
	{
		strcpy(mediaCodecInfo.szAudioName, szAudioCodec);
		mediaCodecInfo.nChannels = nChannels;
		mediaCodecInfo.nSampleRate = SampleRate;
	}

	m_audioFifo.push(pVideoData, nDataLength);

	return 0;
}

int CNetRtspServer::SendVideo()
{
	nRecvDataTimerBySecond = 0;

	unsigned char* pData = NULL;
	int            nLength = 0;
	uint32_t       nVdeoFrameNumber = 0;
	if ((pData = m_videoFifo.pop(&nLength)) != NULL)
	{
 		inputVideo.data = pData;
		inputVideo.datasize = nLength;
 
		if (nMediaSourceType == MediaSourceType_ReplayMedia)
		{//�����¼��طţ�ǰ��4���ֽ�����Ƶ֡��ţ��������֡�������ʱ���ֱ������rtp���
			inputVideo.data = pData + 4;
			inputVideo.datasize = nLength - 4;
			memcpy((char*)&nVdeoFrameNumber, pData, sizeof(uint32_t));
			inputVideo.timestamp = nVdeoFrameNumber * 3600 ;
		}
		rtp_packet_input(&inputVideo);

		m_videoFifo.pop_front();
	}

	return 0;
}

int CNetRtspServer::SendAudio()
{
	nRecvDataTimerBySecond = 0;
	unsigned char* pData = NULL;
	int            nLength = 0;
	uint32_t       nAudioFrameNumber = 0;
	if ((pData = m_audioFifo.pop(&nLength)) != NULL)
	{
		inputAudio.data = pData;
		inputAudio.datasize = nLength;

		if (nMediaSourceType == MediaSourceType_ReplayMedia)
		{//¼��ط�
			inputAudio.data = pData + 4;
			inputAudio.datasize = nLength - 4;
			memcpy((char*)&nAudioFrameNumber, pData, sizeof(uint32_t));
			if(strcmp(mediaCodecInfo.szAudioName,"AAC") == 0)
			  inputAudio.timestamp = nAudioFrameNumber * 1024 ;
			else
			  inputAudio.timestamp = nAudioFrameNumber * 320;
		}
		rtp_packet_input(&inputAudio);

		m_audioFifo.pop_front();
	}

	return 0;
}

//��������ƴ�� 
int CNetRtspServer::InputNetData(NETHANDLE nServerHandle, NETHANDLE nClientHandle, uint8_t* pData, uint32_t nDataLength, void* address)
{
	std::lock_guard<std::mutex> lock(netDataLock);
	//������߼��
	nRecvDataTimerBySecond = 0;

	if (MaxNetDataCacheCount - nNetEnd >= nDataLength)
	{//ʣ��ռ��㹻
		memcpy(netDataCache + nNetEnd, pData, nDataLength);
		netDataCacheLength += nDataLength;
		nNetEnd += nDataLength;
	}
	else
	{//ʣ��ռ䲻������Ҫ��ʣ���buffer��ǰ�ƶ�
		if (netDataCacheLength > 0)
		{//���������ʣ��
			memmove(netDataCache, netDataCache + nNetStart, netDataCacheLength);
			nNetStart = 0;
			nNetEnd = netDataCacheLength;

			if (MaxNetDataCacheCount - nNetEnd < nDataLength)
			{
				nNetStart = nNetEnd = netDataCacheLength = 0;
				WriteLog(Log_Debug, "CNetRtspServer = %X nClient = %llu �����쳣 , ִ��ɾ��", this, nClient);
				pDisconnectBaseNetFifo.push((unsigned char*)&nClient, sizeof(nClient));
				return 0;
			}
		}
		else
		{//û��ʣ�࣬��ô �ף�βָ�붼Ҫ��λ 
			nNetStart = 0;
			nNetEnd = 0;
			netDataCacheLength = 0;
		}
		memcpy(netDataCache + nNetEnd, pData, nDataLength);
		netDataCacheLength += nDataLength;
		nNetEnd += nDataLength;
	}
 	return 0 ;
}

//��ȡ�������� ��ģ��ԭ���ײ�������ȡ���� 
int32_t  CNetRtspServer::XHNetSDKRead(NETHANDLE clihandle, uint8_t* buffer, uint32_t* buffsize, uint8_t blocked, uint8_t certain)
{
	int nWaitCount = 0;
	bExitProcessFlagArray[0] = false;
	while (!bIsInvalidConnectFlag && bRunFlag)
	{
		if (netDataCacheLength >= *buffsize)
		{
			memcpy(buffer, netDataCache + nNetStart, *buffsize);
			nNetStart += *buffsize;
			netDataCacheLength -= *buffsize;
			
			bExitProcessFlagArray[0] = true;
			return 0;
		}
		Sleep(10);
		
		nWaitCount ++;
		if (nWaitCount >= 100 * 3)
			break;
	}
	bExitProcessFlagArray[0] = true;

	return -1;  
}

bool   CNetRtspServer::ReadRtspEnd()
{
	unsigned int nReadLength = 1;
	unsigned int nRet;
	bool     bRet = false;
	bExitProcessFlagArray[1] = false;
	while (!bIsInvalidConnectFlag && bRunFlag)
	{
		nReadLength = 1;
		nRet = XHNetSDKRead(nClient, data_ + data_Length, &nReadLength, true, true);
		if (nRet == 0 && nReadLength == 1)
		{
			data_Length += 1;
			if (data_Length >= 4 && data_[data_Length - 4] == '\r' && data_[data_Length - 3] == '\n' && data_[data_Length - 2] == '\r' && data_[data_Length - 1] == '\n')
			{
				bRet = true;
				break;
			}
		}
		else
		{
			WriteLog(Log_Debug, "ReadRtspEnd() ,��δ��ȡ������ ��CABLRtspClient =%X ,dwClient=%llu ", this, nClient);
			break;
		}

		if (data_Length >= RtspServerRecvDataLength)
		{
			WriteLog(Log_Debug, "ReadRtspEnd() ,�Ҳ��� rtsp �������� ��CABLRtspClient =%X ,dwClient = %llu ", this, nClient);
			break;
		}
	}
	bExitProcessFlagArray[1] = true;
	return bRet;
}

//����
int  CNetRtspServer::FindHttpHeadEndFlag()
{
	if (data_Length <= 0)
		return -1 ;

	for (int i = 0; i < data_Length; i++)
	{
		if (memcmp(data_ + i, szHttpHeadEndFlag, 4) == 0)
		{
			nHttpHeadEndLength = i + 4;
			return nHttpHeadEndLength;
		}
	}
	return -1;
}

int  CNetRtspServer::FindKeyValueFlag(char* szData)
{
	int nSize = strlen(szData);
	for (int i = 0; i < nSize; i++)
	{
		if (memcmp(szData + i, ": ", 2) == 0)
			return i;
	}
	return -1;
}

//��ȡHTTP������httpURL 
void CNetRtspServer::GetHttpModemHttpURL(char* szMedomHttpURL)
{//"POST /getUserName?userName=admin&password=123456 HTTP/1.1"
	if (RtspProtectArrayOrder >= MaxRtspProtectCount)
		return;

	int nPos1, nPos2, nPos3, nPos4;
	string strHttpURL = szMedomHttpURL;
	char   szTempRtsp[512] = { 0 };
	string strTempRtsp;

	strcpy(RtspProtectArray[RtspProtectArrayOrder].szRtspCmdString, szMedomHttpURL);

	nPos1 = strHttpURL.find(" ", 0);
	if (nPos1 > 0)
	{
		nPos2 = strHttpURL.find(" ", nPos1 + 1);
		if (nPos1 > 0 && nPos2 > 0)
		{
			memcpy(RtspProtectArray[RtspProtectArrayOrder].szRtspCommand, szMedomHttpURL, nPos1);

			//memcpy(RtspProtectArray[RtspProtectArrayOrder].szRtspURL, szMedomHttpURL + nPos1 + 1, nPos2 - nPos1 - 1);
			memcpy(szTempRtsp, szMedomHttpURL + nPos1 + 1, nPos2 - nPos1 - 1);
			strTempRtsp = szTempRtsp;
			nPos3 = strTempRtsp.find("?", 0);
			if (nPos3 > 0)
			{//ȥ�������
				szTempRtsp[nPos3] = 0x00;
			}
			strTempRtsp = szTempRtsp;

			//����554 �˿�
			nPos4 = strTempRtsp.find(":", 8);
			if (nPos4 <= 0)
			{//û��554 �˿�
				nPos3 = strTempRtsp.find("/", 8);
				if (nPos3 > 0)
				{
					strTempRtsp.insert(nPos3, ":554");
				}
			}

			strcpy(RtspProtectArray[RtspProtectArrayOrder].szRtspURL, strTempRtsp.c_str());
		}
	}
}

 //��httpͷ������䵽�ṹ��
int  CNetRtspServer::FillHttpHeadToStruct()
{
	RtspProtectArrayOrder = 0;
	if (RtspProtectArrayOrder >= MaxRtspProtectCount)
		return true;

	int  nStart = 0;
	int  nPos = 0;
	int  nFlagLength;
	if (nHttpHeadEndLength <= 0)
		return true;
	int  nKeyCount = 0;
	char szTemp[string_length_2048] = { 0 };
	char szKey[string_length_2048] = { 0 };

	for (int i = 0; i < nHttpHeadEndLength - 2; i++)
	{
		if (memcmp(data_ + i, szHttpHeadEndFlag, 2) == 0)
		{
			memset(szTemp, 0x00, sizeof(szTemp));
			memcpy(szTemp, data_ + nPos, i - nPos);

			if ((nFlagLength = FindKeyValueFlag(szTemp)) >= 0)
			{
				memset(RtspProtectArray[RtspProtectArrayOrder].rtspField[nKeyCount].szKey, 0x00, sizeof(RtspProtectArray[RtspProtectArrayOrder].rtspField[nKeyCount].szKey));//Ҫ���
				memset(RtspProtectArray[RtspProtectArrayOrder].rtspField[nKeyCount].szValue, 0x00, sizeof(RtspProtectArray[RtspProtectArrayOrder].rtspField[nKeyCount].szValue));//Ҫ��� 

				memcpy(RtspProtectArray[RtspProtectArrayOrder].rtspField[nKeyCount].szKey, szTemp, nFlagLength);
				memcpy(RtspProtectArray[RtspProtectArrayOrder].rtspField[nKeyCount].szValue, szTemp + nFlagLength + 2, strlen(szTemp) - nFlagLength - 2);

				strcpy(szKey, RtspProtectArray[RtspProtectArrayOrder].rtspField[nKeyCount].szKey);
				to_lower(szKey);
				if (strcmp(szKey, "content-length") == 0)
				{//���ݳ���
					nContentLength = atoi(RtspProtectArray[RtspProtectArrayOrder].rtspField[nKeyCount].szValue);
					RtspProtectArray[RtspProtectArrayOrder].nRtspSDPLength = nContentLength;
				}

				//�Ѽ�Ȩ�Ĳ����������� 
				if (strlen(szPlayParams) == 0)
				{
					string strRtspCmd = RtspProtectArray[RtspProtectArrayOrder].szRtspCmdString; 
					int    nPos1 = 0, nPos2 = 0;
					nPos1 = strRtspCmd.find("?", 0);
					if(nPos1 > 0)
					  nPos2 = strRtspCmd.find(" RTSP",nPos1);
					if (nPos1 > 0 && nPos2 > 0)
					{
						memcpy(szPlayParams, RtspProtectArray[RtspProtectArrayOrder].szRtspCmdString + nPos1 + 1, nPos2 - nPos1 - 1);
					}
				}

				nKeyCount++;

				//��ֹ������Χ
				if (nKeyCount >= MaxRtspValueCount)
					return true;
			}
			else
			{//���� http ������URL 
				GetHttpModemHttpURL(szTemp);
			} 

			nPos = i + 2;
		}
	}

	return true;
} 

bool CNetRtspServer::GetFieldValue(char* szFieldName, char* szFieldValue)
{
	bool bFindFlag = false;

	for (int i = 0; i < MaxRtspProtectCount; i++)
	{
		for (int j = 0; j < MaxRtspValueCount; j++)
		{
			if (strcmp(RtspProtectArray[i].rtspField[j].szKey, szFieldName) == 0)
			{
				bFindFlag = true;
				strcpy(szFieldValue, RtspProtectArray[i].rtspField[j].szValue);
				break;
			}
		}
	}

	return bFindFlag;
}

//ͳ��rtspURL  rtsp://190.15.240.11:554/Media/Camera_00001 ·�� / ������ 
int  CNetRtspServer::GetRtspPathCount(char* szRtspURL)
{
	string strCurRtspURL = szRtspURL;
	int    nPos1, nPos2,nPos3;
	int    nPathCount = 0;
 	nPos1 = strCurRtspURL.find("//", 0);
	if (nPos1 < 0)
		return 0;//��ַ�Ƿ� 

	while (true)
	{
		nPos1 = strCurRtspURL.find("/", nPos1 + 2);
		if (nPos1 >= 0)
		{
			nPos1 += 1;
			nPathCount ++;
		}
		else
			break;
	}

	//�Ϸ���������ַ �����������������Ϣ
	if (nPathCount == 2)
	{
	  memset((char*)&m_addStreamProxyStruct, 0x00, sizeof(m_addStreamProxyStruct));
	  strcpy(m_addStreamProxyStruct.url, szRtspURL);
	  
	  nPos1 = strCurRtspURL.find("//", 0);
	  if (nPos1 > 0)
	  {
		  nPos2 = strCurRtspURL.find("/", nPos1 + 2);
		  if (nPos2 > 0)
		  {
			  nPos3 = strCurRtspURL.find("/", nPos2 + 1);
			  if (nPos3 > 0)
			  {
				  memcpy(m_addStreamProxyStruct.app, szRtspURL + nPos2 + 1, nPos3 - nPos2 - 1);
				  memcpy(m_addStreamProxyStruct.stream, szRtspURL + nPos3 + 1, strlen(szRtspURL) - nPos3 - 1);
			  }
		  }
	  }
 	}
  
	return nPathCount;
}

//��ȡrtsp��ַ�е�ý���ļ� ������ /Media/Camera_00001
bool   CNetRtspServer::GetMediaURLFromRtspSDP()
{
	strcpy(szCurRtspURL, RtspProtectArray[RtspProtectArrayOrder].szRtspURL);
	string strCurRtspURL = szCurRtspURL;
	int    nPos1, nPos2;
	int    nPathCount;
	bool   bGetMediaSoureURL = false;

	nPathCount = GetRtspPathCount(szCurRtspURL);
	if (nPathCount != 2)
	{//����rtsp��������Ҫ����·��
 			WriteLog(Log_Debug, "rtsp������ַ�����������ı�׼����Ҫ��������URL: rtsp://190.15.240.11:554/live/Camera_00001 ");
			DeleteNetRevcBaseClient(nClient);
			return false;
 	}

	nPos1 = strCurRtspURL.find("//", 0);
	if (nPos1 > 0)
	{
		nPos2 = strCurRtspURL.find("/", nPos1 + 2);
		if (nPos2 > 0)
		{
			memset(szMediaSourceURL, 0x00, sizeof(szMediaSourceURL));
			memcpy(szMediaSourceURL, szCurRtspURL + nPos2, strlen(szCurRtspURL) - nPos2);
			bGetMediaSoureURL = true;
		}
	}
	if (!bGetMediaSoureURL)
	{
		WriteLog(Log_Debug, "��ȡRTSPý���ַʧ�� �� nClient = %llu",nClient);
		DeleteNetRevcBaseClient(nClient);
		return false ;
	}
	else
		return true;
}

//rtp����ص���Ƶ
void Video_rtp_packet_callback_func(_rtp_packet_cb* cb)
{
	CNetRtspServer* pRtspClient = (CNetRtspServer*)cb->userdata;
	if(pRtspClient->bRunFlag && pRtspClient->m_RtspNetworkType == RtspNetworkType_TCP)
	  pRtspClient->ProcessRtpVideoData(cb->data, cb->datasize);
	else if (pRtspClient->bRunFlag && pRtspClient->m_RtspNetworkType == RtspNetworkType_UDP && pRtspClient->nServerVideoUDP[0] > 0 )
	  XHNetSDK_Sendto(pRtspClient->nServerVideoUDP[0], cb->data, cb->datasize, (void*)&pRtspClient->addrClientVideo[0]);
}

//rtp ����ص���Ƶ
void Audio_rtp_packet_callback_func(_rtp_packet_cb* cb)
{
	CNetRtspServer* pRtspClient = (CNetRtspServer*)cb->userdata;
	if (pRtspClient->bRunFlag && pRtspClient->m_RtspNetworkType == RtspNetworkType_TCP)
	  pRtspClient->ProcessRtpAudioData(cb->data, cb->datasize);
	else if (pRtspClient->bRunFlag && pRtspClient->m_RtspNetworkType == RtspNetworkType_UDP && pRtspClient->nServerAudioUDP[0] > 0 )
		XHNetSDK_Sendto(pRtspClient->nServerAudioUDP[0], cb->data, cb->datasize, (void*)&pRtspClient->addrClientAudio[0]);
}

void CNetRtspServer::ProcessRtpVideoData(unsigned char* pRtpVideo,int nDataLength)
{
	if ((MaxRtpSendVideoMediaBufferLength - nSendRtpVideoMediaBufferLength < nDataLength + 4) && nSendRtpVideoMediaBufferLength > 0 )
	{//ʣ��ռ䲻���洢 ,��ֹ���� 
		SumSendRtpMediaBuffer(szSendRtpVideoMediaBuffer, nSendRtpVideoMediaBufferLength);
 		nSendRtpVideoMediaBufferLength = 0;
	}

	memcpy((char*)&nCurrentVideoTimestamp, pRtpVideo + 4, sizeof(uint32_t));
	if (nStartVideoTimestamp != VideoStartTimestampFlag &&  nStartVideoTimestamp != nCurrentVideoTimestamp && nSendRtpVideoMediaBufferLength > 0 )
	{//����һ֡�µ���Ƶ 
		//WriteLog(Log_Debug, "CNetRtspServer= %X, ����һ֡��Ƶ ��Length = %d ", this, nSendRtpVideoMediaBufferLength);
		SumSendRtpMediaBuffer(szSendRtpVideoMediaBuffer, nSendRtpVideoMediaBufferLength);
		nSendRtpVideoMediaBufferLength = 0;
	}
 
	szSendRtpVideoMediaBuffer[nSendRtpVideoMediaBufferLength + 0] = '$';
	szSendRtpVideoMediaBuffer[nSendRtpVideoMediaBufferLength + 1] = 0;
	nVideoRtpLen = htons(nDataLength);
	memcpy(szSendRtpVideoMediaBuffer + (nSendRtpVideoMediaBufferLength + 2), (unsigned char*)&nVideoRtpLen, sizeof(nVideoRtpLen));
 	memcpy(szSendRtpVideoMediaBuffer + (nSendRtpVideoMediaBufferLength + 4), pRtpVideo, nDataLength);
 
	nStartVideoTimestamp = nCurrentVideoTimestamp;
	nSendRtpVideoMediaBufferLength += nDataLength + 4;
}
void CNetRtspServer::ProcessRtpAudioData(unsigned char* pRtpAudio, int nDataLength)
{
	if (hRtpVideo != 0)
	{
		if ((MaxRtpSendAudioMediaBufferLength - nSendRtpAudioMediaBufferLength < nDataLength + 4) && nSendRtpAudioMediaBufferLength > 0)
		{//ʣ��ռ䲻���洢 ,��ֹ���� 
			SumSendRtpMediaBuffer(szSendRtpAudioMediaBuffer, nSendRtpAudioMediaBufferLength);

			nSendRtpAudioMediaBufferLength = 0;
			nCalcAudioFrameCount = 0;
		}

		szSendRtpAudioMediaBuffer[nSendRtpAudioMediaBufferLength + 0] = '$';
		szSendRtpAudioMediaBuffer[nSendRtpAudioMediaBufferLength + 1] = 2;
		nAudioRtpLen = htons(nDataLength);
		memcpy(szSendRtpAudioMediaBuffer + (nSendRtpAudioMediaBufferLength + 2), (unsigned char*)&nAudioRtpLen, sizeof(nAudioRtpLen));
		memcpy(szSendRtpAudioMediaBuffer + (nSendRtpAudioMediaBufferLength + 4), pRtpAudio, nDataLength);

		nSendRtpAudioMediaBufferLength += nDataLength + 4;
		nCalcAudioFrameCount++;

		if (nCalcAudioFrameCount >= 3 && nSendRtpAudioMediaBufferLength > 0)
		{
			SumSendRtpMediaBuffer(szSendRtpAudioMediaBuffer, nSendRtpAudioMediaBufferLength);
			nSendRtpAudioMediaBufferLength = 0;
			nCalcAudioFrameCount = 0;
		}
	}
	else
	{//ֻ����Ƶ
		nSendRtpAudioMediaBufferLength = 0;
		szSendRtpAudioMediaBuffer[nSendRtpAudioMediaBufferLength + 0] = '$';
		szSendRtpAudioMediaBuffer[nSendRtpAudioMediaBufferLength + 1] = 0 ;
		nAudioRtpLen = htons(nDataLength);
		memcpy(szSendRtpAudioMediaBuffer + (nSendRtpAudioMediaBufferLength + 2), (unsigned char*)&nAudioRtpLen, sizeof(nAudioRtpLen));
		memcpy(szSendRtpAudioMediaBuffer + (nSendRtpAudioMediaBufferLength + 4), pRtpAudio, nDataLength);
		XHNetSDK_Write(nClient, szSendRtpAudioMediaBuffer, nDataLength + 4, 1);
	}
}

//�ۻ�rtp��������
void CNetRtspServer::SumSendRtpMediaBuffer(unsigned char* pRtpMedia, int nRtpLength)
{
 	std::lock_guard<std::mutex> lock(MediaSumRtpMutex);
 
	if (bRunFlag)
	{
		nSendRet = XHNetSDK_Write(nClient, pRtpMedia, nRtpLength, 1);
		if (nSendRet != 0)
		{
			nSendRtpFailCount++;
			//WriteLog(Log_Debug, "����rtp ��ʧ�� ,�ۼƴ��� nSendRtpFailCount = %d �� ��nClient = %llu ", nSendRtpFailCount, nClient);
			if (nSendRtpFailCount >= 30)
			{
				bRunFlag = false;

				WriteLog(Log_Debug, "����rtp ��ʧ�� ,�ۼƴ��� �Ѿ��ﵽ nSendRtpFailCount = %d �Σ�����ɾ�� ��nClient = %llu ", nSendRtpFailCount, nClient);
				pDisconnectBaseNetFifo.push((unsigned char*)&nClient, sizeof(nClient));
			}
		}
		else
			nSendRtpFailCount = 0;
	}
}

//����ý����Ϣƴװ SDP ��Ϣ
bool CNetRtspServer::GetRtspSDPFromMediaStreamSource(RtspSDPContentStruct sdpContent, bool bGetFlag)
{
	memset(szRtspSDPContent, 0x00, sizeof(szRtspSDPContent));

	//��Ƶ
	nVideoSSRC = rand();
	memset((char*)&optionVideo, 0x00, sizeof(optionVideo));
	if (strcmp(pMediaSource->m_mediaCodecInfo.szVideoName,"H264") == 0)
	{
		optionVideo.streamtype = e_rtppkt_st_h264;
		if (bGetFlag)
			nVideoPayload = sdpContent.nVidePayload;
		else
			nVideoPayload = 98;
		if(duration == 0)
		  sprintf(szRtspSDPContent, "v=0\r\no=- 0 0 IN IP4 127.0.0.1\r\ns=No Name\r\nc=IN IP4 190.15.240.36\r\nt=0 0\r\na=tool:libavformat 55.19.104\r\na=range: npt=0-\r\nm=video 0 RTP/AVP %d\r\nb=AS:832\r\na=rtpmap:%d H264/90000\r\na=fmtp:%d packetization-mode=1\r\na=control:streamid=0\r\n", nVideoPayload, nVideoPayload, nVideoPayload);
		else 
		  sprintf(szRtspSDPContent, "v=0\r\no=- 0 0 IN IP4 127.0.0.1\r\ns=No Name\r\nc=IN IP4 190.15.240.36\r\nt=0 0\r\na=tool:libavformat 55.19.104\r\na=range: npt=0-%.3f\r\nm=video 0 RTP/AVP %d\r\nb=AS:832\r\na=rtpmap:%d H264/90000\r\na=fmtp:%d packetization-mode=1\r\na=control:streamid=0\r\n",(double)duration, nVideoPayload, nVideoPayload, nVideoPayload);
	}
	else if (strcmp(pMediaSource->m_mediaCodecInfo.szVideoName, "H265") == 0)
	{
		optionVideo.streamtype = e_rtppkt_st_h265;
		if (bGetFlag)
			nVideoPayload = sdpContent.nVidePayload;
		else
		    nVideoPayload = 99;
		if(duration == 0)
		  sprintf(szRtspSDPContent, "v=0\r\no=- 0 0 IN IP4 127.0.0.1\r\ns=No Name\r\nc=IN IP4 190.15.240.36\r\nt=0 0\r\na=tool:libavformat 55.19.104\r\na=range: npt=0-\r\nm=video 0 RTP/AVP %d\r\nb=AS:3007\r\na=rtpmap:%d H265/90000\r\na=fmtp:%d packetization-mode=1\r\na=control:streamid=0\r\n", nVideoPayload, nVideoPayload, nVideoPayload);
		else
		  sprintf(szRtspSDPContent, "v=0\r\no=- 0 0 IN IP4 127.0.0.1\r\ns=No Name\r\nc=IN IP4 190.15.240.36\r\nt=0 0\r\na=tool:libavformat 55.19.104\r\na=range: npt=0-%.3f\r\nm=video 0 RTP/AVP %d\r\nb=AS:3007\r\na=rtpmap:%d H265/90000\r\na=fmtp:%d packetization-mode=1\r\na=control:streamid=0\r\n",(double)duration, nVideoPayload, nVideoPayload, nVideoPayload);
	}

	if (strlen(pMediaSource->m_mediaCodecInfo.szVideoName) > 0)
	{
		int nRet = rtp_packet_start(Video_rtp_packet_callback_func, (void*)this, &hRtpVideo);
		if (nRet != e_rtppkt_err_noerror)
		{
			WriteLog(Log_Debug, "CNetRtspServer = %X ��������Ƶrtp���ʧ��,nClient = %llu,  nRet = %d", this, nClient, nRet);
			return false;
		}
		optionVideo.handle = hRtpVideo;
		optionVideo.ssrc = nVideoSSRC;
		optionVideo.mediatype = e_rtppkt_mt_video;
		optionVideo.payload = nVideoPayload;
		optionVideo.ttincre = 90000 / mediaCodecInfo.nVideoFrameRate; //��Ƶʱ�������

		inputVideo.handle = hRtpVideo;
		inputVideo.ssrc = optionVideo.ssrc;

		nRet = rtp_packet_setsessionopt(&optionVideo);
		if (nRet != e_rtppkt_err_noerror)
		{
			WriteLog(Log_Debug, "CRecvRtspClient = %X ��rtp_packet_setsessionopt ������Ƶrtp���ʧ�� ,nClient = %llu nRet = %d", this, nClient, nRet);
			return false;
		}
	}
	memset(szRtspAudioSDP, 0x00, sizeof(szRtspAudioSDP));
 
	memset((char*)&optionAudio, 0x00, sizeof(optionAudio));
	if (strcmp(pMediaSource->m_mediaCodecInfo.szAudioName, "G711_A") == 0)
	{
		optionAudio.ttincre = 320; //g711a �� ��������
		optionAudio.streamtype = e_rtppkt_st_g711a;
		if (bGetFlag)
			nAudioPayload = sdpContent.nAudioPayload;
		else
 		   nAudioPayload = 8;
		sprintf(szRtspAudioSDP, "m=audio 0 RTP/AVP %d\r\nb=AS:50\r\na=recvonly\r\na=rtpmap:%d PCMA/%d\r\na=control:streamid=1\r\na=framerate:25\r\n",
			nAudioPayload, nAudioPayload, 8000);
	}
	else if (strcmp(pMediaSource->m_mediaCodecInfo.szAudioName, "G711_U") == 0)
	{
		optionAudio.ttincre = 320; //g711u �� ��������
		optionAudio.streamtype = e_rtppkt_st_g711u;
		if (bGetFlag)
			nAudioPayload = sdpContent.nAudioPayload;
		else
			nAudioPayload = 0;
		sprintf(szRtspAudioSDP, "m=audio 0 RTP/AVP %d\r\nb=AS:50\r\na=recvonly\r\na=rtpmap:%d PCMU/%d\r\na=control:streamid=1\r\na=framerate:25\r\n",
			nAudioPayload, nAudioPayload, 8000);
	}
	else if (strcmp(pMediaSource->m_mediaCodecInfo.szAudioName, "AAC") == 0)
	{
		optionAudio.ttincre = 1024 ;//aac �ĳ�������
		optionAudio.streamtype = e_rtppkt_st_aac;
		if (bGetFlag)
			nAudioPayload = sdpContent.nAudioPayload;
		else
			nAudioPayload = 104;
		sprintf(szRtspAudioSDP, "m=audio 0 RTP/AVP %d\r\na=rtpmap:%d MPEG4-GENERIC/%d/%d\r\na=fmtp:%d profile-level-id=15; streamtype=5; mode=AAC-hbr; config=%s;SizeLength=13; IndexLength=3; IndexDeltaLength=3; Profile=1;\r\na=control:streamid=1\r\n",
			nAudioPayload, nAudioPayload, pMediaSource->m_mediaCodecInfo.nSampleRate, pMediaSource->m_mediaCodecInfo.nChannels, nAudioPayload,getAACConfig(pMediaSource->m_mediaCodecInfo.nChannels,pMediaSource->m_mediaCodecInfo.nSampleRate));
    }

	//׷����ƵSDP
	if (strlen(szRtspAudioSDP) > 0 )
	{
		int32_t nRet = rtp_packet_start(Audio_rtp_packet_callback_func, (void*)this, &hRtpAudio);
		if (nRet != e_rtppkt_err_noerror)
		{
			WriteLog(Log_Debug, "CNetRtspServer = %X ��������Ƶrtp���ʧ�� ,nClient = %llu,  nRet = %d", this,nClient, nRet);
			return false;
		}
		optionAudio.handle = hRtpAudio;
		optionAudio.ssrc = nVideoSSRC + 1;
		optionAudio.mediatype = e_rtppkt_mt_audio;
		optionAudio.payload = nAudioPayload;

 		inputAudio.handle = hRtpAudio;
		inputAudio.ssrc = optionAudio.ssrc;

		rtp_packet_setsessionopt(&optionAudio);

		if( strlen(szRtspSDPContent) > 0)
		  strcat(szRtspSDPContent, szRtspAudioSDP);
		else
		  strcpy(szRtspSDPContent, szRtspAudioSDP);
	}

	if (bGetFlag)
		strcpy(szRtspSDPContent, sdpContent.szSDPContent);
	//WriteLog(Log_Debug, "��װ��SDP :\r\n%s", szRtspSDPContent);

	return true;
}

//����rtsp����
void  CNetRtspServer::InputRtspData(unsigned char* pRecvData, int nDataLength)
{
#if  0 
	 if (nDataLength < 1024)
		WriteLog(Log_Debug, "RecvData \r\n%s \r\n", pRecvData);
#endif

	if (memcmp(data_, "OPTIONS", 7) == 0 && strstr((char*)pRecvData, "\r\n\r\n") != NULL)
	{
 		memset(szCSeq, 0x00, sizeof(szCSeq));
 		GetFieldValue("CSeq", szCSeq);

		sprintf(szResponseBuffer, "RTSP/1.0 200 OK\r\nCSeq: %s\r\nPublic: %s\r\n\r\n", szCSeq, RtspServerPublic);
		nSendRet = XHNetSDK_Write(nClient, (unsigned char*)szResponseBuffer, strlen(szResponseBuffer), 1);
		if (nSendRet != 0)
			pDisconnectBaseNetFifo.push((unsigned char*)&nClient, sizeof(nClient));
   	}
	else if ( memcmp(pRecvData, "ANNOUNCE", 8) == 0 && strstr((char*)pRecvData, "\r\n\r\n") != NULL)
	{//&& nRecvLength > 500
 		if (data_Length - nHttpHeadEndLength < nContentLength)
		{
			WriteLog(Log_Debug, "������δ�������� ");
 			return;
		}

		netBaseNetType = NetBaseNetType_RtspServerRecvPush; //RTSP �����������տͻ��˵�����  
		memset(szCSeq, 0x00, sizeof(szCSeq));
		GetFieldValue("CSeq", szCSeq);

		//���� rtspURL DESCRIBE
		if (!GetMediaURLFromRtspSDP())
			return;

		//�ж�������ַ�Ƿ����
		boost::shared_ptr<CMediaStreamSource> pMediaSourceTemp = GetMediaStreamSource(szMediaSourceURL);
		if (pMediaSourceTemp != NULL || strstr(szMediaSourceURL, RecordFileReplaySplitter) != NULL )
		{
			sprintf(szResponseBuffer, "RTSP/1.0 406 Not Acceptable\r\nServer: %s\r\nCSeq: %s\r\n\r\n", MediaServerVerson, szCSeq);
			nSendRet = XHNetSDK_Write(nClient, (unsigned char*)szResponseBuffer, strlen(szResponseBuffer), 1);
			WriteLog(Log_Debug, "ANNOUNCE ������ַ�Ѿ����� %s ", szMediaSourceURL);

			DeleteNetRevcBaseClient(nClient);
			return;
		}

		//����ԭʼý���� 
		pMediaSource = CreateMediaStreamSource(szMediaSourceURL,nClient, MediaSourceType_LiveMedia, 0, m_h265ConvertH264Struct);
		if (pMediaSource)
		{
			pMediaSource->netBaseNetType = netBaseNetType;
			//�����������¼������¼���־
			if (ABL_MediaServerPort.pushEnable_mp4 == 1)
				pMediaSource->enable_mp4 = true; 
		}
		else
		{
			DeleteNetRevcBaseClient(nClient);
			return ;
		}
		
		bPushMediaSuccessFlag = true;//�ɹ����� 

		//200 OK �ظ�
		sprintf(szResponseBuffer, "RTSP/1.0 200 OK\r\nServer: %s\r\nCSeq: %s\r\n\r\n", MediaServerVerson, szCSeq);
 		nSendRet = XHNetSDK_Write(nClient, (unsigned char*)szResponseBuffer, strlen(szResponseBuffer), 1);
		if (nSendRet != 0)
		{
			DeleteNetRevcBaseClient(nClient);
			return ;
		}

		//����SDP��Ϣ
		memset(szRtspContentSDP, 0x00, sizeof(szRtspContentSDP));
		memcpy(szRtspContentSDP, data_ + nHttpHeadEndLength, nContentLength);
		nContentLength = 0; //nContentLength���� Ҫ��λ

		//��SDP�л�ȡ��Ƶ����Ƶ��ʽ��Ϣ 
		if (!GetMediaInfoFromRtspSDP())
		{
			sprintf(szResponseBuffer, "RTSP/1.0 415 Unsupport Media Type\r\nServer: %s\r\nCSeq: %s\r\n\r\n", MediaServerVerson, szCSeq);
			nSendRet = XHNetSDK_Write(nClient, (unsigned char*)szResponseBuffer, strlen(szResponseBuffer), 1);

			WriteLog(Log_Debug, "ANNOUNCE SDP ��Ϣ��û�кϷ���ý������ %s ", szRtspContentSDP);
 			DeleteNetRevcBaseClient(nClient);
			return ;
		}

		//��RTSP��SDP ������ý��Դ 
		strcpy(pMediaSource->rtspSDPContent.szSDPContent, szRtspContentSDP);

		WriteLog(Log_Debug, "nClient = %llu SDP \r\n%s \r\n",nClient, pRecvData);

		GetSPSPPSFromDescribeSDP();
		if (m_nSpsPPSLength > 0 && m_nSpsPPSLength <= sizeof(pMediaSource->pSPSPPSBuffer))
		{
 			memcpy(pMediaSource->pSPSPPSBuffer, m_pSpsPPSBuffer, m_nSpsPPSLength);
			pMediaSource->nSPSPPSLength = m_nSpsPPSLength;
 		}
 
		int nRet2 = 0;
		if (strlen(szVideoName) > 0)
		{
		   nRet2 = rtp_depacket_start(rtppacket_callback, (void*)this, (uint32_t*)&hRtpHandle[0]);
		   if (strcmp(szVideoName, "H264") == 0)
			 rtp_depacket_setpayload(hRtpHandle[0], nVideoPayload, e_rtpdepkt_st_h264);
		   else if (strcmp(szVideoName, "H265") == 0)
			 rtp_depacket_setpayload(hRtpHandle[0], nVideoPayload, e_rtpdepkt_st_h265);
 		}

		nRet2 = rtp_depacket_start(rtppacket_callback, (void*)this, (uint32_t*)&hRtpHandle[1]);

		if (strcmp(szAudioName, "G711_A") == 0)
		{
			rtp_depacket_setpayload(hRtpHandle[1], nAudioPayload, e_rtpdepkt_st_g711a);
		}
		else if (strcmp(szAudioName, "G711_U") == 0)
		{
			rtp_depacket_setpayload(hRtpHandle[1], nAudioPayload, e_rtpdepkt_st_g711u);
		}
		else if (strcmp(szAudioName, "AAC") == 0)
		{
			rtp_depacket_setpayload(hRtpHandle[1], nAudioPayload, e_rtpdepkt_st_aac);
		}
		else if (strstr(szAudioName, "G726LE") != NULL)
		{
			rtp_depacket_setpayload(hRtpHandle[1], nAudioPayload, e_rtpdepkt_st_g726le);
		}

		//ȷ��ADTSͷ��ز���
		sample_index = 11;
		for (int i = 0; i < 13; i++)
		{
			if (avpriv_mpeg4audio_sample_rates[i] == nSampleRate)
			{
				sample_index = i;
				break;
			}
		} 

		//����Ƶ����Ƶ�����Ϣ������ý��Դ
		strcpy(pMediaSource->rtspSDPContent.szVideoName, szVideoName);
		pMediaSource->rtspSDPContent.nVidePayload = nVideoPayload;
		strcpy(pMediaSource->rtspSDPContent.szAudioName, szAudioName);
		pMediaSource->rtspSDPContent.nChannels  = nChannels;
		pMediaSource->rtspSDPContent.nAudioPayload = nAudioPayload;
		pMediaSource->rtspSDPContent.nSampleRate = nSampleRate;
  	}
	else if (memcmp(pRecvData, "DESCRIBE", 8) == 0 && strstr((char*)pRecvData, "\r\n\r\n") != NULL)
	{
		netBaseNetType = NetBaseNetType_RtspServerSendPush; //RTSP ��������ת���ͻ��˵�������������  

		memset(szCSeq, 0x00, sizeof(szCSeq));
		GetFieldValue("CSeq", szCSeq);

		//���� rtspURL DESCRIBE													
		if (!GetMediaURLFromRtspSDP())
			return;

		//�ж�Դ��ý���Ƿ����
		if (strstr(szMediaSourceURL, RecordFileReplaySplitter) == NULL)
		{//�ۿ�ʵ��
			pMediaSource = GetMediaStreamSource(szMediaSourceURL);
			if (pMediaSource == NULL || !(strlen(pMediaSource->m_mediaCodecInfo.szVideoName) > 0 || strlen(pMediaSource->m_mediaCodecInfo.szAudioName) > 0))
			{
				sprintf(szResponseBuffer, "RTSP/1.0 404 Not FOUND\r\nServer: %s\r\nCSeq: %s\r\n\r\n", MediaServerVerson, szCSeq);
				nSendRet = XHNetSDK_Write(nClient, (unsigned char*)szResponseBuffer, strlen(szResponseBuffer), 1);
				WriteLog(Log_Debug, "ý���� %s ������ ,׼��ɾ�� nClient =%llu ", szMediaSourceURL, nClient);

				DeleteNetRevcBaseClient(nClient);
				return;
			}
		}
		else
		{//¼��㲥
		 
			//��ѯ�㲥��¼���Ƿ����
			if (QueryRecordFileIsExiting(szMediaSourceURL) == false)
			{
				sprintf(szResponseBuffer, "RTSP/1.0 404 Not FOUND\r\nServer: %s\r\nCSeq: %s\r\n\r\n", MediaServerVerson, szCSeq);
				nSendRet = XHNetSDK_Write(nClient, (unsigned char*)szResponseBuffer, strlen(szResponseBuffer), 1);
				WriteLog(Log_Debug, "¼���ļ� %s ������ ,׼��ɾ�� nClient = %llu ", szMediaSourceURL, nClient);
 				DeleteNetRevcBaseClient(nClient);
				return;
			}

			//����¼���ļ��㲥
			pMediaSource = CreateReplayClient(szMediaSourceURL,&nReplayClient);
			if (pMediaSource == NULL)
			{
				sprintf(szResponseBuffer, "RTSP/1.0 404 Not FOUND\r\nServer: %s\r\nCSeq: %s\r\n\r\n", MediaServerVerson, szCSeq);
				nSendRet = XHNetSDK_Write(nClient, (unsigned char*)szResponseBuffer, strlen(szResponseBuffer), 1);
				WriteLog(Log_Debug, "RTSP����������¼���ļ��㲥ʧ�� %s  nClient = %llu ", szMediaSourceURL, nClient);
				DeleteNetRevcBaseClient(nClient);
				return;
			}

			m_nXHRtspURLType = XHRtspURLType_RecordPlay;
   		}
 
		//����ý��Դ��Ϣ
		memcpy((char*)&mediaCodecInfo, (char*)&pMediaSource->m_mediaCodecInfo, sizeof(MediaCodecInfo));

		RtspSDPContentStruct sdpContent;
		bool bGetSDP = false;
		if (ABL_MediaServerPort.nG711ConvertAAC == 0)
			bGetSDP = pMediaSource->GetRtspSDPContent(&sdpContent);
		else
			bGetSDP = false;

		//��Ƶת��
		if (pMediaSource->H265ConvertH264_enable)
			bGetSDP = false;

		//����sdp ��Ϣ 
		if (!GetRtspSDPFromMediaStreamSource(sdpContent,false))
		{
			sprintf(szResponseBuffer, "RTSP/1.0 404 Not FOUND\r\nServer: %s\r\nCSeq: %s\r\n\r\n", MediaServerVerson, szCSeq);
			nSendRet = XHNetSDK_Write(nClient, (unsigned char*)szResponseBuffer, strlen(szResponseBuffer), 1);
			WriteLog(Log_Debug, "ý���� %s ������ ,׼��ɾ�� nClient =%llu ", szMediaSourceURL, nClient);

			DeleteNetRevcBaseClient(nClient);
			return;
		}
		sprintf(szResponseBuffer, "RTSP/1.0 200 OK\r\nContent-Length: %d\r\nContent-Type: application/sdp\r\nServer: %s\r\nCSeq: %s\r\nSession: ABLMeidaServer_%llu\r\nx-Accept-Dynamic-Rate: 1\r\nx-Accept-Retransmit: our-retransmit\r\n\r\n%s", strlen(szRtspSDPContent), MediaServerVerson, szCSeq, currentSession, szRtspSDPContent);
		nSendRet = XHNetSDK_Write(nClient, (unsigned char*)szResponseBuffer, strlen(szResponseBuffer), 1);
		if (nSendRet != 0)
		{
			DeleteNetRevcBaseClient(nClient);
			return;
		}

		//WriteLog(Log_Debug, "�ظ�SDP\r\n%s", szRtspSDPContent);
 	}
	else if (memcmp(pRecvData, "SETUP", 5) == 0 && strstr((char*)pRecvData, "\r\n\r\n") != NULL)
	{
		memset(szCSeq, 0x00, sizeof(szCSeq));
		memset(szTransport, 0x00, sizeof(szTransport));
	 	GetFieldValue("CSeq", szCSeq);
		GetFieldValue("Transport", szTransport);

		if ((strlen(szTransport) > 0 && strstr(szTransport, "RTP/AVP/TCP") != NULL) )
		{//TCP
			if (netBaseNetType == NetBaseNetType_RtspServerSendPush)
			{
			  m_RtspNetworkType = RtspNetworkType_TCP;
 			}
		}else if ( (strlen(szTransport) > 0 && strstr(szTransport, "RTP/AVP/UDP") != NULL) || (strlen(szTransport) > 0 && strstr(szTransport, "RTP/AVP") != NULL))
		{//UDP
			if (netBaseNetType == NetBaseNetType_RtspServerSendPush)
			{//udp �㲥
				m_RtspNetworkType = RtspNetworkType_UDP;
				GetAVClientPortByTranspot(szTransport);
			}
			else if (netBaseNetType == NetBaseNetType_RtspServerRecvPush)
			{//udp ��ʽ����
				sprintf(szResponseBuffer, "RTSP/1.0 461 Unsupported transport\r\nServer: %s\r\nCSeq: %s\r\n\r\n", MediaServerVerson, szCSeq);
				nSendRet = XHNetSDK_Write(nClient, (unsigned char*)szResponseBuffer, strlen(szResponseBuffer), 1);
				WriteLog(Log_Debug, "rtsp������֧��UDP��ʽ��ֻ֧��TCP��ʽ���� %s ", RtspProtectArray[RtspProtectArrayOrder].szRtspURL);

				DeleteNetRevcBaseClient(nClient);
				return;
			}
		}
 
		if(netBaseNetType == NetBaseNetType_RtspServerRecvPush)//rtsp ��tcp��ʽ����
		  sprintf(szResponseBuffer, "RTSP/1.0 200 OK\r\nServer: %s\r\nCSeq: %s\r\nCache-Control: no-cache\r\nSession: ABLMediaServer_%llu\r\nDate: Tue, 13 Nov 2019 02:49:48 GMT\r\nExpires: Tue, 13 Nov 2018 02:49:48 GMT\r\nTransport: %s\r\n\r\n", MediaServerVerson, szCSeq, currentSession, szTransport);
		else if (netBaseNetType == NetBaseNetType_RtspServerSendPush)
		{//rtps ���� 
			if (m_RtspNetworkType == RtspNetworkType_TCP)
			{
 				sprintf(szResponseBuffer, "RTSP/1.0 200 OK\r\nServer: %s\r\nCSeq: %s\r\nCache-Control: no-cache\r\nSession: ABLMediaServer_%llu\r\nDate: Tue, 13 Nov 2019 02:49:48 GMT\r\nExpires: Tue, 13 Nov 2018 02:49:48 GMT\r\nTransport: %s\r\n\r\n", MediaServerVerson, szCSeq, currentSession, szTransport);
 			}
			else if (m_RtspNetworkType == RtspNetworkType_UDP)
			{
				int nRet1 = 0 , nRet2 = 0;
				while (true)
				{
				  nRtpPort += 2;
				  if (nSetupOrder == 1)
				  {//��һ��Setup
					  if (strlen(mediaCodecInfo.szVideoName) > 0)
					  {//����Ƶ
						  nRet1 = XHNetSDK_BuildUdp(NULL, nRtpPort, NULL, &nServerVideoUDP[0], onread, 1);//rtp
						  nRet2 = XHNetSDK_BuildUdp(NULL, nRtpPort + 1, NULL, &nServerVideoUDP[1], onread, 1);//rtcp
						  if (nRet1 == 0 && nRet2 == 0)
							  break;
						  if (nRet1 != 0)
							  XHNetSDK_DestoryUdp(nServerVideoUDP[0]);
						  if (nRet2 != 0)
							  XHNetSDK_DestoryUdp(nServerVideoUDP[1]);
					  }
					  else
					  {//ֻ����Ƶ
						  nRet1 = XHNetSDK_BuildUdp(NULL, nRtpPort, NULL, &nServerAudioUDP[0], onread, 1);//rtp
						  nRet2 = XHNetSDK_BuildUdp(NULL, nRtpPort + 1, NULL, &nServerAudioUDP[1], onread, 1);//rtcp
						  if (nRet1 == 0 && nRet2 == 0)
							  break;
						  if (nRet1 != 0)
							  XHNetSDK_DestoryUdp(nServerAudioUDP[0]);
						  if (nRet2 != 0)
							  XHNetSDK_DestoryUdp(nServerAudioUDP[1]);
					  }
				  }
				  else if (nSetupOrder == 2)
				  {//��Ƶ
					  nRet1 = XHNetSDK_BuildUdp(NULL, nRtpPort , NULL, &nServerAudioUDP[0], onread, 1);//rtp
					  nRet2 = XHNetSDK_BuildUdp(NULL, nRtpPort + 1, NULL, &nServerAudioUDP[1], onread, 1);//rtcp
					  if (nRet1 == 0 && nRet2 == 0)
						  break;
					  if (nRet1 != 0)
						  XHNetSDK_DestoryUdp(nServerAudioUDP[0]);
					  if (nRet2 != 0)
						  XHNetSDK_DestoryUdp(nServerAudioUDP[1]);
				  }
				}

				if (nSetupOrder == 1)
				{//��һ��Setup
					if (strlen(mediaCodecInfo.szVideoName) > 0)
					{//����Ƶ
					  nVideoServerPort[0] = nRtpPort;
					  nVideoServerPort[1] = nRtpPort + 1;
					  sprintf(szResponseBuffer, "RTSP/1.0 200 OK\r\nServer: %s\r\nCSeq: %s\r\nCache-Control: no-cache\r\nSession: ABLMediaServer_%llu\r\nDate: Tue, 13 Nov 2019 02:49:48 GMT\r\nExpires: Tue, 13 Nov 2018 02:49:48 GMT\r\nTransport: RTP/AVP;unicast;client_port=%d-%d;server_port=%d-%d\r\n\r\n", MediaServerVerson, szCSeq, currentSession, nVideoClientPort[0], nVideoClientPort[1], nVideoServerPort[0], nVideoServerPort[1]);
					}
					else
					{//ֻ����Ƶ 
						nAudiServerPort[0] = nRtpPort;
						nAudiServerPort[1] = nRtpPort + 1;
						sprintf(szResponseBuffer, "RTSP/1.0 200 OK\r\nServer: %s\r\nCSeq: %s\r\nCache-Control: no-cache\r\nSession: ABLMediaServer_%llu\r\nDate: Tue, 13 Nov 2019 02:49:48 GMT\r\nExpires: Tue, 13 Nov 2018 02:49:48 GMT\r\nTransport: RTP/AVP;unicast;client_port=%d-%d;server_port=%d-%d\r\n\r\n", MediaServerVerson, szCSeq, currentSession, nAudioClientPort[0], nAudioClientPort[1], nAudiServerPort[0], nAudiServerPort[1]);
					}
				}
				else if (nSetupOrder == 2)
				{//����Ƶ
					nAudiServerPort[0] = nRtpPort;
					nAudiServerPort[1] = nRtpPort + 1;
					sprintf(szResponseBuffer, "RTSP/1.0 200 OK\r\nServer: %s\r\nCSeq: %s\r\nCache-Control: no-cache\r\nSession: ABLMediaServer_%llu\r\nDate: Tue, 13 Nov 2019 02:49:48 GMT\r\nExpires: Tue, 13 Nov 2018 02:49:48 GMT\r\nTransport: RTP/AVP;unicast;client_port=%d-%d;server_port=%d-%d\r\n\r\n", MediaServerVerson, szCSeq, currentSession, nAudioClientPort[0], nAudioClientPort[1], nAudiServerPort[0], nAudiServerPort[1]);
				}

				//rtp��rtcp �˿� ��ת
				if (nRtpPort >= 64000)
					nRtpPort = 55000;
 			}
		}
		nSendRet = XHNetSDK_Write(nClient, (unsigned char*)szResponseBuffer, strlen(szResponseBuffer), 1);
		if (nSendRet != 0)
			pDisconnectBaseNetFifo.push((unsigned char*)&nClient, sizeof(nClient));
  	}
	else if (memcmp(pRecvData, "RECORD", 6) == 0 && strstr((char*)pRecvData, "\r\n\r\n") != NULL)
	{
   		GetFieldValue("CSeq", szCSeq);

		sprintf(szResponseBuffer, "RTSP/1.0 200 OK\r\nServer: %s\r\nCSeq: %s\r\nSession: ABLMediaServer_%llu\r\nRTP-Info: %s\r\n\r\n", MediaServerVerson, szCSeq, currentSession, szCurRtspURL);
		nSendRet = XHNetSDK_Write(nClient, (unsigned char*)szResponseBuffer, strlen(szResponseBuffer), 1);
		if (nSendRet != 0)
			pDisconnectBaseNetFifo.push((unsigned char*)&nClient, sizeof(nClient)); 
	}
	else if (memcmp(pRecvData, "PLAY", 4) == 0 && strstr((char*)pRecvData, "\r\n\r\n") != NULL)
	{
		GetFieldValue("CSeq", szCSeq);

		sprintf(szResponseBuffer, "RTSP/1.0 200 OK\r\nServer: %s\r\nCSeq: %s\r\nSession: ABLMediaServer_%llu\r\nRTP-Info: %s\r\n\r\n", MediaServerVerson, szCSeq, currentSession, szCurRtspURL);
		nSendRet = XHNetSDK_Write(nClient, (unsigned char*)szResponseBuffer, strlen(szResponseBuffer), 1);
		if (nSendRet != 0)
		{
 			pDisconnectBaseNetFifo.push((unsigned char*)&nClient, sizeof(nClient));
			return;
		}

		//û����ͣ
		m_bPauseFlag = false;

		if (netBaseNetType == NetBaseNetType_RtspServerSendPush && pMediaSource != NULL)
		{//ת��rtsp ý���� 
			m_videoFifo.InitFifo(MaxLiveingVideoFifoBufferLength);
			m_audioFifo.InitFifo(MaxLiveingAudioFifoBufferLength);
			pMediaSource->AddClientToMap(nClient);//ý�忽��
			pMediaSendThreadPool->AddClientToThreadPool(nClient);//ý�巢���߳�

			if(nReplayClient > 0 )
			{
			  boost::shared_ptr<CNetRevcBase> pBasePtr = GetNetRevcBaseClient(nReplayClient);
			  if (pBasePtr)
			  {
				CReadRecordFileInput* pReplayPtr = (CReadRecordFileInput*)pBasePtr.get();
				pReplayPtr->UpdatePauseFlag(false);//��������

				nRtspPlayCount++;

				//���µ㲥�ٶ�			 
				char szScale[256] = { 0 };
				GetFieldValue("Scale", szScale);
				if (strlen(szScale) > 0)
				{
					m_nScale = atoi(szScale);
					if (nRtspPlayCount == 1) //ȷ����¼��طţ�����¼������
						m_rtspPlayerType = RtspPlayerType_RecordDownload;
					else
						m_rtspPlayerType = RtspPlayerType_RecordReplay;

					pReplayPtr->UpdateReplaySpeed(atof(szScale), m_rtspPlayerType);
				}

				//ʵ���϶�����
				char   szRange[256] = { 0 };
				string strRange;
				int    nPos1 = 0, nPos2 = 0;
				char   szRangeValue[256] = { 0 };
				GetFieldValue("Range", szRange);
				if (strlen(szRange) > 0)
				{
					strRange = szRange;
					nPos1 = strRange.find("npt=", 0);
					nPos2 = strRange.find("-", 0);
					if (nPos1 >= 0 && nPos2 > 0)
					{
						memcpy(szRangeValue, szRange + nPos1 + 4, nPos2 - nPos1 - 4);
						if (atoi(szRangeValue) > 0)
							pReplayPtr->ReaplyFileSeek(atoi(szRangeValue));
					}
				}
			}
		  }
 		}
	}
	else if (memcmp(pRecvData, "PAUSE ", 6) == 0 && strstr((char*)pRecvData, "\r\n\r\n") != NULL)
	{
		m_bPauseFlag = true ;//��ͣ
 
		boost::shared_ptr<CNetRevcBase> pBasePtr = GetNetRevcBaseClient(nReplayClient);
		if (pBasePtr)
		{
			CReadRecordFileInput* pReplayPtr = (CReadRecordFileInput*)pBasePtr.get();
			pReplayPtr->UpdatePauseFlag(true);//��ͣ����
		}
		GetFieldValue("CSeq", szCSeq);

		sprintf(szResponseBuffer, "RTSP/1.0 200 OK\r\nServer: %s\r\nCSeq: %s\r\nSession: ABLMediaServer_%llu\r\nRTP-Info: %s\r\n\r\n", MediaServerVerson, szCSeq, currentSession, szCurRtspURL);
		nSendRet = XHNetSDK_Write(nClient, (unsigned char*)szResponseBuffer, strlen(szResponseBuffer), 1);
		if (nSendRet != 0)
		{
			pDisconnectBaseNetFifo.push((unsigned char*)&nClient, sizeof(nClient));
			return;
		}

		WriteLog(Log_Debug, "�յ��Ͽ� ��ͣ �������ִ����ͣ nClient = %llu , nReplayClient = %llu ", nClient, nReplayClient);
 	}
	else if (memcmp(pRecvData, "TEARDOWN", 8) == 0 && strstr((char*)pRecvData, "\r\n\r\n") != NULL)
	{
		WriteLog(Log_Debug, "�յ��Ͽ� TEARDOWN �������ִ��ɾ�� nClient = %llu ", nClient);
		bRunFlag = false;
		DeleteNetRevcBaseClient(nClient);
	}
	else if (memcmp(pRecvData, "GET_PARAMETER", 13) == 0 && strstr((char*)pRecvData, "\r\n\r\n") != NULL)
	{
		memset(szCSeq, 0x00, sizeof(szCSeq));
		GetFieldValue("CSeq", szCSeq);

		sprintf(szResponseBuffer, "RTSP/1.0 200 OK\r\nCSeq: %s\r\nPublic: %s\r\nx-Timeshift_Range: clock=20100318T021915.84Z-20100318T031915.84Z\r\nx-Timeshift_Current: clock=20100318T031915.84Z\r\n\r\n", szCSeq, RtspServerPublic);
		nSendRet = XHNetSDK_Write(nClient, (unsigned char*)szResponseBuffer, strlen(szResponseBuffer), 1);
		if (nSendRet != 0)
			pDisconnectBaseNetFifo.push((unsigned char*)&nClient, sizeof(nClient));

	}
	else
	{
		bIsInvalidConnectFlag = true; //ȷ��Ϊ�Ƿ����� 
		WriteLog(Log_Debug, "�Ƿ���rtsp �������ִ��ɾ�� nClient = %llu ",nClient);
		DeleteNetRevcBaseClient(nClient);
	}
}

//��sdp��Ϣ�л�ȡ��Ƶ����Ƶ��ʽ��Ϣ ,������Щ��Ϣ����rtp�����rtp��� 
bool   CNetRtspServer::GetMediaInfoFromRtspSDP()
{
	//��sdpװ�������
	string strSDPTemp = szRtspContentSDP;
	char   szTemp[128] = { 0 };
	int    nPos1 = strSDPTemp.find("m=video", 0);
	int    nPos2 = strSDPTemp.find("m=audio", 0);
	int    nPos3;
	memset(szVideoSDP, 0x00, sizeof(szVideoSDP));
	memset(szAudioSDP, 0x00, sizeof(szAudioSDP));
	memset(szVideoName, 0x00, sizeof(szVideoName));
	memset(szAudioName, 0x00, sizeof(szAudioName));
	if (nPos1 >= 0 && nPos2 > 0)
	{
		memcpy(szVideoSDP, szRtspContentSDP + nPos1, nPos2 - nPos1);
		memcpy(szAudioSDP, szRtspContentSDP + nPos2, strlen(szRtspContentSDP) - nPos2);

		sipParseV.ParseSipString(szVideoSDP);
		sipParseA.ParseSipString(szAudioSDP);
	}
	else if (nPos1 >= 0 && nPos2 < 0)
	{
		memcpy(szVideoSDP, szRtspContentSDP + nPos1, strlen(szRtspContentSDP) - nPos1);
		sipParseV.ParseSipString(szVideoSDP);
	}
	else if (nPos2 >= 0)
	{
		memcpy(szAudioSDP, szRtspContentSDP+nPos2,strlen(szRtspContentSDP) - nPos2);
		sipParseA.ParseSipString(szAudioSDP);
	}
	else
		return false;

	//��ȡ��Ƶ��������
	string strVideoName;
	char   szTemp2[64] = { 0 };
	char   szTemp3[64] = { 0 };
	if (sipParseV.GetSize() > 0)
	{
		if (sipParseV.GetFieldValue("a=rtpmap", szTemp))
		{
			strVideoName = szTemp;
			nPos1 = strVideoName.find(" ", 0);
			if (nPos1 > 0)
			{
				memcpy(szTemp2, szTemp, nPos1);
				nVideoPayload = atoi(szTemp2);
			}
			nPos2 = strVideoName.find("/", 0);
			if (nPos1 > 0 && nPos2 > 0)
			{
				memcpy(szVideoName, szTemp + nPos1 + 1, nPos2 - nPos1 - 1);

				//תΪ��С
				strVideoName = szVideoName;
				to_upper(strVideoName);
				strcpy(szVideoName, strVideoName.c_str());
			}

			WriteLog(Log_Debug, "nClient = %llu , ��SDP�У���ȡ������Ƶ��ʽΪ %s , payload = %d ��",nClient,szVideoName, nVideoPayload);
		}
	}

	  //��ȡ��Ƶ��Ϣ
	  if (sipParseA.GetSize() > 0)
	  {
		memset(szTemp2, 0x00, sizeof(szTemp2));
		memset(szTemp, 0x00, sizeof(szTemp));
		if (sipParseA.GetFieldValue("a=rtpmap", szTemp))
		{
			strVideoName = szTemp;
			nPos1 = strVideoName.find(" ", 0);
			if (nPos1 > 0)
			{
				memcpy(szTemp2, szTemp, nPos1);
				nAudioPayload = atoi(szTemp2);
			}
			nPos2 = strVideoName.find("/", 0);
			if (nPos1 > 0 && nPos2 > 0)
			{
				memcpy(szAudioName, szTemp + nPos1 + 1, nPos2 - nPos1 - 1);

				//תΪ��С
				string strName = szAudioName;
				to_upper(strName);
				strcpy(szAudioName, strName.c_str());

				//�ҳ�����Ƶ�ʡ�ͨ����
				nPos3 = strVideoName.find("/", nPos2 + 1);
				memset(szTemp2, 0x00, sizeof(szTemp2));
				if (nPos3 > 0)
				{
					memcpy(szTemp2, szTemp + nPos2 + 1, nPos3 - nPos2 - 1);
					nSampleRate = atoi(szTemp2);

					memcpy(szTemp3, szTemp + nPos3 + 1, strlen(szTemp) - nPos3 - 1);
					nChannels = atoi(szTemp3);//ͨ������
				}
				else
				{
					memcpy(szTemp2, szTemp + nPos2 + 1, strlen(szTemp) - nPos2 - 1);
					nSampleRate = atoi(szTemp2);
					nChannels = 1;
				}

				//��ֹ��Ƶͨ�������� 
				if (nChannels > 2)
					nChannels = 1;
			}
		}
		else
		{//ffmpeg g711a\g711u m=audio 0 RTP/AVP 0 
			sipParseA.GetFieldValue("m=audio", szTemp);
			if (strstr(szTemp, "RTP/AVP 0") != NULL)
			{
				nSampleRate = 8000;
				nChannels = 1;
				strcpy(szAudioName, "PCMU");
			}
			else if (strstr(szTemp, "RTP/AVP 8") != NULL)
			{
				nSampleRate = 8000;
				nChannels = 1;
				strcpy(szAudioName, "PCMA");
			}
		}

		if (strcmp(szAudioName, "PCMA") == 0)
		{
			strcpy(szAudioName, "G711_A");
		}
		else if (strcmp(szAudioName, "PCMU") == 0)
		{
			strcpy(szAudioName, "G711_U");
		}
		else if (strcmp(szAudioName, "MPEG4-GENERIC") == 0)
		{
			strcpy(szAudioName, "AAC");
		}
		else if (strstr(szAudioName, "G726") != NULL)
		{
			strcpy(szAudioName, "G726LE");
		}
		else
			strcpy(szAudioName, "NONE");

		WriteLog(Log_Debug, "��SDP�У���ȡ������Ƶ��ʽΪ %s ,nChannels = %d ,SampleRate = %d , payload = %d ��", szAudioName, nChannels, nSampleRate, nAudioPayload);
	}

  if (!(strlen(szVideoName) > 0 || strlen(szAudioName) > 0))
	 return false;
   else 
      return true;
}

//����rtp����־ 
bool CNetRtspServer::FindRtpPacketFlag()
{
	bool bFindFlag = false;

	unsigned char szRtpFlag[2] = { 0x24, 0x00 };
	int  nPos = 0;

	if (netDataCacheLength > 2)
	{
		for (int i = nNetStart; i < nNetEnd; i++)
		{
			if (memcmp(netDataCache + i, szRtpFlag, 2) == 0)
			{
				nPos = i;
				bFindFlag = true;
				break;
			}
		}
	}

	//�ҵ���־�����¼�����㣬������ 
	if (bFindFlag)
	{
		nNetStart = nPos;
		netDataCacheLength = nNetEnd - nNetStart;
		WriteLog(Log_Debug, "CNetClientRecvRtsp = %X ,�ҵ�RTPλ�ã� nNetStart = %d , nNetEnd = %d , netDataCacheLength = %d ", this, nNetStart, nNetEnd, netDataCacheLength);
	}

	return bFindFlag;
}

//������������
int CNetRtspServer::ProcessNetData()
{
	std::lock_guard<std::mutex> lock(netDataLock);

	bExitProcessFlagArray[2] = false; 
	tRtspProcessStartTime = GetTickCount64();
	int nRet;
	uint32_t nReadLength = 4;

	while (!bIsInvalidConnectFlag && bRunFlag && netDataCacheLength > 4)
	{
		data_Length = 0;
		memcpy((unsigned char*)&rtspHead, netDataCache + nNetStart, sizeof(rtspHead));
		if (true)
		{
			if (rtspHead.head == '$')
			{//rtp ����
				nNetStart += 4;
				netDataCacheLength -= 4;
				nRtpLength = nReadLength = ntohs(rtspHead.Length);

				if (nRtpLength > netDataCacheLength)
				{//ʣ��rtp���Ȳ�����ȡ����Ҫ�˳����ȴ���һ�ζ�ȡ
					bExitProcessFlagArray[2] = true;
					nNetStart -= 4;
					netDataCacheLength += 4;
					return 0;
				}

				if (nRtpLength > 0 && nRtpLength <= 65535 )
				{
					if (rtspHead.chan == 0x00)
					{
						if(hRtpHandle[0] != 0 )//����Ƶ
 						  rtp_depacket_input(hRtpHandle[0], netDataCache + nNetStart, nRtpLength);
						else //ֻ����Ƶ
						  rtp_depacket_input(hRtpHandle[1], netDataCache + nNetStart, nRtpLength);

						if(!bUpdateVideoFrameSpeedFlag)
						 {//������ƵԴ��֡�ٶ�
							if (hRtpHandle[0] != 0)
							{
							   int nVideoSpeed = CalcVideoFrameSpeed(netDataCache + nNetStart, nRtpLength);
							   if (nVideoSpeed > 0 && pMediaSource != NULL)
							  {
								 bUpdateVideoFrameSpeedFlag = true;
								 WriteLog(Log_Debug, "nClient = %llu , ������ƵԴ %s ��֡�ٶȳɹ�����ʼ�ٶ�Ϊ%d ,���º���ٶ�Ϊ%d, ", nClient, pMediaSource->m_szURL, pMediaSource->m_mediaCodecInfo.nVideoFrameRate, nVideoSpeed);
								 pMediaSource->UpdateVideoFrameSpeed(nVideoSpeed, netBaseNetType);
							   }
							}
							else
							{//û����Ƶ
								bUpdateVideoFrameSpeedFlag = true;
								WriteLog(Log_Debug, "nClient = %llu , ������ƵԴ %s ��֡�ٶȳɹ�����ʼ�ٶ�Ϊ%d ,û����Ƶ��ʹ��Ĭ���ٶ�Ϊ%d, ", nClient, pMediaSource->m_szURL, pMediaSource->m_mediaCodecInfo.nVideoFrameRate, 25);
								pMediaSource->UpdateVideoFrameSpeed(25, netBaseNetType);
							}
 						 } 
						//if(nPrintCount % 200 == 0)
 						//  printf("this =%X, Video Length = %d \r\n",this, nReadLength);
					}
					else if (rtspHead.chan == 0x02)
					{
						rtp_depacket_input(hRtpHandle[1], netDataCache + nNetStart , nRtpLength);

						//if(nPrintCount % 100 == 0 )
						//	WriteLog(Log_Debug, "this =%X ,Audio Length = %d ",this,nReadLength);

						nPrintCount ++;
					}
					else if (rtspHead.chan == 0x01)
					{//�յ�RTCP������Ҫ�ظ�rtcp�����
						SendRtcpReportData(nVideoSSRC, rtspHead.chan);
						//WriteLog(Log_Debug, "this =%X ,�յ� ��Ƶ ��RTCP������Ҫ�ظ�rtcp�������netBaseNetType = %d  �յ�RCP������ = %d ", this, netBaseNetType, nReadLength);
					}
					else if (rtspHead.chan == 0x03)
					{//�յ�RTCP������Ҫ�ظ�rtcp�����
						SendRtcpReportData(nVideoSSRC+1, rtspHead.chan);
						//WriteLog(Log_Debug, "this =%X ,�յ� ��Ƶ ��RTCP������Ҫ�ظ�rtcp�������netBaseNetType = %d  �յ�RCP������ = %d ", this, netBaseNetType, nReadLength);
					}

					bExitProcessFlagArray[2] = true;
					nNetStart          += nRtpLength;
					netDataCacheLength -= nRtpLength;
				}
				else
				{
					WriteLog(Log_Debug, "ReadDataFunc() ,��δ��ȡ��rtp���� ! ABLRtspChan = %llu ", nClient);
					bExitProcessFlagArray[2] = true;
					DeleteNetRevcBaseClient(nClient);
					return -1;
				}
			}
			else
			{//rtsp ����
				if (FindRtpPacketFlag() == true)
				{//rtp ���� ,����
					bExitProcessFlagArray[2] = true;
					return 0;
				}

				if (!ReadRtspEnd())
				{
					WriteLog(Log_Debug, "ReadDataFunc() ,��δ��ȡ��rtsp (1)���� ! nClient = %llu ", nClient);
					bExitProcessFlagArray[2] = true;
					DeleteNetRevcBaseClient(nClient);
					return  -1;
				}
				else
				{
					//���rtspͷ
					if (FindHttpHeadEndFlag() > 0)
						FillHttpHeadToStruct();

					if (nContentLength > 0)
					{
						nReadLength = nContentLength;

						//�����ContentLength ����Ҫ������ContentLength�������ٽ��ж�ȡ 
						if (netDataCacheLength < nContentLength)
						{//ʣ�µ����ݲ��� ContentLength ,��Ҫ�����ƶ��Ѿ���ȡ���ֽ��� data_Length  
							nNetStart           -= data_Length ;
							netDataCacheLength  += data_Length ;
							bExitProcessFlagArray[2] = true;
							WriteLog(Log_Debug, "ReadDataFunc (), RTSP �� Content-Length ��������δ��������  nClient = %llu", nClient);
							return 0;
						}

						nRet = XHNetSDKRead(nClient, data_ + data_Length, &nReadLength, true, true);
						if (nRet != 0 || nReadLength != nContentLength)
						{
							WriteLog(Log_Debug, "ReadDataFunc() ,��δ��ȡ��rtsp (2)���� ! nClient = %llu", nClient);
							bExitProcessFlagArray[2] = true;
							DeleteNetRevcBaseClient(nClient);
							return -1;
						}
						else
						{
							data_Length += nContentLength;
						}
					}

					data_[data_Length] = 0x00;
					InputRtspData(data_, data_Length);
				}
			}
		}
		else
		{
			WriteLog(Log_Debug, "CNetRtspServer= %X  , ProcessNetData() ,��δ��ȡ������ ! , nClient = %llu", this, nClient);
			bExitProcessFlagArray[2] = true;
			DeleteNetRevcBaseClient(nClient);
			return -1;
		}

		if (GetTickCount64() - tRtspProcessStartTime > 16000)
		{
			WriteLog(Log_Debug, "CNetRtspServer= %X  , ProcessNetData() ,RTSP ���紦����ʱ ! , nClient = %llu", this, nClient);
			bExitProcessFlagArray[2] = true;
			DeleteNetRevcBaseClient(nClient);
			return -1;
		}
	}

	bExitProcessFlagArray[2] = true;
	return 0;
}

//����rtcp �����
void  CNetRtspServer::SendRtcpReportData(unsigned int nSSRC, int nChan)
{
	memset(szRtcpSRBuffer, 0x00, sizeof(szRtcpSRBuffer));
	rtcpSR.BuildRtcpPacket(szRtcpSRBuffer, rtcpSRBufferLength, nSSRC);

	ProcessRtcpData((char*)szRtcpSRBuffer, rtcpSRBufferLength, nChan);
}

//����rtcp ����� ���ն�
void  CNetRtspServer::SendRtcpReportDataRR(unsigned int nSSRC, int nChan)
{
	memset(szRtcpSRBuffer, 0x00, sizeof(szRtcpSRBuffer));
	rtcpRR.BuildRtcpPacket(szRtcpSRBuffer, rtcpSRBufferLength, nSSRC);

	ProcessRtcpData((char*)szRtcpSRBuffer, rtcpSRBufferLength, nChan);
}

void  CNetRtspServer::ProcessRtcpData(char* szRtcpData, int nDataLength, int nChan)
{
	std::lock_guard<std::mutex> lock(MediaSumRtpMutex);

	szRtcpDataOverTCP[0] = '$';
	szRtcpDataOverTCP[1] = nChan;
	unsigned short nRtpLen = htons(nDataLength);
	memcpy(szRtcpDataOverTCP + 2, (unsigned char*)&nRtpLen, sizeof(nRtpLen));

	memcpy(szRtcpDataOverTCP + 4, szRtcpData, nDataLength);
	XHNetSDK_Write(nClient, szRtcpDataOverTCP, nDataLength + 4, 1);
}

//���͵�һ������
int CNetRtspServer::SendFirstRequst()
{
	return 0;
}

//����m3u8�ļ�
bool  CNetRtspServer::RequestM3u8File()
{
	return true;
}

int CNetRtspServer::sdp_h264_load(uint8_t* data, int bytes, const char* config)
{
	int n, len, off;
	const char* p, *next;
	const uint8_t startcode[] = { 0x00, 0x00, 0x00, 0x01 };

	off = 0;
	p = config;
	while (p)
	{
		next = strchr(p, ',');
		len = next ? (int)(next - p) : (int)strlen(p);
		if (off + (len + 3) / 4 * 3 + (int)sizeof(startcode) > bytes)
			return -1; // don't have enough space

		memcpy(data + off, startcode, sizeof(startcode));
		n = (int)base64_decode(data + off + sizeof(startcode), p, len);
		assert(n <= (len + 3) / 4 * 3);
		off += n + sizeof(startcode);

		p = next ? next + 1 : NULL;
	}

	return off;
}

//�� SDP�л�ȡ  SPS��PPS ��Ϣ
bool  CNetRtspServer::GetSPSPPSFromDescribeSDP()
{
	m_bHaveSPSPPSFlag = false;
	int  nPos1, nPos2;
	char  szSprop_Parameter_Sets[512] = { 0 };

	m_nSpsPPSLength = 0;
	string strSDPTemp = szRtspContentSDP;
	nPos1 = strSDPTemp.find("sprop-parameter-sets=", 0);
	memset(m_szSPSPPSBuffer, 0x00, sizeof(m_szSPSPPSBuffer));
	nPos2 = strSDPTemp.find("\r\n", nPos1 + 1);

	if (nPos1 > 0 && nPos2 > 0)
	{
		memcpy(szSprop_Parameter_Sets, szRtspContentSDP + nPos1, nPos2 - nPos1 + 2);
		strSDPTemp = szSprop_Parameter_Sets;
		nPos1 = strSDPTemp.find("sprop-parameter-sets=", 0);

		nPos2 = strSDPTemp.find(";", nPos1 + 1);
		if (nPos2 > nPos1)
		{//���滹�б����
			memcpy(m_szSPSPPSBuffer, szSprop_Parameter_Sets + nPos1 + strlen("sprop-parameter-sets="), nPos2 - nPos1 - strlen("sprop-parameter-sets="));
		}
		else
		{//����û�б����
			nPos2 = strSDPTemp.find("\r\n", nPos1 + 1);
			if (nPos2 > nPos1)
				memcpy(m_szSPSPPSBuffer, szSprop_Parameter_Sets + nPos1 + strlen("sprop-parameter-sets="), nPos2 - nPos1 - strlen("sprop-parameter-sets="));
		}
	}

	if (strlen(m_szSPSPPSBuffer) > 0)
	{
		m_nSpsPPSLength = sdp_h264_load((unsigned char*)m_pSpsPPSBuffer, sizeof(m_pSpsPPSBuffer), m_szSPSPPSBuffer);
		m_bHaveSPSPPSFlag = true;
	}

	return m_bHaveSPSPPSFlag;
}

bool  CNetRtspServer::GetAVClientPortByTranspot(char* szTransport)
{
	int nPos1 = 0, nPos2 = 0;
	string strTransport = szTransport;
	char   szPort1[64] = { 0 };
	char   szPort2[64] = { 0 };
	nPos1 = strTransport.find("client_port=", 0);
	if (nPos1 > 0)
	{
		nPos2 = strTransport.find("-", nPos1);
		if (nPos2 > 0)
		{
			memcpy(szPort1, szTransport + nPos1 + 12, nPos2 - nPos1 - 12);
			memcpy(szPort2, szTransport + nPos2 + 1, strlen(szTransport) - nPos2 - 1);
			if (nSetupOrder == 0)
			{//��һ��Setup
				if (strlen(mediaCodecInfo.szVideoName) > 0)
				{//����Ƶ
					nVideoClientPort[0] = atoi(szPort1);
					nVideoClientPort[1] = atoi(szPort2);
					addrClientVideo[0].sin_port = htons(nVideoClientPort[0]);
					addrClientVideo[1].sin_port = htons(nVideoClientPort[1]);
				}
				else
				{//ֻ����Ƶ
					nAudioClientPort[0] = atoi(szPort1);
					nAudioClientPort[1] = atoi(szPort2);
					addrClientAudio[0].sin_port = htons(nAudioClientPort[0]);
					addrClientAudio[1].sin_port = htons(nAudioClientPort[1]);
				}
			}
			else
			{//����Ƶ
				nAudioClientPort[0] = atoi(szPort1);
				nAudioClientPort[1] = atoi(szPort2);
				addrClientAudio[0].sin_port = htons(nAudioClientPort[0]);
				addrClientAudio[1].sin_port = htons(nAudioClientPort[1]);
			}

			nSetupOrder ++;
		}
	}
	
	if(nPos1 > 0 && nPos2 > 0 ) 
		return true ;
	else 
	    return false;
}