/*
���ܣ�
    �������GB28181Rtp����������UDP��TCPģʽ 
    ���� ���귢��  ����������յ�ͬʱҲ֧�ֹ��귢�ͣ�    2023-05-18
����    2021-08-08
����    �޼��ֵ�
QQ      79941308
E-Mail  79941308@qq.com
*/

#include "stdafx.h"
#include "NetGB28181RtpServer.h"

extern bool                                  DeleteNetRevcBaseClient(NETHANDLE CltHandle);
extern boost::shared_ptr<CMediaStreamSource> CreateMediaStreamSource(char* szUR, uint64_t nClient, MediaSourceType nSourceType, uint32_t nDuration, H265ConvertH264Struct  h265ConvertH264Struct);
extern boost::shared_ptr<CMediaStreamSource> GetMediaStreamSource(char* szURL);
extern bool                                  DeleteMediaStreamSource(char* szURL);
extern bool                                  DeleteClientMediaStreamSource(uint64_t nClient);
extern MediaServerPort                       ABL_MediaServerPort;

extern CMediaSendThreadPool*                 pMediaSendThreadPool;
extern CMediaFifo                            pDisconnectBaseNetFifo; //�������ѵ����� 
extern char                                  ABL_MediaSeverRunPath[256]; //��ǰ·��
extern boost::shared_ptr<CNetRevcBase>       CreateNetRevcBaseClient(int netClientType, NETHANDLE serverHandle, NETHANDLE CltHandle, char* szIP, unsigned short nPort, char* szShareMediaURL);
extern boost::shared_ptr<CNetRevcBase>       GetNetRevcBaseClientNoLock(NETHANDLE CltHandle);
extern boost::shared_ptr<CNetRevcBase>       GetNetRevcBaseClient(NETHANDLE CltHandle);
extern CMediaFifo                            pMessageNoticeFifo;          //��Ϣ֪ͨFIFO

static void* NetGB28181RtpServer_ps_alloc(void* param, size_t bytes)
{
	CNetGB28181RtpServer* pThis = (CNetGB28181RtpServer*)param;

	return pThis->s_buffer;
}

static void NetGB28181RtpServer_ps_free(void* param, void* /*packet*/)
{
	return;
}

//PS ������{
static int NetGB28181RtpServer_ps_write(void* param, int stream, void* packet, size_t bytes)
{
	CNetGB28181RtpServer* pThis = (CNetGB28181RtpServer*)param;
	if (!pThis->bRunFlag)
		return -1;

	 pThis->GB28181PsToRtPacket((unsigned char*)packet, bytes);

#ifdef WriteSendPsFileFlag
	if (pThis->fWriteSendPsFile)
	{
		fwrite((char*)packet,1,bytes,pThis->fWriteSendPsFile);
		fflush(pThis->fWriteSendPsFile);
	}
#endif 

	return 0 ;
}

void RTP_DEPACKET_CALL_METHOD GB28181_rtppacket_callback_recv(_rtp_depacket_cb* cb)
{
	CNetGB28181RtpServer* pThis = (CNetGB28181RtpServer*)cb->userdata;
	if (!pThis->bRunFlag)
		return;

	if(pThis != NULL)
	{
		if (pThis->pMediaSource == NULL)
		{//���ȴ���ý��Դ,��Ϊ rtp + PS \ rtp + ES \ rtp + XHB ��Ҫʹ�� 
			pThis->pMediaSource = CreateMediaStreamSource(pThis->m_szShareMediaURL, pThis->hParent, MediaSourceType_LiveMedia, 0, pThis->m_h265ConvertH264Struct);
			if (pThis->pMediaSource != NULL)
				pThis->pMediaSource->netBaseNetType = pThis->netBaseNetType;
		}

		if (pThis->nSSRC == 0)
		   pThis->nSSRC = cb->ssrc; //Ĭ�ϵ�һ��ssrc 
		if (pThis->pMediaSource && pThis->nSSRC == cb->ssrc && pThis->m_addStreamProxyStruct.RtpPayloadDataType[0] >= 0x31 && pThis->m_addStreamProxyStruct.RtpPayloadDataType[0] <= 0x33 )
		{
 			if (!pThis->bUpdateVideoFrameSpeedFlag && pThis->m_addStreamProxyStruct.RtpPayloadDataType[0] >= 0x32 && pThis->m_addStreamProxyStruct.RtpPayloadDataType[0] <= 0x33)
			{//������ƵԴ��֡�ٶ�
				int nVideoSpeed = 25;
				if (nVideoSpeed > 0)
				{
					pThis->bUpdateVideoFrameSpeedFlag = true;
  					//����UDP��TCP������Ϊ�����Ѿ����� 
					boost::shared_ptr<CNetRevcBase>  pGB28181Proxy = GetNetRevcBaseClient(pThis->hParent);
					if (pGB28181Proxy != NULL)
						pGB28181Proxy->bUpdateVideoFrameSpeedFlag = true;

					if (pThis->pMediaSource)
					{
						pThis->pMediaSource->enable_mp4 = strcmp(pThis->m_addStreamProxyStruct.enable_mp4, "1") == 0 ? true : false;//��¼�Ƿ�¼��
						pThis->pMediaSource->enable_hls = strcmp(pThis->m_addStreamProxyStruct.enable_hls, "1") == 0 ? true : false;//��¼�Ƿ���hls
					}

					WriteLog(Log_Debug, "nClient = %llu , ������ƵԴ %s ��֡�ٶȳɹ�����ʼ�ٶ�Ϊ%d ,���º���ٶ�Ϊ%d, ", pThis->nClient, pThis->pMediaSource->m_szURL, pThis->pMediaSource->m_mediaCodecInfo.nVideoFrameRate, nVideoSpeed);
					pThis->pMediaSource->UpdateVideoFrameSpeed(nVideoSpeed, pThis->netBaseNetType);
				}
			}

			if (pThis->m_addStreamProxyStruct.RtpPayloadDataType[0] == 0x31)
			{//rtp + PS 
				if (ABL_MediaServerPort.gb28181LibraryUse == 1)
				{//����
				  ps_demux_input(pThis->psDeMuxHandle, cb->data, cb->datasize);
				}
				else
				{//�����ϳ�
				   if(pThis->psBeiJingLaoChen)
					 ps_demuxer_input(pThis->psBeiJingLaoChen, cb->data, cb->datasize);
				}
			}
			else if (pThis->m_addStreamProxyStruct.RtpPayloadDataType[0] >= 0x32 && pThis->m_addStreamProxyStruct.RtpPayloadDataType[0] <= 0x33)
			{// rtp + ES \ rtp + XHB 
				 if(cb->payload == 98 )
				    pThis->pMediaSource->PushVideo(cb->data, cb->datasize, "H264");
				 else if(cb->payload == 99)
				    pThis->pMediaSource->PushVideo(cb->data, cb->datasize, "H265");
				 else if(cb->payload == 0)//g711u 
					pThis->pMediaSource->PushAudio(cb->data, cb->datasize, "G711_U",1,8000);
				 else if (cb->payload == 8)//g711a
					 pThis->pMediaSource->PushAudio(cb->data, cb->datasize, "G711_A", 1, 8000);
				 else if (cb->payload == 97)//aac
				 {
#ifdef WriteRecvAACDataFlag
					 if (pThis->fWriteRecvAACFile)
					 {
						 fwrite(cb->data, 1, cb->datasize, pThis->fWriteRecvAACFile);
						 fflush(pThis->fWriteRecvAACFile);
					 }
#endif
					 pThis->GetAACAudioInfo(cb->data, cb->datasize);//��ȡAACý����Ϣ
 					 if(cb->datasize > 0 &&  cb->datasize < 2048 )
					    pThis->pMediaSource->PushAudio((unsigned char*)cb->data, cb->datasize, pThis->mediaCodecInfo.szAudioName, pThis->mediaCodecInfo.nChannels, pThis->mediaCodecInfo.nSampleRate);
				 }
			}
  		}

	   if (ABL_MediaServerPort.nSaveGB28181Rtp == 1 && pThis->fWritePsFile != NULL && (GetTickCount64() - pThis->nCreateDateTime) < 1000 * 180 )
 	   {
		   fwrite(cb->data,1,cb->datasize,pThis->fWritePsFile);
		   fflush(pThis->fWritePsFile);
	   }
 	}
}

