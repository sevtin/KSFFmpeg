## An ffmpeg and SDL Tutorial

Page 1 2 3 4 5 6 7 End Prev Home Next &nbsp_place_holder;

Text version

## Tutorial 01: Making Screencaps

Code: tutorial01.c

### Overview

Movie files have a few basic components. First, the file itself is called a
**container**, and the type of container determines where the information in
the file goes. Examples of containers are AVI and Quicktime. Next, you have a
bunch of **streams**; for example, you usually have an audio stream and a
video stream. (A "stream" is just a fancy word for "a succession of data
elements made available over time".) The data elements in a stream are called
**frames**. Each stream is encoded by a different kind of **codec**. The codec
defines how the actual data is COded and DECoded - hence the name CODEC.
Examples of codecs are DivX and MP3. **Packets** are then read from the
stream. Packets are pieces of data that can contain bits of data that are
decoded into raw frames that we can finally manipulate for our application.
For our purposes, each packet contains complete frames, or multiple frames in
the case of audio.

At its very basic level, dealing with video and audio streams is very easy:

    
    
    10 OPEN video_stream FROM video.avi
    20 READ packet FROM video_stream INTO frame
    30 IF frame NOT COMPLETE GOTO 20
    40 DO SOMETHING WITH frame
    50 GOTO 20
    

Handling multimedia with ffmpeg is pretty much as simple as this program,
although some programs might have a very complex "DO SOMETHING" step. So in
this tutorial, we're going to open a file, read from the video stream inside
it, and our DO SOMETHING is going to be writing the frame to a PPM file.

### Opening the File

First, let's see how we open a file in the first place. With ffmpeg, you have
to first initialize the library.

    
    
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <ffmpeg/swscale.h>
    ...
    int main(int argc, charg *argv[]) {
    av_register_all();
    

This registers all available file formats and codecs with the library so they
will be used automatically when a file with the corresponding format/codec is
opened. Note that you only need to call av_register_all() once, so we do it
here in main(). If you like, it's possible to register only certain individual
file formats and codecs, but there's usually no reason why you would have to
do that.

Now we can actually open the file:

    
    
    AVFormatContext *pFormatCtx = NULL;
    
    // Open video file
    if(avformat_open_input(&pFormatCtx;, argv[1], NULL, 0, NULL)!=0)
      return -1; // Couldn't open file
    

We get our filename from the first argument. This function reads the file
header and stores information about the file format in the AVFormatContext
structure we have given it. The last three arguments are used to specify the
file format, buffer size, and format options, but by setting this to NULL or
0, libavformat will auto-detect these.

This function only looks at the header, so next we need to check out the
stream information in the file.:

    
    
    // Retrieve stream information
    if(avformat_find_stream_info(pFormatCtx, NULL)<0)
      return -1; // Couldn't find stream information
    

This function populates `pFormatCtx->streams` with the proper information. We
introduce a handy debugging function to show us what's inside:

    
    
    // Dump information about file onto standard error
    av_dump_format(pFormatCtx, 0, argv[1], 0);
    

Now `pFormatCtx->streams` is just an array of pointers, of size
`pFormatCtx->nb_streams`, so let's walk through it until we find a video
stream.

    
    
    int i;
    AVCodecContext *pCodecCtxOrig = NULL;
    AVCodecContext *pCodecCtx = NULL;
    
    // Find the first video stream
    videoStream=-1;
    for(i=0; i<pFormatCtx->nb_streams; i++)
      if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
        videoStream=i;
        break;
      }
    if(videoStream==-1)
      return -1; // Didn't find a video stream
    
    // Get a pointer to the codec context for the video stream
    pCodecCtx=pFormatCtx->streams[videoStream]->codec;
    

The stream's information about the codec is in what we call the "codec
context." This contains all the information about the codec that the stream is
using, and now we have a pointer to it. But we still have to find the actual
codec and open it:

    
    
    AVCodec *pCodec = NULL;
    
    // Find the decoder for the video stream
    pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
    if(pCodec==NULL) {
      fprintf(stderr, "Unsupported codec!\n");
      return -1; // Codec not found
    }
    // Copy context
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if(avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
      fprintf(stderr, "Couldn't copy codec context");
      return -1; // Error copying codec context
    }
    // Open codec
    if(avcodec_open2(pCodecCtx, pCodec)<0)
      return -1; // Could not open codec
    

Note that we must not use the AVCodecContext from the video stream directly!
So we have to use avcodec_copy_context() to copy the context to a new location
(after allocating memory for it, of course).

### Storing the Data

Now we need a place to actually store the frame:

    
    
    AVFrame *pFrame = NULL;
    
    // Allocate video frame
    pFrame=av_frame_alloc();
    

Since we're planning to output PPM files, which are stored in 24-bit RGB,
we're going to have to convert our frame from its native format to RGB. ffmpeg
will do these conversions for us. For most projects (including ours) we're
going to want to convert our initial frame to a specific format. Let's
allocate a frame for the converted frame now.

    
    
    // Allocate an AVFrame structure
    pFrameRGB=av_frame_alloc();
    if(pFrameRGB==NULL)
      return -1;
    

Even though we've allocated the frame, we still need a place to put the raw
data when we convert it. We use `avpicture_get_size` to get the size we need,
and allocate the space manually:

    
    
    uint8_t *buffer = NULL;
    int numBytes;
    // Determine required buffer size and allocate buffer
    numBytes=avpicture_get_size(PIX_FMT_RGB24, pCodecCtx->width,
                                pCodecCtx->height);
    buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));
    

