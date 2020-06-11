## An ffmpeg and SDL Tutorial

Page 1 2 3 4 5 6 7 End Prev Home Next &nbsp_place_holder;

Text version

## Tutorial 04: Spawning Threads

Code: tutorial04.c

### Overview

Last time we added audio support by taking advantage of SDL's audio functions.
SDL started a thread that made callbacks to a function we defined every time
it needed audio. Now we're going to do the same sort of thing with the video
display. This makes the code more modular and easier to work with - especially
when we want to add syncing. So where do we start?

First we notice that our main function is handling an awful lot: it's running
through the event loop, reading in packets, and decoding the video. So what
we're going to do is split all those apart: we're going to have a thread that
will be responsible for decoding the packets; these packets will then be added
to the queue and read by the corresponding audio and video threads. The audio
thread we have already set up the way we want it; the video thread will be a
little more complicated since we have to display the video ourselves. We will
add the actual display code to the main loop. But instead of just displaying
video every time we loop, we will integrate the video display into the event
loop. The idea is to decode the video, save the resulting frame in _another_
queue, then create a custom event (`FF_REFRESH_EVENT`) that we add to the
event system, then when our event loop sees this event, it will display the
next frame in the queue. Here's a handy ASCII art illustration of what is
going on:

    
    
     ________ audio  _______      _____
    |        | pkts |       |    |     | to spkr
    | DECODE |----->| AUDIO |--->| SDL |-->
    |________|      |_______|    |_____|
        |  video     _______
        |   pkts    |       |
        +---------->| VIDEO |
     ________       |_______|   _______
    |       |          |       |       |
    | EVENT |          +------>| VIDEO | to mon.
    | LOOP  |----------------->| DISP. |-->
    |_______|<---FF_REFRESH----|_______|
    

The main purpose of moving controlling the video display via the event loop is
that using an SDL_Delay thread, we can control exactly when the next video
frame shows up on the screen. When we finally sync the video in the next
tutorial, it will be a simple matter to add the code that will schedule the
next video refresh so the right picture is being shown on the screen at the
right time.

### Simplifying Code

We're also going to clean up the code a bit. We have all this audio and video
codec information, and we're going to be adding queues and buffers and who
knows what else. All this stuff is for one logical unit, _viz._ the movie. So
we're going to make a large struct that will hold all that information called
the `VideoState`.

    
    
    typedef struct VideoState {
    
      AVFormatContext *pFormatCtx;
      int             videoStream, audioStream;
      AVStream        *audio_st;
      AVCodecContext  *audio_ctx;
      PacketQueue     audioq;
      uint8_t         audio_buf[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2];
      unsigned int    audio_buf_size;
      unsigned int    audio_buf_index;
      AVPacket        audio_pkt;
      uint8_t         *audio_pkt_data;
      int             audio_pkt_size;
      AVStream        *video_st;
      AVCodecContext  *video_ctx;
      PacketQueue     videoq;
    
      VideoPicture    pictq[VIDEO_PICTURE_QUEUE_SIZE];
      int             pictq_size, pictq_rindex, pictq_windex;
      SDL_mutex       *pictq_mutex;
      SDL_cond        *pictq_cond;
      
      SDL_Thread      *parse_tid;
      SDL_Thread      *video_tid;
    
      char            filename[1024];
      int             quit;
    } VideoState;
    

Here we see a glimpse of what we're going to get to. First we see the basic
information - the format context and the indices of the audio and video
stream, and the corresponding AVStream objects. Then we can see that we've
moved some of those audio buffers into this structure. These (audio_buf,
audio_buf_size, etc.) were all for information about audio that was still
lying around (or the lack thereof). We've added another queue for the video,
and a buffer (which will be used as a queue; we don't need any fancy queueing
stuff for this) for the decoded frames (saved as an overlay). The VideoPicture
struct is of our own creations (we'll see what's in it when we come to it). We
also notice that we've allocated pointers for the two extra threads we will
create, and the quit flag and the filename of the movie.

