## An ffmpeg and SDL Tutorial

Page 1 2 3 4 5 6 7 End Prev Home Next &nbsp_place_holder;

Text version

## Tutorial 02: Outputting to the Screen

Code: tutorial02.c

### SDL and Video

To draw to the screen, we're going to use SDL. SDL stands for Simple Direct
Layer, and is an excellent library for multimedia, is cross-platform, and is
used in several projects. You can get the library at the official website or
you can download the development package for your operating system if there is
one. You'll need the libraries to compile the code for this tutorial (and for
the rest of them, too).

SDL has many methods for drawing images to the screen, and it has one in
particular that is meant for displaying movies on the screen - what it calls a
YUV overlay. YUV (technically not YUV but YCbCr) *** A note: **There is a
great deal of annoyance from some people at the convention of calling "YCbCr"
"YUV". Generally speaking, YUV is an analog format and YCbCr is a digital
format. ffmpeg and SDL both refer to YCbCr as YUV in their code and macros. is
a way of storing raw image data like RGB. Roughly speaking, Y is the
brightness (or "luma") component, and U and V are the color components. (It's
more complicated than RGB because some of the color information is discarded,
and you might have only 1 U and V sample for every 2 Y samples.) SDL's YUV
overlay takes in a raw array of YUV data and displays it. It accepts 4
different kinds of YUV formats, but YV12 is the fastest. There is another YUV
format called YUV420P that is the same as YV12, except the U and V arrays are
switched. The 420 means it is subsampled at a ratio of 4:2:0, basically
meaning there is 1 color sample for every 4 luma samples, so the color
information is quartered. This is a good way of saving bandwidth, as the human
eye does not percieve this change. The "P" in the name means that the format
is "planar" -- simply meaning that the Y, U, and V components are in separate
arrays. ffmpeg can convert images to YUV420P, with the added bonus that many
video streams are in that format already, or are easily converted to that
format.

So our current plan is to replace the `SaveFrame()` function from Tutorial 1,
and instead output our frame to the screen. But first we have to start by
seeing how to use the SDL Library. First we have to include the libraries and
initalize SDL:

    
    
    #include <SDL.h>
    #include <SDL_thread.h>
    
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
      fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
      exit(1);
    }
    

`SDL_Init()` essentially tells the library what features we're going to use.
`SDL_GetError()`, of course, is a handy debugging function.

### Creating a Display

Now we need a place on the screen to put stuff. The basic area for displaying
images with SDL is called a **surface**:

    
    
    SDL_Surface *screen;
    
    screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 0, 0);
    if(!screen) {
      fprintf(stderr, "SDL: could not set video mode - exiting\n");
      exit(1);
    }
    

This sets up a screen with the given width and height. The next option is the
bit depth of the screen - 0 is a special value that means "same as the current
display". (This does not work on OS X; see source.)

Now we create a YUV overlay on that screen so we can input video to it, and
set up our SWSContext to convert the image data to YUV420:

    
    
    SDL_Overlay     *bmp = NULL;
    struct SWSContext *sws_ctx = NULL;
    
    bmp = SDL_CreateYUVOverlay(pCodecCtx->width, pCodecCtx->height,
                               SDL_YV12_OVERLAY, screen);
    
    // initialize SWS context for software scaling
    sws_ctx = sws_getContext(pCodecCtx->width,
                             pCodecCtx->height,
    			 pCodecCtx->pix_fmt,
    			 pCodecCtx->width,
    			 pCodecCtx->height,
    			 PIX_FMT_YUV420P,
    			 SWS_BILINEAR,
    			 NULL,
    			 NULL,
    			 NULL
    			 );
    
    

As we said before, we are using YV12 to display the image, and getting YUV420
data from ffmpeg.

### Displaying the Image

