//XVideoWriter.cpp
#include "XVideoWriter.h"
extern "C"
{
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}
#include <iostream>
using namespace std;
#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avutil.lib")
#pragma comment(lib,"swscale.lib")
#pragma comment(lib,"swresample.lib")

class CXVideoWriter :public XVideoWriter
{
public:
	AVFormatContext *ic = NULL;	//???why dont have oc???   //封装mp4输出上下文

	//vc和ac相同，释放方式相同
	AVCodecContext *vc = NULL;	//视频编码器上下文
	AVCodecContext *ac = NULL;	//音频编码器上下文

	AVStream *vs = NULL;		//视频流
	AVStream *as = NULL;		//音频流

	SwsContext *vsc = NULL;		//像素转换的上下文
	SwrContext *asc = NULL;		//音频重采样上下文

	AVFrame *yuv = NULL;		//输出yuv
	AVFrame *pcm = NULL;		//重采样后输出的PCM

	int vpts = 0;				//视频的pts
	int apts = 0;				//音频的pts

	void Close()
	{
		if (ic) avformat_close_input(&ic);

		if (vc)
		{
			avcodec_close(vc);
			avcodec_free_context(&vc);
		}

		if (ac)
		{
			avcodec_close(ac);
			avcodec_free_context(&ac);
		}
		
		if (vsc)
		{
			//swr_free(&vsc)
			sws_freeContext(vsc);
			vsc = NULL;
		}
		if (asc) swr_free(&asc);

		if (yuv) av_frame_free(&yuv);
		if (pcm) av_frame_free(&pcm);
	}

	bool Init(const char* file)
	{
		Close();
		//封装文件输出上下文
		avformat_alloc_output_context2(&ic, NULL, NULL, file);
		if (!ic)
		{
			cerr << "avformat_alloc_output_context2 failed!" << endl;
			return false;
		}
		filename = file;
		return true;
	}

	bool AddVideoStream()
	{
		if (!ic) return false;

		//1 视频编码器创建
		AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
		if (!codec)
		{
			cerr << "avcodec_find_encoder AV_CODEC_ID_H264 failed!" << endl;
			return false;
		}

		vc = avcodec_alloc_context3(codec);
		if (!vc)
		{
			cerr << "avcodec_alloc_context3 failed!" << endl;
			return false;
		}

		vc->bit_rate = vBitrate;//比特率，压缩后每秒大小
		vc->width = outWidth;
		vc->height = outHeight;
		vc->time_base = { 1, outFPS };//时间基数
		vc->framerate = { outFPS, 1 };
		vc->gop_size = 50;//画面组大小，多少帧一个关键帧
		vc->max_b_frames = 0;
		vc->pix_fmt = AV_PIX_FMT_YUV420P;
		vc->codec_id = AV_CODEC_ID_H264;
		av_opt_set(vc->priv_data, "preset", "superfast", 0);
		vc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

		//打开编码器
		int ret = avcodec_open2(vc, codec, NULL);
		if (ret != 0)
		{
			cerr << "avcodec_open2 failed!" << endl;
			return false;
		}
		cout << "avcodec_open2 ========success========!" <<endl;

		//添加视频流到输出上下文
		vs = avformat_new_stream(ic, NULL);
		if (!vs)
		{
			cerr << "avformat_new_stream failed!" << endl;
			return false;
		}
		vs->codecpar->codec_tag = 0;//配置流信息
		avcodec_parameters_from_context(vs->codecpar, vc);//拷贝方式
		av_dump_format(ic, 0, filename.c_str(), 1);//1是输出

		//rgb to yuv

		//像素（尺寸）转换上下文 
		vsc = sws_getCachedContext(vsc,
			inWidth, inHeight, (AVPixelFormat)inPixFmt,//输入的信息
			outWidth, outHeight, AV_PIX_FMT_YUV420P,//输出的信息
			SWS_BICUBIC,//算法
			NULL,NULL,NULL//过滤器置空
			);
		if (!vsc)
		{
			cerr << "sws_getCachedContext failed!" << endl;
			return false;
		}

		if (!yuv)
		{
			yuv = av_frame_alloc();
			yuv->format = AV_PIX_FMT_YUV420P;//格式
			yuv->width = outWidth;//宽
			yuv->height = outHeight;//高
			yuv->pts = 0;//
			int ret = av_frame_get_buffer(yuv, 32);//创建空间  32为对齐方式
			if (ret != 0)
			{
				cerr << "av_frame_get_buffer failed!" << endl;
				return false;
			}
		}
		return true;
	}

	AVPacket * EncodeVideo(const unsigned char *rgb)
	{
		uint8_t *indata[AV_NUM_DATA_POINTERS] = { 0 };
		indata[0] = (uint8_t *)rgb;
		int insize[AV_NUM_DATA_POINTERS] = { 0 };
		insize[0] = inWidth * 4;

		//rgb to yuv
		int h = sws_scale(vsc, indata, insize, 0, inHeight,//输入的
			yuv->data, yuv->linesize//输出的
			);
		if (h < 0) return NULL;
		//cout << h << "|";

		yuv->pts = vpts;
		vpts++;//需要增长
		//编码
		int ret = avcodec_send_frame(vc, yuv);
		if (ret != 0) return NULL;

		AVPacket *p = av_packet_alloc();
		ret = avcodec_receive_packet(vc, p);
		if (ret != 0 ||p->size<=0)
		{
			av_packet_free(&p);
			return NULL;
		}
		av_packet_rescale_ts(p, vc->time_base, vs->time_base);
		p->stream_index = vs->index;
		return p;//返回
	}

