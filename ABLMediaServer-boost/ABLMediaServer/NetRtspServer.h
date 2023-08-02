#ifndef _NetRtspServer_H
#define _NetRtspServer_H

#include "MediaStreamSource.h"
#include "rtp_packet.h"
#include "RtcpPacket.h"

#include <boost/unordered/unordered_map.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/unordered/unordered_map.hpp>
#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>

#include "mpeg4-avc.h"

#define  MaxRtspValueCount          64 
#define  MaxRtspProtectCount        24 
#define   MaxRtpHandleCount         2

//#define  WriteRtpDepacketFileFlag   1 //�Ƿ�дrtp����ļ�
//#define    WriteVideoDataFlag         1 //д����Ƶ��־  

#define    RtspServerRecvDataLength             1024*32      //�����и�����Ĵ�С 
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

//rtsp �� rtp ���紫������
enum RtspNetworkType
{
	RtspNetworkType_Unknow = -1, //δ֪
	RtspNetworkType_TCP    = 1,//TCP
	RtspNetworkType_UDP    = 2,//UDP 
};

class CNetRtspServer : public CNetRevcBase
{
public:
	CNetRtspServer(NETHANDLE hServer, NETHANDLE hClient, char* szIP, unsigned short nPort, char* szShareMediaURL);
   ~CNetRtspServer() ;

   virtual int InputNetData(NETHANDLE nServerHandle, NETHANDLE nClientHandle, uint8_t* pData, uint32_t nDataLength, void* address) ;
   virtual int ProcessNetData();

   virtual int PushVideo(uint8_t* pVideoData, uint32_t nDataLength, char* szVideoCodec) ;//������Ƶ����
   virtual int PushAudio(uint8_t* pVideoData, uint32_t nDataLength, char* szAudioCodec, int nChannels, int SampleRate) ;//������Ƶ����

   virtual int SendVideo() ;//������Ƶ����
   virtual int SendAudio() ;//������Ƶ����
   virtual int SendFirstRequst();//���͵�һ������
   virtual bool RequestM3u8File();//����m3u8�ļ�

#ifdef WriteVideoDataFlag 
   FILE*   fWriteVideoFile;
#endif 

   static  unsigned short nRtpPort;
   bool                   GetAVClientPortByTranspot(char* szTransport);
   sockaddr_in            addrClientVideo[MaxRtpHandleCount];
   sockaddr_in            addrClientAudio[MaxRtpHandleCount];
   NETHANDLE              nServerVideoUDP[MaxRtpHandleCount];
   NETHANDLE              nServerAudioUDP[MaxRtpHandleCount];
   unsigned short         nVideoServerPort[MaxRtpHandleCount];
   unsigned short         nAudiServerPort[MaxRtpHandleCount];
   unsigned short         nVideoClientPort[MaxRtpHandleCount];
   unsigned short         nAudioClientPort[MaxRtpHandleCount];
   unsigned short         nSetupOrder;
   RtspNetworkType        m_RtspNetworkType;

   int                     nRtspPlayCount;//play�Ĵ���
   bool                    FindRtpPacketFlag();

   unsigned char           s_extra_data[256];
   int                     extra_data_size;
   struct mpeg4_avc_t      avc;
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

   CRtcpPacketSR           rtcpSR;
   CRtcpPacketRR           rtcpRR;
   unsigned char           szRtcpSRBuffer[512];
   unsigned int            rtcpSRBufferLength;
   unsigned char           szRtcpDataOverTCP[1500];
   void                    SendRtcpReportData(unsigned int nSSRC,int nChan);//����rtcp �����,���Ͷ�
   void                    SendRtcpReportDataRR(unsigned int nSSRC, int nChan);//����rtcp �����,���ն�
   void                    ProcessRtcpData(char* szRtpData, int nDataLength, int nChan);

   int                     GetRtspPathCount(char* szRtspURL);//ͳ��rtsp URL ·������

   volatile                uint64_t tRtspProcessStartTime; //��ʼʱ��

   unsigned  char          szSendRtpVideoMediaBuffer[MaxRtpSendVideoMediaBufferLength];
   unsigned  char          szSendRtpAudioMediaBuffer[MaxRtpSendAudioMediaBufferLength];
   int                     nSendRtpVideoMediaBufferLength; //�Ѿ����۵ĳ���  ��Ƶ
   int                     nSendRtpAudioMediaBufferLength; //�Ѿ����۵ĳ���  ��Ƶ
   uint32_t                nStartVideoTimestamp; //��һ֡��Ƶ��ʼʱ��� ��
   uint32_t                nCurrentVideoTimestamp;// ��ǰ֡ʱ���
   int                     nCalcAudioFrameCount;//������Ƶ������

   void                    ProcessRtpVideoData(unsigned char* pRtpVideo, int nDataLength);
   void                    ProcessRtpAudioData(unsigned char* pRtpAudio, int nDataLength);
   void                    SumSendRtpMediaBuffer(unsigned char* pRtpMedia,int nRtpLength);//�ۻ�rtp����׼������
   std::mutex              MediaSumRtpMutex;

   unsigned short          nVideoRtpLen, nAudioRtpLen;

   _rtp_packet_sessionopt  optionVideo;
   _rtp_packet_input       inputVideo;
   _rtp_packet_sessionopt  optionAudio;
   _rtp_packet_input       inputAudio;
   uint32_t                hRtpVideo, hRtpAudio;
   uint32_t                nVideoSSRC;

   bool                    GetRtspSDPFromMediaStreamSource(RtspSDPContentStruct sdpContent,bool bGetFlag);
   char                    szRtspSDPContent[512];
   char                    szRtspAudioSDP[512];
   bool                    GetMediaURLFromRtspSDP(); //��ȡrtsp��ַ�е�ý���ļ� ������ /Media/Camera_00001

   bool                    GetMediaInfoFromRtspSDP();
   void                    SplitterRtpAACData(unsigned char* rtpAAC, int nLength);
   int32_t                 XHNetSDKRead(NETHANDLE clihandle, uint8_t* buffer, uint32_t* buffsize, uint8_t blocked, uint8_t certain);
   bool                    ReadRtspEnd();

   int                     au_header_length;
   uint8_t                 *ptr, *pau, *pend;
   int                     au_size ; // only AU-size
   int                     au_numbers ;
   int                     SplitterSize[16];

   volatile bool           bRunFlag;

   std::mutex              netDataLock;
   unsigned char           netDataCache[MaxNetDataCacheBufferLength]; //�������ݻ���
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

   char            szResponseBuffer[string_length_4096];
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
   char           szRtspContentSDP[512];
   char           szVideoSDP[512];
   char           szAudioSDP[512];
   CABLSipParse   sipParseV, sipParseA;   //sdp ��Ϣ����

   boost::shared_ptr<CMediaStreamSource> pMediaSource;

   volatile bool  bIsInvalidConnectFlag; //�Ƿ�Ϊ�Ƿ����� 
   volatile bool  bExitProcessFlagArray[3];

#ifdef WriteRtpDepacketFileFlag
   FILE*          fWriteRtpVideo;
   FILE*          fWriteRtpAudio;
#endif
};

#endif