static int on_gb28181_unpacket(void* param, int stream, int avtype, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
	CNetGB28181RtpServer* pThis = (CNetGB28181RtpServer*)param;
	if (!pThis->bRunFlag)
		return -1;

	if (pThis->pMediaSource == NULL)
	{
		pThis->bRunFlag = false;
		DeleteNetRevcBaseClient(pThis->nClient);
		return -1;
	}

 	if ((PSI_STREAM_AAC == avtype || PSI_STREAM_AUDIO_G711A == avtype || PSI_STREAM_AUDIO_G711U == avtype) && pThis->m_addStreamProxyStruct.disableAudio[0] == 0x30)
	{
		if (PSI_STREAM_AAC == avtype)
		{//aac
			pThis->GetAACAudioInfo((unsigned char*)data, bytes);//��ȡAACý����Ϣ
			pThis->pMediaSource->PushAudio((unsigned char*)data, bytes, pThis->mediaCodecInfo.szAudioName, pThis->mediaCodecInfo.nChannels, pThis->mediaCodecInfo.nSampleRate);
		}
		else if (PSI_STREAM_AUDIO_G711A == avtype)
		{// G711A  
			pThis->pMediaSource->PushAudio((unsigned char*)data, bytes, "G711_A", 1, 8000);
		}
		else if (PSI_STREAM_AUDIO_G711U == avtype)
		{// G711U  
			pThis->pMediaSource->PushAudio((unsigned char*)data, bytes, "G711_U", 1, 8000);
		}
	}
	else if (PSI_STREAM_H264 == avtype || PSI_STREAM_H265 == avtype || PSI_STREAM_VIDEO_SVAC == avtype)
	{
		if (pThis->m_addStreamProxyStruct.disableVideo[0] == 0x30)
		{//�������û�й��˵���Ƶ
			if (PSI_STREAM_H264 == avtype)
				pThis->pMediaSource->PushVideo((unsigned char*)data, bytes, "H264");
			else if (PSI_STREAM_H265 == avtype)
				pThis->pMediaSource->PushVideo((unsigned char*)data, bytes, "H265");
		}
	}

	if (!pThis->bUpdateVideoFrameSpeedFlag)
	{//������ƵԴ��֡�ٶ�
		int nVideoSpeed = 25 ;
		if (nVideoSpeed > 0 && pThis->pMediaSource != NULL)
		{
			pThis->bUpdateVideoFrameSpeedFlag = true;

			//����UDP��TCP������Ϊ�����Ѿ����� 
			boost::shared_ptr<CNetRevcBase>  pGB28181Proxy = GetNetRevcBaseClient(pThis->hParent);
			if (pGB28181Proxy != NULL)
				pGB28181Proxy->bUpdateVideoFrameSpeedFlag = true;

			if (pThis->pMediaSource)
			{
				pThis->pMediaSource->enable_mp4 = strcmp(pThis->m_addStreamProxyStruct.enable_mp4, "1") == 0 ? true : false;//��¼�Ƿ�¼��
				pThis->pMediaSource->enable_hls = strcmp(pThis->m_addStreamProxyStruct.enable_hls, "1") == 0 ? true : false;//��¼�Ƿ���hls
			}

			WriteLog(Log_Debug, "nClient = %llu , ������ƵԴ %s ��֡�ٶȳɹ�����ʼ�ٶ�Ϊ%d ,���º���ٶ�Ϊ%d, ", pThis->nClient, pThis->pMediaSource->m_szURL, pThis->pMediaSource->m_mediaCodecInfo.nVideoFrameRate, nVideoSpeed);
			pThis->pMediaSource->UpdateVideoFrameSpeed(nVideoSpeed, pThis->netBaseNetType);
		}
	}

	return 0;
}
static void mpeg_ps_dec_testonstream(void* param, int stream, int codecid, const void* extra, int bytes, int finish)
{
	printf("stream %d, codecid: %d, finish: %s\n", stream, codecid, finish ? "true" : "false");
}