	bool AddAudioStream()
	{
		if (!ic) return false;

		//1 找到音频编码
		AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
		if (!codec)
		{
			cerr << "avcodec_find_encoder  AV_CODEC_ID_AAC failed!" << endl;
			return false;
		}

		//2 创建并打开音频编码器
		ac = avcodec_alloc_context3(codec);
		if (!ac)
		{
			cerr << "avcodec_alloc_context3 failed!" << endl;
			return false;
		}
		ac->bit_rate = aBitrate;
		ac->sample_rate = outSampleRate;
		ac->sample_fmt = (AVSampleFormat)outSampleFmt;
		ac->channels = outChannels;
		ac->channel_layout = av_get_default_channel_layout(outChannels);
		ac->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

		int ret = avcodec_open2(ac, codec, NULL);
		if (ret != 0)
		{
			avcodec_free_context(&ac);
			cerr << "avcodec_open2 failed!" << endl;
			return false;
		}
		cout << "avcodec_open2 AV_CODEC_ID_AAC ========success======== " << endl;

		//3 新增音频流
		as = avformat_new_stream(ic, NULL);
		if (!as)
		{
			cerr << "avformat_new_stream failed!" << endl;
			return false;
		}
		as->codecpar->codec_tag = 0;
		avcodec_parameters_from_context(as->codecpar, ac);
		av_dump_format(ic, 0, filename.c_str(), 1);

		//4 音频重采样上下文件创建
		asc = swr_alloc_set_opts(asc,
			ac->channel_layout,ac->sample_fmt,ac->sample_rate,	//输出格式
			av_get_default_channel_layout(inChannels), 
			(AVSampleFormat)inSampleFmt,inSampleRate,
			0,0//日志偏移
			);
		ret = swr_init(asc);
		if (ret != 0)
		{
			cerr << "swr_init failed !" << endl;
			return false;
		}

		//5 音频重采样后输出AVFrame
		if (!pcm)
		{
			pcm = av_frame_alloc();
			pcm->format = ac->sample_fmt;
			pcm->channels = ac->channels;
			pcm->channel_layout = ac->channel_layout;
			pcm->nb_samples = nb_sample;//一帧音频的样本数量
			ret = av_frame_get_buffer(pcm, 0);//音频不需要对齐
			if (ret < 0)
			{
				cerr << "av_frame_get_buffer failed! " << endl;
				return false;
			}
			cout << "audio AVFrame create ========success========!" << endl;
		}
		return true;
	}

	AVPacket * EncodeAudio(const unsigned char *d)
	{
		//1 音频重采样
		const uint8_t *data[AV_NUM_DATA_POINTERS] = {0};
		data[0] = (uint8_t *)d;
		int len  = swr_convert(asc, pcm->data, pcm->nb_samples,//输出的
			data, pcm->nb_samples//输入的
			);
		//cout << len << "*";

		//2 音频的编码
		int ret = avcodec_send_frame(ac, pcm);
		if (ret != 0)
		{
			return NULL;
		}
		AVPacket *pkt = av_packet_alloc();
		av_init_packet(pkt);
		ret = avcodec_receive_packet(ac, pkt);
		if (ret != 0)
		{
			av_packet_free(&pkt);
			return NULL;
		}
		cout << pkt->size << "|";
		pkt->stream_index = as->index;
		pkt->pts = apts;
		pkt->dts = pkt->pts;
		apts += av_rescale_q(pcm->nb_samples, { 1, ac->sample_rate }, ac->time_base);
		return pkt;
	}

	bool WriteHead()
	{
		if (!ic)return false;
		//打开io
		int ret = avio_open(&ic->pb, filename.c_str(), AVIO_FLAG_WRITE);
		if (ret!= 0)
		{
			cerr << "avio_open failed!" << endl;
			return false;
		}

		//写入封装头
		ret = avformat_write_header(ic, NULL);
		if (ret != 0)
		{
			cerr << "avformat_write_header failed!" << endl;
			return false;
		}
		cout << "write " << filename<<" head ========success========" << endl;
		return true;
	}

	bool WriteFrame(AVPacket *pkt)
	{
		if (!ic || !pkt || pkt->size <= 0) return false;
		//av_write_frame
		if (av_interleaved_write_frame(ic, pkt) != 0)return false;
		return true;
	}

	virtual bool WriteEnd()//和写入头部信息差不多
	{
		if (!ic || !ic->pb) return false;
		
		//写入尾部信息索引
		if (av_write_trailer(ic) != 0)
		{
			cerr << "av_write_trailer failed!" << endl;
			return false;
		}

		//关闭输入io
		if (avio_close(ic->pb) != 0)
		{
			cerr << "avio_close failed!" << endl;
			return false;
		}
		cout << endl;
		cout << "WriteEnd ========success========!" << endl;
		return true;
	}

	bool IsVideoBefor()//最后的音视频同步
	{
		if (!ic || !as || !vs) return false;
		int re = av_compare_ts(vpts, vc->time_base, apts, ac->time_base);//进行比较
		if (re <= 0) return true;//a在b后
		return false;
	}
};

XVideoWriter * XVideoWriter::Get(unsigned short index)
{
	static bool isfirst = true;
	if (isfirst)
	{
		av_register_all();
		avcodec_register_all();
		isfirst = false;
	}
	static CXVideoWriter wrs[65535];
	return &wrs[index];
}

XVideoWriter::XVideoWriter() {}
XVideoWriter::~XVideoWriter() {}
