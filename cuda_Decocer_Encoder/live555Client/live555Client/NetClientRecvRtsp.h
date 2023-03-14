#ifndef _NetClientRecvRtsp_H
#define _NetClientRecvRtsp_H

#include "DigestAuthentication.hh"

#include "rtp_depacket.h"
#include "RtcpPacket.h"

#include <boost/unordered/unordered_map.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/unordered/unordered_map.hpp>
#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>

#define  MaxRtspValueCount          64 
#define  MaxRtspProtectCount        24 
#define   MaxRtpHandleCount         2

//#define  WriteRtpDepacketFileFlag   1 //�Ƿ�дrtp����ļ�
#define    PrintRtspLogFlag           1 //�Ƿ��ӡrtsp��������

#define    RtspServerRecvDataLength             1024*128      //�����и�����Ĵ�С 
#define    MaxRtpSendVideoMediaBufferLength     1024*64      //����ƴ��RTP����׼������ 
#define    MaxRtpSendAudioMediaBufferLength     1024*8       //����ƴ��RTP����׼������ 
#define    VideoStartTimestampFlag              0xEFEFEFEF   //��Ƶ��ʼʱ��� 

//HTTPͷ�ṹ
struct HttpHeadStruct
{
	char szKey[128];
	char szValue[512];
};

struct RtspFieldValue
{
	char szKey[64];
	char szValue[384];
};

//rtsp Э������
struct RtspProtect
{
	char  szRtspCmdString[512];//  OPTIONS rtsp://190.15.240.36:554/Camera_00001.sdp RTSP/1.0
	char  szRtspCommand[64];//  rtsp�������� ,OPTIONS ANNOUNCE SETUP  RECORD   
	char  szRtspURL[512];// rtsp://190.15.240.36:554/Camera_00001.sdp

	RtspFieldValue rtspField[MaxRtspValueCount];
	char  szRtspContentSDP[1024]; //  ý����������
	int   nRtspSDPLength;
};

class CNetClientRecvRtsp : public CNetRevcBase
{
public:
	CNetClientRecvRtsp(NETHANDLE hServer, NETHANDLE hClient2, char* szIP, unsigned short nPort, char* szShareMediaURL, void* pCustomerPtr, LIVE555RTSP_AudioVideo callbackFunc, uint64_t hParent, int nXHRtspURLType);
   ~CNetClientRecvRtsp() ;

   virtual int InputNetData(NETHANDLE nServerHandle, NETHANDLE nClientHandle, uint8_t* pData, uint32_t nDataLength) ;
   virtual int ProcessNetData();

   virtual int PushVideo(uint8_t* pVideoData, uint32_t nDataLength, char* szVideoCodec) ;//������Ƶ����
   virtual int PushAudio(uint8_t* pVideoData, uint32_t nDataLength, char* szAudioCodec, int nChannels, int SampleRate) ;//������Ƶ����

   virtual int SendVideo() ;//������Ƶ����
   virtual int SendAudio() ;//������Ƶ����
   virtual int SendFirstRequst();//���͵�һ������
   virtual bool RequestM3u8File();//����m3u8�ļ�

   bool         RtspPause();
   bool         RtspResume();
   bool         RtspSpeed(int nSpeed);
   bool         RtspSeek(int64_t nSeekTime);

   double                     dRange;
   _rtp_header                rtpHead;
   bool                       bPauseFlag;
   char                       szMediaCodecName[256];
   uint32_t                   psHandle; 
   volatile  bool             cbMediaCodecNameFlag;//�Ƿ�ص�ý����Ϣ


   bool                       FindRtpPacketFlag();

   int                        nRtspProcessStep;
   int                        nTrackIDOrer;
   int                        CSeq;
   unsigned int               nMediaCount;
   unsigned int               nSendSetupCount;
   char                       szWww_authenticate[384];//ժҪ��֤�������ɷ��������͹�����
   WWW_AuthenticateType       AuthenticateType;//rtsp��ʲô������֤
   char                       szBasic[512];//����rtsp������֤
   char                       szSessionID[512];//sessionID 
   char                       szTrackIDArray[16][512];

   bool  GetWWW_Authenticate();
   bool  getRealmAndNonce(char* szDigestString, char* szRealm, char* szNonce);
   void  SendPlay(WWW_AuthenticateType wwwType);
   void  SendSetup(WWW_AuthenticateType wwwType);
   void  SendDescribe(WWW_AuthenticateType wwwType);
   void  SendOptions(WWW_AuthenticateType wwwType,bool bRecordStatus);
   void  UserPasswordBase64(char* szUserPwdBase64);
   void  FindVideoAudioInSDP();

   unsigned char           s_extra_data[512];
   int                     extra_data_size;
   int                     sdp_h264_load(uint8_t* data, int bytes, const char* config);
#ifdef WriteRtpDepacketFileFlag
   bool                     bStartWriteFlag ;
#endif 

   int                     nSendRtpFailCount;//�ۼƷ���rtp��ʧ�ܴ��� 

