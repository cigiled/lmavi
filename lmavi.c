#include <string.h>
#include <error.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "lmavi.h"
#include "sps.h"


avminf_t avinf_m[MAX_FILE_SZ];//记录每个封装的avi文件的音频, 视频 信息.
fminf_t lminf_m[MAX_FILE_SZ]; //可以同时封装100个avi文件
static uint filenums = 0;

finf_t avi_m[ ] ={ {NULL, 0},
					{"jpeg",  MEDIA_TP_JPEG},  {"h264", MEDIA_TP_H264}, {"h265",  MEDIA_TP_H265},
				   {"aac",   MEDIA_TP_AAC},   {"pcm",  MEDIA_TP_PCM},  {"g711a", MEDIA_TP_G711A}, 
				   {"G711u", MEDIA_TP_G711U}, {"G726", MEDIA_TP_G726}, {"opus",  MEDIA_TP_OPUS}			  
				 }; 

int pack_jpeg();
int pack_h264();
int pack_h265();

int pack_aac();
int pack_pcm();
int pack_g711x();
int pack_g726();
int pack_opus();

void print_tnt(char *buf, int len, const char *comment)
{
	int i = 0;
	printf("%s start buf:%p len:%d\n", comment, buf, len);
	for(i = 0; i < len; i++)
	{
    	if((i>0) && (i%16 == 0))
			printf("\r\n");
    	 printf("%02x ", buf[i] & 0xff); //&0xff 是为了防止64位平台中打印0xffffffa0 这样的，实际打印0xa0即可
	}
	printf("\n");
}

//******************************//
//*******private interface******//
//******************************//

int check_file(char *infn[], mtp_t vty, mtp_t aty, char *isaudio)
{
	if(!infn[0])
	{
		printf("Input file is empty, exit !\n");
		return -1;
	}
	
	if(vty < MEDIA_TP_JPEG || vty > MEDIA_TP_H265)
	{
		printf("video type set error, exit !\n");
		return -1;
	}

	if(!strcasestr(infn[0], avi_m[vty].fty))
	{
		printf("Video file [%s]:[%s]check fialed, please use example: xx.h264 or xx.h265\n", infn[0], avi_m[vty].fty);
		return -1;
	}

	*isaudio = 0x00;
	if(infn[1])
	{//有音频文件输入的话, 判断其是否正确.
		if(aty < MEDIA_TP_AAC || aty > MEDIA_TP_OPUS)
		{
			printf("audio type set error, exit !\n");
			return -1;
		}

		if(!strcasestr(infn[1], avi_m[aty].fty))
		{
			printf("Audio file [%s]:[%s]check fialed, please use example: xx.h264 or xx.h265\n", infn[1], avi_m[aty].fty);
			return -1;
		}

		*isaudio = 0x01;
	}
		
	return 0;
}

void close_file(fminf_t *inf)
{
	if(inf->hd.vfd)
		fclose(inf->hd.vfd);

	if(inf->hd.afd)
		fclose(inf->hd.afd);

	if(inf->hd.avifd)
		fclose(inf->hd.avifd);
}

int open_file(char *infn[], fminf_t *inf)
{
	if(infn[0])
	{
		inf->hd.vfd = fopen(infn[0], "r+");
		if(inf->hd.vfd < 0)
		{
			perror("Fopen vidoe file failed, error:\n");
			return -1;
		}

		struct stat  vst;
		lstat(infn[0], &vst);
		inf->hd.vfsz = vst.st_size;
	}

	if(infn[1])
	{
		inf->hd.afd = fopen(infn[1], "r+");
		if(inf->hd.afd < 0)
		{
			perror("Fopen audio file failed, error:\n");
			return -1;
		}

		struct stat  ast;
		lstat(infn[0], &ast);
		inf->hd.afsz = ast.st_size;
	}

	char avifile[120] ={'\0'};
	if(infn[2])
	{
		if(!strcasestr(infn[2], ".avi"))
		{
			if(infn[2][0] == '\0')
				strcpy(avifile, "test1.avi");
			else
				sprintf(avifile, "%s.avi", infn[2]);
		}
		else {
			memcpy(avifile, infn[2], strlen(infn[2]));
		}
	}
	else{
		printf("Don't set avi file path, will use tets.avi in the current directory.");
		strcpy(avifile, "tets.avi");
	}

	printf("[%s]\n", avifile);
	inf->hd.avifd = fopen(avifile, "w+");
	if(inf->hd.avifd < 0)
	{
		perror("Fopen avi file failed, error:\n");
		return -1;
	}
		
	return 0;
}

int get_aac_adts_head(FILE *fd,  aminf_t *am)
{
	if(!am)
	{
		printf("Input aac point is NULL, exit\n");
		return -1;
	}
	
	if(fd <= 0)
	{
		perror("Open file failed :");
		return -1;
	}
	
	char head[9] = {'\0'};
	int sz = fread(head, sizeof(head), 1, fd);
	if(sz > 0)
	{
		if(((head[0]&0xff) == 0xff) && ((head[1]&0xf0) == 0xf0))
		{
			am->mpeg_ver = (head[1] & 0x08) >> 3;
			am->profile  = (head[2] & 0xc0) >> 6;
			am->sampling = (head[2] & 0x3c) >> 2;
			am->chns 	 = (head[2] & 0x01) << 2 | ((head[3] & 0xc0) >> 6);
			am->frame_sz |= (head[3] & 0x03) <<11;   //low    2 bit
			am->frame_sz |= (head[4] & 0xff) <<3;	//middle 8 bit
			am->frame_sz |= (head[5] & 0xe0) >>5;	//hight  3bit
		}
	}

	fseek(fd, 0L, SEEK_SET);
	return 0;
}

