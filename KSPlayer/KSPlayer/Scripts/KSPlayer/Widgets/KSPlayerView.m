//
//  KSPlayerView.m
//  KSYuvPlayer
//
//  Created by saeipi on 2020/7/1.
//  Copyright © 2020 saeipi. All rights reserved.
//

#import "KSPlayerView.h"
#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/ES3/gl.h>
#import <OpenGLES/ES3/glext.h>
#import <OpenGLES/EAGL.h>

#define STRINGIZE(x) #x
#define STRINGIZE2(x) STRINGIZE(x)
#define SHADER_STRING(text) @ STRINGIZE2(text)

NSString *const vertexShaderString = SHADER_STRING
(
 attribute vec4 position;
 attribute vec2 TexCoordIn;
 varying   vec2 TexCoordOut;
 
 void main(void)
 {
    gl_Position = position;
    TexCoordOut = TexCoordIn;
 }
);

NSString *const rgbFragmentShaderString = SHADER_STRING
(
 varying highp vec2 v_texcoord;
 uniform sampler2D s_texture;
 
 void main()
 {
    gl_FragColor = texture2D(s_texture, v_texcoord);
 }
);

NSString *const yuvFragmentShaderString = SHADER_STRING
(
 varying lowp vec2 TexCoordOut;
 
 uniform sampler2D SamplerY;
 uniform sampler2D SamplerU;
 uniform sampler2D SamplerV;
 
 void main(void)
 {
    mediump vec3 yuv;
    lowp vec3 rgb;
    
    yuv.x = texture2D(SamplerY, TexCoordOut).r;
    yuv.y = texture2D(SamplerU, TexCoordOut).r - 0.5;
    yuv.z = texture2D(SamplerV, TexCoordOut).r - 0.5;
    
    rgb = mat3( 1,1,1,
               0,-0.39465,2.03211,
               1.13983,-0.58060,0) * yuv;
    
    gl_FragColor = vec4(rgb, 1);
 }
);

@interface KSPlayerView () {
    GLuint _texture_YUV[3];    //纹理数组。 分别用来存放Y,U,V
    GLuint _framebuffer;       //缓存buf区
    GLuint _renderbuffer;      //渲染buf区
}
@property (strong,nonatomic) EAGLContext * context;       //上下文
@property (strong,nonatomic) CAEAGLLayer * drawLayer;     //画布
@property (assign,nonatomic) GLuint        program_handle;//程序集句柄。
@property (assign,nonatomic) GLuint        width;
@property (assign,nonatomic) GLuint        height;
@end

@implementation KSPlayerView

- (void)initKit {
    [self initLayer];//显示画布
    [self initContext];//上下文
    [self initTexture];//纹理
    [self initShader];//着色器
}

- (void)initLayer {
    self.drawLayer = (CAEAGLLayer*)self.layer;
    //默认透明，改为不透明。
    self.drawLayer.opaque = YES;
    //以及颜色存储格式。
    self.drawLayer.drawableProperties = [NSDictionary dictionaryWithObjectsAndKeys:
                                         [NSNumber numberWithBool:NO], kEAGLDrawablePropertyRetainedBacking,
                                         kEAGLColorFormatRGBA8, kEAGLDrawablePropertyColorFormat,
                                         nil];
    // 当renderbuffer需要保持其内容不变时，我们才设置 kEAGLDrawablePropertyRetainedBacking 为 TRUE
}

- (void)initContext {
    //创建上下文
    self.context = [[EAGLContext alloc]initWithAPI:kEAGLRenderingAPIOpenGLES2];
    if (self.context == nil) {
        NSLog(@"API NOT SUPPORT!");
        return;
    }
    
    //设置当前的上下文。
    BOOL result = [EAGLContext setCurrentContext:self.context];
    if (!result) {
        NSLog(@"CAN'T SET CURRENT CONTEXT.");
    }
}