`av_malloc` is ffmpeg's malloc that is just a simple wrapper around malloc
that makes sure the memory addresses are aligned and such. It will _not_
protect you from memory leaks, double freeing, or other malloc problems.

Now we use avpicture_fill to associate the frame with our newly allocated
buffer. About the AVPicture cast: the AVPicture struct is a subset of the
AVFrame struct - the beginning of the AVFrame struct is identical to the
AVPicture struct.

    
    
    // Assign appropriate parts of buffer to image planes in pFrameRGB
    // Note that pFrameRGB is an AVFrame, but AVFrame is a superset
    // of AVPicture
    avpicture_fill((AVPicture *)pFrameRGB, buffer, PIX_FMT_RGB24,
                    pCodecCtx->width, pCodecCtx->height);
    

Finally! Now we're ready to read from the stream!

### Reading the Data

What we're going to do is read through the entire video stream by reading in
the packet, decoding it into our frame, and once our frame is complete, we
will convert and save it.

    
    
    struct SwsContext *sws_ctx = NULL;
    int frameFinished;
    AVPacket packet;
    // initialize SWS context for software scaling
    sws_ctx = sws_getContext(pCodecCtx->width,
        pCodecCtx->height,
        pCodecCtx->pix_fmt,
        pCodecCtx->width,
        pCodecCtx->height,
        PIX_FMT_RGB24,
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL
        );
    
    i=0;
    while(av_read_frame(pFormatCtx, &packet;)>=0) {
      // Is this a packet from the video stream?
      if(packet.stream_index==videoStream) {
    	// Decode video frame
        avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished;, &packet;);
        
        // Did we get a video frame?
        if(frameFinished) {
        // Convert the image from its native format to RGB
            sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
    		  pFrame->linesize, 0, pCodecCtx->height,
    		  pFrameRGB->data, pFrameRGB->linesize);
    	
            // Save the frame to disk
            if(++i<=5)
              SaveFrame(pFrameRGB, pCodecCtx->width, 
                        pCodecCtx->height, i);
        }
      }
        
      // Free the packet that was allocated by av_read_frame
      av_free_packet(&packet;);
    }
    

**A note on packets**

Technically a packet can contain partial frames or other bits of data, but
ffmpeg's parser ensures that the packets we get contain either complete or
multiple frames.

The process, again, is simple: `av_read_frame()` reads in a packet and stores
it in the `AVPacket` struct. Note that we've only allocated the packet
structure - ffmpeg allocates the internal data for us, which is pointed to by
`packet.data`. This is freed by the `av_free_packet()` later.
`avcodec_decode_video()` converts the packet to a frame for us. However, we
might not have all the information we need for a frame after decoding a
packet, so `avcodec_decode_video()` sets frameFinished for us when we have the
next frame. Finally, we use `sws_scale()` to convert from the native format
(`pCodecCtx->pix_fmt`) to RGB. Remember that you can cast an AVFrame pointer
to an AVPicture pointer. Finally, we pass the frame and height and width
information to our SaveFrame function.

Now all we need to do is make the SaveFrame function to write the RGB
information to a file in PPM format. We're going to be kind of sketchy on the
PPM format itself; trust us, it works.

    
    
    void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
      FILE *pFile;
      char szFilename[32];
      int  y;
      
      // Open file
      sprintf(szFilename, "frame%d.ppm", iFrame);
      pFile=fopen(szFilename, "wb");
      if(pFile==NULL)
        return;
      
      // Write header
      fprintf(pFile, "P6\n%d %d\n255\n", width, height);
      
      // Write pixel data
      for(y=0; y<height; y++)
        fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);
      
      // Close file
      fclose(pFile);
    }
    

We do a bit of standard file opening, etc., and then write the RGB data. We
write the file one line at a time. A PPM file is simply a file that has RGB
information laid out in a long string. If you know HTML colors, it would be
like laying out the color of each pixel end to end like `#ff0000#ff0000`....
would be a red screen. (It's stored in binary and without the separator, but
you get the idea.) The header indicated how wide and tall the image is, and
the max size of the RGB values.

Now, going back to our main() function. Once we're done reading from the video
stream, we just have to clean everything up:

    
    
    // Free the RGB image
    av_free(buffer);
    av_free(pFrameRGB);
    
    // Free the YUV frame
    av_free(pFrame);
    
    // Close the codecs
    avcodec_close(pCodecCtx);
    avcodec_close(pCodecCtxOrig);
    
    // Close the video file
    avformat_close_input(&pFormatCtx;);
    
    return 0;
    

You'll notice we use av_free for the memory we allocated with
avcode_alloc_frame and av_malloc.

That's it for the code! Now, if you're on Linux or a similar platform, you'll
run:

    
    
    gcc -o tutorial01 tutorial01.c -lavutil -lavformat -lavcodec -lz -lavutil -lm
    

If you have an older version of ffmpeg, you may need to drop -lavutil:

    
    
    gcc -o tutorial01 tutorial01.c -lavformat -lavcodec -lz -lm
    

Most image programs should be able to open PPM files. Test it on some movie
files.

_**>>** Tutorial 2: Outputting to the Screen_

* * *

Function Reference

Data Reference

email:

dranger at gmail dot com

Back to dranger.com

This work is licensed under the Creative Commons Attribution-Share Alike 2.5
License. To view a copy of this license, visit
http://creativecommons.org/licenses/by-sa/2.5/ or send a letter to Creative
Commons, 543 Howard Street, 5th Floor, San Francisco, California, 94105, USA.

  
Code examples are based off of FFplay, Copyright (c) 2003 Fabrice Bellard, and
a tutorial by Martin Bohme.