int get_sampling(uint samp_seq)
{
	int samp[] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000};
	if( (samp_seq > sizeof(samp))  || (samp_seq < 0))
	{
		printf("Input sampling que is error, do not rang.\n");
		return -1;
	}
	
	return samp[samp_seq];
}

int spare_aac(FILE *fd, aminf_t *a)
{
	get_aac_adts_head(fd, a);
	a->sampling = get_sampling(a->sampling);
	
	printf("----vfdsasa---[%d]:[%d]:[%d]--->\n", a->sampling * a->chns, a->sampling, a->chns);
	return 0;
}

int get_sps_pps(FILE *vfd, v_sps_t *sps, v_pps_t *pps)
{
	int buflen = 1024 *256, i = 0, data = 0, offsz = 0;
    uchar tmp[buflen], nalu_ty = 0, flag = 0x00;
	int ret = fread(tmp, 1, buflen, vfd);
	if(ret <= 0)
	{
		perror("Fread vidoe failed:");
		return -1;
	}
	
	while(i < buflen)
	{
		data = tmp[i] << 24 | tmp[i+1] << 16 | tmp[i+2] << 8;
		if(data != 0x000001)
		{
			data |= tmp[i+3];
			if(data != 0x00000001)
			{
				i++;
				continue;
			}
			else
			{
				nalu_ty = tmp[i+4] & 0x1f;
				offsz = 4;
			}
		}
		else 
		{
			nalu_ty = tmp[i+3] & 0x1f;
			offsz = 3;
		}

		i += offsz;
		if(nalu_ty == 0x07)
		{
			sps->data = &tmp[i];
		}
		else if(nalu_ty == 0x08)
		{
			pps->data = &tmp[i];
			sps->len = pps->data - sps->data - offsz; // get last nalu size: sps size.
			memcpy(sps->data, sps->data, sps->len);
			flag = 1;
		}
		else if(pps)
		{
			pps->len = &tmp[i] - pps->data - offsz; // get last nalu size: pps size.
			memcpy(pps->data, pps->data, pps->len);
			flag <<= 1;
			
			break;
		}	

	}

	if(flag != 0x02)
	{
		printf("Do't find the sps ,pps , eixt!\n");
		return -1;
	}

	fseek(vfd, 0L, SEEK_SET);
	return 0;
}

int spare_sps(v_sps_t *sps, vminf_t *vm1)
{
	uchar *date = &sps->data[1];
	int sz = sps->len -1;
	
	sps_t _sps; 
	uchar tmpbuf[40] ={'\0'};
	int len = (int)sz;
	
	nal_to_rbsp(0, date, &sz, tmpbuf, &len);
	bs_t* b = bs_new(tmpbuf, len);

	//从sps 中获取信息  
	read_seq_parameter_set_rbsp(b, &_sps); 

	vm1->width = h264_get_width(&_sps);  
	vm1->hight = h264_get_height(&_sps);
	vm1->fps = h264_get_framerate(&_sps);  
	if(vm1->fps < 0.0)    
		vm1->fps = 21;

	printf("video: wxh:[%dx%d, fps:%d]\n", vm1->width, vm1->hight, vm1->fps);
	bs_free(b);
	return 0;
}

void get_video_pars(FILE *fd, vminf_t *v)
{
	uchar temp[1024] = {'\0'};
	uchar temp1[1024] = {'\0'};
	v_sps_t sps;
	v_pps_t pps;
	
	sps.data = temp;
	pps.data = temp1;

	if(v->encmode == MEDIA_TP_H264)
	{
		get_sps_pps(fd, &sps, &pps);
		
		spare_sps(&sps, v);
	}
	else if(v->encmode == MEDIA_TP_H265)
	{
		
	}	
}

int adjust_av_para(fminf_t *inf, avminf_t *avinf)
{
	aminf_t *am = &avinf->am;
	vminf_t *vm = &avinf->vm;
	
	//audio
	if(inf->hd.afd)
	{
		//aac audio message ,it can be get from aac file.
		if(inf->aty == MEDIA_TP_AAC)
		{
			if(am->sampling == 0 || am->frame_sz == 0  ||  am->chns == 0 )
				spare_aac(inf->hd.afd, am);
		}
	}
	
	//video
	if(inf->hd.vfd)
	{
		if(vm->encmode < MEDIA_TP_JPEG || vm->encmode > MEDIA_TP_H265)
			vm->encmode = inf->vty;

		if(vm->width == 0 || vm->fps == 0)
			get_video_pars(inf->hd.vfd, vm);
	}					

	return 0;
}

int avi_write_flush(void *data, int len, FILE* fd)
{
	static int flush_sz = 0;
    if(fwrite((char *)data, len, 1, fd) < 0)
    {
    	perror("fwrite file failed,error:");
		return -1;
	}

	flush_sz += len;
	if(flush_sz >= MAX_FFLUSH_SZ)
	{
		fflush(fd);
		//fsync(fd);
		flush_sz = 0;
	}
	
    return 0;
}

