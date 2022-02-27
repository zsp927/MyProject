#include <iostream>
using namespace std;
#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avutil.lib")
#pragma comment(lib,"swscale.lib")
extern "C"
{
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}
int main()
{
	char infile[] =  "test.rgb";
	char outfile[] = "test.mp4";
	
	av_register_all();
	avcodec_register_all();


	FILE *fp = fopen(infile, "rb");
	if (!fp)
	{
		cout << infile << " open failed!" << endl;
		getchar();
		return -1;
	}

	int width  = 1920;
	int height = 1080;
	int fps    = 25;

	//寻找编码器
	AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!codec)
	{
		cout << " avcodec_find_encoder AV_CODEC_ID_H264 failed!" << endl;
		getchar();
		return -1;
	}
	//创建编码器空间
	AVCodecContext *c = avcodec_alloc_context3(codec);
	if (!c)
	{
		cout << " avcodec_alloc_context3  failed!" << endl;
		getchar();
		return -1;
	}
	//压缩比特率
	c->bit_rate 	= 400000000;

	c->width 		= width;
	c->height 		= height;
	c->time_base 	= { 1, fps };
	c->framerate 	= { fps, 1 };
	
	//画面组大小，关键帧
	c->gop_size = 50;


	c->max_b_frames = 0;

	c->pix_fmt = AV_PIX_FMT_YUV420P;
	c->codec_id = AV_CODEC_ID_H264;
	c->thread_count = 12;
	
	//全局的编码信息  流媒体
	c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	int ret = avcodec_open2(c, codec, NULL);
	if (ret < 0)
	{
		cout << " avcodec_open2  failed!" << endl;
		getchar();
		return -1;
	}
	cout << "avcodec_open2 success!" << endl;

	//创建输出上下文
	AVFormatContext *oc = NULL;
	avformat_alloc_output_context2(&oc, 0, 0, outfile);

	//添加视频流
	AVStream *st = avformat_new_stream(oc, NULL);
	//st->codec = c;
	//如果这里不写  有时候会发生莫名错误
	st->id = 0;
	st->codecpar->codec_tag = 0;
	avcodec_parameters_from_context(st->codecpar, c);

	cout << "***********************************************" << endl;
	av_dump_format(oc, 0, outfile, 1);
	cout << "***********************************************" << endl;

	//RGB转yuv
	SwsContext *ctx = NULL;
	//sws_getCachedContext
	ctx = sws_getContext(
		width, height, AV_PIX_FMT_BGRA,
		width, height, AV_PIX_FMT_YUV420P,
		SWS_BICUBIC,
		NULL, NULL, NULL
		);
	//输入空间
	unsigned char *rgb = new unsigned char[width*height * 4];

	//输出的空间
	AVFrame *yuv = av_frame_alloc();
	yuv->format = AV_PIX_FMT_YUV420P;
	yuv->width = width;
	yuv->height = height;
	ret = av_frame_get_buffer(yuv, 32);

	if (ret < 0)
	{
		cout << " av_frame_get_buffer  failed!" << endl;
		getchar();
		return -1;
	}


	// 写入mp4头文件
	ret = avio_open(&oc->pb, outfile, AVIO_FLAG_WRITE);
	if (ret < 0)
	{
		cout << " avio_open  failed!" << endl;
		getchar();
		return -1;
	}
	ret = avformat_write_header(oc, NULL);
	if (ret < 0)
	{
		cout << " avformat_write_header  failed!" << endl;
		getchar();
		return -1;
	}
	int p = 0;
	while(true)
	{
		int len = fread(rgb, 1, width*height * 4, fp);
		if (len <= 0)break;
		uint8_t *indata[AV_NUM_DATA_POINTERS] = { 0 };
		indata[0] = rgb;
		int inlinesize[AV_NUM_DATA_POINTERS] = { 0 };
		inlinesize[0] = width * 4;

		int h = sws_scale(ctx, indata, inlinesize, 0, height,
			yuv->data, yuv->linesize
			);
		if (h <= 0)break;
		cout << "高度:" << h << endl;;

		//编码视频帧 
		yuv->pts = p;
		p += 3600;
		ret = avcodec_send_frame(c, yuv);
		if (ret )
			continue;
		AVPacket pkt;
		av_init_packet(&pkt);
		ret = avcodec_receive_packet(c, &pkt);
		if (ret)
			continue;
		//av_write_frame();
		
		av_interleaved_write_frame(oc, &pkt);

		cout << "<"<<pkt.size<<">";
	}
	
	//写入视频索引
	av_write_trailer(oc);

	//关闭视频输出io
	avio_close(oc->pb);

	//清理封装输出上下文
	avformat_free_context(oc);

	//关闭编码器
	avcodec_close(c);

	//清理编码器上下文
	avcodec_free_context(&c);

	//清理视频重采样上下文
	sws_freeContext(ctx);


	cout << "***************************end***************************" << endl;



	delete rgb;
	getchar();
	return 0;
}