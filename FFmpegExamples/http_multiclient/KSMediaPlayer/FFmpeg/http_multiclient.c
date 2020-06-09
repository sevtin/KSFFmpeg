//
//  http_multiclient.c
//  KSMediaPlayer
//
//  Created by saeipi on 2020/6/9.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "http_multiclient.h"
#include <unistd.h>
#include "libavformat/avformat.h"
#include "libavutil/opt.h"

static void process_client(AVIOContext *client, const char *in_uri) {
    AVIOContext *input = NULL;
    uint8_t buf[1024];
    int ret, n, reply_code;
    uint8_t *resource = NULL;
    
    while ((ret = avio_handshake(client)) > 0) {
        av_opt_get(client, "resource", AV_OPT_SEARCH_CHILDREN, &resource);
        // check for strlen(resource) is necessary, because av_opt_get()
        // may return empty string.
        if (resource && strlen(resource)) {
            break;
        }
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
    
    if ((ret = avio_open2(&input, in_uri, AVIO_FLAG_READ, NULL, NULL)) < 0) {
        av_log(input, AV_LOG_ERROR, "Failed to open input: %s: %s.\n", in_uri,av_err2str(ret));
        goto ksfail;
    }
    
    for (; ; ) {
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
    AVIOContext *client = NULL, *server = NULL;
    
    //const char *in_uri, *out_uri;
    int ret, pid;
    av_log_set_level(AV_LOG_TRACE);
    
    if (!in_uri || !out_uri) {
        return -1;
    }
    
    avformat_network_init();
    if ((ret = av_dict_set(&options, "listen", "2", 0)) < 0) {
        fprintf(stderr, "Failed to set listen mode for server: %s\n", av_err2str(ret));
        return ret;
    }
    
    if ((ret = avio_open2(&server, out_uri, AVIO_FLAG_WRITE, NULL, &options)) < 0) {
        fprintf(stderr, "Failed to open server: %s\n", av_err2str(ret));
        return ret;
    }
    //STDERR(标准错误输出)
    fprintf(stderr, "Entering main loop.\n");
    
    for (; ; ) {
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
        if (pid == 0) {
            fprintf(stderr, "In child.\n");
            process_client(client, in_uri);
            avio_close(server);
            goto ksfail;
        }
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
