/*
���ܣ�
    ������ GB28181 Rtp ����������UDP��TCPģʽ  
 	���� �������  �������귢�͵�ͬʱҲ֧�ֹ�����գ�    2023-05-19
����    2021-08-15
����    �޼��ֵ�
QQ      79941308
E-Mail  79941308@qq.com
*/

#include "stdafx.h"
#include "NetGB28181RtpClient.h"

extern bool                                  DeleteNetRevcBaseClient(NETHANDLE CltHandle);
extern boost::shared_ptr<CMediaStreamSource> CreateMediaStreamSource(char* szUR, uint64_t nClient, MediaSourceType nSourceType, uint32_t nDuration, H265ConvertH264Struct  h265ConvertH264Struct);
extern boost::shared_ptr<CMediaStreamSource> GetMediaStreamSource(char* szURL);
extern bool                                  DeleteMediaStreamSource(char* szURL);
extern bool                                  DeleteClientMediaStreamSource(uint64_t nClient);

extern CMediaSendThreadPool*                 pMediaSendThreadPool;
extern CMediaFifo                            pDisconnectBaseNetFifo; //�������ѵ����� 
extern char                                  ABL_MediaSeverRunPath[256]; //��ǰ·��
extern boost::shared_ptr<CNetRevcBase>       CreateNetRevcBaseClient(int netClientType, NETHANDLE serverHandle, NETHANDLE CltHandle, char* szIP, unsigned short nPort, char* szShareMediaURL);
extern MediaServerPort                       ABL_MediaServerPort;
extern int                                   SampleRateArray[];

void PS_MUX_CALL_METHOD GB28181_Send_mux_callback(_ps_mux_cb* cb)
{
	CNetGB28181RtpClient* pThis = (CNetGB28181RtpClient*)cb->userdata;
	if (pThis == NULL || !pThis->bRunFlag)
		return;
 
	pThis->GB28181PsToRtPacket(cb->data, cb->datasize);

#ifdef  WriteGB28181PSFileFlag
	fwrite(cb->data,1,cb->datasize,pThis->writePsFile);
	fflush(pThis->writePsFile);
#endif
}

static void* ps_alloc(void* param, size_t bytes)
{
	CNetGB28181RtpClient* pThis = (CNetGB28181RtpClient*)param;
	 
	return pThis->s_buffer;
}

static void ps_free(void* param, void* /*packet*/)
{
	return;
}

static int ps_write(void* param, int stream, void* packet, size_t bytes)
{
	CNetGB28181RtpClient* pThis = (CNetGB28181RtpClient*)param;

	if(pThis->bRunFlag)
	  pThis->GB28181PsToRtPacket((unsigned char*)packet, bytes);

	return true;
}

//rtp����ص���Ƶ
void GB28181_rtp_packet_callback_func_send(_rtp_packet_cb* cb)
{
	CNetGB28181RtpClient* pThis = (CNetGB28181RtpClient*)cb->userdata;
	if (pThis == NULL || !pThis->bRunFlag)
		return;

	if (pThis->netBaseNetType == NetBaseNetType_NetGB28181SendRtpUDP)
	{//udp ֱ�ӷ��� 
		pThis->SendGBRtpPacketUDP(cb->data, cb->datasize);
	}
	else if (pThis->netBaseNetType == NetBaseNetType_NetGB28181SendRtpTCP_Connect || pThis->netBaseNetType == NetBaseNetType_NetGB28181SendRtpTCP_Passive)
	{//TCP ��Ҫƴ�� ��ͷ
		pThis->GB28181SentRtpVideoData(cb->data, cb->datasize);
	}
}

//PS ���ݴ����rtp 
void  CNetGB28181RtpClient::GB28181PsToRtPacket(unsigned char* pPsData, int nLength)
{
	if(hRtpPS > 0 && bRunFlag)
	{
		inputPS.data = pPsData;
		inputPS.datasize = nLength;
		rtp_packet_input(&inputPS);
	}
}