int set_riff(FILE *fd)
{
	int ret;
	riff_header_t riff_header;

	riff_header.chunkid = MAKEFOURCC('R','I','F','F');
	riff_header.size = 0; //(pack avi file len) - 4ByteRIFF - 4ByteLen
	riff_header.flag = MAKEFOURCC('A','V','I',' ');
	ret = avi_write_flush(&riff_header, sizeof(riff_header_t), fd);
	return ret;
}

//size =     avi_main_header_t +
int set_hdrl_list(FILE *fd)
{
	int ret;
	list_t hdrl_list;

	hdrl_list.chunkid = MAKEFOURCC('L','I','S','T');
	hdrl_list.size = 292; //HDRL_SZ;[TWT]
	hdrl_list.flag = MAKEFOURCC('h','d','r','l');

	ret = avi_write_flush(&hdrl_list, sizeof(list_t), fd);
	return  ret;
}

int set_avimain_header_avih(FILE *fd)
{
	int ret;
	avi_main_header_t avimainheader;

	avimainheader.fcc = MAKEFOURCC('a','v','i','h');
	avimainheader.sz  = AVIH_SZ; //sizefo(avimainheader) - fcc:4B - sz:4B  .

	avimainheader.dwMicroSecPerFrame = 0;//1000000/h264att->framePerSec;
	avimainheader.dwMaxBytesPerSec = 0;//AVI文件的最大数据率
	avimainheader.dwPaddingGranularity = 0;
	avimainheader.dwFlags = 0x00000110; //[TWT]: index:0x10 + interleaved:0x100 
	avimainheader.dwTotalFrames = 0;//总帧数------（avi文件中位置为第13int）
	avimainheader.dwInitialFrames =7;
	avimainheader.dwStreams = 1;
	avimainheader.dwSuggestedBufferSize = 0;
	avimainheader.dwWidth  = 0;//h264att->width;
	avimainheader.dwHeight = 0;//h264att->height;
	avimainheader.dwReserved[0] = 0;
	avimainheader.dwReserved[1] = 0;
	avimainheader.dwReserved[2] = 0;
	avimainheader.dwReserved[3] = 0;
	
	ret = avi_write_flush(&avimainheader, sizeof(avi_main_header_t), fd);
	return ret;
}

int set_video_avi_strl_list(FILE *fd)
{
	int ret;
	list_t strl_list;
	
	strl_list.chunkid = MAKEFOURCC('L','I','S','T');
	strl_list.size = STRL_SZ; // flag:4Byte + avi_stream_header_t:64B + avi_stream_strf_t:48B. 
	strl_list.flag = MAKEFOURCC('s','t','r','l');

	ret = avi_write_flush(&strl_list, sizeof(list_t), fd);
	return ret;
}

int set_video_avistream_strh(FILE *fd, vminf_t *vm)
{
	int ret;
	stream_header_t  v_strh_header;

	v_strh_header.fcc = MAKEFOURCC('s','t','r','h');
	v_strh_header.sz = 56; //sizefo(avi_stream_header_t) - fcc:4B - sz:4B  .


	v_strh_header.fccType = MAKEFOURCC('v','i','d','s');	
	if(vm->encmode == MEDIA_TP_H264)
		v_strh_header.fccHandler = MAKEFOURCC('H','2','6','4');
	else if(vm->encmode == MEDIA_TP_H265)
		v_strh_header.fccHandler = MAKEFOURCC('H','2','6','5');
	else if(vm->encmode == MEDIA_TP_JPEG)
		v_strh_header.fccHandler = MAKEFOURCC('M','J','P','G');

	v_strh_header.dwFlags   = 0;
	v_strh_header.wPriority = 0;
	v_strh_header.wLanguage = 0;
	v_strh_header.dwInitialFrames = 0;
	v_strh_header.dwScale  = 1;
	v_strh_header.dwRate   = vm->fps;//h264att->framePerSec;
	v_strh_header.dwStart  = 0;
	v_strh_header.dwLength = 0;
	
	v_strh_header.dwSuggestedBufferSize = 0x000016FFF; //94k
	
	v_strh_header.dwQuality = 0xFFFFFFFF;
	v_strh_header.dwSampleSize   = 0;
	v_strh_header.rcFrame.left   = 0;
	v_strh_header.rcFrame.top    = 0;
	v_strh_header.rcFrame.right  = 0;//h264att->width;
	v_strh_header.rcFrame.bottom = 0;//h264att->height;
	
	ret = avi_write_flush(&v_strh_header, sizeof(stream_header_t), fd);

	return ret;
}

int set_video_avistream_strf(FILE *fd, vminf_t *vm)
{
	int ret;
	video_strf_t v_strf;

	v_strf.chunkid = MAKEFOURCC('s','t','r','f');
	v_strf.size    = 40; //len: sizefo(avi_stream_header_t) - chunkid:4B + size:4B =40B 

	v_strf.biSize = 40; //len: sizefo(avi_stream_header_t) - chunkid:4B + size:4B =40B 
	v_strf.biWidth    = vm->width;//h264att->width;
	v_strf.biHeigth   = vm->hight;//h264att->height;
	v_strf.biplanes   = 0x0001; //[TNT]???
	v_strf.biBitcount = 0x0018; //[TNT]??? 这个值表示什么意思.

	if(vm->encmode == MEDIA_TP_H264)
		v_strf.biCompression = MAKEFOURCC('H','2','6','4');
	else if(vm->encmode == MEDIA_TP_H265)
		v_strf.biCompression = MAKEFOURCC('H','2','6','5');
	else if(vm->encmode == MEDIA_TP_JPEG)
		v_strf.biCompression = MAKEFOURCC('M','J','P','G');
	
	v_strf.biSizeImage = vm->width * vm->hight;//[TNT]???
	v_strf.biXPelsPerMeter = 0;
	v_strf.biYPelsPerMeter = 0;
	v_strf.biClrImportant  = 0;
	v_strf.biClrUsed = 0;

	ret = avi_write_flush(&v_strf, sizeof(video_strf_t), fd);
	return ret;
}

