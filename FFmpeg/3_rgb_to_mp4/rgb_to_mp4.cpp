/*RGB_to_MP4*/
#include <iostream>
using namespace std;
#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable:6031)
#pragma warning(disable:26812)
#pragma warning(disable:6283)
extern "C"
{
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}
#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avutil.lib")
#pragma comment(lib,"swscale.lib")
int main()
{
	char infile[] = "test.rgb";
	char outfile[] = "test_out_rgb.mp4";
	av_register_all();//注册所有的封装器+解封器
	avcodec_register_all();//注册所有的编码器+解码器

	FILE* fp = fopen(infile, "rb");//用二进制只读方式打开文件
	if (!fp)
	{
		cout << infile << " open failed!" << endl;
		getchar();
		return -1;
	}

	//宽高不一致会出错
	int width = 848;//已知的信息（因为我们的项目是录屏：屏幕大小固定已知了）
	int height = 480;
	//int width = 1920;
	//int height = 1080;
	int fps = 25;

	//1：先找到编码器
	AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);//寻找编码器函数有两种：通过ID号+通过名称(by_name)
	if (!codec)//codec是用来存放找到的编码器
	{
		cout << " avcodec_find_encoder AV_CODEC_ID_H264 failed!" << endl;
		getchar();
		return -1;
	}

	//2：创建编码的上下文
	AVCodecContext* c = avcodec_alloc_context3(codec);
	if (!c)//c是用来存放编码上下文
	{
		cout << " avcodec_alloc_context3  failed!" << endl;
		getchar();
		return -1;
	}

	//3：配置编码器的参数信息
	c->bit_rate = 400000000;//压缩比特率
	c->width = width;
	c->height = height;
	c->time_base = { 1, fps };//时间基准（根据帧率，新版本基本上不用了）
	c->framerate = { fps, 1 };
	c->gop_size = 50;//设置画面组大小：表示每50帧就有一个关键帧
	c->max_b_frames = 0;//设置B帧数量：B帧越多，压缩率越好，但是播放效率低（所以置空）
	c->pix_fmt = AV_PIX_FMT_YUV420P;//设置像素格式：yuv420p
	c->codec_id = AV_CODEC_ID_H264;//设置编码格式：h264
	c->thread_count = 8;//yuv编码帧的时候的线程
	c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;//全局的编码信息（流媒体编码信息不太头部，在帧间，所以可以随时打开）

	//4：打开编码器
	int ret = avcodec_open2(c, codec, NULL);
	if (ret < 0)
	{
		cout << " avcodec_open2  failed!" << endl;//失败原因是上面的参数设置有误
		getchar();
		return -1;
	}
	cout << "avcodec_open2 success!" << endl;

	//5：创建输出的上下文
	AVFormatContext* oc = NULL;//oc是输出
	avformat_alloc_output_context2(&oc, 0, 0, outfile);//学过

	//6：创建放入流信息(视频+音频)空间--空间
	AVStream* st = avformat_new_stream(oc, NULL);
	st->id = 0;//默认为0
	st->codecpar->codec_tag = 0;//置空，不做编码
	avcodec_parameters_from_context(st->codecpar, c);//把编码格式告诉封装器格式（直接copy），c是编码上下文

	cout << "——打印整个封装器信息如下：——" << endl;
	av_dump_format(oc, 0, outfile, 1);//av_dump_format打印整个封装器，有4个参数：oc是封装对象，最后的1是输出（0是输入）
	cout << "————————————————" << endl;

	//以上把上下文和编码器全部创建好了

	//7：把RGB转成YUV
	SwsContext* ctx = NULL;//SwsContext函数是视频的重采样函数（负责转换），需要添加头文件
	ctx = sws_getCachedContext(ctx,
		width, height, AV_PIX_FMT_BGRA,//输入的
		width, height, AV_PIX_FMT_YUV420P,//输出的（这里把宽高改变就是放大/缩小）
		SWS_BICUBIC,//一个转换时用的算法
		NULL, NULL, NULL
	);
	//设置输入的空间  unsigned char是防止负数  乘4的原因是RGBA
	unsigned char* rgb = new unsigned char[width * height * 4];

	//8：开始死循环写每一帧的信息
	//死循环前的单独信息配置
	//设置输出的空间：yuv
	AVFrame* yuv = av_frame_alloc();//AVFrame存放编码前的原始数据   AVPacket存放编码后的数据
	yuv->format = AV_PIX_FMT_YUV420P;//设置输出格式
	yuv->width = width;
	yuv->height = height;
	ret = av_frame_get_buffer(yuv, 32);//用av_frame_get_buffer读取的空间来确定空间大小(放入yuv的data)，32是默认对齐方式
	if (ret < 0)
	{
		cout << " av_frame_get_buffer  failed!" << endl;
		getchar();
		return -1;
	}

	//yuv->h264需要的
	ret = avio_open(&oc->pb, outfile, AVIO_FLAG_WRITE);//打开文件
	if (ret < 0)
	{
		cout << " avio_open  failed!" << endl;
		getchar();
		return -1;
	}
	ret = avformat_write_header(oc, NULL);//写入头
	if (ret < 0)
	{
		cout << " avformat_write_header  failed!" << endl;
		getchar();
		return -1;
	}

	//下面开始每一帧的操作
	int p = 0;
	for (;;)//无限循环
	{
		int len = fread(rgb, 1, width * height * 4, fp);//fread读取，
		if (len <= 0)  break;

		uint8_t* indata[AV_NUM_DATA_POINTERS] = { 0 };
		indata[0] = rgb;//输入的数据
		int inlinesize[AV_NUM_DATA_POINTERS] = { 0 };
		inlinesize[0] = width * 4;//一行数据的大小（RGBA4个字节）
		int h = sws_scale(ctx, indata, inlinesize, 0, height,//inlinesize是一行大小，0表示y的深度值，height表示输入的高度（不变）
			yuv->data, yuv->linesize);//输出的
		if (h <= 0)  break;

		//rgb已转成yuv，下面是yuv编码成h264

		//编码帧
		yuv->pts = p;//就是为了让frame有次序（pts是显示时间）
		//yuv->pict_type = AV_PICTURE_TYPE_I;//动态改变关键帧
		p = p + 3600;//p递增：900000/25=3600

		ret = avcodec_send_frame(c, yuv);//发送帧来编码：在线程内部编码
		if (ret != 0)  continue;//0是成功
		AVPacket pkt;
		av_init_packet(&pkt);
		ret = avcodec_receive_packet(c, &pkt);//发送的帧用pkt接收
		if (ret != 0)  continue;//0是成功

		//av_write_frame(oc, &pkt);
		//av_packet_unref(&pkt);
		av_interleaved_write_frame(oc, &pkt);//不用上两行了，尽量用这个函数：直接清理pkt,按照dts顺序

		cout << "<" << pkt.size << ">";//打印编码后数据的大小：大的里面有I帧
		//为什么都是0呢？
	}
	av_write_trailer(oc);//写入尾部视频索引
	avio_close(oc->pb);//关闭视频输出io
	avformat_free_context(oc);//清理封装输出上下文
	avcodec_close(c);//关闭编码器
	avcodec_free_context(&c);//清理编码器上下文
	sws_freeContext(ctx);//清理视频重采样上下文
	cout << "——运行成功，程序结束——" << endl;
	delete rgb;//成对编程：对应unsigned char *rgb
	getchar();
	return 0;
}