//����28181PS����TCP��ʽ���� 
void  CNetGB28181RtpClient::GB28181SentRtpVideoData(unsigned char* pRtpVideo, int nDataLength)
{
	if (bRunFlag == false)
		return;
	
	if ((nMaxRtpSendVideoMediaBufferLength - nSendRtpVideoMediaBufferLength < nDataLength + 4) && nSendRtpVideoMediaBufferLength > 0)
	{//ʣ��ռ䲻���洢 ,��ֹ���� 
 		nSendRet = XHNetSDK_Write(nClient, szSendRtpVideoMediaBuffer, nSendRtpVideoMediaBufferLength, 1);
		if (nSendRet != 0)
		{
			bRunFlag = false;

			WriteLog(Log_Debug, "CNetGB28181RtpClient = %X, ���͹���RTP�������� ��Length = %d ,nSendRet = %d", this, nSendRtpVideoMediaBufferLength, nSendRet);
			DeleteNetRevcBaseClient(nClient);
			return;
		}

		nSendRtpVideoMediaBufferLength = 0;
	}

	memcpy((char*)&nCurrentVideoTimestamp, pRtpVideo + 4, sizeof(uint32_t));
	if (nStartVideoTimestamp != GB28181VideoStartTimestampFlag &&  nStartVideoTimestamp != nCurrentVideoTimestamp && nSendRtpVideoMediaBufferLength > 0)
	{//����һ֡�µ���Ƶ 
		nSendRet = XHNetSDK_Write(nClient, szSendRtpVideoMediaBuffer, nSendRtpVideoMediaBufferLength, 1);
		if (nSendRet != 0)
		{
			WriteLog(Log_Debug, "CNetGB28181RtpClient = %X, ���͹���RTP�������� ��Length = %d ,nSendRet = %d", this, nSendRtpVideoMediaBufferLength, nSendRet);
			DeleteNetRevcBaseClient(nClient);
			return;
		}

		nSendRtpVideoMediaBufferLength = 0;
	}

	if (ABL_MediaServerPort.nGBRtpTCPHeadType == 1)
	{//���� TCP���� 4���ֽڷ�ʽ
		szSendRtpVideoMediaBuffer[nSendRtpVideoMediaBufferLength + 0] = '$';
		szSendRtpVideoMediaBuffer[nSendRtpVideoMediaBufferLength + 1] = 0;
		nVideoRtpLen = htons(nDataLength);
		memcpy(szSendRtpVideoMediaBuffer + (nSendRtpVideoMediaBufferLength + 2), (unsigned char*)&nVideoRtpLen, sizeof(nVideoRtpLen));
		memcpy(szSendRtpVideoMediaBuffer + (nSendRtpVideoMediaBufferLength + 4), pRtpVideo, nDataLength);

		nStartVideoTimestamp = nCurrentVideoTimestamp;
 		nSendRtpVideoMediaBufferLength += nDataLength + 4;
	}
	else if (ABL_MediaServerPort.nGBRtpTCPHeadType == 2)
	{//���� TCP���� 2 ���ֽڷ�ʽ
		nVideoRtpLen = htons(nDataLength);
		memcpy(szSendRtpVideoMediaBuffer + nSendRtpVideoMediaBufferLength, (unsigned char*)&nVideoRtpLen, sizeof(nVideoRtpLen));
		memcpy(szSendRtpVideoMediaBuffer + (nSendRtpVideoMediaBufferLength + 2), pRtpVideo, nDataLength);

		nStartVideoTimestamp = nCurrentVideoTimestamp;
 		nSendRtpVideoMediaBufferLength += nDataLength + 2;
	}
	else
	{
		bRunFlag = false;
		WriteLog(Log_Debug, "CNetGB28181RtpClient = %X, �Ƿ��Ĺ���TCP��ͷ���ͷ�ʽ(����Ϊ 1��2 )nGBRtpTCPHeadType = %d ", this, ABL_MediaServerPort.nGBRtpTCPHeadType);
		DeleteNetRevcBaseClient(nClient);
	}
}

CNetGB28181RtpClient::CNetGB28181RtpClient(NETHANDLE hServer, NETHANDLE hClient, char* szIP, unsigned short nPort,char* szShareMediaURL)
{
	memset(m_recvMediaSource, 0x00, sizeof(m_recvMediaSource));
	pRecvMediaSource = NULL;
	psBeiJingLaoChenDemuxer = NULL;
	netDataCache = NULL; //�������ݻ���
	netDataCacheLength = 0;//�������ݻ����С
	nNetStart = nNetEnd = 0; //����������ʼλ��\����λ��
	MaxNetDataCacheCount = 1024*1024*2;
	nRtpRtcpPacketType = 0;

	nMaxRtpSendVideoMediaBufferLength = 640;//Ĭ���ۼ�640
	strcpy(m_szShareMediaURL,szShareMediaURL);
	nClient = hClient;
	nServer = hServer;
	psMuxHandle = 0;

	nVideoStreamID = nAudioStreamID = -1;
	handler.alloc = ps_alloc;
	handler.write = ps_write;
	handler.free = ps_free;
    videoPTS = audioPTS = 0;
	s_buffer = NULL;
	psBeiJingLaoChen = NULL;
	if (ABL_MediaServerPort.gb28181LibraryUse == 2)
	{
		s_buffer = new  char[IDRFrameMaxBufferLength];
		psBeiJingLaoChen = ps_muxer_create(&handler, this);
	}
	hRtpPS = 0;
	bRunFlag = true;

	nSendRtpVideoMediaBufferLength = 0; //�Ѿ����۵ĳ���  ��Ƶ
	nStartVideoTimestamp           = GB28181VideoStartTimestampFlag ; //��һ֡��Ƶ��ʼʱ��� ��
	nCurrentVideoTimestamp         = 0;// ��ǰ֡ʱ���

	m_videoFifo.InitFifo(MaxLiveingVideoFifoBufferLength);
	m_audioFifo.InitFifo(MaxLiveingAudioFifoBufferLength);

	if (nPort == 0) 
	{//��Ϊudp ʹ�ã���Ϊtcpʱ�� ��SendFirstRequst() �⺯������ 
		
		boost::shared_ptr<CMediaStreamSource> pMediaSource = GetMediaStreamSource(m_szShareMediaURL);
		if (pMediaSource != NULL)
		{
			memcpy((char*)&mediaCodecInfo,(char*)&pMediaSource->m_mediaCodecInfo, sizeof(MediaCodecInfo));
			pMediaSource->AddClientToMap(nClient);
		}

		//��nClient ����Video ,audio �����߳�
		pMediaSendThreadPool->AddClientToThreadPool(nClient);
	}

#ifdef  WriteGB28181PSFileFlag
	char    szFileName[256] = { 0 };
	sprintf(szFileName, "%s%X.ps", ABL_MediaSeverRunPath,this);
	writePsFile = fopen(szFileName,"wb");
#endif
#ifdef WriteRecvPSDataFlag
	fWritePSDataFile = fopen("E:\\recv_app_recv.ps","wb");
#endif

 	WriteLog(Log_Debug, "CNetGB28181RtpClient ���� = %X  nClient = %llu ", this, nClient);
}