int set_audio_avi_strl_list(FILE *fd)
{
	int ret;

	list_t strl_list;
	
	strl_list.chunkid =  MAKEFOURCC('L','I','S','T');
	strl_list.size =  STRL_AUDIO_SZ; // flag:4Byte + aac_stream_header_t:64B + aac_stream_strf_t:24B.
	strl_list.flag = MAKEFOURCC('s','t','r','l');

	ret = avi_write_flush(&strl_list, sizeof(list_t), fd);
	return ret;
}

int set_audio_avistream_strh(FILE *fd, aminf_t *am)
{
	int ret;
	stream_header_t a_strh_header;

	a_strh_header.fcc = MAKEFOURCC('s','t','r','h');
	a_strh_header.sz = 56;
	a_strh_header.fccType = MAKEFOURCC('a','u','d','s');
	a_strh_header.fccHandler = 1;
	a_strh_header.dwFlags    = 0;
	a_strh_header.wPriority  = 0;
	a_strh_header.wLanguage  = 0;
	a_strh_header.dwInitialFrames = 0;
	a_strh_header.dwScale  = 0x00000400;//一帧的采样数1024
	a_strh_header.dwRate   = am->sampling;//采样率
	a_strh_header.dwStart  = 0;
	a_strh_header.dwLength = am->frame_sz;//帧数
	a_strh_header.dwSuggestedBufferSize = 0x00002000;
	a_strh_header.dwQuality = 0xFFFFFFFF;
	a_strh_header.dwSampleSize   = 0;
	a_strh_header.rcFrame.left   = 0;
	a_strh_header.rcFrame.top    = 0;
	a_strh_header.rcFrame.right  = 0;
	a_strh_header.rcFrame.bottom = 0;
	
	ret = avi_write_flush(&a_strh_header, sizeof(stream_header_t), fd);
	return ret;
}

int  set_audio_avistream_strf(FILE *fd, aminf_t *am)
{
	int ret;
	audio_strf_t a_streamstrf;

	a_streamstrf.chunkid = MAKEFOURCC('s','t','r','f');
	a_streamstrf.size = 16;
	a_streamstrf.wFormatTag = 0x00FF;
	a_streamstrf.nChannels  = am->chns;
	a_streamstrf.nSamplesPerSec  = am->sampling;
	a_streamstrf.nAvgBytesPerSec = 0x00000800;
	a_streamstrf.nBlockAlign = 0x0800;
	a_streamstrf.nBitsPerSample = 0;
	
	ret = avi_write_flush(&a_streamstrf, sizeof(audio_strf_t), fd);
	return ret;
}

int set_movi_list(FILE *fd)
{
	int ret;
	int junksz = HEADERBYTES - ftell(fd);
	if(junksz <= 0)
	{
		return -1;
	}

	char *data = NULL;
	data = (char *)malloc(junksz);
	if(!data)
		return -1;

	memset(data, 0, junksz);

	memcpy(data, "JUNK", 4);
	data[4] = (junksz) &0xff;
	data[5] = (junksz>>8)&0xff;
	data[6] = (junksz>>16)&0xff;
	data[7] = (junksz>>24)&0xff;

	movi_header_t moviheader;
	moviheader.chunkid = MAKEFOURCC('L','I','S','T');
	moviheader.size = 0;
	moviheader.Flag = MAKEFOURCC('m','o','v','i');

	memcpy(&data[junksz -12], &moviheader, sizeof(movi_header_t));
	
	ret = avi_write_flush(data, junksz, fd);
	free(data);
	return ret;
}

int set_id(FILE *fd, avminf_t *avinf)
{
	int ret;
	avi_index_t  avioldindex;

	avioldindex.fcc = MAKEFOURCC('i','d','x','1');
	avioldindex.sz  = 16*(avinf->am.frame_sz + avinf->am.frame_sz);

	ret = avi_write_flush(&avioldindex, sizeof(avi_index_t), fd);
	return ret;
}

int write_avi_header(FILE *avifd, avminf_t *avinf)
{
	set_riff(avifd);
	set_hdrl_list(avifd);
	set_avimain_header_avih(avifd);
	
	//video header.
	set_video_avi_strl_list(avifd);
	set_video_avistream_strh(avifd, &avinf->vm);
	set_video_avistream_strf(avifd, &avinf->vm);
	
	//audio header.
	set_audio_avi_strl_list(avifd);
	set_audio_avistream_strh(avifd, &avinf->am);
	set_audio_avistream_strf(avifd, &avinf->am);

	set_movi_list(avifd);
	return 0;
}