So now we take it all the way back to the main function to see how this
changes our program. Let's set up our `VideoState` struct:

    
    
    int main(int argc, char *argv[]) {
    
      SDL_Event       event;
    
      VideoState      *is;
    
      is = av_mallocz(sizeof(VideoState));
    

`av_mallocz()` is a nice function that will allocate memory for us and zero it
out.

Then we'll initialize our locks for the display buffer (`pictq`), because
since the event loop calls our display function - the display function,
remember, will be pulling pre-decoded frames from `pictq`. At the same time,
our video decoder will be putting information into it - we don't know who will
get there first. Hopefully you recognize that this is a classic **race
condition**. So we allocate it now before we start any threads. Let's also
copy the filename of our movie into our `VideoState`.

    
    
    av_strlcpy(is->filename, argv[1], sizeof(is->filename));
    
    is->pictq_mutex = SDL_CreateMutex();
    is->pictq_cond = SDL_CreateCond();
    

`av_strlcpy` is a function from ffmpeg that does some extra bounds checking
beyond strncpy.

### Our First Thread

Now let's finally launch our threads and get the real work done:

    
    
    schedule_refresh(is, 40);
    
    is->parse_tid = SDL_CreateThread(decode_thread, is);
    if(!is->parse_tid) {
      av_free(is);
      return -1;
    }
    

`schedule_refresh` is a function we will define later. What it basically does
is tell the system to push a `FF_REFRESH_EVENT` after the specified number of
milliseconds. This will in turn call the video refresh function when we see it
in the event queue. But for now, let's look at `SDL_CreateThread()`.

`SDL_CreateThread()` does just that - it spawns a new thread that has complete
access to all the memory of the original process, and starts the thread
running on the function we give it. It will also pass that function user-
defined data. In this case, we're calling `decode_thread()` and with our
`VideoState` struct attached. The first half of the function has nothing new;
it simply does the work of opening the file and finding the index of the audio
and video streams. The only thing we do different is save the format context
in our big struct. After we've found our stream indices, we call another
function that we will define, `stream_component_open()`. This is a pretty
natural way to split things up, and since we do a lot of similar things to set
up the video and audio codec, we reuse some code by making this a function.

The `stream_component_open()` function is where we will find our codec
decoder, set up our audio options, save important information to our big
struct, and launch our audio and video threads. This is where we would also
insert other options, such as forcing the codec instead of autodetecting it
and so forth. Here it is:

    
    
    int stream_component_open(VideoState *is, int stream_index) {
    
      AVFormatContext *pFormatCtx = is->pFormatCtx;
      AVCodecContext *codecCtx;
      AVCodec *codec;
      SDL_AudioSpec wanted_spec, spec;
    
      if(stream_index < 0 || stream_index >= pFormatCtx->nb_streams) {
        return -1;
      }
    
      codec = avcodec_find_decoder(pFormatCtx->streams[stream_index]->codec->codec_id);
      if(!codec) {
        fprintf(stderr, "Unsupported codec!\n");
        return -1;
      }
    
      codecCtx = avcodec_alloc_context3(codec);
      if(avcodec_copy_context(codecCtx, pFormatCtx->streams[stream_index]->codec) != 0) {
        fprintf(stderr, "Couldn't copy codec context");
        return -1; // Error copying codec context
      }
    
    
      if(codecCtx->codec_type == AVMEDIA_TYPE_AUDIO) {
        // Set audio settings from codec info
        wanted_spec.freq = codecCtx->sample_rate;
        /* ...etc... */
        wanted_spec.callback = audio_callback;
        wanted_spec.userdata = is;
        
        if(SDL_OpenAudio(&wanted;_spec, &spec;) < 0) {
          fprintf(stderr, "SDL_OpenAudio: %s\n", SDL_GetError());
          return -1;
        }
      }
      if(avcodec_open2(codecCtx, codec, NULL) < 0) {
        fprintf(stderr, "Unsupported codec!\n");
        return -1;
      }
    
      switch(codecCtx->codec_type) {
      case AVMEDIA_TYPE_AUDIO:
        is->audioStream = stream_index;
        is->audio_st = pFormatCtx->streams[stream_index];
        is->audio_ctx = codecCtx;
        is->audio_buf_size = 0;
        is->audio_buf_index = 0;
        memset(&is-;>audio_pkt, 0, sizeof(is->audio_pkt));
        packet_queue_init(&is-;>audioq);
        SDL_PauseAudio(0);
        break;
      case AVMEDIA_TYPE_VIDEO:
        is->videoStream = stream_index;
        is->video_st = pFormatCtx->streams[stream_index];
        is->video_ctx = codecCtx;
        
        packet_queue_init(&is-;>videoq);
        is->video_tid = SDL_CreateThread(video_thread, is);
        is->sws_ctx = sws_getContext(is->video_st->codec->width, is->video_st->codec->height,
    				 is->video_st->codec->pix_fmt, is->video_st->codec->width,
    				 is->video_st->codec->height, PIX_FMT_YUV420P,
    				 SWS_BILINEAR, NULL, NULL, NULL
    				 );
        break;
      default:
        break;
      }
    }
    
    

This is pretty much the same as the code we had before, except now it's
generalized for audio and video. Notice that instead of aCodecCtx, we've set
up our big struct as the userdata for our audio callback. We've also saved the
streams themselves as `audio_st` and `video_st`. We also have added our video
queue and set it up in the same way we set up our audio queue. Most of the
point is to launch the video and audio threads. These bits do it:

    
    
        SDL_PauseAudio(0);
        break;
    
    /* ...... */
    
        is->video_tid = SDL_CreateThread(video_thread, is);
    

We remember `SDL_PauseAudio()` from last time, and `SDL_CreateThread()` is
used as in the exact same way as before. We'll get back to our
`video_thread()` function.

Before that, let's go back to the second half of our `decode_thread()`
function. It's basically just a for loop that will read in a packet and put it
on the right queue:

    
    
      for(;;) {
        if(is->quit) {
          break;
        }
        // seek stuff goes here
        if(is->audioq.size > MAX_AUDIOQ_SIZE ||
           is->videoq.size > MAX_VIDEOQ_SIZE) {
          SDL_Delay(10);
          continue;
        }
        if(av_read_frame(is->pFormatCtx, packet) < 0) {
          if((is->pFormatCtx->pb->error) == 0) {
    	SDL_Delay(100); /* no error; wait for user input */
    	continue;
          } else {
    	break;
          }
        }
        // Is this a packet from the video stream?
        if(packet->stream_index == is->videoStream) {
          packet_queue_put(&is-;>videoq, packet);
        } else if(packet->stream_index == is->audioStream) {
          packet_queue_put(&is-;>audioq, packet);
        } else {
          av_free_packet(packet);
        }
      }
    

Nothing really new here, except that we now have a max size for our audio and
video queue, and we've added a check for read errors. The format context has a
`ByteIOContext` struct inside it called `pb`. `ByteIOContext` is the structure
that basically keeps all the low-level file information in it.

After our for loop, we have all the code for waiting for the rest of the
program to end or informing it that we've ended. This code is instructive
because it shows us how we push events - something we'll have to later to
display the video.

    
    
      while(!is->quit) {
        SDL_Delay(100);
      }
    
     fail:
      if(1){
        SDL_Event event;
        event.type = FF_QUIT_EVENT;
        event.user.data1 = is;
        SDL_PushEvent(&event;);
      }
      return 0;
    

We get values for user events by using the SDL constant `SDL_USEREVENT`. The
first user event should be assigned the value `SDL_USEREVENT`, the next
`SDL_USEREVENT + 1`, and so on. `FF_QUIT_EVENT` is defined in our program as
`SDL_USEREVENT + 1`. We can also pass user data if we like, too, and here we
pass our pointer to the big struct. Finally we call `SDL_PushEvent()`. In our
event loop switch, we just put this by the `SDL_QUIT_EVENT` section we had
before. We'll see our event loop in more detail; for now, just be assured that
when we push the `FF_QUIT_EVENT`, we'll catch it later and raise our `quit`
flag.

### Getting the Frame: `video_thread`

After we have our codec prepared, we start our video thread. This thread reads
in packets from the video queue, decodes the video into frames, and then calls
a `queue_picture` function to put the processed frame onto a picture queue:

    
    
    int video_thread(void *arg) {
      VideoState *is = (VideoState *)arg;
      AVPacket pkt1, *packet = &pkt1;
      int frameFinished;
      AVFrame *pFrame;
    
      pFrame = av_frame_alloc();
    
      for(;;) {
        if(packet_queue_get(&is-;>videoq, packet, 1) < 0) {
          // means we quit getting packets
          break;
        }
        // Decode video frame
        avcodec_decode_video2(is->video_st->codec, pFrame, &frameFinished, packet);
    
        // Did we get a video frame?
        if(frameFinished) {
          if(queue_picture(is, pFrame) < 0) {
    	break;
          }
        }
        av_free_packet(packet);
      }
      av_free(pFrame);
      return 0;
    }
    

Most of this function should be familiar by this point. We've moved our
`avcodec_decode_video2` function here, just replaced some of the arguments;
for example, we have the AVStream stored in our big struct, so we get our
codec from there. We just keep getting packets from our video queue until
someone tells us to quit or we encounter an error.

### Queueing the Frame

Let's look at the function that stores our decoded frame, `pFrame` in our
picture queue. Since our picture queue is an SDL overlay (presumably to allow
the video display function to have as little calculation as possible), we need
to convert our frame into that. The data we store in the picture queue is a
struct of our making:

    
    
    typedef struct VideoPicture {
      SDL_Overlay *bmp;
      int width, height; /* source height & width */
      int allocated;
    } VideoPicture;
    

Our big struct has a buffer of these in it where we can store them. However,
we need to allocate the `SDL_Overlay` ourselves (notice the `allocated` flag
that will indicate whether we have done so or not).

To use this queue, we have two pointers - the writing index and the reading
index. We also keep track of how many actual pictures are in the buffer. To
write to the queue, we're going to first wait for our buffer to clear out so
we have space to store our `VideoPicture`. Then we check and see if we have
already allocated the overlay at our writing index. If not, we'll have to
allocate some space. We also have to reallocate the buffer if the size of the
window has changed!

    
    
    int queue_picture(VideoState *is, AVFrame *pFrame) {
    
      VideoPicture *vp;
      int dst_pix_fmt;
      AVPicture pict;
    
      /* wait until we have space for a new pic */
      SDL_LockMutex(is->pictq_mutex);
      while(is->pictq_size >= VIDEO_PICTURE_QUEUE_SIZE &&
    	!is->quit) {
        SDL_CondWait(is->pictq_cond, is->pictq_mutex);
      }
      SDL_UnlockMutex(is->pictq_mutex);
    
      if(is->quit)
        return -1;
    
      // windex is set to 0 initially
      vp = &is-;>pictq[is->pictq_windex];
    
      /* allocate or resize the buffer! */
      if(!vp->bmp ||
         vp->width != is->video_st->codec->width ||
         vp->height != is->video_st->codec->height) {
        SDL_Event event;
    
        vp->allocated = 0;
        alloc_picture(is);
        if(is->quit) {
          return -1;
        }
      }
    

Let's look at the `alloc_picture()` function:

    
    
    void alloc_picture(void *userdata) {
    
      VideoState *is = (VideoState *)userdata;
      VideoPicture *vp;
    
      vp = &is-;>pictq[is->pictq_windex];
      if(vp->bmp) {
        // we already have one make another, bigger/smaller
        SDL_FreeYUVOverlay(vp->bmp);
      }
      // Allocate a place to put our YUV image on that screen
      SDL_LockMutex(screen_mutex);
      vp->bmp = SDL_CreateYUVOverlay(is->video_st->codec->width,
    				 is->video_st->codec->height,
    				 SDL_YV12_OVERLAY,
    				 screen);
      SDL_UnlockMutex(screen_mutex);
      vp->width = is->video_st->codec->width;
      vp->height = is->video_st->codec->height;  
      vp->allocated = 1;
    }
    

You should recognize the `SDL_CreateYUVOverlay` function that we've moved from
our main loop to this section. This code should be fairly self-explanitory by
now. However, now we have a mutex lock around it because two threads cannot
write information to the screen at the same time! This will prevent our
`alloc_picture` function from stepping on the toes of the function that will
display the picture. (We've created this lock as a global variable and
initialized it in `main()`; see code.) Remember that we save the width and
height in the `VideoPicture` structure because we need to make sure that our
video size doesn't change for some reason.

Okay, we're all settled and we have our YUV overlay allocated and ready to
receive a picture. Let's go back to `queue_picture` and look at the code to
copy the frame into the overlay. You should recognize that part of it:

    
    
    int queue_picture(VideoState *is, AVFrame *pFrame) {
    
      /* Allocate a frame if we need it... */
      /* ... */
      /* We have a place to put our picture on the queue */
    
      if(vp->bmp) {
    
        SDL_LockYUVOverlay(vp->bmp);
        
        dst_pix_fmt = PIX_FMT_YUV420P;
        /* point pict at the queue */
    
        pict.data[0] = vp->bmp->pixels[0];
        pict.data[1] = vp->bmp->pixels[2];
        pict.data[2] = vp->bmp->pixels[1];
        
        pict.linesize[0] = vp->bmp->pitches[0];
        pict.linesize[1] = vp->bmp->pitches[2];
        pict.linesize[2] = vp->bmp->pitches[1];
        
        // Convert the image into YUV format that SDL uses
        sws_scale(is->sws_ctx, (uint8_t const * const *)pFrame->data,
    	      pFrame->linesize, 0, is->video_st->codec->height,
    	      pict.data, pict.linesize);
        
        SDL_UnlockYUVOverlay(vp->bmp);
        /* now we inform our display thread that we have a pic ready */
        if(++is->pictq_windex == VIDEO_PICTURE_QUEUE_SIZE) {
          is->pictq_windex = 0;
        }
        SDL_LockMutex(is->pictq_mutex);
        is->pictq_size++;
        SDL_UnlockMutex(is->pictq_mutex);
      }
      return 0;
    }
    

The majority of this part is simply the code we used earlier to fill the YUV
overlay with our frame. The last bit is simply "adding" our value onto the
queue. The queue works by adding onto it until it is full, and reading from it
as long as there is something on it. Therefore everything depends upon the
`is->pictq_size` value, requiring us to lock it. So what we do here is
increment the write pointer (and rollover if necessary), then lock the queue
and increase its size. Now our reader will know there is more information on
the queue, and if this makes our queue full, our writer will know about it.

### Displaying the Video

That's it for our video thread! Now we've wrapped up all the loose threads
except for one -- remember that we called the `schedule_refresh()` function
way back? Let's see what that actually did:

    
    
    /* schedule a video refresh in 'delay' ms */
    static void schedule_refresh(VideoState *is, int delay) {
      SDL_AddTimer(delay, sdl_refresh_timer_cb, is);
    }
    

`SDL_AddTimer()` is an SDL function that simply makes a callback to the user-
specfied function after a certain number of milliseconds (and optionally
carrying some user data). We're going to use this function to schedule video
updates - every time we call this function, it will set the timer, which will
trigger an event, which will have our `main()` function in turn call a
function that pulls a frame from our picture queue and displays it! Phew!

But first thing's first. Let's trigger that event. That sends us over to:

    
    
    static Uint32 sdl_refresh_timer_cb(Uint32 interval, void *opaque) {
      SDL_Event event;
      event.type = FF_REFRESH_EVENT;
      event.user.data1 = opaque;
      SDL_PushEvent(&event;);
      return 0; /* 0 means stop timer */
    }
    

Here is the now-familiar event push. `FF_REFRESH_EVENT` is defined here as
`SDL_USEREVENT + 1`. One thing to notice is that when we `return 0`, SDL stops
the timer so the callback is not made again.

Now we've pushed an `FF_REFRESH_EVENT`, we need to handle it in our event
loop:

    
    
    for(;;) {
    
      SDL_WaitEvent(&event;);
      switch(event.type) {
      /* ... */
      case FF_REFRESH_EVENT:
        video_refresh_timer(event.user.data1);
        break;
    

and _that_ sends us to this function, which will actually pull the data from
our picture queue:

    
    
    void video_refresh_timer(void *userdata) {
    
      VideoState *is = (VideoState *)userdata;
      VideoPicture *vp;
      
      if(is->video_st) {
        if(is->pictq_size == 0) {
          schedule_refresh(is, 1);
        } else {
          vp = &is-;>pictq[is->pictq_rindex];
          /* Timing code goes here */
    
          schedule_refresh(is, 80);
          
          /* show the picture! */
          video_display(is);
          
          /* update queue for next picture! */
          if(++is->pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE) {
    	is->pictq_rindex = 0;
          }
          SDL_LockMutex(is->pictq_mutex);
          is->pictq_size--;
          SDL_CondSignal(is->pictq_cond);
          SDL_UnlockMutex(is->pictq_mutex);
        }
      } else {
        schedule_refresh(is, 100);
      }
    }
    

For now, this is a pretty simple function: it pulls from the queue when we
have something, sets our timer for when the next video frame should be shown,
calls video_display to actually show the video on the screen, then increments
the counter on the queue, and decreases its size. You may notice that we don't
actually do anything with `vp` in this function, and here's why: we will.
Later. We're going to use it to access timing information when we start
syncing the video to the audio. See where it says "timing code here"? In that
section, we're going to figure out how soon we should show the next video
frame, and then input that value into the `schedule_refresh()` function. For
now we're just putting in a dummy value of 80. Technically, you could guess
and check this value, and recompile it for every movie you watch, but 1) it
would drift after a while and 2) it's quite silly. We'll come back to it
later, though.

We're almost done; we just have one last thing to do: display the video!
Here's that `video_display` function:

    
    
    
    
    
    void video_display(VideoState *is) {
    
      SDL_Rect rect;
      VideoPicture *vp;
      float aspect_ratio;
      int w, h, x, y;
      int i;
    
      vp = &is-;>pictq[is->pictq_rindex];
      if(vp->bmp) {
        if(is->video_st->codec->sample_aspect_ratio.num == 0) {
          aspect_ratio = 0;
        } else {
          aspect_ratio = av_q2d(is->video_st->codec->sample_aspect_ratio) *
    	is->video_st->codec->width / is->video_st->codec->height;
        }
        if(aspect_ratio <= 0.0) {
          aspect_ratio = (float)is->video_st->codec->width /
    	(float)is->video_st->codec->height;
        }
        h = screen->h;
        w = ((int)rint(h * aspect_ratio)) & -3;
        if(w > screen->w) {
          w = screen->w;
          h = ((int)rint(w / aspect_ratio)) & -3;
        }
        x = (screen->w - w) / 2;
        y = (screen->h - h) / 2;
        
        rect.x = x;
        rect.y = y;
        rect.w = w;
        rect.h = h;
        SDL_LockMutex(screen_mutex);
        SDL_DisplayYUVOverlay(vp->bmp, &rect;);
        SDL_UnlockMutex(screen_mutex);
      }
    }
    

Since our screen can be of any size (we set ours to 640x480 and there are ways
to set it so it is resizable by the user), we need to dynamically figure out
how big we want our movie rectangle to be. So first we need to figure out our
movie's **aspect ratio**, which is just the width divided by the height. Some
codecs will have an odd **sample aspect ratio**, which is simply the
width/height radio of a single pixel, or sample. Since the height and width
values in our codec context are measured in pixels, the actual aspect ratio is
equal to the aspect ratio times the sample aspect ratio. Some codecs will show
an aspect ratio of 0, and this indicates that each pixel is simply of size
1x1. Then we scale the movie to fit as big in our `screen` as we can. The `&
-3` bit-twiddling in there simply rounds the value to the nearest multiple of
4. Then we center the movie, and call `SDL_DisplayYUVOverlay()`, making sure
we use the screen mutex to access it.

So is that it? Are we done? Well, we still have to rewrite the audio code to
use the new `VideoStruct`, but those are trivial changes, and you can look at
those in the sample code. The last thing we have to do is to change our
callback for ffmpeg's internal "quit" callback function:

    
    
    VideoState *global_video_state;
    
    int decode_interrupt_cb(void) {
      return (global_video_state && global_video_state->quit);
    }
    

We set `global_video_state` to the big struct in `main()`.

So that's it! Go ahead and compile it:

    
    
    gcc -o tutorial04 tutorial04.c -lavutil -lavformat -lavcodec -lswscale -lz -lm \
    `sdl-config --cflags --libs`
    

and enjoy your unsynced movie! Next time we'll finally build a video player
that actually works!

_**>>** Syncing Video_

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