void PS_DEMUX_CALL_METHOD GB28181_RtpRecv_demux_callback(_ps_demux_cb* cb)
{
	CNetGB28181RtpServer* pThis = (CNetGB28181RtpServer*)cb->userdata;
	if (!pThis->bRunFlag)
		return;
 
	if (pThis && cb->streamtype == e_rtpdepkt_st_h264 || cb->streamtype == e_rtpdepkt_st_h265 ||
		cb->streamtype == e_rtpdepkt_st_mpeg4 || cb->streamtype == e_rtpdepkt_st_mjpeg)
	{
		if (pThis->pMediaSource == NULL)
		{
			pThis->pMediaSource = CreateMediaStreamSource(pThis->m_szShareMediaURL, pThis->hParent, MediaSourceType_LiveMedia, 0, pThis->m_h265ConvertH264Struct);
			if (pThis->pMediaSource != NULL)
				pThis->pMediaSource->netBaseNetType = pThis->netBaseNetType;
		}
		
		if(pThis->pMediaSource == NULL)
		{
			pThis->bRunFlag = false;
 			DeleteNetRevcBaseClient(pThis->nClient);
			return ;
		}

		if (pThis->m_addStreamProxyStruct.disableVideo[0] == 0x30)
		{//�������û�й��˵���Ƶ
 			if (cb->streamtype == e_rtpdepkt_st_h264)
				pThis->pMediaSource->PushVideo(cb->data, cb->datasize, "H264");
			else if (cb->streamtype == e_rtpdepkt_st_h265)
				pThis->pMediaSource->PushVideo(cb->data, cb->datasize, "H265");
		}

		if (!pThis->bUpdateVideoFrameSpeedFlag)
		{//������ƵԴ��֡�ٶ�
			int nVideoSpeed = pThis->CalcFlvVideoFrameSpeed(cb->pts, 90000);
			if (nVideoSpeed > 0 && pThis->pMediaSource != NULL)
			{
				pThis->bUpdateVideoFrameSpeedFlag = true;

				//����UDP��TCP������Ϊ�����Ѿ����� 
				boost::shared_ptr<CNetRevcBase>  pGB28181Proxy = GetNetRevcBaseClient(pThis->hParent);
				if (pGB28181Proxy != NULL)
					pGB28181Proxy->bUpdateVideoFrameSpeedFlag = true;

				if (pThis->pMediaSource)
				{
					pThis->pMediaSource->enable_mp4 = strcmp(pThis->m_addStreamProxyStruct.enable_mp4, "1") == 0 ? true : false;//��¼�Ƿ�¼��
					pThis->pMediaSource->enable_hls = strcmp(pThis->m_addStreamProxyStruct.enable_hls, "1") == 0 ? true : false;//��¼�Ƿ���hls
				}

				WriteLog(Log_Debug, "nClient = %llu , ������ƵԴ %s ��֡�ٶȳɹ�����ʼ�ٶ�Ϊ%d ,���º���ٶ�Ϊ%d, ", pThis->nClient, pThis->pMediaSource->m_szURL, pThis->pMediaSource->m_mediaCodecInfo.nVideoFrameRate, nVideoSpeed);
				pThis->pMediaSource->UpdateVideoFrameSpeed(nVideoSpeed, pThis->netBaseNetType);
			}
		}
	}
	else 
	{
		if (pThis->pMediaSource == NULL)
		{
			pThis->pMediaSource = CreateMediaStreamSource(pThis->m_szShareMediaURL, pThis->hParent, MediaSourceType_LiveMedia, 0, pThis->m_h265ConvertH264Struct);
			if (pThis->pMediaSource)
			{
				pThis->pMediaSource->netBaseNetType = pThis->netBaseNetType;
				pThis->pMediaSource->enable_mp4 = strcmp(pThis->m_addStreamProxyStruct.enable_mp4, "1") == 0 ? true : false;//��¼�Ƿ�¼��
				pThis->pMediaSource->enable_hls = strcmp(pThis->m_addStreamProxyStruct.enable_hls, "1") == 0 ? true : false;//��¼�Ƿ���hls
			}
		}

		if (pThis->pMediaSource == NULL)
		{
			pThis->bRunFlag = false;
			DeleteNetRevcBaseClient(pThis->nClient);
			return ;
		}

		if (pThis->m_addStreamProxyStruct.disableAudio[0] == 0x30)
		{//��Ƶû�й���
			if (cb->streamtype == e_rtpdepkt_st_aac)
			{//aac
				pThis->GetAACAudioInfo(cb->data, cb->datasize);//��ȡAACý����Ϣ
				pThis->pMediaSource->PushAudio(cb->data, cb->datasize, pThis->mediaCodecInfo.szAudioName, pThis->mediaCodecInfo.nChannels, pThis->mediaCodecInfo.nSampleRate);
			}
			else if (cb->streamtype == e_rtpdepkt_st_g711a)
			{// G711A  
				pThis->pMediaSource->PushAudio(cb->data, cb->datasize, "G711_A", 1, 8000);
			}
			else if (cb->streamtype == e_rtpdepkt_st_g711u)
			{// G711U  
				pThis->pMediaSource->PushAudio(cb->data, cb->datasize, "G711_U", 1, 8000);
			}
		}
	}
}