   bool                    GetSPSPPSFromDescribeSDP();
   bool                    m_bHaveSPSPPSFlag;
   char                    m_szSPSPPSBuffer[512];
   char                    m_pSpsPPSBuffer[512];
   unsigned int            m_nSpsPPSLength;
   volatile bool           m_bCallBackSPSFlag;//�Ƿ�ص�SPS��PPS֡

   time_t                  nCurrentTime;
   int                     videoSSRC;
   int                     audioSSRC;
   bool                    bSendRRReportFlag;
   CRtcpPacketSR           rtcpSR;
   CRtcpPacketRR           rtcpRR;
   unsigned char           szRtcpSRBuffer[512];
   unsigned int            rtcpSRBufferLength;
   unsigned char           szRtcpDataOverTCP[1500];
   void                    SendRtcpReportData();//����rtcp �����,���Ͷ�
   void                    SendRtcpReportDataRR(unsigned int nSSRC, int nChan);//����rtcp �����,���ն�
   void                    ProcessRtcpData(char* szRtpData, int nDataLength, int nChan);

   int                     GetRtspPathCount(char* szRtspURL);//ͳ��rtsp URL ·������

   volatile                uint64_t tRtspProcessStartTime; //��ʼʱ��

   unsigned short          nVideoRtpLen, nAudioRtpLen;

   unsigned char            szRtpDataOverTCP[1500];
   unsigned char            szAudioRtpDataOverTCP[1500];

   uint32_t                hRtpVideo, hRtpAudio;
   int                     nVideoSSRC;

   char                    szRtspSDPContent[512];
   char                    szRtspAudioSDP[512];

   bool                    GetMediaInfoFromRtspSDP();
   void                    SplitterRtpAACData(unsigned char* rtpAAC, int nLength, uint32_t nTimestamp);
   int32_t                 XHNetSDKRead(NETHANDLE clihandle, uint8_t* buffer, uint32_t* buffsize, uint8_t blocked, uint8_t certain);
   bool                    ReadRtspEnd();

   int                     au_header_length;
   uint8_t                 *ptr, *pau, *pend;
   int                     au_size ; // only AU-size
   int                     au_numbers ;
   int                     SplitterSize[16];

   volatile bool           bRunFlag;

   std::mutex              netDataLock;
   unsigned char*          netDataCache; //�������ݻ���
   int                     netDataCacheLength;//�������ݻ����С
   int                     nNetStart, nNetEnd; //����������ʼλ��\����λ��
   int                     MaxNetDataCacheCount;

   unsigned char           data_[RtspServerRecvDataLength];//ÿһ֡rtsp���ݣ�����rtsp �� rtp �� 
   unsigned int            data_Length;
   unsigned short          nRtpLength;
   int                     nContentLength;

   RtspProtect      RtspProtectArray[MaxRtspProtectCount];
   int              RtspProtectArrayOrder;
 
   int             FindHttpHeadEndFlag();
   int             FindKeyValueFlag(char* szData);
   void            GetHttpModemHttpURL(char* szMedomHttpURL);
   int             FillHttpHeadToStruct();
   bool            GetFieldValue(char* szFieldName, char* szFieldValue);

   bool            bReadHeadCompleteFlag; //�Ƿ��ȡ���HTTPͷ
   int             nRecvLength;           //�Ѿ���ȡ��ϵĳ���
   unsigned char   szHttpHeadEndFlag[8];  //Httpͷ������־
   int             nHttpHeadEndLength;    //Httpͷ������־��ĳ��� 
   char            szResponseHttpHead[512];
   char            szCSeq[128];
   char            szTransport[256];

   char            szResponseBuffer[2048];
   int             nSendRet;
  static   uint64_t Session ;
  uint64_t         currentSession;
  char             szCurRtspURL[512];
  int64_t           nPrintCount;

   //ֻ����rtsp������� OPTIONS,DESCRIBE,SETUP,PALY 
   void            InputRtspData(unsigned char* pRecvData, int nDataLength);

   void           AddADTSHeadToAAC(unsigned char* szData, int nAACLength);
   unsigned char  aacData[2048];
   int            timeValue;
   uint32_t       hRtpHandle[MaxRtpHandleCount];
   char           szVideoName[64];
   char           szAudioName[64];
   int            nVideoPayload;
   int            nAudioPayload;
   int            sample_index;//����Ƶ������Ӧ����� 
   int            nChannels; //��Ƶͨ����
   int            nSampleRate; //��Ƶ����Ƶ��
   char           szMediaSourceURL[512];//ý������ַ������ /Media/Camera_00001 
   char           szRtspContentSDP[1024];
   char           szVideoSDP[512];
   char           szAudioSDP[512];
   CRtspABLSipParse   sipParseV, sipParseA;   //sdp ��Ϣ����
  
   volatile bool  bIsInvalidConnectFlag; //�Ƿ�Ϊ�Ƿ����� 
   volatile bool  bExitProcessFlagArray[3];

#ifdef WriteRtpDepacketFileFlag
   FILE*          fWriteRtpVideo;
   FILE*          fWriteRtpAudio;
#endif
};

#endif