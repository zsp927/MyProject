//XVideoWriter.h
#pragma once
#include <string>
class AVPacket;
enum XSAMPLEFMT
{
	X_S16 = 1,
	X_FLATP = 8
};

class XVideoWriter
{
public:
	//视频输入参数
	int inWidth = 848;
	int inHeight = 480;
	int inPixFmt = 30;   //AV_PIX_FMT_BGRA

	//音频输入参数
	int inSampleRate = 44100;
	int inChannels = 2;
	XSAMPLEFMT inSampleFmt = X_S16;
	
	//视频输出参数
	int vBitrate = 4000000;
	int outWidth = 848;
	int outHeight = 480;
	int outFPS = 25;

	//音频输出参数
	int aBitrate = 64000;
	int outChannels = 2;
	int outSampleRate = 44100;
	XSAMPLEFMT outSampleFmt = X_FLATP;

	int nb_sample = 1024; //输入输出的每帧数据每通道样本数量

	virtual void Close() = 0;
	virtual bool Init(const char * file) = 0;
	virtual bool AddVideoStream() = 0;
	virtual bool AddAudioStream() = 0;
	virtual AVPacket * EncodeVideo(const unsigned char *rgb) = 0;
	virtual AVPacket * EncodeAudio(const unsigned char *pcm) = 0;
	virtual bool WriteHead() = 0;

	//会释放pkt的空间
	virtual bool WriteFrame(AVPacket *pkt) = 0;//写入头后写入帧
	virtual bool WriteEnd() = 0;//最后写入尾部，索引
	virtual bool IsVideoBefor() = 0;//最后的音视频同步

	static XVideoWriter * Get(unsigned short index = 0);
	~XVideoWriter();
	std::string filename;
protected:
	XVideoWriter();
};