CNetGB28181RtpClient::~CNetGB28181RtpClient()
{
	bRunFlag = false;
	std::lock_guard<std::mutex> lock(businessProcMutex);

	if (netBaseNetType == NetBaseNetType_NetGB28181SendRtpUDP)
	{
		XHNetSDK_DestoryUdp(nClient);
	}
	else if (netBaseNetType == NetBaseNetType_NetGB28181SendRtpTCP_Connect )
	{
	}
	else if (netBaseNetType == NetBaseNetType_NetGB28181SendRtpTCP_Passive)
	{
		pDisconnectBaseNetFifo.push((unsigned char*)&hParent, sizeof(hParent));
	}
	m_videoFifo.FreeFifo();
	m_audioFifo.FreeFifo();
	ps_mux_stop(psDeMuxHandle);
	rtp_packet_stop(hRtpPS);
	if(psBeiJingLaoChen != NULL )
	  ps_muxer_destroy(psBeiJingLaoChen);
	if (psBeiJingLaoChenDemuxer != NULL)
		ps_demuxer_destroy(psBeiJingLaoChenDemuxer);
	SAFE_ARRAY_DELETE(s_buffer);
	SAFE_ARRAY_DELETE(netDataCache);
	//����ɾ��ý��Դ
	if (strlen(m_recvMediaSource) > 0 && pRecvMediaSource != NULL)
		DeleteMediaStreamSource(m_recvMediaSource);

#ifdef  WriteGB28181PSFileFlag
	fclose(writePsFile);
#endif
#ifdef WriteRecvPSDataFlag
	if(fWritePSDataFile != NULL)
	  fclose(fWritePSDataFile);
#endif

	WriteLog(Log_Debug, "CNetGB28181RtpClient ���� = %X  nClient = %llu ,nMediaClient = %llu\r\n", this, nClient, nMediaClient);
	malloc_trim(0);
}

int CNetGB28181RtpClient::PushVideo(uint8_t* pVideoData, uint32_t nDataLength, char* szVideoCodec)
{
	nRecvDataTimerBySecond = 0 ;
	if (!bRunFlag || m_startSendRtpStruct.disableVideo[0] == 0x31)
		return -1;
	std::lock_guard<std::mutex> lock(businessProcMutex);

	if (strlen(mediaCodecInfo.szVideoName) == 0)
		strcpy(mediaCodecInfo.szVideoName, szVideoCodec);

	m_videoFifo.push(pVideoData, nDataLength);
	return 0;
}

int CNetGB28181RtpClient::PushAudio(uint8_t* pVideoData, uint32_t nDataLength, char* szAudioCodec, int nChannels, int SampleRate)
{
	nRecvDataTimerBySecond = 0;

	//��������Ƶʱ����������Ƶ���
	if (!bRunFlag || m_startSendRtpStruct.disableAudio[0] == 0x31)
		return -1;

	std::lock_guard<std::mutex> lock(businessProcMutex);

	if (strlen(mediaCodecInfo.szAudioName) == 0)
	{
		strcpy(mediaCodecInfo.szAudioName, szAudioCodec);
		mediaCodecInfo.nChannels = nChannels;
		mediaCodecInfo.nSampleRate = SampleRate;
	}

	if (ABL_MediaServerPort.nEnableAudio == 0)
		return -1;

	m_audioFifo.push(pVideoData, nDataLength);
	return 0;
}

