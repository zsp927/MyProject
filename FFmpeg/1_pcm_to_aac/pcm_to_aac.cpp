/*PCM_to_AAC*/
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
extern "C"
{
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
#pragma warning(disable:6031)
#pragma warning(disable:26812)
#pragma warning(disable:6283)
#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avutil.lib")
#pragma comment(lib,"swscale.lib")
#pragma comment(lib,"swresample.lib")

//wav和aac的大小相似，内部有什么区别吗？

int main()
{
	char infile[] = "test.pcm";//wavis the same as pcm,naked data,just add a head of file
	char outfile[] = "test.aac";
	av_register_all();//register all of the 封装器+解封器
	avcodec_register_all();//注册所有的编码器+解码器

	//1：先找到音频编码器
	AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (!codec)
	{
		std::cout << "avcodec_find_encoder error" << std::endl;
		getchar();
		return -1;
	}

	//2：创建编码器上下文（指定编码器类型）
	AVCodecContext* c = avcodec_alloc_context3(codec);
	if (!c)
	{
		std::cout << "avcodec_alloc_context3 error" << std::endl;
		getchar();
		return -1;
	}

	c->bit_rate = 64000;//比特率
	c->sample_rate = 44100;//采样率
	c->sample_fmt = AV_SAMPLE_FMT_FLTP;
	c->channel_layout = AV_CH_LAYOUT_STEREO;//通道类型：立体声
	c->channels = 2;//通道数
	c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;//文件头信息

	//3：打开音频编码器
	int ret = avcodec_open2(c, codec, NULL);
	if (ret < 0)
	{
		std::cout << "avcodec_open2 error" << std::endl;
		getchar();
		return -1;
	}

	//4：创建输出的封装上下文
	AVFormatContext* oc = NULL;
	avformat_alloc_output_context2(&oc, NULL, NULL, outfile);
	if (!oc)
	{
		std::cout << "avformat_alloc_output_context2 error" << std::endl;
		getchar();
		return -1;
	}

	//5：创建输出流
	AVStream* st = avformat_new_stream(oc, NULL);//先新建流空间-st
	st->codecpar->codec_tag = 0;//不做编码，置0
	avcodec_parameters_from_context(st->codecpar, c);//封装格式的参数(st)与上面编码器的参数(c)一致，所以使用copy
	//avcodec_parameters_copy(st->codecpar,);//不能这样copy
	av_dump_format(oc, 0, outfile, 1);//配置后打印出来查看信息  参数1是输出的意思

	//6：先打开文件，再写入头
	ret = avio_open(&oc->pb, outfile, AVIO_FLAG_WRITE);//打开文件
	if (ret < 0)
	{
		std::cout << "avio_open error" << std::endl;
		getchar();
		return -1;
	}
	ret = avformat_write_header(oc, NULL);//写入头

	//输入的信息是原生，但是我们需要的是能支持H264的aac格式的信息——重采样
	//7：创建音频重采样上下文--空间
	SwrContext* actx = NULL;//SwrContext是音频独有的，SwsContext是视频配置像素尺寸用的，要分清
	actx = swr_alloc_set_opts(actx,//上一行：需要添加#include <libswresample/swresample.h>
		c->channel_layout, c->sample_fmt, c->sample_rate,//输出格式（我们需要的）
		AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, 44100,//输入格式（原生）
		0, 0);//日志偏移
	if (!actx)//0是成功！
	{
		std::cout << "swr_alloc_set_opts error" << std::endl;
		getchar();
		return -1;
	}

	ret = swr_init(actx);//初始化
	if (ret < 0)
	{
		std::cout << "swr_init error" << std::endl;
		getchar();
		return -1;
	}

	//8：上下文空间创建好了，下面打开输入的音频文件，开始进行死循环重采样
	//死循环前的单独信息配置
	AVFrame* frame = av_frame_alloc();
	frame->format = AV_SAMPLE_FMT_FLTP;
	frame->channels = 2;
	frame->channel_layout = AV_CH_LAYOUT_STEREO;
	frame->nb_samples = 1024;//音频独有的样本数：nb（一帧音频的样本数：默认是1024）

	ret = av_frame_get_buffer(frame, 0);//通过样本大小来分配空间
	if (ret < 0)
	{
		std::cout << "av_frame_get_buffer error" << std::endl;
		getchar();
		return -1;
	}

	int readSize = frame->nb_samples * 2 * 2;//设置一次读取的样本大小
	char* pcm = new char[readSize];//读出来后存放的空间
	FILE* fp = fopen(infile, "rb");//二进制只读方式打开文件
	for (;;)
	{
		int len = fread(pcm, 1, readSize, fp);
		if (len <= 0)break;

		//真正的重采样
		const uint8_t* data[1];
		data[0] = (uint8_t*)pcm;

		len = swr_convert(actx, frame->data, frame->nb_samples,//输出的信息
									  data, frame->nb_samples);//输入的信息
		if (len <= 0)  break;

		//音频编码
		AVPacket pkt; 
		av_init_packet(&pkt);

		ret = avcodec_send_frame(c, frame);
		if (ret != 0) continue;
		ret = avcodec_receive_packet(c, &pkt);
		if (ret != 0) continue;

		//把音频封装入aac文件
		pkt.stream_index = 0;
		pkt.pts = 0;//因为音频不存在B帧这种
		pkt.dts = 0;
		ret = av_interleaved_write_frame(oc, &pkt);//释放pkt空间

		std::cout << "[" << len << "]";//不要换行
	}
	delete pcm;
	pcm = NULL;
	av_write_trailer(oc);//写入视频索引
	avio_close(oc->pb);//关闭视频输出io
	avformat_free_context(oc);//清理封装输出上下文
	avcodec_close(c);//关闭编码器
	avcodec_free_context(&c);//清理编码器上下文
	std::cout << "SUCCESS!!!" << std::endl;
	getchar();
	return 0;
}