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




#include "XVideoThread.h"
#include "XDecode.h"
#include <iostream>
using namespace std;
//打开，不管成功与否都清理
bool XVideoThread::Open(AVCodecParameters *para, IVideoCall *call,int width,int height)
{
	if (!para)return false;
	Clear();

	vmux.lock();
	synpts = 0;
	//初始化显示窗口
	this->call = call;
	if (call)
	{
		call->Init(width, height);
	}
	vmux.unlock();
	int re = true;
	if (!decode->Open(para))
	{
		cout << "audio XDecode open failed!" << endl;
		re = false;
	}
	
	cout << "XAudioThread::Open :" << re << endl;
	return re;
}
void XVideoThread::SetPause(bool isPause)
{
	vmux.lock();
	this->isPause = isPause;
	vmux.unlock();
}
void XVideoThread::run()
{
	while (!isExit)
	{
		vmux.lock();
		if (this->isPause)
		{
			vmux.unlock();
			msleep(5);
			continue;
		}
		//cout << "synpts = " << synpts << " dpts =" << decode->pts << endl;
		//音视频同步
		if (synpts >0 && synpts < decode->pts)
		{
			vmux.unlock();
			msleep(1);
			continue;
		}
		AVPacket *pkt = Pop();
		bool re = decode->Send(pkt);
		if (!re)
		{
			vmux.unlock();
			msleep(1);
			continue;
		}
		//一次send 多次recv
		while (!isExit)
		{
			AVFrame * frame = decode->Recv();
			if (!frame)break;
			//显示视频
			if (call)
			{
				call->Repaint(frame);
			}

		}
		vmux.unlock();
	}
}
//解码pts，如果接收到的解码数据pts >= seekpts return true 并且显示画面
bool XVideoThread::RepaintPts(AVPacket *pkt, long long seekpts)
{
	vmux.lock();
	bool re = decode->Send(pkt);
	if (!re)
	{
		vmux.unlock();
		return true; //表示结束解码
	}
	AVFrame *frame = decode->Recv();
	if (!frame)
	{
		vmux.unlock();
		return false;
	}
	//到达位置
	if (decode->pts >= seekpts)
	{
		if(call)
			call->Repaint(frame);
		vmux.unlock();
		return true;
	}
	XFreeFrame(&frame);
	vmux.unlock();
	return false;
}
XVideoThread::XVideoThread()
{
}


XVideoThread::~XVideoThread()
{

}