void  CNetGB28181RtpClient::CreateRtpHandle()
{
	if (hRtpPS == 0)
	{
		if (strcmp(m_startSendRtpStruct.disableVideo, "1") == 0)
			nMaxRtpSendVideoMediaBufferLength = 640;
		else
			nMaxRtpSendVideoMediaBufferLength = MaxRtpSendVideoMediaBufferLength ;

		int nRet = rtp_packet_start(GB28181_rtp_packet_callback_func_send, (void*)this, &hRtpPS);
		if (nRet != e_rtppkt_err_noerror)
		{
			WriteLog(Log_Debug, "CNetGB28181RtpClient = %X ��������Ƶrtp���ʧ��,nClient = %llu,  nRet = %d", this, nClient, nRet);
			return ;
		}
		optionPS.handle = hRtpPS;
		if (m_startSendRtpStruct.RtpPayloadDataType[0] == 0x31)
		{//����PS���
			optionPS.mediatype = e_rtppkt_mt_video;
			optionPS.payload = atoi(m_startSendRtpStruct.payload);
			optionPS.streamtype = e_rtppkt_st_gb28181;
			optionPS.ssrc = atoi(m_startSendRtpStruct.ssrc);
			optionPS.ttincre = (90000 / mediaCodecInfo.nVideoFrameRate);
			rtp_packet_setsessionopt(&optionPS);
		}
		else if (m_startSendRtpStruct.RtpPayloadDataType[0] == 0x32 || m_startSendRtpStruct.RtpPayloadDataType[0] == 0x33)
		{//ES \ XHB ���
			if (atoi(m_startSendRtpStruct.disableAudio) == 1)
			{//ֻ����Ƶ
				optionPS.mediatype = e_rtppkt_mt_video;
				if (strcmp(mediaCodecInfo.szVideoName, "H264") == 0)
				{
					strcpy(m_startSendRtpStruct.payload, "98");
					optionPS.payload = 98;
					optionPS.streamtype = e_rtppkt_st_h264;
				}
				else  if (strcmp(mediaCodecInfo.szVideoName, "H265") == 0)
				{
					strcpy(m_startSendRtpStruct.payload, "99");
					optionPS.payload = 99;
					optionPS.streamtype = e_rtppkt_st_h265;
				}
				optionPS.ssrc = atoi(m_startSendRtpStruct.ssrc);
				optionPS.ttincre = (90000 / mediaCodecInfo.nVideoFrameRate);
 			}
			else if (atoi(m_startSendRtpStruct.disableVideo) == 1)
			{//ֻ����Ƶ 
				optionPS.mediatype = e_rtppkt_mt_audio;
				optionPS.ssrc = atoi(m_startSendRtpStruct.ssrc);
				if (strcmp(mediaCodecInfo.szAudioName, "G711_A") == 0)
				{
					optionPS.ttincre = 320; 
					optionPS.streamtype = e_rtppkt_st_g711a;
					optionPS.payload = 8;
				}else if (strcmp(mediaCodecInfo.szAudioName, "G711_U") == 0)
				{
					optionPS.ttincre = 320;  
					optionPS.streamtype = e_rtppkt_st_g711u;
					optionPS.payload = 0;
				}
				else if (strcmp(mediaCodecInfo.szAudioName, "AAC") == 0) 
				{
					optionPS.ttincre = 1024;  
					optionPS.streamtype = e_rtppkt_st_aac_have_adts;
					optionPS.payload = 97;
				}
 			}
			rtp_packet_setsessionopt(&optionPS);
		}

		inputPS.handle = hRtpPS;
		inputPS.ssrc = optionPS.ssrc;

		memset((char*)&gbDstAddr, 0x00, sizeof(gbDstAddr));
		gbDstAddr.sin_family = AF_INET;
		gbDstAddr.sin_addr.s_addr = inet_addr(m_startSendRtpStruct.dst_url);
		gbDstAddr.sin_port = htons(atoi(m_startSendRtpStruct.dst_port));

		//����ý��Դ
		SplitterAppStream(m_szShareMediaURL);
		sprintf(m_addStreamProxyStruct.url, "rtp://%s:%s/%s/%s", m_startSendRtpStruct.dst_url, m_startSendRtpStruct.dst_port,m_addStreamProxyStruct.app, m_addStreamProxyStruct.stream);
		strcpy(szClientIP, m_startSendRtpStruct.dst_url);
	}
}

int CNetGB28181RtpClient::SendVideo()
{
	if (!bRunFlag )
		return -1;

	//���������ɹ������ӳɹ�
	if (!bUpdateVideoFrameSpeedFlag)
		bUpdateVideoFrameSpeedFlag = true;

	if (ABL_MediaServerPort.gb28181LibraryUse == 1)
	{//����
		if (psMuxHandle == 0)
		{
			memset(&init, 0, sizeof(init));
			init.cb = (void*)GB28181_Send_mux_callback;
			init.userdata = this;
			init.alignmode = e_psmux_am_4octet;
			init.ttmode = 0;
			init.ttincre = (90000 / mediaCodecInfo.nVideoFrameRate);
			init.h = &psMuxHandle;
			int32_t ret = ps_mux_start(&init);

			input.handle = psMuxHandle;

			WriteLog(Log_Debug, "CNetGB28181RtpClient = %X ������ ps ����ɹ�  ,nClient = %llu,  nRet = %d", this, nClient, ret);
		}
	}
	else
	{//�����ϳ�
		if (nVideoStreamID == -1 && psBeiJingLaoChen != NULL )
		{
			if (strcmp(mediaCodecInfo.szVideoName,"H264") == 0 )
			  nVideoStreamID = ps_muxer_add_stream((ps_muxer_t*)psBeiJingLaoChen, PSI_STREAM_H264, NULL, 0);
			else if (strcmp(mediaCodecInfo.szVideoName, "H265") == 0)
			  nVideoStreamID = ps_muxer_add_stream((ps_muxer_t*)psBeiJingLaoChen, PSI_STREAM_H265, NULL, 0);
		}
	}

	//����rtp���
	CreateRtpHandle();

	unsigned char* pData = NULL;
	int            nLength = 0;
	if ((pData = m_videoFifo.pop(&nLength)) != NULL)
	{
		input.mediatype = e_psmux_mt_video;
		if (strcmp(mediaCodecInfo.szVideoName, "H264") == 0)
			input.streamtype = e_psmux_st_h264;
		else if (strcmp(mediaCodecInfo.szVideoName, "H265") == 0)
			input.streamtype = e_psmux_st_h265;
		else
		{
			m_videoFifo.pop_front();
			return 0;
		}

		if (m_startSendRtpStruct.RtpPayloadDataType[0] == 0x31)
		{//PS ���
			//����PS���
			if (ABL_MediaServerPort.gb28181LibraryUse == 1)
			{
	  		  input.data = pData;
			  input.datasize = nLength;
			  ps_mux_input(&input);
			}
			else
			{//�����ϳ�PS���
				if (nVideoStreamID != -1 && psBeiJingLaoChen != NULL && strlen(mediaCodecInfo.szVideoName) > 0 )
				{
					if(strcmp(mediaCodecInfo.szVideoName,"H264") == 0)
					  nflags = CheckVideoIsIFrame("H264", pData, nLength);
					else if (strcmp(mediaCodecInfo.szVideoName, "H265") == 0)
					  nflags = CheckVideoIsIFrame("H265", pData, nLength);

					ps_muxer_input((ps_muxer_t*)psBeiJingLaoChen, nVideoStreamID, nflags, videoPTS, videoPTS, pData, nLength);
					videoPTS += (90000 / mediaCodecInfo.nVideoFrameRate);
				}
			}
		}
		else if (m_startSendRtpStruct.RtpPayloadDataType[0] == 0x32 || m_startSendRtpStruct.RtpPayloadDataType[0] == 0x33)
		{//ES ��� \ XHB ���
			if (hRtpPS > 0 && bRunFlag)
			{
				inputPS.data = pData;
				inputPS.datasize = nLength;
				rtp_packet_input(&inputPS);
			}
		}
 
		m_videoFifo.pop_front();
	}
	return 0;
}

