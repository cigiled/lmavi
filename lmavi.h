#ifndef __LM_AVI_H__
#define __LM_AVI_H__

#ifdef  __cplusplus
extern "C"  {
#endif

#include <stdio.h>

#define _GNU_SOURCE 

#define MAX_VIDEO_BUFF  (1024*800)
#define MAX_V_FAME_BUFF (1024*136)
#define MAX_AUDIO_BUFF  (1024*256)
#define MAX_A_FAME_BUFF (1024*8)
#define MAX_FFLUSH_SZ   (1024*512)
#define MAX_FILE_SZ     (100)
#define MAX_INDEX_SZ    (1024*40)


#define HDRL_SZ (5*4+56+5*4+40+56+2*4+5*4+56+20+4) 
#define AVIH_SZ (56)
#define STRL_SZ (116)
#define STRL_AUDIO_SZ (96)

#define HEADERBYTES  (1024) //8192//1024

#define uint    unsigned int
#define uchar   unsigned char
#define ulong   unsigned long
#define ushort  unsigned short

#define AVI_VIDEO_FLAG	"00dc"
#define AVI_AUDIO_FLAG	"01wb"

#define DATA_ALIGN(u32Data , u8Align)  ((u32Data + u8Align - 1) & (~(u8Align - 1)))
#define MAKEFOURCC(ch0, ch1, ch2, ch3) ((ulong)(uchar)(ch0) | ((ulong)(uchar)(ch1) << 8) |     \
										((ulong)(uchar)(ch2) << 16) | ((ulong)(uchar)(ch3) << 24 ))

//media type.
typedef enum
{
	MEDIA_TP_MIN = 0,
	MEDIA_TP_JPEG,
	MEDIA_TP_H264,
	MEDIA_TP_H265,
	MEDIA_TP_AAC,	
	MEDIA_TP_PCM,
	MEDIA_TP_G711A,
	MEDIA_TP_G711U,
	MEDIA_TP_G726,
	MEDIA_TP_OPUS,
	MEDIA_TP_MAX,
}mtp_t; 

//local media info.
//if realy stream , vfd and afd is NULL, vfsz and afsz is 0.
//else if file stream[local stream], they are available value.
typedef struct
{
	FILE *vfd;
	uint vfsz;
	FILE *afd;
	uint afsz;
	
	FILE *avifd;
}handle_t;

typedef struct
{
	ulong  videframes;
	ulong  audioframes;
	ulong  avidatasz;
	ulong  aviriffsz;
}rewrite_t;

typedef struct	
{
	ulong dwChunkId;  // 表征本数据块的四字符码
	ulong dwFlags;	  // 说明本数据块是不是关键帧、是不是‘rec ’列表等信息
	ulong dwOffset;   // 本数据块在文件中的偏移量
	ulong dwSize;	  // 本数据块的大小
}aIndex_t;

typedef struct 
{
	ulong fcc; 	      // 必须为‘idx1’ 4Byte
	ulong sz; 		 // 本数据结构的大小，不包括最初的8个字节(fcc和cb两个域).

	int 	  indexnum;  			
    aIndex_t  *aIndex; // 这是一个数组！为每个媒体数据块都定义一个索引信息
} avi_index_t;

//file media info.
typedef struct
{
	handle_t  	 hd;
	mtp_t    	 vty;
	mtp_t	 	 aty;
	rewrite_t    rewrite;
	avi_index_t  *index;
}fminf_t;

//video media inf
typedef struct
{
	uint width;
	uint hight;
	uint fps;
	uint encmode;
}vminf_t;

//audio media inf
typedef struct
{
	uint mpeg_ver;
	uint profile;
	uint sampling;
	uint frame_sz;
	uint chns;
	uint encmode;
}aminf_t;

//audio and vidoe media inf
typedef struct
{
	vminf_t  vm;
	aminf_t  am;
}avminf_t;

typedef struct
{
	char *fty;
	char ty_id;
}finf_t;

typedef struct
{
	uchar* data;
	uint  len;
}v_sps_t;

typedef struct
{
    uchar* data;
	uint  len;
}v_pps_t;

typedef struct
{
	ulong chunkid;
	ulong size;
	ulong flag;
}riff_header_t;

typedef struct
{
	ulong chunkid;
	ulong size;
	ulong flag;
}list_t;