CNetGB28181RtpServer::CNetGB28181RtpServer(NETHANDLE hServer, NETHANDLE hClient, char* szIP, unsigned short nPort,char* szShareMediaURL)
{
 #ifdef WriteRecvAACDataFlag
	fWriteRecvAACFile = fopen("e:\\recvES.aac", "wb");
#endif
#ifdef WriteSendPsFileFlag
	char szFileName[256] = { 0 };
	sprintf(szFileName, "%s%X.ps", ABL_MediaSeverRunPath, this);
	fWriteSendPsFile = fopen(szFileName, "wb");
#endif 
#ifdef  WriteRtpFileFlag
	char szFileName[256] = { 0 };
	sprintf(szFileName, "%s%X.rtp", ABL_MediaSeverRunPath, this);
	fWriteRtpFile = fopen(szFileName, "wb");
#endif
#ifdef  WriteRtpTimestamp
	nRtpTimestampType = -1;
	nRptDataArrayOrder = 0;
	bCheckRtpTimestamp = false;
	nStartTimestap = 3600;
#endif 
 	aacDataLength = 0;
	nMaxRtpSendVideoMediaBufferLength = 640;//Ĭ���ۼ�640
	nSendRtpVideoMediaBufferLength = 0; //�Ѿ����۵ĳ���  ��Ƶ
	nStartVideoTimestamp = GB28181VideoStartTimestampFlag; //��һ֡��Ƶ��ʼʱ��� ��
	nCurrentVideoTimestamp = 0;// ��ǰ֡ʱ���

	addThreadPoolFlag = false;
	videoPTS = audioPTS = 0;
	nVideoStreamID = nAudioStreamID = -1;
	handler.alloc = NetGB28181RtpServer_ps_alloc;
	handler.write = NetGB28181RtpServer_ps_write;
	handler.free = NetGB28181RtpServer_ps_free;
	videoPTS = audioPTS = 0;
	s_buffer = NULL;
	psBeiJingLaoChenMuxer = NULL;
	s_buffer = NULL;
	hRtpPS = 0;
	nClientPort = nPort;
	nClientRtcp = hParent = 0;
	fWritePsFile = NULL;
	nRtpRtcpPacketType = 0;
	strcpy(m_szShareMediaURL,szShareMediaURL);
	nClient = hClient;
	nServer = hServer;
	m_gbPayload = 96;
	hRtpHandle = psDeMuxHandle  = 0;
	psBeiJingLaoChen = NULL;
	memset((char*)&mediaCodecInfo, 0x00, sizeof(mediaCodecInfo));
	bInitFifoFlag = false;
	pMediaSource = NULL; 
	netDataCache = NULL ; //�������ݻ���
	netDataCacheLength = 0;//�������ݻ����С
	nNetStart = nNetEnd = 0; //����������ʼλ��\����λ��
	MaxNetDataCacheCount = MaxNetDataCacheBufferLength;
	nSendRtcpTime = 0;
	pRtpAddress = pSrcAddress = NULL;
	bRunFlag = true;
	fWritePsFile  = NULL ;
	pWriteRtpFile = NULL ;
	strcpy(szClientIP, szIP);
	
	if (ABL_MediaServerPort.nSaveGB28181Rtp == 1)
	{
		char szVFile[256];
		sprintf(szVFile, "%s%llu_%X.ps", ABL_MediaServerPort.debugPath, nClient, this);
 	    fWritePsFile = fopen(szVFile,"wb") ;
		sprintf(szVFile, "%s%llu_%X.rtp", ABL_MediaServerPort.debugPath, nClient, this);
		pWriteRtpFile = fopen(szVFile, "wb");
	}

 	WriteLog(Log_Debug, "CNetGB28181RtpServer ���� = %X  nClient = %llu ", this, nClient);
}

CNetGB28181RtpServer::~CNetGB28181RtpServer()
{
	bRunFlag = false;
	std::lock_guard<std::mutex> lock(netDataLock);

	if (netBaseNetType == NetBaseNetType_NetGB28181RtpServerUDP)
	{
		XHNetSDK_DestoryUdp(nClient);
	}
	else if (netBaseNetType == NetBaseNetType_NetGB28181RtpServerTCP_Server)
	{
	}

	if (ABL_MediaServerPort.gb28181LibraryUse == 1)
	{//����
		if (psDeMuxHandle > 0)
		{
			ps_demux_stop(psDeMuxHandle);
			psDeMuxHandle = 0;
		}
	}
	else
	{//�����ϳ�
		if (psBeiJingLaoChen != NULL)
		{
			ps_demuxer_destroy(psBeiJingLaoChen);
			psBeiJingLaoChen = NULL;
		}
	}
 
	if (hRtpHandle > 0)
	{
		rtp_depacket_stop(hRtpHandle);
		hRtpHandle = 0;
 	}

	if (netBaseNetType == NetBaseNetType_NetGB28181RtpServerUDP)
	{
	  if(bInitFifoFlag)
	    NetDataFifo.FreeFifo();
	}
	else if (netBaseNetType == NetBaseNetType_NetGB28181RtpServerTCP_Server || netBaseNetType == NetBaseNetType_NetGB28181RtpServerTCP_Active )
	{
	  SAFE_ARRAY_DELETE(netDataCache);
 	}
 
	if (fWritePsFile)
	{
	    fclose(fWritePsFile);
		fWritePsFile = NULL;
	 }
	 if (pWriteRtpFile)
	 {
		 fclose(pWriteRtpFile);
		 pWriteRtpFile = NULL;
	 }
 
	 if(hRtpPS > 0)
	   rtp_packet_stop(hRtpPS);
	 if (psBeiJingLaoChenMuxer != NULL)
		 ps_muxer_destroy(psBeiJingLaoChenMuxer);

	 SAFE_DELETE(pRtpAddress);
	 SAFE_DELETE(pSrcAddress);
	 SAFE_ARRAY_DELETE(s_buffer);
 
	 malloc_trim(0);

	 if (hParent > 0 && netBaseNetType != NetBaseNetType_NetGB28181RtpServerTCP_Active)
	 {
		 WriteLog(Log_Debug, "CNetGB28181RtpServer = %X �������������������� hParent = %llu, nClient = %llu ,nMediaClient = %llu", this, hParent, nClient, nMediaClient);
		 pDisconnectBaseNetFifo.push((unsigned char*)&hParent, sizeof(hParent));
	 }

	 //����rtcp 
	 if (nClientRtcp > 0)
		 XHNetSDK_DestoryUdp(nClientRtcp); 

	 //����ɾ��ý��Դ
	 if (strlen(m_szShareMediaURL) > 0 && pMediaSource != NULL )
	    DeleteMediaStreamSource(m_szShareMediaURL);

#ifdef WriteSendPsFileFlag
	 if(fWriteSendPsFile)
 	    fclose(fWriteSendPsFile);
#endif 
#ifdef  WriteRtpFileFlag
	 if(fWriteRtpFile)
 	   fclose(fWriteRtpFile);
#endif
#ifdef WriteRecvAACDataFlag
	 fclose(fWriteRecvAACFile);
#endif
	 //����û�дﵽ֪ͨ
	 if (ABL_MediaServerPort.hook_enable == 1 && ABL_MediaServerPort.nClientNotArrive > 0 && bUpdateVideoFrameSpeedFlag == false)
	 {
		 MessageNoticeStruct msgNotice;
		 msgNotice.nClient = ABL_MediaServerPort.nClientNotArrive;
		 sprintf(msgNotice.szMsg, "{\"mediaServerId\":\"%s\",\"app\":\"%s\",\"stream\":\"%s\",\"networkType\":%d,\"key\":%llu}", ABL_MediaServerPort.mediaServerID,m_addStreamProxyStruct.app, m_addStreamProxyStruct.stream,  netBaseNetType, nClient);
		 pMessageNoticeFifo.push((unsigned char*)&msgNotice, sizeof(MessageNoticeStruct));
	 }

	 WriteLog(Log_Debug, "CNetGB28181RtpServer ���� = %X  nClient = %llu , hParent= %llu , app = %s ,stream = %s ,bUpdateVideoFrameSpeedFlag = %d ", this, nClient, hParent, m_addStreamProxyStruct.app, m_addStreamProxyStruct.stream, bUpdateVideoFrameSpeedFlag);
}