int CNetGB28181RtpClient::SendAudio()
{
	if ( ABL_MediaServerPort.nEnableAudio == 0 || !bRunFlag || m_startSendRtpStruct.disableAudio[0] == 0x31 )
		return 0;

	unsigned char* pData = NULL;
	int            nLength = 0;
	if ((pData = m_audioFifo.pop(&nLength)) != NULL)
	{
		input.mediatype = e_psmux_mt_audio;
		if (strcmp(mediaCodecInfo.szAudioName, "AAC") == 0)
			input.streamtype = e_psmux_st_aac;
		else if (strcmp(mediaCodecInfo.szAudioName, "G711_A") == 0)
			input.streamtype = e_psmux_st_g711a;
		else if (strcmp(mediaCodecInfo.szAudioName, "G711_U") == 0)
			input.streamtype = e_psmux_st_g711u;
		else
		{
			m_audioFifo.pop_front();
			return 0;
		}

		//����rtp���
		CreateRtpHandle();

		if (m_startSendRtpStruct.RtpPayloadDataType[0] == 0x31)
		{//����PS���ʱ,���԰���Ƶ����Ƶ�����һ��,����ES���,��Ƶ\��Ƶ ���ܴ����һ��
			//����PS���
			if (ABL_MediaServerPort.gb28181LibraryUse == 1)
			{
			   input.data = pData;
			   input.datasize = nLength;
 			   ps_mux_input(&input);
			}
			else
			{//�����ϳ�PS���
				if (nAudioStreamID == -1 && psBeiJingLaoChen != NULL && strlen(mediaCodecInfo.szAudioName) > 0 )
				{
					if ( strcmp(mediaCodecInfo.szAudioName,"AAC") == 0 )
						nAudioStreamID = ps_muxer_add_stream((ps_muxer_t*)psBeiJingLaoChen, PSI_STREAM_AAC, NULL, 0);
					else if (strcmp(mediaCodecInfo.szAudioName, "G711_A") == 0)
						nAudioStreamID = ps_muxer_add_stream((ps_muxer_t*)psBeiJingLaoChen, PSI_STREAM_AUDIO_G711A, NULL, 0);
					else if (strcmp(mediaCodecInfo.szAudioName, "G711_U") == 0)
						nAudioStreamID = ps_muxer_add_stream((ps_muxer_t*)psBeiJingLaoChen, PSI_STREAM_AUDIO_G711U, NULL, 0);
				}

				if (nAudioStreamID != -1 && psBeiJingLaoChen != NULL && strlen(mediaCodecInfo.szAudioName) > 0 )
				{
					ps_muxer_input((ps_muxer_t*)psBeiJingLaoChen, nAudioStreamID, 0, audioPTS, audioPTS, pData, nLength);

					if (strcmp(mediaCodecInfo.szAudioName, "AAC") == 0)
						audioPTS += mediaCodecInfo.nBaseAddAudioTimeStamp;
					else if (strcmp(mediaCodecInfo.szAudioName, "G711_A") == 0 || strcmp(mediaCodecInfo.szAudioName, "G711_U") == 0)
						audioPTS += nLength / 8;
				}
			}
		}
		else if (m_startSendRtpStruct.RtpPayloadDataType[0] == 0x32 || m_startSendRtpStruct.RtpPayloadDataType[0] == 0x33)
		{//ES\XHB ���
 			inputPS.data = pData;
			inputPS.datasize = nLength;
 		    rtp_packet_input(&inputPS);
 		}
 
		m_audioFifo.pop_front();
	}
	return 0;
}