void rewrite_avi_header(fminf_t *finf)
{
	uint  offsize = 0;
	rewrite_t  *rew = &finf->rewrite;

	//riff:   riff_header_t  size.
	fseek(finf->hd.avifd, 4L, SEEK_SET);
	avi_write_flush(&rew->aviriffsz, 4, finf->hd.avifd);

	//video: struct    avi_main_header_t dwTotalFrames.
	offsize = sizeof(riff_header_t) + sizeof(list_t) ;
	fseek(finf->hd.avifd, offsize + 6*4 , SEEK_SET);
	avi_write_flush(&rew->videframes, 4, finf->hd.avifd);

	//video:    struct    stream_header_t     . dwLength  
	offsize += sizeof(list_t);
	fseek(finf->hd.avifd, offsize + 10*4, SEEK_SET);
	avi_write_flush(&rew->videframes, 4, finf->hd.avifd);

	//audio:    struct    stream_header_t     . dwLength  
	offsize += sizeof(stream_header_t) +  sizeof(video_strf_t) + sizeof(list_t);
	fseek(finf->hd.avifd, offsize + 10*4, SEEK_SET);
	avi_write_flush(&rew->videframes, 4, finf->hd.avifd);

	fseek(finf->hd.avifd, 0L, SEEK_SET);

}

int write_avi_video_audio_data(uint type, FILE *fd, char *frame, uint frame_sz)
{
	int ret;
	if(type >= MEDIA_TP_JPEG  && type <= MEDIA_TP_H265)
		memcpy(frame, AVI_VIDEO_FLAG, 4);
	else if(type >= MEDIA_TP_AAC  && type <= MEDIA_TP_OPUS)
		memcpy(frame, AVI_AUDIO_FLAG, 4);
	
	frame_sz = DATA_ALIGN(frame_sz,4);

	frame[4] = (frame_sz)&0xff;
	frame[5] = (frame_sz>> 8)&0xff;
	frame[6] = (frame_sz>>16)&0xff;
	frame[7] = (frame_sz>>24)&0xff;

	frame_sz += 8 ; //4ByteFlag + 4ByteLen	
	ret = avi_write_flush(frame, frame_sz, fd);
	if(ret < 0)
	{
		perror("Write frame data failed, error:");
		return -1;
	}

	return 0;
}

int write_avi_index(fminf_t *finf)
{
	char temp[8] ={'\0'};
	uint ret;
	//Fill Fcc
	temp[3]	 = (finf->index->fcc)&0xff;
	temp[2]	 = (finf->index->fcc>>8)&0xff;
	temp[1]	 = (finf->index->fcc>>16)&0xff;
	temp[0]	 = (finf->index->fcc>>24)&0xff;
	
	//Fill Len
	temp[7]	 = (finf->index->sz)&0xff;
	temp[6]	 = (finf->index->sz>>8)&0xff;
	temp[5]	 = (finf->index->sz>>16)&0xff;
	temp[4]	 = (finf->index->sz>>24)&0xff;

	ret = avi_write_flush(temp, 8, finf->hd.avifd);

	//fill    aIndex_t data.
	uint lent = finf->index->indexnum * 16;
	ret = avi_write_flush(finf->index->aIndex, lent, finf->hd.avifd);
	if(ret < 0)
	{
		perror("Write avi index data failed, error:");
		return -1;
	}
	return 0;
}

inline int check_nalu_type(char *data, uint type)
{
	if(type == MEDIA_TP_H264)
	{
		if( (data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 1) || 
			(data[0] == 0 && data[1] == 0 && data[2] == 1))
			 return 1;
	}
	else if(type == MEDIA_TP_H265)
	{

	}
	
	return 0;
}

int get_one_nalu(uint enc_ty, FILE *fd, uint fsiz, char *frame0, int *frame_sz0, char *buff0, int *nal_sz)
{
	if(!frame0 || !fd || !buff0)
	{
		printf("Input [frame0] or [fd]  or vidoe mem [buff0]  is  NULL !\n");
		return -1;
	}

	int sqe = 0;
	static int l_nalsz = 0;
	static int p   = 0;
	static int len = 0;
	static int remain = 0;
	static int file_sz = 0;
	static char end_flag = 1;
	static char *start = NULL;

	while(1)
	{
		if(end_flag)
		{
			p = 0;
			end_flag = 0;
			memset(buff0, 0, MAX_VIDEO_BUFF);
			if((len = fread(buff0, 1, MAX_VIDEO_BUFF, fd)) <= 0)
			{
				if(len == 0)
					printf("read video file over !\n");
				else
					perror("read video file ERROR");
				return -2; //file over.
			}
		}

		if(check_nalu_type(&buff0[p], enc_ty))
		{					
			(buff0[p+2] == 1)?(sqe = 3):(sqe = 4);
			if(!start)
			{ //首次检测到帧.									
				start    = &buff0[p];
				p	    += sqe;
				l_nalsz  = sqe;
				end_flag = 0;
				continue;
			}
			else
			{
				if( (remain + (&buff0[p] - start) > MAX_V_FAME_BUFF) || ((&buff0[p] - start) > MAX_V_FAME_BUFF ) )
				{//出现过在720x576的视频里,帧大小为967k的情况, 这里的处理是跳过,不存.
					printf("Frame error, frame size [%d] is lager, more than [%d].\n", remain + p, MAX_V_FAME_BUFF);
					p	    += sqe;

					start    = &buff0[p];
					*nal_sz  = l_nalsz;
					l_nalsz  = sqe;
					end_flag = 0;
					remain = 0; 
					continue;
				}
				
				if(remain > 0)
				{
					memcpy(frame0 + remain, &buff0[p], p); //加上 上次不完整部分数据.
					*frame_sz0 = p + remain;
					remain = 0; //上次read 剩下的不完整帧.
				}
				else
				{
					*frame_sz0 = &buff0[p] - start;
					memcpy(frame0, start, *frame_sz0);
				}

				start    = &buff0[p];
				p	    += sqe;
				*nal_sz  = l_nalsz;
				l_nalsz  = sqe;
				end_flag = 0;
				return 1;
			} 
		}
		else if( len == p)
		{//存下这次read帧中, 剩下的不完整帧数据.
			remain = &buff0[p] - start;	
			if(remain > 0)
			{				
				memcpy(frame0, start, remain);
				if(file_sz + p == fsiz) //写最后一帧数据.			
				{ 	
					frame_sz0 = remain;
					return 1;
				}
			}

			file_sz += len;
			end_flag = 1;
			continue;
		}
		
		p++;
	}

	return 0;
}

