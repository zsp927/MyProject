/*MP4_to_MOV*/
#include <iostream>
#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable:6031)
extern "C"
{
#include <libavformat/avformat.h>//缺少会发生编译错误
}
#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avutil.lib")//缺少时会出现LNK链接错误

//int main()
//{
//	//std::cout << AVClass::version << std::endl;
//	std::cout << avcodec_configuration() << std::endl;
//	return 0;
//}
 
//??
//为什么不能生成m3u8文件，就算生成了如何打开？FFplay??
//mp4和mov大小为什么一样

int main()
{
	char infile[] = "test.mp4";
	char outfile[] = "test.mov";//名字可以随意换：因为avformat_alloc_output_context2会根据输出文件名匹配出响应的视频格式
	av_register_all();//注册所有的封装器+解封器，这个可以识别各种封装格式

	//1：打开封装
	AVFormatContext *ic = NULL;//ic是输入的
	//AVFormatContext 内容包括两个：
							//AVIOContext *pb：输入输出上下文(写入文件时需要，读取文件时不需要)   
							//AVStream **streams：每个stream存放一个流：视频/音频/字幕
	avformat_open_input(&ic,infile,0,0);
	//4个参数：
		//AVFormatContext**ps：存放整个封装格式的上下文
		//const char *url：路径（本地文件/http网站流/rtsp摄像头流）
		//AVInputFormat *fmt：输入格式的打开方式（一般置空：让url自己识别）
		//AVDictionary **options：设置格式的对应参数（一般置空）
	if (!ic)
	{
		std::cout << "avformat_open_input failed!" << std::endl;
		getchar();
		return -1;
	}

	///2：创建输出上下文--新建空间
	AVFormatContext *oc = NULL;//oc是输出
	avformat_alloc_output_context2(&oc, NULL, NULL, outfile);
	//内容与avformat_open_input类似：
								//第二个参数：指定输出格式（一般置空）
								//第三个参数：格式名（一般置空）
	if (!oc)
	{
		std::cerr << "avformat_alloc_output_context2 " << outfile << " failed!" << std::endl;
		getchar();
		return -1;
	}

	//3：创建放入流信息(视频+音频)空间--空间  这两个函数一写：就把配置的视频+音频信息写入到oc里了
	AVStream *videoStream = avformat_new_stream(oc, NULL);//两个参数：输出上下文+编码格式（因为只转封装，不做编码，所以置空）
	AVStream *audioStream = avformat_new_stream(oc, NULL);

	///4：通过copy配置参数信息--两个参数：目标文件<-------源文件
	avcodec_parameters_copy(videoStream->codecpar, ic->streams[0]->codecpar);//0是视频
	avcodec_parameters_copy(audioStream->codecpar, ic->streams[1]->codecpar);//1是音频

	videoStream->codecpar->codec_tag = 0;//因为不做编码，所以置空
	audioStream->codecpar->codec_tag = 0;

	av_dump_format(ic, 0, infile, 0);//打印信息  4个参数：第二个参数：索引值，第四个参数：0是输入，1是输出
	std::cout<<std::endl;//上一行：方便调试(经验)，这次的参数是copy的，以后学的手动配置，尤其是在音频上
	std::cout << "———————以上输出的是输入的内容信息（视频+音频），下面是新生成的文件的视频+信息————————" << std::endl;
	std::cout << std::endl;
	av_dump_format(oc, 0, outfile, 1);

	//5：打开输出的文件，开始写入头部信息
	int ret = avio_open(&oc->pb, outfile, AVIO_FLAG_WRITE);//三个参数：输出文件的上下文,url,写入的标志
	if (ret < 0)
	{
		std::cerr << "avio open failed!" << std::endl;
		getchar();
		return -1;
	}

	ret = avformat_write_header(oc, NULL);//写入头文件
	if (ret < 0)//这里易错：因为出错不返回错误信息，正常打印
	{
		std::cerr << "avformat_write_header failed!" << std::endl;
		getchar();
	}

	//6：开始死循环写每一帧的信息
	AVPacket pkt;//死循环前的单独信息配置
	for (;;)//无限循环
	{
		int re = av_read_frame(ic, &pkt);//读取一帧开始写入，两个参数：上写文+压缩的一帧（因为每帧不同，所以读取一帧就释放一帧，使用unref防止内存泄漏）
		if (re < 0)//读完所有帧：re<0
			break;

		//要是不写下面的直接报错！！！
		//接下来的pkt开头的代码（三段代码很像，就是参数不同）作用是：调试时间基准：time_base，让输入的视频与输出的视频时间对应一致
		AVRational itime = ic->streams[pkt.stream_index]->time_base;
		AVRational otime = oc->streams[pkt.stream_index]->time_base;
		pkt.pts = av_rescale_q_rnd(pkt.pts, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
		pkt.dts = av_rescale_q_rnd(pkt.dts, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
		pkt.duration = av_rescale_q_rnd(pkt.duration, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
		pkt.pos = -1;
		//av_write_frame(oc, &pkt);//写入一帧：与read_frame对应
		//av_packet_unref(&pkt);//成对编程：与读取一帧写入对应，防止内存泄漏
		av_interleaved_write_frame(oc, &pkt);//内部自动缓冲 对pts进行排序+释放，不用上2行了

		//std::cout << "帧";//每读一帧，打印一个"帧"
		std::cout << pkt.pts<<"  ";//每读一帧，打印一个"帧"
	}
	av_write_trailer(oc);//写入尾部索引，否则时长为0
	avio_close(oc->pb);//成对编程：与第五步写入头部对应，才能正常输出
	std::cout << std::endl;
	std::cout <<"——运行成功，程序结束——" << std::endl;
	getchar();
	return 0;
}