//udp��ʽ����rtp��
void  CNetGB28181RtpClient::SendGBRtpPacketUDP(unsigned char* pRtpData, int nLength)
{
	XHNetSDK_Sendto(nClient, pRtpData, nLength, (void*)&gbDstAddr);
}

int CNetGB28181RtpClient::InputNetData(NETHANDLE nServerHandle, NETHANDLE nClientHandle, uint8_t* pData, uint32_t nDataLength, void* address)
{
 	std::lock_guard<std::mutex> lock(businessProcMutex);
	if (!bRunFlag || nDataLength <= 0 )
		return -1;
	if (!(strlen(m_startSendRtpStruct.recv_app) > 0 && strlen(m_startSendRtpStruct.recv_stream) > 0))
		return -1;

	if (netBaseNetType == NetBaseNetType_NetGB28181SendRtpUDP)
	{//UDP
	   //��ȡrtp���İ�ͷ
		rtpHeadPtr = (_rtp_header*)(pData);

		//���ȺϷ� ������rtp�� ��rtpHeadPtr->v == 2) ,��ֹrtcp����ִ��rtp���
		if (nDataLength > 0 && nDataLength < 1500  && rtpHeadPtr->v == 2)
 		  RtpDepacket(pData, nDataLength);
	}
	else if (netBaseNetType == NetBaseNetType_NetGB28181SendRtpTCP_Connect  || netBaseNetType == NetBaseNetType_NetGB28181SendRtpTCP_Passive)
	{//TCP 
		if (netDataCache == NULL)
			netDataCache = new unsigned char[MaxNetDataCacheCount];

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
					WriteLog(Log_Debug, "CNetGB28181RtpClient = %X nClient = %llu �����쳣 , ִ��ɾ��", this, nClient);
					pDisconnectBaseNetFifo.push((unsigned char*)&nClient, sizeof(nClient));
					return 0;
				}
			}
			else
			{//û��ʣ�࣬��ô �ף�βָ�붼Ҫ��λ 
				nNetStart = nNetEnd = netDataCacheLength = 0;
			}
			memcpy(netDataCache + nNetEnd, pData, nDataLength);
			netDataCacheLength += nDataLength;
			nNetEnd += nDataLength;
		}
	}

    return 0;
}

int CNetGB28181RtpClient::ProcessNetData()
{
 	std::lock_guard<std::mutex> lock(businessProcMutex);
	if (!bRunFlag)
		return -1;
	if (!(strlen(m_startSendRtpStruct.recv_app) > 0 && strlen(m_startSendRtpStruct.recv_stream) > 0))
		return -1;

	unsigned char* pData = NULL;
	int            nLength;

	if (netBaseNetType == NetBaseNetType_NetGB28181SendRtpUDP)
	{//UDP
 
	}
	else if (netBaseNetType == NetBaseNetType_NetGB28181SendRtpTCP_Connect  || netBaseNetType == NetBaseNetType_NetGB28181SendRtpTCP_Passive)
	{//TCP ��ʽ��rtp����ȡ 
		while (netDataCacheLength > 2048)
		{//���ܻ���̫��buffer,������ɽ��չ�������ֻ����Ƶ��ʱ���������ʱ�ܴ� 2048 �ȽϺ��� 
			memcpy(rtpHeadOfTCP, netDataCache + nNetStart, 2);
			if ((rtpHeadOfTCP[0] == 0x24 && rtpHeadOfTCP[1] == 0x00) || (rtpHeadOfTCP[0] == 0x24 && rtpHeadOfTCP[1] == 0x01) || (rtpHeadOfTCP[0] == 0x24))
			{
				nNetStart += 2;
				memcpy((char*)&nRtpLength, netDataCache + nNetStart, 2);
				nNetStart += 2;
				netDataCacheLength -= 4;

				if (nRtpRtcpPacketType == 0)
					nRtpRtcpPacketType = 2;//4���ֽڷ�ʽ
			}
			else
			{
				memcpy((char*)&nRtpLength, netDataCache + nNetStart, 2);
				nNetStart += 2;
				netDataCacheLength -= 2;

				if (nRtpRtcpPacketType == 0)
					nRtpRtcpPacketType = 1;//2���ֽڷ�ʽ��
			}

			//��ȡrtp���İ�ͷ
			rtpHeadPtr = (_rtp_header*)(netDataCache + nNetStart);

			nRtpLength = ntohs(nRtpLength);
			if (nRtpLength > 65535)
			{
				WriteLog(Log_Debug, "CNetGB28181RtpServer = %X rtp��ͷ��������  nClient = %llu ,nRtpLength = %llu", this, nClient, nRtpLength);
				DeleteNetRevcBaseClient(nClient);
				return -1;
			}

			//���ȺϷ� ������rtp�� ��rtpHeadPtr->v == 2) ,��ֹrtcp����ִ��rtp���
			if (nRtpLength > 0 && rtpHeadPtr->v == 2)
			{
				//����rtpͷ�����payload���н��,��Ч��ֹ�û���д��
				if (hRtpHandle == 0)
					m_gbPayload = rtpHeadPtr->payload;

				RtpDepacket(netDataCache + nNetStart, nRtpLength);
			}

			nNetStart += nRtpLength;
			netDataCacheLength -= nRtpLength;
		}
	}

 	return 0;
}

