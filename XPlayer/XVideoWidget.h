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



#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QGLShaderProgram>
#include <mutex>
#include "IVideoCall.h"
struct AVFrame;
class XVideoWidget : public QOpenGLWidget, protected QOpenGLFunctions,public IVideoCall
{
	Q_OBJECT

public:
	virtual void Init(int width, int height);

	//不管成功与否都释放frame空间
	virtual void Repaint(AVFrame *frame);

	XVideoWidget(QWidget *parent);
	~XVideoWidget();
protected:
	//刷新显示
	void paintGL();

	//初始化gl
	void initializeGL();

	// 窗口尺寸变化
	void resizeGL(int width, int height);
private:
	std::mutex mux;

	//shader程序
	QGLShaderProgram program;

	//shader中yuv变量地址
	GLuint unis[3] = { 0 };
	//openg的 texture地址
	GLuint texs[3] = { 0 };

	//材质内存空间
	unsigned char *datas[3] = { 0 };

	int width = 240;
	int height = 128;

};