int CNetGB28181RtpServer::PushVideo(uint8_t* pVideoData, uint32_t nDataLength, char* szVideoCodec)
{
	std::lock_guard<std::mutex> lock(netDataLock);

	if (!bRunFlag || psBeiJingLaoChenMuxer == NULL || m_openRtpServerStruct.send_disableVideo[0] == 0x31)
		return -1;

	if (nVideoStreamID != -1 && psBeiJingLaoChenMuxer != NULL && strlen(mediaCodecInfo.szVideoName) > 0)
	{
		if (strcmp(mediaCodecInfo.szVideoName, "H264") == 0)
			nflags = CheckVideoIsIFrame("H264", pVideoData, nDataLength);
		else if (strcmp(mediaCodecInfo.szVideoName, "H265") == 0)
			nflags = CheckVideoIsIFrame("H265", pVideoData, nDataLength);

		ps_muxer_input((ps_muxer_t*)psBeiJingLaoChenMuxer, nVideoStreamID, nflags, videoPTS, videoPTS, pVideoData, nDataLength);
		videoPTS += (90000 / mediaCodecInfo.nVideoFrameRate);
	}
	return 0;
}

int CNetGB28181RtpServer::PushAudio(uint8_t* pAudioData, uint32_t nDataLength, char* szAudioCodec, int nChannels, int SampleRate)
{
	std::lock_guard<std::mutex> lock(netDataLock);

	if (!bRunFlag || psBeiJingLaoChenMuxer == NULL || m_openRtpServerStruct.send_disableAudio[0] == 0x31)
		return -1;

	if (nAudioStreamID != -1 && psBeiJingLaoChen != NULL && strlen(mediaCodecInfo.szAudioName) > 0)
	{
		ps_muxer_input((ps_muxer_t*)psBeiJingLaoChenMuxer, nAudioStreamID, 0, audioPTS, audioPTS, pAudioData, nDataLength);

		if (strcmp(mediaCodecInfo.szAudioName, "AAC") == 0)
			audioPTS += mediaCodecInfo.nBaseAddAudioTimeStamp;
		else if (strcmp(mediaCodecInfo.szAudioName, "G711_A") == 0 || strcmp(mediaCodecInfo.szAudioName, "G711_U") == 0)
			audioPTS += nDataLength / 8;
	}
	return 0;
}

int CNetGB28181RtpServer::SendVideo()
{

	return 0;
}

int CNetGB28181RtpServer::SendAudio()
{

	return 0;
}

