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

#include "XResample.h"
extern "C" {
#include <libswresample/swresample.h>
#include <libavcodec/avcodec.h>
}
#pragma comment(lib,"swresample.lib")
#include <iostream>
using namespace std;

void XResample::Close()
{
	mux.lock();
	if (actx)
		swr_free(&actx);

	mux.unlock();
}

//输出参数和输入参数一致除了采样格式，输出为S16
bool XResample::Open(AVCodecParameters *para,bool isClearPara)
{
	if (!para)return false;
	mux.lock();
	//音频重采样 上下文初始化
	//if(!actx)
	//	actx = swr_alloc();

	//如果actx为NULL会分配空间
	actx = swr_alloc_set_opts(actx,
		av_get_default_channel_layout(2),	//输出格式
		(AVSampleFormat)outFormat,			//输出样本格式 1 AV_SAMPLE_FMT_S16
		para->sample_rate,					//输出采样率
		av_get_default_channel_layout(para->channels),//输入格式
		(AVSampleFormat)para->format,
		para->sample_rate,
		0, 0
	);
	if(isClearPara)
		avcodec_parameters_free(&para);
	int re = swr_init(actx);
	mux.unlock();
	if (re != 0)
	{
		char buf[1024] = { 0 };
		av_strerror(re, buf, sizeof(buf) - 1);
		cout << "swr_init  failed! :" << buf << endl;
		return false;
	}
	//unsigned char *pcm = NULL;
	return true;
}

//返回重采样后大小,不管成功与否都释放indata空间
int XResample::Resample(AVFrame *indata, unsigned char *d)
{
	if (!indata) return 0;
	if (!d)
	{
		av_frame_free(&indata);
		return 0;
	}
	uint8_t *data[2] = { 0 };
	data[0] = d;
	int re = swr_convert(actx,
		data, indata->nb_samples,		//输出
		(const uint8_t**)indata->data, indata->nb_samples	//输入
	);
	int outSize = re * indata->channels * av_get_bytes_per_sample((AVSampleFormat)outFormat);
	av_frame_free(&indata);
	if (re <= 0)return re;
	
	return outSize;
}
XResample::XResample()
{
}


XResample::~XResample()
{
}