//���͵�һ������
int CNetGB28181RtpClient::SendFirstRequst()
{//�� gb28181 Ϊtcpʱ�������ú��� 

	if (netBaseNetType == NetBaseNetType_NetGB28181SendRtpTCP_Connect)
	{//�ظ�http�������ӳɹ���
		sprintf(szResponseBody, "{\"code\":0,\"port\":%d,\"memo\":\"success\",\"key\":%llu}", nReturnPort, nClient);
		ResponseHttp(nClient_http, szResponseBody, false);
	}

	boost::shared_ptr<CMediaStreamSource> pMediaSource = GetMediaStreamSource(m_szShareMediaURL);
	if (pMediaSource != NULL)
	{
		memcpy((char*)&mediaCodecInfo, (char*)&pMediaSource->m_mediaCodecInfo, sizeof(MediaCodecInfo));
		pMediaSource->AddClientToMap(nClient);
	}

	//��nClient ����Video ,audio �����߳�
	pMediaSendThreadPool->AddClientToThreadPool(nClient);
	return 0;
}

//rtp����ص�
void RTP_DEPACKET_CALL_METHOD NetGB28181RtpClient_rtppacket_callback_recv(_rtp_depacket_cb* cb)
{
	CNetGB28181RtpClient* pThis = (CNetGB28181RtpClient*)cb->userdata;
	if (!pThis->bRunFlag)
		return;

	if (pThis->m_startSendRtpStruct.RtpPayloadDataType[0] == 0x31)
	{//����PS���
		if (pThis->psBeiJingLaoChenDemuxer)
			ps_demuxer_input(pThis->psBeiJingLaoChenDemuxer, cb->data, cb->datasize);
	}
	else if (pThis->m_startSendRtpStruct.RtpPayloadDataType[0] == 0x32)
	{//RTP ���
		if (pThis->pRecvMediaSource == NULL)
		{//rtp��� 
			pThis->pRecvMediaSource = CreateMediaStreamSource(pThis->m_recvMediaSource, pThis->nClient, MediaSourceType_LiveMedia, 0, pThis->m_h265ConvertH264Struct);
			if (pThis->pRecvMediaSource != NULL)
			{
				pThis->pRecvMediaSource->netBaseNetType = pThis->netBaseNetType;
				WriteLog(Log_Debug, "NetGB28181RtpClient_rtppacket_callback_recv ����ý��Դ %s �ɹ�  ", pThis->m_recvMediaSource);
			}
		}

		if (pThis->pRecvMediaSource != NULL)
		{
			if (cb->payload == 98)
				pThis->pRecvMediaSource->PushVideo(cb->data, cb->datasize, "H264");
			else if (cb->payload == 99)
				pThis->pRecvMediaSource->PushVideo(cb->data, cb->datasize, "H265");
			else if (cb->payload == 0)//g711u 
				pThis->pRecvMediaSource->PushAudio(cb->data, cb->datasize, "G711_U", 1, 8000);
			else if (cb->payload == 8)//g711a
				pThis->pRecvMediaSource->PushAudio(cb->data, cb->datasize, "G711_A", 1, 8000);
			else if (cb->payload == 97)//aac
			{
				pThis->GetAACAudioInfo(cb->data, cb->datasize);//��ȡAACý����Ϣ
				if (cb->datasize > 0 && cb->datasize < 2048)
					pThis->pRecvMediaSource->PushAudio((unsigned char*)cb->data, cb->datasize, pThis->mediaCodecInfo.szAudioName, pThis->mediaCodecInfo.nChannels, pThis->mediaCodecInfo.nSampleRate);
			}
		}
	}
}

static void mpeg_ps_dec_NetGB28181RtpClient(void* param, int stream, int codecid, const void* extra, int bytes, int finish)
{
	printf("stream %d, codecid: %d, finish: %s\n", stream, codecid, finish ? "true" : "false");
}
//rtp ���
struct ps_demuxer_notify_t notify_CNetGB28181RtpClient = { mpeg_ps_dec_NetGB28181RtpClient, };