int get_nalu_type(uint fmt, int sqe, char *data, int *type)
{ 
	uchar  naluType;
	char *pNalu = NULL;
	
	pNalu = data+sqe;

	//只记录关键帧,为了填写索引块.
	if(fmt == MEDIA_TP_H264)
	{
		naluType = pNalu[0]&0x1F;
		switch (naluType)
		{
			case 5: 
					*type = 0x01;
					break;
			
			default:
					*type = 0x00;
					break;
		}		
	}
	else if (fmt == MEDIA_TP_H265)
	{
		naluType = (pNalu[0] & 0x7E)>>1;
		switch (naluType)
		{			
			case 19: 
					*type = 0x01;
					break;
						
			default:
					*type = 0x00;
					break;
		}	
	}
	return 0;
}

int sav_index_msg(avi_index_t    	* index, char keframe, uint frame_sz)
{
	static uint lsize = 0;
	if(index->indexnum == 0)
	{
		index->fcc = MAKEFOURCC('i','d','x','1');
		index->sz  = 0;
		lsize = 8;
	}

	index->sz  += 16;
	index->aIndex[index->indexnum].dwChunkId = 0x2233; 
	index->aIndex[index->indexnum].dwFlags   = keframe;
	index->aIndex[index->indexnum].dwOffset  = lsize;
	index->aIndex[index->indexnum++].dwSize  = frame_sz;
	lsize += frame_sz + 8;

	return 0;
}


//为了减少fread的次数.这里每次读多帧到buff, 然后再解析出一帧来.
int video_pro(uint enc_ty, fminf_t *finf, char* vframe, char* vbuff)
{
	if(!vbuff)
		return -2;
	
	int keyframe = 0, frame_sz =0, ret, nalsz;
	
	ret = get_one_nalu(enc_ty, finf->hd.vfd, finf->hd.vfsz, &vframe[8], &frame_sz, vbuff, &nalsz);
	if(ret < 0)
	{
		return ret;
	}

	if(frame_sz > 0)
	{ //有的文件帧不完整,
		get_nalu_type(enc_ty, nalsz, &vframe[8], &keyframe);	
		ret = write_avi_video_audio_data(enc_ty, finf->hd.avifd, vframe, frame_sz);
		if(ret < 0)
		{
			printf("put video frame failed \n");
			return -1;
		}
		
		finf->rewrite.videframes++; 
		sav_index_msg(finf->index, keyframe, frame_sz);
	}

	static int nums = 0;
	if(nums >= 1682)
	{
		if(vbuff != NULL)
		{
			printf("---------fffff------>\n");
			free(vbuff);
		}
		printf("---------hhhhh------>\n");
		vbuff = NULL;
	}
	nums++;

	if(ret == 1)
	{
		printf("read video file over !\n");

		return -2;
	}

	return 0;
}

inline int find_audio_flag(uint fmt, char *data, int *size)
{	
	if(fmt == MEDIA_TP_AAC)
	{
		//Sync words: 0xfff;
		if(( (data[0]&0xff) == 0xff) && ((data[1]&0xf0) == 0xf0) )
		{
			//frame_length: 13 bit
			*size |= (data[3] & 0x03) <<11; //low    2 bit
			*size |= (data[4] & 0xff) <<3;	//middle 8 bit
			*size |= (data[5] & 0xe0) >>5;	//hight    3 bit
			
			return 1;
		}
	}
	else
	{
		*size = 3528;
		return 1;
	}
	
	return 0;
}