Well that was simple enough! Now we just need to display the image. Let's go
all the way down to where we had our finished frame. We can get rid of all
that stuff we had for the RGB frame, and we're going to replace the
`SaveFrame()` with our display code. To display the image, we're going to make
an AVPicture struct and set its data pointers and linesize to our YUV overlay:

    
    
      if(frameFinished) {
        SDL_LockYUVOverlay(bmp);
    
        AVPicture pict;
        pict.data[0] = bmp->pixels[0];
        pict.data[1] = bmp->pixels[2];
        pict.data[2] = bmp->pixels[1];
    
        pict.linesize[0] = bmp->pitches[0];
        pict.linesize[1] = bmp->pitches[2];
        pict.linesize[2] = bmp->pitches[1];
    
        // Convert the image into YUV format that SDL uses
        sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
    	      pFrame->linesize, 0, pCodecCtx->height,
    	      pict.data, pict.linesize);
        
        SDL_UnlockYUVOverlay(bmp);
      }    
    

First, we lock the overlay because we are going to be writing to it. This is a
good habit to get into so you don't have problems later. The AVPicture struct,
as shown before, has a `data` pointer that is an array of 4 pointers. Since we
are dealing with YUV420P here, we only have 3 channels, and therefore only 3
sets of data. Other formats might have a fourth pointer for an alpha channel
or something. `linesize` is what it sounds like. The analogous structures in
our YUV overlay are the `pixels` and `pitches` variables. ("pitches" is the
term SDL uses to refer to the width of a given line of data.) So what we do is
point the three arrays of `pict.data` at our overlay, so when we write to
pict, we're actually writing into our overlay, which of course already has the
necessary space allocated. Similarly, we get the linesize information directly
from our overlay. We change the conversion format to `PIX_FMT_YUV420P`, and we
use `sws_scale` just like before.

### Drawing the Image

But we still need to tell SDL to actually show the data we've given it. We
also pass this function a rectangle that says where the movie should go and
what width and height it should be scaled to. This way, SDL does the scaling
for us, and it can be assisted by your graphics processor for faster scaling:

    
    
    SDL_Rect rect;
    
      if(frameFinished) {
        /* ... code ... */
        // Convert the image into YUV format that SDL uses
        sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
                  pFrame->linesize, 0, pCodecCtx->height,
    	      pict.data, pict.linesize);
        
        SDL_UnlockYUVOverlay(bmp);
    	rect.x = 0;
    	rect.y = 0;
    	rect.w = pCodecCtx->width;
    	rect.h = pCodecCtx->height;
    	SDL_DisplayYUVOverlay(bmp, &rect;);
      }
    

Now our video is displayed!

Let's take this time to show you another feature of SDL: its event system. SDL
is set up so that when you type, or move the mouse in the SDL application, or
send it a signal, it generates an **event**. Your program then checks for
these events if it wants to handle user input. Your program can also make up
events to send the SDL event system. This is especially useful when
multithread programming with SDL, which we'll see in Tutorial 4. In our
program, we're going to poll for events right after we finish processing a
packet. For now, we're just going to handle the `SDL_QUIT` event so we can
exit:

    
    
    SDL_Event       event;
    
        av_free_packet(&packet;);
        SDL_PollEvent(&event;);
        switch(event.type) {
        case SDL_QUIT:
          SDL_Quit();
          exit(0);
          break;
        default:
          break;
        }
    

And there we go! Get rid of all the old cruft, and you're ready to compile. If
you are using Linux or a variant, the best way to compile using the SDL libs
is this:

    
    
    gcc -o tutorial02 tutorial02.c -lavformat -lavcodec -lswscale -lz -lm \
    `sdl-config --cflags --libs`
    

sdl-config just prints out the proper flags for gcc to include the SDL
libraries properly. You may need to do something different to get it to
compile on your system; please check the SDL documentation for your system.
Once it compiles, go ahead and run it.

What happens when you run this program? The video is going crazy! In fact,
we're just displaying all the video frames as fast as we can extract them from
the movie file. We don't have any code right now for figuring out _when_ we
need to display video. Eventually (in Tutorial 5), we'll get around to syncing
the video. But first we're missing something even more important: sound!

_**>>** Playing Sound_

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