- (void)initTexture {
    //YUV三个位置。
    int Y = 0,U = 1,V = 2;
    if (_texture_YUV)//如果存在先删除。
    {
        glDeleteTextures(3, _texture_YUV);
    }
    
    //创建纹理
    glGenTextures(3, _texture_YUV);
    
    if (!_texture_YUV[Y] || !_texture_YUV[U] || !_texture_YUV[V])
    {
        NSLog(@"glGenTextures faild.");
        return;
    }
    
    //分别对Y,U,V进行设置。
    
    //Y
    //glActiveTexture:选择可以由纹理函数进行修改的当前纹理单位
    //并绑定
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _texture_YUV[Y]);
    //纹理过滤
    //GL_LINEAR 线性取平均值纹素，GL_NEAREST 取最近点的纹素
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);//放大过滤。
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);//缩小过滤
    //纹理包装
    //包装模式有：GL_REPEAT重复，GL_CLAMP_TO_EDGE采样纹理边缘，GL_MIRRORED_REPEAT镜像重复纹理。
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);//纹理超过S轴
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);//纹理超过T轴
    
    //U
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, _texture_YUV[U]);
    //纹理过滤
    //GL_LINEAR 线性取平均值纹素，GL_NEAREST 取最近点的纹素
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);//放大过滤。
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);//缩小过滤
    //纹理包装
    //包装模式有：GL_REPEAT重复，GL_CLAMP_TO_EDGE采样纹理边缘，GL_MIRRORED_REPEAT镜像重复纹理。
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);//纹理超过S轴
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);//纹理超过T轴
    
    //V
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, _texture_YUV[V]);
    //纹理过滤
    //GL_LINEAR 线性取平均值纹素，GL_NEAREST 取最近点的纹素
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);//放大过滤。
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);//缩小过滤
    //纹理包装
    //包装模式有：GL_REPEAT重复，GL_CLAMP_TO_EDGE采样纹理边缘，GL_MIRRORED_REPEAT镜像重复纹理。
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);//纹理超过S轴
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);//纹理超过T轴
}

/*
 1.创建着色器
 2.指定着色器源代码字符串
 3.编译着色器
 4.创建着色器可执行程序
 5.向可执行程序中添加着色器
 6.链接可执行程序
 */

- (void)initShader {
    //读取编译shader
    //顶点shader
    GLuint vertexShader = [self compileShader:vertexShaderString withType:GL_VERTEX_SHADER];
    //片元shader
    GLuint fragmentShader = [self compileShader:yuvFragmentShaderString withType:GL_FRAGMENT_SHADER];
    
    //创建程序
    self.program_handle = glCreateProgram();
    
    //向program中添加顶点着色器
    glAttachShader(self.program_handle, vertexShader);
    //向program中添加片元着色器
    glAttachShader(self.program_handle, fragmentShader);
    //绑定position属性到顶点着色器的0位置，绑定TexCoordIn到顶点着色器的1位置
    glBindAttribLocation(self.program_handle, 0, "position");
    glBindAttribLocation(self.program_handle, 1, "TexCoordIn");
    //链接程序
    glLinkProgram(self.program_handle);
    
    //查看状态
    GLint linkSuccess;
    glGetProgramiv(self.program_handle, GL_LINK_STATUS, &linkSuccess);
    if (linkSuccess == GL_FALSE) {
        GLchar messages[256];
        glGetProgramInfoLog(self.program_handle, sizeof(messages), 0, &messages[0]);
        NSString *messageString = [NSString stringWithUTF8String:messages];
        NSLog(@"%@", messageString);
        exit(1);
    }
    
    //删除
    if (vertexShader)   glDeleteShader(vertexShader);
    if (fragmentShader) glDeleteShader(fragmentShader);
    
    //使用当前程序。
    glUseProgram(self.program_handle);
    
    //从片元着色器中获取到Y,U,V变量。
    GLuint textureUniformY = glGetUniformLocation(self.program_handle, "SamplerY");
    GLuint textureUniformU = glGetUniformLocation(self.program_handle, "SamplerU");
    GLuint textureUniformV = glGetUniformLocation(self.program_handle, "SamplerV");
    
    //分别设置为0，1，2.
    glUniform1i(textureUniformY, 0);
    glUniform1i(textureUniformU, 1);
    glUniform1i(textureUniformV, 2);
}