int get_audio_data(uint enc_ty, FILE *fd, uint afsize, char *frame1, int *frame_sz1, char *buff1)
{
	if(!frame1 || !fd || !buff1)
	{
		printf("Input [frame1] or [fd]  or audio mem [buff1]  is  NULL !\n");
		return -1;
	}
	static int i = 0;
	static int re_sz = 0;
	static int a_remain = 0;;
	static char end_flag = 1;
	static uchar  fullframe = 1;

	int l_offsz = 0;
	
	while(1)
	{	
		if(end_flag)
		{
			memset(buff1, 0, MAX_AUDIO_BUFF);
			re_sz = fread(buff1, 1, MAX_AUDIO_BUFF, fd);
			if(re_sz<= 0)
			{
				if(re_sz == 0)
					printf("read audio file over !\n");
				else
					perror("read audio file ERROR");
				return -2; //file over.

			}
			end_flag = 0;
		}
		
		if(find_audio_flag(enc_ty, &buff1[i], frame_sz1) > 0)
		{
			if(re_sz >= *frame_sz1)
			{
				if(a_remain > 0 )
				{
					memcpy(frame1 + l_offsz, &buff1[i], a_remain);
					i += a_remain;
					re_sz -= a_remain;
					a_remain = 0;
				}
				else
				{
					memcpy(frame1, &buff1[i], *frame_sz1);
					i += *frame_sz1;
					re_sz -= *frame_sz1;//剩下的帧数据...
				}

				end_flag = 0;
				
				return 1;
			}
			else
			{ //剩下帧不完整.
				memcpy(frame1, &buff1[i], re_sz); //保存剩下的帧数.
				a_remain = *frame_sz1 - re_sz;
				l_offsz = re_sz;
				
				end_flag  = fullframe = 0;
				i = re_sz = 0;
				end_flag = 1;
				
				continue;
			}
		}
		else
		{
			if(a_remain > 0)
			{//处理上次不完整的帧.
				if(!fullframe)
				{
					fullframe = 1;
					memcpy(frame1 , &buff1[i], a_remain);
					i += a_remain;   
					re_sz -= a_remain;
					a_remain = 0;
					return 0;
				}
			}
		}
		i++;
	} 

	return 0;
}

int audio_pro(uint enc_ty, fminf_t *finf, char* aframe, char* abuff)
{
	int frame_sz, ret;

	ret = get_audio_data(enc_ty, finf->hd.afd, finf->hd.afsz, &aframe[8], &frame_sz, abuff);
	if(ret < 0)
	{
		return ret;
	}

	if(frame_sz > 0)
	{
		ret = write_avi_video_audio_data(enc_ty, finf->hd.avifd, aframe, frame_sz);
		if(ret < 0)
		{
			printf("put audio frame failed \n");
			return -1;
		}

		finf->rewrite.videframes++;
		sav_index_msg(finf->index, 0x00, frame_sz);
	}

	static int times = 0;

	if(times == 0)
		print_tnt(&aframe[8], 32, "VEND:");
	
	times++;
	
	return 0;
}

int write_av_data(fminf_t *finf, avminf_t *avinf)
{
	uint time_v = 0, time_a = 0;
	int ret = 0;

	handle_t *hd = &finf->hd;
	vminf_t  *v	 = &avinf->vm;
	aminf_t  *a  = &avinf->am;
	
	aIndex_t *tmp = NULL;
	char video_end = 0; //file over.
	char audio_end = 0;

	
	char *v_buff, *vbuf, *v_frame, *vframe;
	char *a_buff, *abuf, *a_frame, *aframe;

	v_buff  = (char *)malloc(MAX_VIDEO_BUFF);
	v_frame = (char *)malloc(MAX_V_FAME_BUFF);
	a_buff  = (char *)malloc(MAX_AUDIO_BUFF);
	a_frame = (char *)malloc(MAX_A_FAME_BUFF);
	if(!v_buff || !v_frame || !a_buff || !a_frame)
	{
		printf("Malloc file proc [v_buff or v_frame or a_buff or a_frame] mem failed.\n");
		return -1;
	}

	vbuf = v_buff;
	vframe = v_frame;
	abuf = a_buff;
	aframe = a_frame;

	finf->index = (avi_index_t *)malloc(sizeof(avi_index_t));
	if(!finf->index)
	{
		printf("Malloc avi index arry mem failed.\n");
		return -1;
	}

	finf->index->aIndex = (aIndex_t *)malloc(MAX_INDEX_SZ *sizeof(aIndex_t));
	if(!finf->index)
	{
		printf("Malloc avi index arry mem failed.\n");
		return -1;
	}

	while(1)
	{
		if((hd->vfd) && (!video_end))
		{ // video  
			if(time_v <= time_a)
			{	
				ret = video_pro(v->encmode, finf, v_frame, v_buff);
				if(ret == -2)
				{
					video_end = 1;
				}
				time_v += 1000/v->fps;
			 }
			 else if(audio_end == 1)
			 {
				time_a += (1024000/(a->sampling * a->chns));
			 }
		}
		
		if((hd->afd > 0) && (!audio_end))
		{// audio. 
			if(time_v > time_a)
			{	
  				ret = audio_pro(a->encmode, finf, a_frame, a_buff);
				if(ret == -2)
					audio_end = 1;
								
				time_a += (1024000/(a->sampling * a->chns));
			}
			else if(video_end == 1)
			{
				time_v += 1000/v->fps;
			}
		}

		if(finf->index->indexnum >= MAX_INDEX_SZ)
		{	
			tmp  = (aIndex_t *)realloc(finf->index->aIndex, sizeof(aIndex_t)*MAX_INDEX_SZ);
			if(!tmp)
			{	
				printf("Expand malloc avi index arry mem failed.\n");
				return -1;
			}

			finf->index->aIndex = tmp;
		}
	
		if(audio_end && video_end)
			break;
	}

	if(vbuf != NULL)
	{
		printf("---------aaaaaa------>\n");
		//free(vbuf);
	}
	
	if(vframe != NULL){
		free(vframe);
		vframe = NULL;
	}
	
	if(abuf != NULL){
		free(abuf);
		abuf = NULL;
	}
	
	if(aframe != NULL){
		free(aframe);
		aframe = NULL;
	}
	
	return 0;
}

