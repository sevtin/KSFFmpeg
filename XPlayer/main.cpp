/*******************************************************************************
**                                                                            **
**                     Jiedi(China nanjing)Ltd.                               **
**	               创建：夏曹俊，此代码可用作为学习参考                       **
*******************************************************************************/

/*****************************FILE INFOMATION***********************************
**
** Project       : FFmpeg
** Description   : FFMPEG项目创建示例
** Contact       : xiacaojun@qq.com
**        博客   : http://blog.csdn.net/jiedichina
**		视频课程
**网易云课堂	http://study.163.com/u/xiacaojun
**腾讯课堂		https://jiedi.ke.qq.com/
**csdn学院		http://edu.csdn.net/lecturer/lecturer_detail?lecturer_id=961
**51cto学院	    http://edu.51cto.com/lecturer/index/user_id-12016059.html
**下载最新的ffmpeg版本 http://www.ffmpeg.club
**
**   ffmpeg+qt播放器 学员群 ：462249121 加入群下载代码和交流
**   微信公众号  : jiedi2007
**		头条号	 : 夏曹俊
**
*******************************************************************************/
//！！！！！！！！！ 学员加群462249121下载代码和交流




#include "XPlay2.h"
#include <QtWidgets/QApplication>
#include <iostream>
using namespace std;
#include "XDemux.h"
#include "XDecode.h"
#include "XResample.h"
#include <QThread>
#include "XAudioPlay.h"
#include "XAudioThread.h"
#include "XVideoThread.h"
class TestThread :public QThread
{
public:
	XAudioThread at;
	XVideoThread vt;
	void Init()
	{
		//香港卫视
        const char *url = "rtmp://live.hkstv.hk.lxdns.com/live/hks";
		cout << "demux.Open = " << demux.Open(url);
		demux.Read();
		demux.Clear();
		demux.Close();
		url = "v1080.mp4";
		cout << "demux.Open = " << demux.Open(url);
		//cout << "CopyVPara = " << demux.CopyVPara() << endl;
		//cout << "CopyAPara = " << demux.CopyAPara() << endl;
		//cout << "seek=" << demux.Seek(0.95) << endl;

		/////////////////////////////
		//cout << "vdecode.Open() = " << vdecode.Open(demux.CopyVPara()) << endl;
		//vdecode.Clear();
		//vdecode.Close();
		//cout << "adecode.Open() = " << adecode.Open(demux.CopyAPara()) << endl;
		//cout << "resample.Open = " << resample.Open(demux.CopyAPara()) << endl;
		//XAudioPlay::Get()->channels = demux.channels;
		//XAudioPlay::Get()->sampleRate = demux.sampleRate;

		//cout << "XAudioPlay::Get()->Open() = " << XAudioPlay::Get()->Open()<<endl;

		cout << "at.Open = "<<at.Open(demux.CopyAPara(), demux.sampleRate, demux.channels);
		vt.Open(demux.CopyVPara(), video, demux.width, demux.height);
		at.start();
		vt.start();
	}
	unsigned char *pcm = new unsigned char[1024 * 1024];
	void run()
	{
		for (;;)
		{
			AVPacket *pkt = demux.Read();
			if (demux.IsAudio(pkt))
			{
				at.Push(pkt);
				/*adecode.Send(pkt);
				AVFrame *frame = adecode.Recv();
				int len = resample.Resample(frame, pcm);
				cout<<"Resample:"<<len<<" ";
				while (len > 0)
				{
					if (XAudioPlay::Get()->GetFree() >= len)
					{
						XAudioPlay::Get()->Write(pcm, len);
						break;
					}
					msleep(1);
				}*/
				//cout << "Audio:" << frame << endl;
			}
			else
			{
				vt.Push(pkt);
				//vdecode.Send(pkt);
				//AVFrame *frame = vdecode.Recv();
				//video->Repaint(frame);
				//msleep(40);
				//cout << "Video:" << frame << endl;
			}
			if (!pkt)
			{
				demux.Seek(0);
				//break;
			}
		}
	}
	///测试XDemux
	XDemux demux;
	///解码测试
	//XDecode vdecode;
	//XDecode adecode;
	//XResample resample;
	XVideoWidget *video = 0;

};
#include "XDemuxThread.h"
//v1080.mp4
int main(int argc, char *argv[])
{

	//初始化显示
	/*

	for (;;)
	{
		AVPacket *pkt = demux.Read();
		if (demux.IsAudio(pkt))
		{
			adecode.Send(pkt);
			AVFrame *frame = adecode.Recv();
			//cout << "Audio:" << frame << endl;
		}
		else
		{
			vdecode.Send(pkt);
			AVFrame *frame = vdecode.Recv();
			//cout << "Video:" << frame << endl;
		}
		if (!pkt)break;
	}*/
	//TestThread tt;
	QApplication a(argc, argv);
	XPlay2 w;
	w.show();

	//初始化gl窗口
	//w.ui.video->Init(tt.demux.width, tt.demux.height);
	//tt.video = w.ui.video;
	//tt.Init();
	//tt.start();
	//XDemuxThread dt;
	//char *url = "rtmp://live.hkstv.hk.lxdns.com/live/hks";
	//url = "v1080.mp4";
	//dt.Open(url, w.ui.video);
	//dt.Start();

	return a.exec();
}