int CNetGB28181RtpServer::InputNetData(NETHANDLE nServerHandle, NETHANDLE nClientHandle, uint8_t* pData, uint32_t nDataLength, void* address)
{
	if (!bRunFlag || nDataLength <= 0 )
		return -1;
	std::lock_guard<std::mutex> lock(netDataLock);
 
#ifdef  WriteRtpFileFlag
	if (fWriteRtpFile)
	{
		fwrite((unsigned char*)&nDataLength, 1, sizeof(nDataLength), fWriteRtpFile);
		fwrite(pData, 1, nDataLength, fWriteRtpFile);
		fflush(fWriteRtpFile);
		return 0;
     }
#endif

	//���ӱ���ԭʼ��rtp���ݣ����еײ����
	if (ABL_MediaServerPort.nSaveGB28181Rtp == 1 && pWriteRtpFile != NULL && (GetTickCount64() - nCreateDateTime) < 1000 * 300)
	{
		fwrite(pData, 1, nDataLength, pWriteRtpFile);
		fflush(pWriteRtpFile);
	}

	//������߼��
	nRecvDataTimerBySecond = 0;

	if (netBaseNetType == NetBaseNetType_NetGB28181RtpServerUDP)
	{//UDP
		if (pRtpAddress == NULL && pSrcAddress == NULL )
		{
			pRtpAddress = new sockaddr_in;
			pSrcAddress = new sockaddr_in;
			memcpy((char*)pRtpAddress, (char*)address, sizeof(sockaddr_in));
			memcpy((char*)pSrcAddress, (char*)address, sizeof(sockaddr_in));
			unsigned short nPort = ntohs(pRtpAddress->sin_port);
			pRtpAddress->sin_port = htons(nPort + 1);
			sprintf(m_addStreamProxyStruct.url, "rtp://%s:%d/%s/%s", inet_ntoa(pSrcAddress->sin_addr), ntohs(pSrcAddress->sin_port), m_addStreamProxyStruct.app, m_addStreamProxyStruct.stream);
 		}

		//��ȡ��һ��ssrc 
		if (nSSRC == 0 && pData != NULL &&  nDataLength > sizeof(rtpHeader) )
		{
			rtpHeaderPtr = (_rtp_header*)pData;
			nSSRC = rtpHeaderPtr->ssrc;
		}else 
			rtpHeaderPtr = (_rtp_header*)pData;

		if (!bInitFifoFlag)
		{
	        NetDataFifo.InitFifo(MaxLiveingVideoFifoBufferLength);
			bInitFifoFlag = true;
	    }

		if (address && pRtpAddress)
		{//��ֻ֤�ϵ�һ��IP���˿ڽ�����PS���� 
			if (strcmp(inet_ntoa(pRtpAddress->sin_addr), inet_ntoa(((sockaddr_in*)address)->sin_addr)) == 0 &&  //IP ��ͬ
				ntohs(pRtpAddress->sin_port) - 1 == ntohs(((sockaddr_in*)address)->sin_port) &&  //�˿���ͬ
				nSSRC == rtpHeaderPtr->ssrc  // ssrc ��ͬ 
				)
			{
 	         NetDataFifo.push((unsigned char*)pData, nDataLength);
			 if (nClientPort == 0)
			 {
				 strcpy(szClientIP, inet_ntoa(pRtpAddress->sin_addr));
				 nClientPort = ntohs(((sockaddr_in*)address)->sin_port);
			 }
			}
 		}
	}
	else if (netBaseNetType == NetBaseNetType_NetGB28181RtpServerTCP_Server || netBaseNetType == NetBaseNetType_NetGB28181RtpServerTCP_Active )
	{//TCP 
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
					WriteLog(Log_Debug, "CNetGB28181RtpServer = %X nClient = %llu �����쳣 , ִ��ɾ��", this, nClient);
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

int CNetGB28181RtpServer::ProcessNetData()
{
	std::lock_guard<std::mutex> lock(netDataLock);
	if(!bRunFlag)
		return -1 ;

	//���ݵ���Ŵ����ظ���rtp���� 
	CreateSendRtpByPS();

	unsigned char* pData = NULL;
	int            nLength;

	if (netBaseNetType == NetBaseNetType_NetGB28181RtpServerUDP)
	{//UDP
		pData = NetDataFifo.pop(&nLength);
		if (pData != NULL)
		{
			if (nLength > 0)
			{
				//����rtpͷ�����payload���н��,��Ч��ֹ�û���д��
				if (hRtpHandle == 0)
				{
					rtpHeadPtr = (_rtp_header*)pData ;
					m_gbPayload = rtpHeadPtr->payload;
				}

				RtpDepacket(pData, nLength);
			}

			NetDataFifo.pop_front();
		}

		//��������rtcp ��
		if (GetTickCount64() - nSendRtcpTime >= 5 * 1000 && psBeiJingLaoChenMuxer == NULL ) // psBeiJingLaoChenMuxer == NULL ʱ ��û�лظ� /send_app/send_stream 
		{
			nSendRtcpTime = ::GetTickCount64();

			memset(szRtcpSRBuffer, 0x00, sizeof(szRtcpSRBuffer));
			rtcpSRBufferLength = sizeof(szRtcpSRBuffer);
			rtcpRR.BuildRtcpPacket(szRtcpSRBuffer, rtcpSRBufferLength, nSSRC);

			XHNetSDK_Sendto(nClientRtcp, szRtcpSRBuffer, rtcpSRBufferLength, pRtpAddress);
 		}
	}
	else if (netBaseNetType == NetBaseNetType_NetGB28181RtpServerTCP_Server || netBaseNetType == NetBaseNetType_NetGB28181RtpServerTCP_Active )
	{//TCP ��ʽ��rtp����ȡ 
		while (netDataCacheLength > 2048)
		{//���ܻ���̫��buffer,������ɽ��չ�������ֻ����Ƶ��ʱ���������ʱ�ܴ� 2048 �ȽϺ��� 
			memcpy(rtpHeadOfTCP, netDataCache + nNetStart, 2);
			if ((rtpHeadOfTCP[0] == 0x24 && rtpHeadOfTCP[1] == 0x00) || (rtpHeadOfTCP[0] == 0x24 && rtpHeadOfTCP[1] == 0x01) || (rtpHeadOfTCP[0] == 0x24 ))
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
			if (nRtpLength > 65535 )
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

		//��������rtcp ��
		if (GetTickCount64() - nSendRtcpTime >= 5 * 1000 && psBeiJingLaoChenMuxer == NULL )  // psBeiJingLaoChenMuxer == NULL ʱ ��û�лظ� /send_app/send_stream 
		{
			nSendRtcpTime = ::GetTickCount64();

			memset(szRtcpSRBuffer, 0x00, sizeof(szRtcpSRBuffer));
			rtcpSRBufferLength = sizeof(szRtcpSRBuffer);
			rtcpRR.BuildRtcpPacket(szRtcpSRBuffer, rtcpSRBufferLength, nSSRC);

			ProcessRtcpData(szRtcpSRBuffer, rtcpSRBufferLength, 1);
		}
 	}
 	return 0;
}

//rtp ���
struct ps_demuxer_notify_t notify_NetGB28181RtpServer = { mpeg_ps_dec_testonstream,};

bool  CNetGB28181RtpServer::RtpDepacket(unsigned char* pData, int nDataLength) 
{
	if (pData == NULL || nDataLength > 65536 || !bRunFlag || nDataLength < 12 )
		return false;

	//����rtp���
	if (hRtpHandle == 0)
	{
		rtp_depacket_start(GB28181_rtppacket_callback_recv, (void*)this, (uint32_t*)&hRtpHandle);

		if (m_addStreamProxyStruct.RtpPayloadDataType[0] == 0x31)
		{//rtp + PS
		  rtp_depacket_setpayload(hRtpHandle, m_gbPayload, e_rtpdepkt_st_gbps);
 		}
		else if (m_addStreamProxyStruct.RtpPayloadDataType[0] == 0x32)
		{//rtp + ES
			if(m_gbPayload == 98)
			  rtp_depacket_setpayload(hRtpHandle, m_gbPayload, e_rtpdepkt_st_h264);
			else if(m_gbPayload == 99)
			  rtp_depacket_setpayload(hRtpHandle, m_gbPayload, e_rtpdepkt_st_h265);
		}
		else if (m_addStreamProxyStruct.RtpPayloadDataType[0] == 0x33)
		{//rtp + xhb
			rtp_depacket_setpayload(hRtpHandle, m_gbPayload, e_rtpdepkt_st_xhb);
		}
 
 		WriteLog(Log_Debug, "CNetGB28181RtpServer = %X ,����rtp��� hRtpHandle = %d ,psDeMuxHandle = %d", this, hRtpHandle, psDeMuxHandle);
	}

	if (ABL_MediaServerPort.gb28181LibraryUse == 1)
	{//����
		if(psDeMuxHandle == 0)
		  ps_demux_start(GB28181_RtpRecv_demux_callback, (void*)this, e_ps_dux_timestamp, &psDeMuxHandle);
	}
	else
	{//�����ϳ�
		if (psBeiJingLaoChen == NULL)
		{
			psBeiJingLaoChen = ps_demuxer_create(on_gb28181_unpacket, this);
			if(psBeiJingLaoChen != NULL )
		      ps_demuxer_set_notify(psBeiJingLaoChen, &notify_NetGB28181RtpServer, this);
		}
	}

	if (hRtpHandle > 0 )
	{
		if (m_addStreamProxyStruct.RtpPayloadDataType[0] == 0x31 || m_addStreamProxyStruct.RtpPayloadDataType[0] == 0x32)
		{//rtp + PS �� rtp + ES 
#ifdef  WriteRtpTimestamp
			if (!bCheckRtpTimestamp)
			{
				if (nDataLength > 0 && nDataLength < (16 * 1024))
				{
					memcpy(szRtpDataArray[nRptDataArrayOrder], pData, nDataLength);
					nRtpDataArrayLength[nRptDataArrayOrder] = nDataLength;
				}
				nRptDataArrayOrder ++;
				if (nRptDataArrayOrder >= 3)
				{//��3֡
					bCheckRtpTimestamp = true;
					unsigned char szZeroTimestamp[4] = { 0 };
					if (memcmp(szRtpDataArray[0] + 4, szZeroTimestamp, 4) == 0 && memcmp(szRtpDataArray[1] + 4, szZeroTimestamp, 4) == 0 && memcmp(szRtpDataArray[2] + 4, szZeroTimestamp, 4) == 0)
					{//ʱ���Ϊ0 
						nRtpTimestampType = 1;

						for (int i = 0; i < 3; i++)
						{//��ԭ�������rtp�����н��
 							writeRtpHead = (_rtp_header*)szRtpDataArray[i];
							nWriteTimeStamp = nStartTimestap;
							nWriteTimeStamp = htonl(nWriteTimeStamp);
							memcpy(szRtpDataArray[i] + 4, (char*)&nWriteTimeStamp, sizeof(nWriteTimeStamp));
							if (writeRtpHead->mark == 1)
								nStartTimestap += 3600;
							rtp_depacket_input(hRtpHandle, szRtpDataArray[i], nRtpDataArrayLength[i]);
						}
					}
					else
					{//������ʱ���
						nRtpTimestampType = 2;
 						for (int i = 0; i < 3; i++)//��ԭ�������rtp�����н��
 							rtp_depacket_input(hRtpHandle, szRtpDataArray[i], nRtpDataArrayLength[i]);
  					}
 					return true ; //�Ѿ��ѻ����3֡�������
  				}
			}
 
			if (nRtpTimestampType == 1)
			{
			   writeRtpHead = (_rtp_header*)pData;
			   nWriteTimeStamp = nStartTimestap;
			   nWriteTimeStamp = htonl(nWriteTimeStamp);
			   memcpy(pData + 4, (char*)&nWriteTimeStamp, sizeof(nWriteTimeStamp));
 			   if(writeRtpHead->mark == 1)
			      nStartTimestap += 3600;
			   if(nStartTimestap >= 0xFFFFFFFF - (3600 * 25))
				   nStartTimestap = 3600 ; //ʱ�����ת
  			}
			if (nRtpTimestampType >= 1)
				rtp_depacket_input(hRtpHandle, pData, nDataLength);
#else
			rtp_depacket_input(hRtpHandle, pData, nDataLength);
#endif
		}
		else if (m_addStreamProxyStruct.RtpPayloadDataType[0] == 0x33)
		{// rtp + XHB  ��rtp ��ͷ����չ 
 		   rtpHeaderXHB = (_rtp_header*)pData;
		   if (rtpHeaderXHB->x == 1 && nDataLength > 16 )
		   {
			   memcpy((unsigned char*)&rtpExDataLength, pData + 12 + 2, sizeof(rtpExDataLength));
			   rtpExDataLength = ntohs(rtpExDataLength) * 4 ;

			   if (nDataLength - (12 + 4 + rtpExDataLength ) > 0  )
			   {
			     pData[0] = 0x80;
			     memmove(pData + 12, pData + (12 + 4 + rtpExDataLength), nDataLength - (12 + 4 + rtpExDataLength));
			     rtp_depacket_input(hRtpHandle, pData, nDataLength - ( 4 + rtpExDataLength) );
			   }
		   }else
			   rtp_depacket_input(hRtpHandle, pData, nDataLength);
		}
  	}

	return true;
}

//TCP��ʽ����rtcp��
void  CNetGB28181RtpServer::ProcessRtcpData(unsigned char* szRtcpData, int nDataLength, int nChan)
{
	if ( !(netBaseNetType == NetBaseNetType_NetGB28181RtpServerTCP_Server || netBaseNetType == NetBaseNetType_NetGB28181RtpServerTCP_Active) || !bRunFlag)
		return;

	if (nRtpRtcpPacketType == 1)
	{//2�ֽڷ�ʽ
		unsigned short nRtpLen = htons(nDataLength);
		memcpy(szRtcpDataOverTCP, (unsigned char*)&nRtpLen, sizeof(nRtpLen));

		memcpy(szRtcpDataOverTCP + 2, szRtcpData, nDataLength);
		XHNetSDK_Write(nClient, szRtcpDataOverTCP, nDataLength + 2 , 1);
	}
	else if (nRtpRtcpPacketType == 2)
	{///4�ֽڷ�ʽ
	  szRtcpDataOverTCP[0] = '$';
	  szRtcpDataOverTCP[1] = nChan;
	  unsigned short nRtpLen = htons(nDataLength);
	  memcpy(szRtcpDataOverTCP + 2, (unsigned char*)&nRtpLen, sizeof(nRtpLen));

	  memcpy(szRtcpDataOverTCP + 4, szRtcpData, nDataLength);
	  XHNetSDK_Write(nClient, szRtcpDataOverTCP, nDataLength + 4, 1);
	}
}

//rtp����ص���Ƶ
void GB28181RtpServer_rtp_packet_callback_func_send(_rtp_packet_cb* cb)
{
	CNetGB28181RtpServer* pThis = (CNetGB28181RtpServer*)cb->userdata;
	if (pThis == NULL || !pThis->bRunFlag)
		return;

	if (pThis->netBaseNetType == NetBaseNetType_NetGB28181RtpServerUDP)
	{//udp ֱ�ӷ��� 
	    pThis->SendGBRtpPacketUDP(cb->data, cb->datasize);
	}
	else if (pThis->netBaseNetType == NetBaseNetType_NetGB28181RtpServerTCP_Server || pThis->netBaseNetType == NetBaseNetType_NetGB28181RtpServerTCP_Active)
	{//TCP ��Ҫƴ�� ��ͷ
		pThis->GB28181SentRtpVideoData(cb->data, cb->datasize);
	}
}

//�����ظ�rtp\ps 
void  CNetGB28181RtpServer::CreateSendRtpByPS()
{
	if (strlen(m_openRtpServerStruct.send_app) > 0 && strlen(m_openRtpServerStruct.send_stream_id) > 0 && addThreadPoolFlag == false  )
	{//��֤�����̳߳�ֻ��һ��
		addThreadPoolFlag = true;

		char szMediaSource[512] = { 0 };
		sprintf(szMediaSource, "/%s/%s", m_openRtpServerStruct.send_app, m_openRtpServerStruct.send_stream_id);
		boost::shared_ptr<CMediaStreamSource> pMediaSource = GetMediaStreamSource(szMediaSource);

		if (pMediaSource != NULL)
		{//�ѿͻ��� ����Դ��ý�忽������
			pMediaSource->AddClientToMap(nClient);
		}	
	}
 
	if (strlen(m_openRtpServerStruct.send_app) > 0 && strlen(m_openRtpServerStruct.send_stream_id) > 0 &&  mediaCodecInfo.nVideoFrameRate > 0 && hRtpPS == 0 )
	{
		if (strlen(mediaCodecInfo.szVideoName) == 0 )
			nMaxRtpSendVideoMediaBufferLength = 640;
		else
			nMaxRtpSendVideoMediaBufferLength = MaxRtpSendVideoMediaBufferLength;
 
		//����rtp 
		int nRet = rtp_packet_start(GB28181RtpServer_rtp_packet_callback_func_send, (void*)this, &hRtpPS);
		if (nRet != e_rtppkt_err_noerror)
		{
			WriteLog(Log_Debug, "CNetGB28181RtpClient = %X ��������Ƶrtp���ʧ��,nClient = %llu,  nRet = %d", this, nClient, nRet);
			return;
		}
		optionPS.handle = hRtpPS;
		optionPS.mediatype = e_rtppkt_mt_video;
 		optionPS.payload = 96 ;
		optionPS.streamtype = e_rtppkt_st_gb28181;
 		optionPS.ssrc =  rand() ;
		optionPS.ttincre = (90000 / mediaCodecInfo.nVideoFrameRate);
		rtp_packet_setsessionopt(&optionPS);

		inputPS.handle = hRtpPS;
		inputPS.ssrc = optionPS.ssrc;

		//����PS
		s_buffer = new  char[IDRFrameMaxBufferLength];
		psBeiJingLaoChenMuxer = ps_muxer_create(&handler, this);

		if (nVideoStreamID == -1 && psBeiJingLaoChenMuxer != NULL && m_openRtpServerStruct.send_disableVideo[0] == 0x30 )
		{//������Ƶ 
			if (strcmp(mediaCodecInfo.szVideoName, "H264") == 0)
				nVideoStreamID = ps_muxer_add_stream((ps_muxer_t*)psBeiJingLaoChenMuxer, PSI_STREAM_H264, NULL, 0);
			else if (strcmp(mediaCodecInfo.szVideoName, "H265") == 0)
				nVideoStreamID = ps_muxer_add_stream((ps_muxer_t*)psBeiJingLaoChenMuxer, PSI_STREAM_H265, NULL, 0);
		}

		if (nAudioStreamID == -1 && psBeiJingLaoChenMuxer != NULL && strlen(mediaCodecInfo.szAudioName) > 0 && m_openRtpServerStruct.send_disableAudio[0] == 0x30)
		{//������Ƶ
			if (strcmp(mediaCodecInfo.szAudioName, "AAC") == 0)
				nAudioStreamID = ps_muxer_add_stream((ps_muxer_t*)psBeiJingLaoChenMuxer, PSI_STREAM_AAC, NULL, 0);
			else if (strcmp(mediaCodecInfo.szAudioName, "G711_A") == 0)
				nAudioStreamID = ps_muxer_add_stream((ps_muxer_t*)psBeiJingLaoChenMuxer, PSI_STREAM_AUDIO_G711A, NULL, 0);
			else if (strcmp(mediaCodecInfo.szAudioName, "G711_U") == 0)
				nAudioStreamID = ps_muxer_add_stream((ps_muxer_t*)psBeiJingLaoChenMuxer, PSI_STREAM_AUDIO_G711U, NULL, 0);
		}
 	}
}

//PS ���ݴ����rtp 
void  CNetGB28181RtpServer::GB28181PsToRtPacket(unsigned char* pPsData, int nLength)
{
	if (hRtpPS > 0 && bRunFlag)
	{
		inputPS.data = pPsData;
		inputPS.datasize = nLength;
		rtp_packet_input(&inputPS);
	}
}

//udp��ʽ����rtp��
void  CNetGB28181RtpServer::SendGBRtpPacketUDP(unsigned char* pRtpData, int nLength)
{
	if(pSrcAddress != NULL && bRunFlag && pRtpData != NULL && nLength > 0 )
	  XHNetSDK_Sendto(nClient, pRtpData, nLength, pSrcAddress);
}

//����28181PS����TCP��ʽ���� 
void  CNetGB28181RtpServer::GB28181SentRtpVideoData(unsigned char* pRtpVideo, int nDataLength)
{
	if (bRunFlag == false  )
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

//׷��adts��Ϣͷ
void  CNetGB28181RtpServer::AddADTSHeadToAAC(unsigned char* szData, int nAACLength)
{
	aacDataLength  = nAACLength + 7;
	uint8_t profile = 2;
	uint8_t channel_configuration = mediaCodecInfo.nChannels;
	aacData[0] = 0xFF; /* 12-syncword */
	aacData[1] = 0xF0 /* 12-syncword */ | (0 << 3)/*1-ID*/ | (0x00 << 2) /*2-layer*/ | 0x01 /*1-protection_absent*/;
	aacData[2] = ((profile - 1) << 6) | ((sampling_frequency_index & 0x0F) << 2) | ((channel_configuration >> 2) & 0x01);
	aacData[3] = ((channel_configuration & 0x03) << 6) | ((aacDataLength >> 11) & 0x03); /*0-original_copy*/ /*0-home*/ /*0-copyright_identification_bit*/ /*0-copyright_identification_start*/
	aacData[4] = (uint8_t)(aacDataLength >> 3);
	aacData[5] = ((aacDataLength & 0x07) << 5) | 0x1F;
	aacData[6] = 0xFC | ((aacDataLength / 1024) & 0x03);

	memcpy(aacData + 7, szData, nAACLength);
}

//���͵�һ������
int CNetGB28181RtpServer::SendFirstRequst()
{
	if (netBaseNetType == NetBaseNetType_NetGB28181RtpServerTCP_Active)
	{//�ظ�http�������ӳɹ���
		sprintf(szResponseBody, "{\"code\":0,\"port\":%d,\"memo\":\"success\",\"key\":%llu}", nReturnPort, nClient);
		ResponseHttp(nClient_http, szResponseBody, false);
	}
	return 0;
}

//����m3u8�ļ�
bool  CNetGB28181RtpServer::RequestM3u8File()
{
	return true;
}