int avi_file_proc(fminf_t *finf, avminf_t *avinf)
{
	write_avi_header(finf->hd.avifd, avinf);	
	write_av_data(finf, avinf);

	finf->rewrite.avidatasz = ftell(finf->hd.avifd);
	write_avi_index(finf);

	finf->rewrite.aviriffsz = ftell(finf->hd.avifd) -8;
	rewrite_avi_header(finf);

	free(finf->index->aIndex);
	finf->index->aIndex = NULL;
	free(finf->index);
	finf->index = NULL;

	return 0;
}

//*****************************//
//*******public interface******//
//*****************************//
int lm_start_lmavi()
{
	filenums = 0;

	return 0;
}

int lm_destroy_lmavi(fminf_t *finf)
{	
	filenums = 0;
	memset(avinf_m, 0, sizeof(avinf_m));
	memset(lminf_m, 0, sizeof(lminf_m));
	
	if(finf && finf->index)
	{
		free(finf->index);
		finf->index = NULL;
	}

	if(finf)
	{
		free(finf);
		finf = NULL;
	}

	return 0;
}

int lm_set_av_para(avminf_t *avinf)
{
	if(avinf)
		avinf_m[filenums] = *avinf;
	
	return 0;
}

//========pack avi====
int lm_pack_by_file(char *infn[], mtp_t vty, mtp_t aty)
{
	int ret = 0;
	char isaudio = 0;
	if(check_file(infn, vty, aty, &isaudio) < 0)
	{
		printf("File check failed !\n");
		return -1;
	}
	
	ret = open_file(infn, &lminf_m[filenums]);
	if(ret < 0)
	{
		printf("Open file failed !\n");
		return -1;
	}

	lminf_m[filenums].vty  = vty;
	if(isaudio == 0x01)
		lminf_m[filenums].aty = aty;

	adjust_av_para(&lminf_m[filenums], &avinf_m[filenums]);
	filenums++;

	avi_file_proc(&lminf_m[filenums -1], &avinf_m[filenums -1]);

	close_file(&lminf_m[filenums -1]);
	return 0;
}

fminf_t* lm_pack_by_livestream(char *avifilename, avminf_t *live_am)
{
	if(!avifilename || !live_am)
	{
		printf("Input avifilename  or live_am is  NULL, exit !\n");
		return NULL;
	}

	FILE* fd = fopen(avifilename, "w+");
	if(fd < 0)
	{
		perror("Fopen avi file failed, error:\n");
		return NULL;
	}

	if(live_am->vm.encmode < MEDIA_TP_JPEG || live_am->vm.encmode > MEDIA_TP_H265)
	{
		printf("video type set error, exit !\n");
		return NULL;
	}

	if((live_am->am.encmode != 0) && (live_am->am.chns != 0) && (live_am->am.sampling != 0))
	{
		if(live_am->am.encmode < MEDIA_TP_AAC || live_am->am.encmode > MEDIA_TP_OPUS)
		{
			printf("audio type set error, exit !\n");
			return NULL;
		}
	}
	
	write_avi_header(fd, live_am);

	fminf_t *fm;
	fm = (fminf_t *)malloc(sizeof(fminf_t));
	if(!fm)
	{
		printf("Malloc avi index arry mem failed.\n");
		return NULL;
	}

	fm->hd.avifd = fd;
	fm->aty      = live_am->am.encmode;
	fm->vty      = live_am->vm.encmode;;
	fm->index    = (avi_index_t *)malloc(MAX_INDEX_SZ *sizeof(avi_index_t));
	if(!fm->index)
	{
		printf("Malloc avi index arry mem failed.\n");
		return NULL;
	}

	return fm;
}

int lm_put_video_frame(fminf_t *finf, uint type, char *vframe, uint frame_sz, uint nalsz)
{
	int keyframe = 0;
	get_nalu_type(type, nalsz, &vframe[8], &keyframe);	
	int ret = write_avi_video_audio_data(type, finf->hd.avifd, vframe, frame_sz);
	if(ret < 0)
	{
		printf("put video frame failed \n");
		return -1;
	}

	finf->rewrite.videframes++;
	sav_index_msg(finf->index, keyframe, frame_sz);
	return 0;
}

int lm_put_audio_frame(fminf_t *finf, uint type, char *aframe, uint frame_sz)
{
	int ret = write_avi_video_audio_data(type, finf->hd.avifd, aframe, frame_sz);
	if(ret < 0)
	{
		printf("put audio frame failed \n");
		return -1;
	}

	//索引表信息,若需要则加，不需要刻意不加.
	finf->rewrite.videframes++;
	sav_index_msg(finf->index, 0x00, frame_sz);
	return 0;
}

int lm_stop_write_avi(fminf_t *finf)
{
	 write_avi_index(finf);
	 rewrite_avi_header(finf);
	 return 0;
}
//========spare avi====
int lm_spare_avi_file()
{

	return 0;
}

//========spare avi and split video or audio====
int lm_get_video_frame()
{

	return 0;
}

int lm_get_audio_frame()
{

	return 0;
}

int lm_save_video_file()
{

	return 0;
}

int lm_save_audio_to_file()
{

	return 0;
}