//����PS����ص�
static int NetGB28181RtpClient_on_gb28181_unpacket(void* param, int stream, int avtype, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
	CNetGB28181RtpClient* pThis = (CNetGB28181RtpClient*)param;
	if (!pThis->bRunFlag)
		return -1;

	if (pThis->pRecvMediaSource == NULL)
	{//���ȴ���ý��Դ 
		pThis->pRecvMediaSource = CreateMediaStreamSource(pThis->m_recvMediaSource, pThis->nClient, MediaSourceType_LiveMedia, 0, pThis->m_h265ConvertH264Struct);
		if (pThis->pRecvMediaSource != NULL)
		{
 			pThis->pRecvMediaSource->netBaseNetType = pThis->netBaseNetType;
			WriteLog(Log_Debug, "NetGB28181RtpClient_on_gb28181_unpacket ����ý��Դ %s �ɹ�  ", pThis->m_recvMediaSource);
		}
	}

	if (pThis->pRecvMediaSource == NULL)
		return -1;

	if (!pThis->pRecvMediaSource->bUpdateVideoSpeed)
	{//��Ҫ����ý��Դ��֡�ٶ�
		pThis->pRecvMediaSource->UpdateVideoFrameSpeed(25, pThis->netBaseNetType);
		pThis->pRecvMediaSource->bUpdateVideoSpeed = true;
	}

	if (pThis->m_startSendRtpStruct.recv_disableAudio[0] == 0x30  && (PSI_STREAM_AAC == avtype || PSI_STREAM_AUDIO_G711A == avtype || PSI_STREAM_AUDIO_G711U == avtype))
	{
		if (PSI_STREAM_AAC == avtype)
		{//aac
			pThis->GetAACAudioInfo((unsigned char*)data, bytes);//��ȡAACý����Ϣ
			pThis->pRecvMediaSource->PushAudio((unsigned char*)data, bytes, pThis->mediaCodecInfo.szAudioName, pThis->mediaCodecInfo.nChannels, pThis->mediaCodecInfo.nSampleRate);
		}
		else if (PSI_STREAM_AUDIO_G711A == avtype)
		{// G711A  
			pThis->pRecvMediaSource->PushAudio((unsigned char*)data, bytes, "G711_A", 1, 8000);
		}
		else if (PSI_STREAM_AUDIO_G711U == avtype)
		{// G711U  
			pThis->pRecvMediaSource->PushAudio((unsigned char*)data, bytes, "G711_U", 1, 8000);
		}
	}
	else if (pThis->m_startSendRtpStruct.recv_disableVideo[0] == 0x30 && (PSI_STREAM_H264 == avtype || PSI_STREAM_H265 == avtype || PSI_STREAM_VIDEO_SVAC == avtype))
	{
#ifdef WriteRecvPSDataFlag
		if (pThis->fWritePSDataFile != NULL )
		{
			fwrite(data, 1, bytes, pThis->fWritePSDataFile);
			fflush(pThis->fWritePSDataFile);
		}
#endif	
 		if (PSI_STREAM_H264 == avtype)
			pThis->pRecvMediaSource->PushVideo((unsigned char*)data, bytes, "H264");
		else if (PSI_STREAM_H265 == avtype)
			pThis->pRecvMediaSource->PushVideo((unsigned char*)data, bytes, "H265");
 	}
}

//rtp��� 
bool  CNetGB28181RtpClient::RtpDepacket(unsigned char* pData, int nDataLength)
{
	if (pData == NULL || nDataLength > 65536 || !bRunFlag || nDataLength < 12)
		return false;

	//����rtp���
	if (hRtpHandle == 0)
	{
		rtp_depacket_start(NetGB28181RtpClient_rtppacket_callback_recv, (void*)this, (uint32_t*)&hRtpHandle);
 
		if (m_startSendRtpStruct.RtpPayloadDataType[0] == 0x31)
		{//rtp + PS
			rtp_depacket_setpayload(hRtpHandle, 96, e_rtpdepkt_st_gbps);
  		}
		else if (m_startSendRtpStruct.RtpPayloadDataType[0] == 0x32 && rtpHeadPtr != NULL)
		{//rtp + ES
			if (rtpHeadPtr->payload == 98)
				rtp_depacket_setpayload(hRtpHandle, rtpHeadPtr->payload, e_rtpdepkt_st_h264);
			else if (rtpHeadPtr->payload == 99)
				rtp_depacket_setpayload(hRtpHandle, rtpHeadPtr->payload, e_rtpdepkt_st_h265);
		}
		else if (m_startSendRtpStruct.RtpPayloadDataType[0] == 0x33)
		{//rtp + xhb
			rtp_depacket_setpayload(hRtpHandle, m_gbPayload, e_rtpdepkt_st_xhb);
		}
		strcpy(m_addStreamProxyStruct.app, m_startSendRtpStruct.recv_app);
		strcpy(m_addStreamProxyStruct.stream, m_startSendRtpStruct.recv_stream);
		sprintf(m_recvMediaSource, "/%s/%s", m_startSendRtpStruct.recv_app, m_startSendRtpStruct.recv_stream);
		WriteLog(Log_Debug, "CNetGB28181RtpClient = %X ,����rtp����ɹ� nClient = %llu ,hRtpHandle = %d", this, nClient, hRtpHandle);
	}

	if (psBeiJingLaoChenDemuxer == NULL)
	{
		psBeiJingLaoChenDemuxer = ps_demuxer_create(NetGB28181RtpClient_on_gb28181_unpacket, this);
		if (psBeiJingLaoChenDemuxer != NULL)
		{
			ps_demuxer_set_notify(psBeiJingLaoChenDemuxer, &notify_CNetGB28181RtpClient, this);
			WriteLog(Log_Debug, "CNetGB28181RtpClient = %X ,��������PS����ɹ� nClent = %llu ,psBeiJingLaoChenDemuxer = %X", this, nClient, psBeiJingLaoChenDemuxer);
		}
	}

	if (hRtpHandle > 0)
	{//rtp���
		rtp_depacket_input(hRtpHandle, pData, nDataLength);
	}

	return true;
}

//����m3u8�ļ�
bool  CNetGB28181RtpClient::RequestM3u8File()
{
	return true;
}