//编译shader
- (GLuint)compileShader:(NSString*)content withType:(GLenum)shaderType
{
    //创建shader句柄
    GLuint shaderHandle = glCreateShader(shaderType);
    
    //读取文件内容
    const GLchar* source = (GLchar *)[content UTF8String];
    
    //将文件内容设置给shader
    glShaderSource(shaderHandle, 1,&source,NULL);
    //编译shader
    glCompileShader(shaderHandle);
    GLint compileSuccess;
    //获取状态
    glGetShaderiv(shaderHandle, GL_COMPILE_STATUS, &compileSuccess);
    if (compileSuccess == GL_FALSE) {
        GLchar messages[256];
        glGetShaderInfoLog(shaderHandle, sizeof(messages), 0, &messages[0]);
        NSString *messageString = [NSString stringWithUTF8String:messages];
        NSLog(@"%@", messageString);
        exit(1);
    }
    return shaderHandle;
}

- (void)displayYUVI420Data:(void *)data width:(GLuint)width height:(GLuint)height {
    
    /*
     glTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
     target参数必须是glCopyTexImage2D中的对应的target可使用的值。
     level 是mipmap等级。
     xoffset,yoffset是要修改的图像的左下角偏移。
     width,height是要修改的图像宽高像素。修改的范围在原图之外并不受到影响。
     format,type描述了图像数据的格式和类型,和glTexImage2D中的format,type一致。
     pixels 是子图像的纹理数据，替换为的值。
     */
    @synchronized(self)
    {
        //设置着色器属性
        [self setVertexAttributeWidth:width height:height];
        
        /*定义2d图层
         
         之所以要定义，是因为
         glTexSubImage2D:定义一个存在的一维纹理图像的一部分,但不能定义新的纹理
         glTexImage2D:   定义一个二维的纹理图
         所以每次宽高变化的时候需要调用glTexImage2D重新定义一次
         */
        [self image2DdefineWidth:width height:height];
        
        //设置width，height
        [self setWidth:width height:height];
        
        //YUV420p 4个Y对应一套UV，平面结构。 YUV的分布：YYYYUV。Y在一开始。长度等于点数，即宽高的积。
        glBindTexture(GL_TEXTURE_2D, _texture_YUV[0]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
        //U在Y之后，长度等于点数的1/4，即宽高的积的1/4。
        glBindTexture(GL_TEXTURE_2D, _texture_YUV[1]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width/2, height/2, GL_LUMINANCE, GL_UNSIGNED_BYTE, data + width * height);
        //V在U之后，长度等于点数的1/4，即宽高的积的1/4。
        glBindTexture(GL_TEXTURE_2D, _texture_YUV[2]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width/2, height/2, GL_LUMINANCE, GL_UNSIGNED_BYTE, data + width * height * 5 / 4);
        
        //❀
        [self render];
    }
}

- (void)setWidth:(GLuint)width height:(GLuint)height {
    if (self.width == width && self.height == height) {
        return;
    }
    //取宽高。
    self.width = width;
    self.height = height;
}

- (void)image2DdefineWidth:(GLuint)width  height:(GLuint)height{
    if (self.width == width&&self.height == height) {
        return;
    }
    //根据宽高生成空的YUV数据
    void *blackData = malloc(width * height * 1.5);
    //全部填0，实际出来的是一张绿色的图- -；但是没有去渲染就直接替换了，所以不会造成影响。只起定义作用。
    if(blackData) memset(blackData, 0x0, width * height * 1.5);
    
    /*
     GL_APIENTRY glTexImage2D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid* pixels);
     
     //target参数用于定义二维纹理；
     //如果提供了多种分辨率的纹理图像，可以使用level参数，否则level设置为0；
     //internalformat确定了哪些成分(RGBA, 深度, 亮度和强度)被选定为图像纹理单元
     //width和height表示纹理图像的宽度和高度；
     //border参数表示边框的宽度
     //format和type参数描述了纹理图像数据的格式和数据类型
     //pixels参数包含了纹理图像的数据，这个数据描述了纹理图像本身和它的边框
     */
    
    //Y
    glBindTexture(GL_TEXTURE_2D, _texture_YUV[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, blackData);
    //U
    glBindTexture(GL_TEXTURE_2D, _texture_YUV[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width/2, height/2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, blackData + width * height);
    //V
    glBindTexture(GL_TEXTURE_2D, _texture_YUV[2]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width/2, height/2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, blackData + width * height * 5 / 4);
    
    free(blackData);
}

- (void)render {
    /*
     glDrawArrays 根据顶点数组中的坐标数据和指定的模式，进行绘制。
     glDrawArrays (GLenum mode, GLint first, GLsizei count);
     mode，绘制方式
     GL_TRIANGLES:
     第一次取1，2，3，第二次取4，5，6，以此类推，不足三个就停止。
     
     GL_TRIANGLE_STRIP:
     从第一个开始取前三个1，2，3，第二次从第二开始取2，3，4，以此类推到不足3个停止。
     
     GL_TRIANGLE_FAN:
     从第一个开始取，1，2，3，第二次的第二个坐标从3开始，1，3，4，以此类推，到不足三个停止。
     
     first，从数组缓存中的哪一位开始绘制，一般为0。
     count，数组中顶点的数量。
     */
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    [self.context presentRenderbuffer:GL_RENDERBUFFER];
}

- (void)setVertexAttributeWidth:(GLuint)width height:(GLuint)height {
    if (self.width == width && self.height == height) {
        return;
    }
    
    CGSize size = self.bounds.size;
    //视口变换函数
    /*
     glViewPort(x:GLInt;y:GLInt;Width:GLSizei;Height:GLSizei);
     其中，参数X，Y指定了视见区域的左下角在窗口中的位置
     Width和Height指定了视见区域的宽度和高度。注意OpenGL使用的窗口坐标和WindowsGDI使用的窗口坐标是不一样的
     */
    glViewport(1, 1, size.width, size.height);
    
    //以屏幕中心为原点。
    /*
     这里的排布 按三个点确定一个面原则。
     由GL_TRIANGLE_STRIP定义：
     
     先取前三个坐标组成一个三角形。
     再取除了去掉第一个坐标，剩下的组成一个三角形。
     
     这样组成一个矩形最少需要4个坐标，并且排序规则为相邻的三个点第一个点为第四个点的对角。
     */
    
    //这个是用于传给顶点着色器的坐标。
    static const GLfloat squareVertices[] = {
        -1.0f,-1.0f,  //左下角。
        1.0f ,-1.0f,  //右下角。
        -1.0f,1.0f,   //左上角
        1.0f ,1.0f,   //右上角
    };
    
    //这个是用于传给片元着色器的坐标，由顶点着色器代传。
    //由于图像的存放一般是以左上角为原点，从上到下，但是OpenGL的处理是从左下角由下到上，所以图像的上下是颠倒的。
    //所以需要把其中一个坐标的上下改为相反的。左右不用换，不然左右又不对了。
    static const GLfloat coordVertices[] = {
        0.0f, 1.0f,   //左上角
        1.0f, 1.0f,   //右上角
        0.0f, 0.0f,   //左下角
        1.0f, 0.0f,   //右下角
    };
    
    /*激活顶点着色器属性*/
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    
    //设置顶点着色器的属性。如果视图不变化，就不用变。
    /*
     GL_APIENTRY glVertexAttribPointer (GLuint indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* ptr)
     
     indx       着色器代码对应变量ID
     size       此类型数据的个数
     type       数据类型
     normalized 是否对非float类型数据转化到float时候进行归一化处理
     stride     此类型数据在数组中的重复间隔宽度，byte类型计数，0为紧密排布。
     ptr        数据指针， 这个值受到VBO的影响
     */
    glVertexAttribPointer(0, 2, GL_FLOAT, 0, 0, squareVertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, 0, 0, coordVertices);
}

- (void)layoutSubviews {
    [self destoryBuffer];
    [self bufferCreate];
}

- (void)destoryBuffer {
    if (_framebuffer) {
        glDeleteFramebuffers(1, &_framebuffer);
    }
    
    if (_renderbuffer) {
        glDeleteRenderbuffers(1, &_renderbuffer);
    }
    _framebuffer = 0;
    _renderbuffer = 0;
}

- (BOOL)bufferCreate {
    //生成framebuffer和renderbuffer
    glGenFramebuffers(1, &_framebuffer);
    glGenRenderbuffers(1, &_renderbuffer);
    
    //绑定到OpenGL
    glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _renderbuffer);
    
    if (![self.context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer *)self.drawLayer])
    {
        NSLog(@"attach渲染缓冲区失败");
    }
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _renderbuffer);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        NSLog(@"创建缓冲区错误 0x%x", glCheckFramebufferStatus(GL_FRAMEBUFFER));
        return NO;
    }
    return YES;
}

//修改当前layer的创建出来的类为CAEAGLLayer。
+ (Class)layerClass {
    return [CAEAGLLayer class];
}

@end
