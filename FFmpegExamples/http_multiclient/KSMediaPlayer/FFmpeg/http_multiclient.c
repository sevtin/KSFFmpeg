//
//  http_multiclient.c
//  KSMediaPlayer
//
//  Created by saeipi on 2020/6/9.
//  Copyright © 2020 saeipi. All rights reserved.
//  多客户端接受数据

#include "http_multiclient.h"
#include <unistd.h>
#include "libavformat/avformat.h"
#include "libavutil/opt.h"

static void process_client(AVIOContext *client, const char *in_uri) {
    AVIOContext *input = NULL;
    uint8_t buf[1024];
    int ret, n, reply_code;
    uint8_t *resource = NULL;
    
    //avio_handshake:返回值大于0表示成功
    /**
     *执行协议握手的一个步骤以接受新客户端。
     *必须在avio_accept（）返回的客户端上调用此函数
     *将其用作读/写上下文。
     *它与avio_accept（）是分开的，因为它可能会阻塞。
     *握手步骤由应用程序可能在哪里定义
     *决定更改程序。
     *例如，在具有请求标头和答复标头的协议上，
     *一个步骤可以构成一个步骤，因为应用程序可以使用参数
     *从请求更改回复中的参数；或每个人
     *请求的一部分可以构成一个步骤。
     *如果握手已经完成，则avio_handshake（）不执行任何操作，
     *立即返回0。
     *
     * @param c在客户端上下文上执行握手
     * @成功完成一次成功握手后返回0
     * >0（如果握手已进行，但未完成）
     * <0 表示AVERROR代码
     */
    while ((ret = avio_handshake(client)) > 0) {
        av_opt_get(client, "resource", AV_OPT_SEARCH_CHILDREN, &resource);
        // check for strlen(resource) is necessary, because av_opt_get()
        // may return empty string.
        /*
         int strlen(char *s);
         功能：计算给定字符串的（unsigned int型）长度，不包括'\0'在内
         说明：返回s的长度，不包括结束符NULL。
         */
        if (resource && strlen(resource)) {
            break;
        }
        /*
         *释放已使用av_malloc（）函数分配的内存块
         *或av_realloc（）系列，并将指向它的指针设置为NULL。
         */
        av_freep(&resource);
    }
    
    if (ret < 0) {
        goto ksfail;
    }
    av_log(client, AV_LOG_TRACE, "resource=%p\n", resource);
    if (resource && resource[0] =='/' && !strcmp((resource + 1), in_uri)) {
        reply_code = 200;
    }
    else{
        reply_code = AVERROR_HTTP_NOT_FOUND;
    }
    
    if ((ret = av_opt_set_int(client, "reply_code", reply_code, AV_OPT_SEARCH_CHILDREN)) < 0) {
        av_log(client, AV_LOG_ERROR, "Failed to set reply_code: %s.\n", av_err2str(ret));
        goto ksfail;
    }
    
    av_log(client, AV_LOG_TRACE, "Set reply code to %d\n", reply_code);
    
    while ((ret = avio_handshake(client)) > 0);
    if (ret < 0) {
        goto ksfail;
    }
    fprintf(stderr, "Handshake performed.\n");
    if (reply_code != 200) {
        goto ksfail;
    }
    fprintf(stderr, "Opening input file.\n");
    
    //avio_open2：用于打开ffmpeg的输入输出文件
    //1、server:函数调用成功后创建的AVIOContext结构体
    //2、out_uri：输入输出协议的地址(或是文件地址)
    //3、AVIO_FLAG_READ：只读
    if ((ret = avio_open2(&input, in_uri, AVIO_FLAG_READ, NULL, NULL)) < 0) {
        av_log(input, AV_LOG_ERROR, "Failed to open input: %s: %s.\n", in_uri,av_err2str(ret));
        goto ksfail;
    }
    
    for (; ; ) {
        /*
         函数avio_read()读size大小的数据到buf
         int avio_read(AVIOContext *s, unsigned char *buf, int size)
         */
        /*
         avio_read()函数执行流程：
         以首次调用av_probe_input_buffer2()函数为例，avio_read(pb, buf + buf_offset,probe_size - buf_offset)；buf的大小为2048+32，buf_offset大小为0，probe_size大小为2048.

         执行函数fill_buffer()
         因为初始化后，s->buf_end 和 s->buf_ptr相等，size为2048，所以len值为0，又因为s->direct和s->update_checksum值都为0，所以执行函数fill_buffer()。通过函数fill_buffer()，将数据复制到AVIOContext *buffer中，并得到len的值为s->buf_end - s->buf_ptr.

         执行函数memcpy()
         下一次while循环，len的大小为s->buf_end - s->buf_ptr和2048取最小值，并执行memcpy，将数据复制到buf。

         反复执行，直到读取2048个字节的内容为止，则跳出循环，
         */
        n = avio_read(input, buf, sizeof(buf));
        if (n < 0) {
            if (n == AVERROR_EOF) {
                break;
            }
            av_log(input, AV_LOG_ERROR, "Error reading from input: %s.\n",
                   av_err2str(n));
            break;
        }
        avio_write(client, buf, n);
        avio_flush(client);
    }
    
ksfail:
    fprintf(stderr, "Flushing client\n");
    avio_flush(client);
    
    fprintf(stderr, "Closing client\n");
    avio_close(client);
    
    fprintf(stderr, "Closing input\n");
    avio_close(input);
    av_freep(&resource);
}

int http_multiclient_port(const char *in_uri, const char *out_uri) {
    AVDictionary *options = NULL;
    //AVIOContext是管理输入输出数据的结构体
    AVIOContext *client = NULL, *server = NULL;
    
    //const char *in_uri, *out_uri;
    int ret, pid;
    av_log_set_level(AV_LOG_TRACE);
    
    if (!in_uri || !out_uri) {
        return -1;
    }
    //avformat_network_init：初始化网络，加载socket库以及网络加密协议相关的库，为后续使用网络相关提供支持
    avformat_network_init();
    if ((ret = av_dict_set(&options, "listen", "2", 0)) < 0) {
        fprintf(stderr, "Failed to set listen mode for server: %s\n", av_err2str(ret));
        return ret;
    }
    
    //avio_open2：用于打开ffmpeg的输入输出文件
    //1、server:函数调用成功后创建的AVIOContext结构体
    //2、out_uri：输入输出协议的地址(或是文件地址)
    //3、AVIO_FLAG_WRITE：只写
    if ((ret = avio_open2(&server, out_uri, AVIO_FLAG_WRITE, NULL, &options)) < 0) {
        fprintf(stderr, "Failed to open server: %s\n", av_err2str(ret));
        return ret;
    }
    //STDERR(标准错误输出)
    fprintf(stderr, "Entering main loop.\n");
    
    for (; ; ) {
        //avio_accept：根据server创建其上的client，大于等于0表示成功
        if ((ret = avio_accept(server, &client)) < 0) {
            goto ksfail;
        }
        fprintf(stderr, "Accepted client, forking process.\n");
        
        // XXX: Since we don't reap our children and don't ignore signals
        //      this produces zombie processes.
        pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            ret = AVERROR(errno);
            goto ksfail;
        }
        //子进程
        if (pid == 0) {
            fprintf(stderr, "In child.\n");
            process_client(client, in_uri);
            avio_close(server);
            goto ksfail;
        }
        //父进程
        if (pid > 0) {
            avio_close(client);
        }
    }
    
ksfail:
    avio_close(server);
    if (ret < 0 && ret != AVERROR_EOF) {
        fprintf(stderr, "Some errors occurred: %s\n", av_err2str(ret));
        return 1;
    }
    return 0;
}