// len:    fcc:4B + sz:4B     + 56B = 64B 
typedef struct
{
    ulong  fcc;  				 // 必须为‘avih�?
    ulong  sz;    				 // 本数据结构的大小，不包括最初的8个字节（fcc和cb两个域）

	ulong  dwMicroSecPerFrame;   // 视频帧间隔时间（以毫秒为单位�?
    ulong  dwMaxBytesPerSec;     // 这个AVI文件的最大数据率
    ulong  dwPaddingGranularity; // 数据填充的粒�?
    ulong  dwFlags;             // AVI文件的全局标记，比如是否含有索引块�?
    ulong  dwTotalFrames;       // video 总帧�?
    ulong  dwInitialFrames;     // 为交互格式指定初始帧数（非交互格式应该指定为0�?
    ulong  dwStreams;          // 本文件包含的流的个数
    ulong  dwSuggestedBufferSize; // 建议读取本文件的缓存大小（应能容纳最大的块）
    ulong  dwWidth;           // 视频图像的宽（以像素为单位）
    ulong  dwHeight;          // 视频图像的高（以像素为单位）
    ulong  dwReserved[4];     // 保留
}avi_main_header_t;

// len:    fcc:4B + sz:4B     + 56B = 64B 
typedef struct
{
     ulong fcc;  // 必须为‘strh�?
     ulong sz;   // 本数据结构的大小，不包括最初的8个字节（fcc和sz两个域）

	 ulong fccType;    // 流的类型：‘auds’（音频流）、‘vids’（视频流）、‘mids’（MIDI流）、‘txts’（文字流）
     ulong fccHandler; // 指定流的处理者，对于音视频来说就是解码器
     ulong  dwFlags;    // 标记：是否允许这个流输出？调色板是否变化�?
     ushort wPriority;  // 流的优先级（当有多个相同类型的流时优先级最高的为默认流�?
     ushort wLanguage;
     ulong  dwInitialFrames; // 为交互格式指定初始帧�?
     ulong  dwScale;   // 这个流使用的时间尺度
     ulong  dwRate;
     ulong  dwStart;   // 流的开始时�?
     ulong  dwLength;  // 流的长度（单位与dwScale和dwRate的定义有关）
     ulong  dwSuggestedBufferSize; // 读取这个流数据建议使用的缓存大小
     ulong  dwQuality;    // 流数据的质量指标�? ~ 10,000�?
     ulong  dwSampleSize; // Sample的大�?
     struct 
	 {
         short int left;
         short int top;
         short int right;
         short int bottom;
	 }rcFrame;  // 指定这个流（视频流或文字流）在视频主窗口中的显示位置
               // 视频主窗口由AVIMAINHEADER结构中的dwWidth和dwHeight决定
}stream_header_t; 

// len:    chunkid:4B + size:4B     + 40B = 48B 
typedef struct
{
	ulong chunkid;
	ulong size;
	
	ulong biSize;  //本数据结构的大小，不包括最初的8个字�?chunkid和size两个)
	ulong biWidth;
	ulong biHeigth;
	ushort biplanes;
	ushort biBitcount;
	ulong biCompression;
	ulong biSizeImage;
	ulong biXPelsPerMeter;
	ulong biYPelsPerMeter;
	ulong biClrImportant;
	ulong biClrUsed;
}video_strf_t;

// len:    chunkid:4B + size:4B     + 16B = 24B 
typedef struct
{
	ulong  chunkid;
	ulong  size;
	
	ushort wFormatTag;
	ushort nChannels;
	ulong  nSamplesPerSec;
	ulong  nAvgBytesPerSec;
	ushort nBlockAlign;
	ushort nBitsPerSample;
}audio_strf_t;

typedef struct
{
	ulong chunkid;
	ulong size;
	ulong Flag;
}movi_header_t;

int lm_start_lmavi();
int lm_destroy_lmavi();

//if can't know parameter value, whill try get it from audio or video file if it can.
//so please set parameter valueis 0, if don't know this value.
int lm_set_av_para(avminf_t *avinf);

//========pack avi====
//infn[0]: video file name, infn[1]: audio file name.
int lm_pack_by_file(char *infn[], mtp_t vty, mtp_t aty); 
fminf_t* lm_pack_by_livestream(char *avifilename, avminf_t *live_am);

void lm_write_avi_header(FILE *avifd, avminf_t *avinf);

// for live stream.
int lm_put_video_frame(fminf_t *finf, uint type, char *vframe, uint frame_sz, uint nalsz);
int lm_put_audio_frame(fminf_t *finf, uint type, char *aframe, uint frame_sz);

int lm_stop_write_avi(fminf_t *finf);

//========spare avi====
int lm_spare_avi_file();

//========spare avi and split video or audio====
int lm_get_video_frame();
int lm_get_audio_frame();

int lm_save_video_file();
int lm_save_audio_to_file();

#ifdef  __cplusplus
}
#endif

#endif
