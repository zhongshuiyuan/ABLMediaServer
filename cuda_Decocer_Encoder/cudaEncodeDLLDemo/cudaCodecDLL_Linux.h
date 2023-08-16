#ifndef  _cudeCodecDLL_H
#define  _cudeCodecDLL_H

#include <stdint.h>

//��Ƶ��ʽ 
typedef enum cudaCodecVideo_enum {
	cudaCodecVideo_MPEG1 = 0,                                         /**<  MPEG1             */
	cudaCodecVideo_MPEG2,                                           /**<  MPEG2             */
	cudaCodecVideo_MPEG4,                                           /**<  MPEG4             */
	cudaCodecVideo_VC1,                                             /**<  VC1               */
	cudaCodecVideo_H264,                                            /**<  H264              */
	cudaCodecVideo_JPEG,                                            /**<  JPEG              */
	cudaCodecVideo_H264_SVC,                                        /**<  H264-SVC          */
	cudaCodecVideo_H264_MVC,                                        /**<  H264-MVC          */
	cudaCodecVideo_HEVC,                                            /**<  HEVC              */
	cudaCodecVideo_VP8,                                             /**<  VP8               */
	cudaCodecVideo_VP9,                                             /**<  VP9               */
	cudaCodecVideo_NumCodecs,                                       /**<  Max codecs        */
	// Uncompressed YUV
	cudaCodecVideo_YUV420 = (('I' << 24) | ('Y' << 16) | ('U' << 8) | ('V')),   /**< Y,U,V (4:2:0)      */
	cudaCodecVideo_YV12 = (('Y' << 24) | ('V' << 16) | ('1' << 8) | ('2')),   /**< Y,V,U (4:2:0)      */
	cudaCodecVideo_NV12 = (('N' << 24) | ('V' << 16) | ('1' << 8) | ('2')),   /**< Y,UV  (4:2:0)      */
	cudaCodecVideo_YUYV = (('Y' << 24) | ('U' << 16) | ('Y' << 8) | ('V')),   /**< YUYV/YUY2 (4:2:2)  */
	cudaCodecVideo_UYVY = (('U' << 24) | ('Y' << 16) | ('V' << 8) | ('Y'))    /**< UYVY (4:2:2)       */
} cudaCodecVideo;

/*
���ܣ�
   ��ʼ��
������
   ��
*/
typedef bool (*ABL_cudaCodec_Init)();

/*
���ܣ�
   ��ȡӢΰ���Կ�����
����
   ��
����ֵ��
   0   û��Ӣΰ���Կ�
   N   ��N��Ӣΰ���Կ�
*/
typedef int (*ABL_cudaCodec_GetDeviceGetCount)();

/*
����:
   ��ȡӢΰ���Կ�����
������
   int    nOrder   �Կ����
   char*  szName   �Կ�����
����ֵ
   true            ��ȡ�ɹ�
   false           ��ȡʧ�� 
*/
typedef bool (*ABL_cudaCodec_GetDeviceName) (int nOrder,char* szName);


/*
����:
   ��ȡӢΰ���Կ���ʹ����
������
   int    nOrder   �Կ����

����ֵ
   �Կ�ʹ���� 
*/
typedef int  (*ABL_cudaCodec_GetDeviceUse) (int nOrder);

/*
����:
   ������Ƶ������
������
   cudaCodecVideo_enum videoCodec,      ���������Ƶ��ʽ cudaCodecVideo_H264 �� cudaCodecVideo_HEVC �ȵ� 
   cudaCodecVideo_enum outYUVType       �����֧�������YUV��ʽ������ֻ֧���������YUV��ʽ��cudaCodecVideo_NV12����cudaCodecVideo_YV12��
   int                 nWidth,          ���� 1920 
   int                 nHeight,         ���� 1080 
   uint64_t&           nCudaChan        ����ɹ������ش���0 ������ 

����ֵ
 
*/
typedef bool (*ABL_cudaCodec_CreateVideoDecode) (cudaCodecVideo_enum videoCodec, cudaCodecVideo_enum outYUVType, int nWidth, int nHeight, uint64_t& nCudaChan);


/*
���ܣ�
  ����cuda������Ƶ 
������
  uint64_t        nCudaChan           cudaͨ����
  unsigned char*  pVideoData          δ��������
  int             nVideoLength        ���ݳ���
  int             nDecodeFrameCount   ����ɹ��󷵻ض���֡�� 1��4 �ȵ� 
  int&            nOutDecodeLength    ���뷵��һ֡buffer���� 
*/
typedef unsigned char* (*ABL_cudaCodec_CudaVideoDecode) (uint64_t nCudaChan,unsigned char* pVideoData,int nVideoLength,int& nDecodeFrameCount,int& nOutDecodeLength);

/*
���ܣ�
   ɾ��cuda��Ƶ������
������
uint64_t        nCudaChan           cudaͨ����
 
*/
typedef  bool (*ABL_cudaCodec_DeleteVideoDecode) (uint64_t nCudaChan);

/*
���ܣ�
  �����Ѿ�����CUDAӲ����������������·�Կ�Ӳ��
������
��
*/
typedef  int (*ABL_cudaCodec_GetCudaDecodeCount) ();

/*
���ܣ�
  ����ʼ��
������
  ��
*/
typedef bool (*ABL_cudaCodec_UnInit) ();

#endif
