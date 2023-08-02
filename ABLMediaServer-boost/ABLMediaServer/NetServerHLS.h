#ifndef _CNetServerHLS_H
#define _CNetServerHLS_H

#define  MaxHttp_FlvNetCacheBufferLength    1024*32 
#define  Send_TsFile_MaxPacketCount         1024*64  //TS ������ֽ�����
#define  MaxDefaultTsFmp4FileByteCount      1024*1024*5 //ȱʡTS��FMP4�ļ��ֽڴ�С 

class CNetServerHLS : public CNetRevcBase
{
public:
	CNetServerHLS(NETHANDLE hServer,NETHANDLE hClient,char* szIP,unsigned short nPort, char* szShareMediaURL);
	~CNetServerHLS();
   
    virtual int InputNetData(NETHANDLE nServerHandle, NETHANDLE nClientHandle, uint8_t* pData, uint32_t nDataLength, void* address) ;
	virtual int ProcessNetData() ;

	virtual int PushVideo(uint8_t* pVideoData, uint32_t nDataLength, char* szVideoCodec) ;//������Ƶ����
	virtual int PushAudio(uint8_t* pAudioData, uint32_t nDataLength, char* szAudioCodec, int nChannels, int SampleRate) ;//������Ƶ����

	virtual int SendVideo() ;//������Ƶ����
	virtual int SendAudio() ;//������Ƶ����
	virtual int SendFirstRequst();//���͵�һ������
	virtual bool RequestM3u8File();//����m3u8�ļ�

private :
	unsigned char*          pTsFileBuffer;//��ȡTS��FMP4�ļ�����
	int                     nCurrentTsFileBufferSize; //��ǰTS��FMP4�����ֽڴ�С 

	bool                    ReadHttpRequest();
	char                    szHttpRequestBuffer[1280];

	std::mutex              netDataLock;
 	char                    szCookieNumber[64];
	char                    szDateTime1[64];
	char                    szDateTime2[64];
	bool                    bRequestHeadFlag; //����head����ʽ 
	char                    szOrigin[256];//��Դ
	int64_t                 GetTsFileNameOrder(char* szTsFileName);
	int64_t                 nTsFileNameOrder; //TS��Ƭ�ļ���� 0 ��1 ��2 ��3 ~ N
	char                    szConnectionType[64];//Close�� Keep-live
	CABLSipParse            httpParse;
	unsigned long           fFileByteCount;
	bool                    GetHttpRequestFileName(char* szGetRequestFile, char* szHttpHeadData);
	int                     nWriteRet, nWriteRet2;

	char                    httpResponseData[1024];
	char                    szM3u8Content[512];
	unsigned char           netDataCache[MaxHttp_FlvNetCacheBufferLength+4]; //�������ݻ���
	int                     netDataCacheLength;//�������ݻ����С
	int                     nNetStart, nNetEnd; //����������ʼλ��\����λ��
	int                     MaxNetDataCacheCount;
 	int                     data_Length;
	char                    szPushName[512];//hls ���ڵ���������
	char                    szRequestFileName[512];//http������ļ����� 
	char                    szReadFileName[512];
	volatile bool           bFindHLSNameFlag;
};

#endif