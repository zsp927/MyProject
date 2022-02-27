//rgb_pcm_to_mp4.cpp
#include "XVideoWriter.h"
#include <iostream>
using namespace std;
#pragma warning(disable:6031)
#pragma warning(disable:6283)
int main()
{
	char outfile[] = "test_rgb+pcm.mp4";
	char rgbfile[] = "test.rgb";
	char pcmfile[] = "test.pcm";
	
	XVideoWriter *xw = XVideoWriter::Get(0);
	cout<<xw->Init(outfile);
	cout<<xw->AddVideoStream();
	xw->AddAudioStream();

	FILE *fp = fopen(rgbfile, "rb");
	if (!fp)
	{
		cout << "open " << rgbfile <<" failed!"<< endl;
		getchar();
		return -1;
	}

	FILE *fa = fopen(pcmfile, "rb");
	if (!fa)
	{
		cout << "open " << pcmfile << " failed!" << endl;
		getchar();
		return -1;
	}

	int size = xw->inWidth*xw->inHeight * 4;
	unsigned char *rgb = new unsigned char[size];
	int asize = xw->nb_sample*xw->inChannels * 2;
	unsigned char *pcm = new unsigned char[asize];

	xw->WriteHead();
	AVPacket *pkt = NULL;
	int len = 0;
	for (;;)
	{
		if (xw->IsVideoBefor())//处理视频信息
		{
			len = fread(rgb, 1, size, fp);
			if (len <= 0)break;
			pkt = xw->EncodeVideo(rgb);
			if (pkt) cout << ".";
			else
			{
				cout << "-";
				continue; 
			}
			if (xw->WriteFrame(pkt))
				cout << "+";
		}
		else//处理音频信息
		{
			len = fread(pcm, 1, asize, fa);
			if (len <= 0)break;
			pkt = xw->EncodeAudio(pcm);
			xw->WriteFrame(pkt);
		}
	}
	xw->WriteEnd();
	delete rgb;
	rgb = NULL;
	cout << "\n============================end============================" << endl;
	//rgb转yuv

	//编码视频帧

	getchar();
	return 0;
}