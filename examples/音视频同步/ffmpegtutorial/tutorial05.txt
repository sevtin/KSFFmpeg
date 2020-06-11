## An ffmpeg and SDL Tutorial

Page 1 2 3 4 5 6 7 End Prev Home Next &nbsp_place_holder;

Text version

## Tutorial 05: Synching Video

Code: tutorial05.c

## CAVEAT

When I first made this tutorial, all of my syncing code was pulled from
ffplay.c. Today, it is a totally different program, and improvements in the
ffmpeg libraries (and in ffplay.c itself) have caused some strategies to
change. While this code still works, it doesn't look good, and there are many
more improvements that this tutorial could use.

### How Video Syncs

So this whole time, we've had an essentially useless movie player. It plays
the video, yeah, and it plays the audio, yeah, but it's not quite yet what we
would call a _movie_. So what do we do?

#### PTS and DTS

Fortunately, both the audio and video streams have the information about how
fast and when you are supposed to play them inside of them. Audio streams have
a sample rate, and the video streams have a frames per second value. However,
if we simply synced the video by just counting frames and multiplying by frame
rate, there is a chance that it will go out of sync with the audio. Instead,
packets from the stream might have what is called a **decoding time stamp
(DTS)** and a **presentation time stamp (PTS)**. To understand these two
values, you need to know about the way movies are stored. Some formats, like
MPEG, use what they call "B" frames (B stands for "bidirectional"). The two
other kinds of frames are called "I" frames and "P" frames ("I" for "intra"
and "P" for "predicted"). I frames contain a full image. P frames depend upon
previous I and P frames and are like diffs or deltas. B frames are the same as
P frames, but depend upon information found in frames that are displayed both
_before_ and _after_ them! This explains why we might not have a finished
frame after we call `avcodec_decode_video2`.

So let's say we had a movie, and the frames were displayed like: I B B P. Now,
we need to know the information in P before we can display either B frame.
Because of this, the frames might be stored like this: I P B B. This is why we
have a decoding timestamp and a presentation timestamp on each frame. The
decoding timestamp tells us when we need to decode something, and the
presentation time stamp tells us when we need to display something. So, in
this case, our stream might look like this:

    
    
       PTS: 1 4 2 3
       DTS: 1 2 3 4
    Stream: I P B B
    

Generally the PTS and DTS will only differ when the stream we are playing has
B frames in it.

When we get a packet from `av_read_frame()`, it will contain the PTS and DTS
values for the information inside that packet. But what we really want is the
PTS of our newly decoded raw frame, so we know when to display it.

Fortunately, FFMpeg supplies us with a "best effort" timestamp, which you can
get via, av_frame_get_best_effort_timestamp()

#### Synching

Now, while it's all well and good to know when we're supposed to show a
particular video frame, but how do we actually do so? Here's the idea: after
we show a frame, we figure out when the next frame should be shown. Then we
simply set a new timeout to refresh the video again after that amount of time.
As you might expect, we check the value of the PTS of the next frame against
the system clock to see how long our timeout should be. This approach works,
but there are two issues that need to be dealt with.

First is the issue of knowing when the next PTS will be. Now, you might think
that we can just add the video rate to the current PTS -- and you'd be mostly
right. However, some kinds of video call for frames to be repeated. This means
that we're supposed to repeat the current frame a certain number of times.
This could cause the program to display the next frame too soon. So we need to
account for that.

The second issue is that as the program stands now, the video and the audio
chugging away happily, not bothering to sync at all. We wouldn't have to worry
about that if everything worked perfectly. But your computer isn't perfect,
and a lot of video files aren't, either. So we have three choices: sync the
audio to the video, sync the video to the audio, or sync both to an external
clock (like your computer). For now, we're going to sync the video to the
audio.

### Coding it: getting the frame PTS

Now let's get into the code to do all this. We're going to need to add some
more members to our big struct, but we'll do this as we need to. First let's
look at our video thread. Remember, this is where we pick up the packets that
were put on the queue by our decode thread. What we need to do in this part of
the code is get the PTS of the frame given to us by `avcodec_decode_video2`.
The first way we talked about was getting the DTS of the last packet
processed, which is pretty easy:

    
    
      double pts;
    
      for(;;) {
        if(packet_queue_get(&is-;>videoq, packet, 1) < 0) {
          // means we quit getting packets
          break;
        }
        pts = 0;
        // Decode video frame
        len1 = avcodec_decode_video2(is->video_st->codec,
                                    pFrame, &frameFinished;, packet);
        if(packet->dts != AV_NOPTS_VALUE) {
          pts = av_frame_get_best_effort_timestamp(pFrame);
        } else {
          pts = 0;
        }
        pts *= av_q2d(is->video_st->time_base);
    

We set the PTS to 0 if we can't figure out what it is.

Well, that was easy. A technical note: You may have noticed we're using
`int64` for the PTS. This is because the PTS is stored as an integer. This
value is a timestamp that corresponds to a measurement of time in that
stream's time_base unit. For example, if a stream has 24 frames per second, a
PTS of 42 is going to indicate that the frame should go where the 42nd frame
would be if there we had a frame every 1/24 of a second (certainly not
necessarily true).

We can convert this value to seconds by dividing by the framerate. The
`time_base` value of the stream is going to be 1/framerate (for fixed-fps
content), so to get the PTS in seconds, we multiply by the `time_base`.

### Coding: Synching and using the PTS

So now we've got our PTS all set. Now we've got to take care of the two
synchronization problems we talked about above. We're going to define a
function called `synchronize_video` that will update the PTS to be in sync
with everything. This function will also finally deal with cases where we
don't get a PTS value for our frame. At the same time we need to keep track of
when the next frame is expected so we can set our refresh rate properly. We
can accomplish this by using an internal video_clock value which keeps track
of how much time has passed according to the video. We add this value to our
big struct.

    
    
    typedef struct VideoState {
      double          video_clock; // pts of last decoded frame / predicted pts of next decoded frame
    

Here's the synchronize_video function, which is pretty self-explanatory:

    
    
    double synchronize_video(VideoState *is, AVFrame *src_frame, double pts) {
    
      double frame_delay;
    
      if(pts != 0) {
        /* if we have pts, set video clock to it */
        is->video_clock = pts;
      } else {
        /* if we aren't given a pts, set it to the clock */
        pts = is->video_clock;
      }
      /* update the video clock */
      frame_delay = av_q2d(is->video_st->codec->time_base);
      /* if we are repeating a frame, adjust clock accordingly */
      frame_delay += src_frame->repeat_pict * (frame_delay * 0.5);
      is->video_clock += frame_delay;
      return pts;
    }
    

You'll notice we account for repeated frames in this function, too.

Now let's get our proper PTS and queue up the frame using `queue_picture`,
adding a new pts argument:

    
    
        // Did we get a video frame?
        if(frameFinished) {
          pts = synchronize_video(is, pFrame, pts);
          if(queue_picture(is, pFrame, pts) < 0) {
    	break;
          }
        }
    

The only thing that changes about `queue_picture` is that we save that pts
value to the VideoPicture structure that we queue up. So we have to add a pts
variable to the struct and add a line of code:

    
    
    typedef struct VideoPicture {
      ...
      double pts;
    }
    int queue_picture(VideoState *is, AVFrame *pFrame, double pts) {
      ... stuff ...
      if(vp->bmp) {
        ... convert picture ...
        vp->pts = pts;
        ... alert queue ...
      }
    

So now we've got pictures lining up onto our picture queue with proper PTS
values, so let's take a look at our video refreshing function. You may recall
from last time that we just faked it and put a refresh of 80ms. Well, now
we're going to find out how to actually figure it out.

Our strategy is going to be to predict the time of the next PTS by simply
measuring the time between the previous pts and this one. At the same time, we
need to sync the video to the audio. We're going to make an **audio clock**:
an internal value thatkeeps track of what position the audio we're playing is
at. It's like the digital readout on any mp3 player. Since we're synching the
video to the audio, the video thread uses this value to figure out if it's too
far ahead or too far behind.

We'll get to the implementation later; for now let's assume we have a
`get_audio_clock` function that will give us the time on the audio clock. Once
we have that value, though, what do we do if the video and audio are out of
sync? It would silly to simply try and leap to the correct packet through
seeking or something. Instead, we're just going to adjust the value we've
calculated for the next refresh: if the PTS is too far behind the audio time,
we double our calculated delay. if the PTS is too far ahead of the audio time,
we simply refresh as quickly as possible. Now that we have our adjusted
refresh time, or **delay**, we're going to compare that with our computer's
clock by keeping a running `frame_timer`. This frame timer will sum up all of
our calculated delays while playing the movie. In other words, this
frame_timer is _what time it should be when we display the next frame._ We
simply add the new delay to the frame timer, compare it to the time on our
computer's clock, and use that value to schedule the next refresh. This might
be a bit confusing, so study the code carefully:

    
    
    void video_refresh_timer(void *userdata) {
    
      VideoState *is = (VideoState *)userdata;
      VideoPicture *vp;
      double actual_delay, delay, sync_threshold, ref_clock, diff;
      
      if(is->video_st) {
        if(is->pictq_size == 0) {
          schedule_refresh(is, 1);
        } else {
          vp = &is-;>pictq[is->pictq_rindex];
    
          delay = vp->pts - is->frame_last_pts; /* the pts from last time */
          if(delay <= 0 || delay >= 1.0) {
    	/* if incorrect delay, use previous one */
    	delay = is->frame_last_delay;
          }
          /* save for next time */
          is->frame_last_delay = delay;
          is->frame_last_pts = vp->pts;
    
          /* update delay to sync to audio */
          ref_clock = get_audio_clock(is);
          diff = vp->pts - ref_clock;
    
          /* Skip or repeat the frame. Take delay into account
    	 FFPlay still doesn't "know if this is the best guess." */
          sync_threshold = (delay > AV_SYNC_THRESHOLD) ? delay : AV_SYNC_THRESHOLD;
          if(fabs(diff) < AV_NOSYNC_THRESHOLD) {
    	if(diff <= -sync_threshold) {
    	  delay = 0;
    	} else if(diff >= sync_threshold) {
    	  delay = 2 * delay;
    	}
          }
          is->frame_timer += delay;
          /* computer the REAL delay */
          actual_delay = is->frame_timer - (av_gettime() / 1000000.0);
          if(actual_delay < 0.010) {
    	/* Really it should skip the picture instead */
    	actual_delay = 0.010;
          }
          schedule_refresh(is, (int)(actual_delay * 1000 + 0.5));
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
    

There are a few checks we make: first, we make sure that the delay between the
PTS and the previous PTS make sense. If it doesn't we just guess and use the
last delay. Next, we make sure we have a synch threshold because things are
never going to be perfectly in synch. ffplay uses 0.01 for its value. We also
make sure that the synch threshold is never smaller than the gaps in between
PTS values. Finally, we make the minimum refresh value 10 milliseconds*.*****
Really here we should skip the frame, but we're not going to bother.

We added a bunch of variables to the big struct so don't forget to check the
code. Also, don't forget to initialize the frame timer and the initial
previous frame delay in `stream_component_open`:

    
    
        is->frame_timer = (double)av_gettime() / 1000000.0;
        is->frame_last_delay = 40e-3;
    

### Synching: The Audio Clock

Now it's time for us to implement the audio clock. We can update the clock
time in our `audio_decode_frame` function, which is where we decode the audio.
Now, remember that we don't always process a new packet every time we call
this function, so there are two places we have to update the clock at. The
first place is where we get the new packet: we simply set the audio clock to
the packet's PTS. Then if a packet has multiple frames, we keep time the audio
play by counting the number of samples and multiplying them by the given
samples-per-second rate. So once we have the packet:

    
    
        /* if update, update the audio clock w/pts */
        if(pkt->pts != AV_NOPTS_VALUE) {
          is->audio_clock = av_q2d(is->audio_st->time_base)*pkt->pts;
        }
    

And once we are processing the packet:

    
    
          /* Keep audio_clock up-to-date */
          pts = is->audio_clock;
          *pts_ptr = pts;
          n = 2 * is->audio_st->codec->channels;
          is->audio_clock += (double)data_size /
    	(double)(n * is->audio_st->codec->sample_rate);
    

A few fine details: the template of the function has changed to include
`pts_ptr`, so make sure you change that. `pts_ptr` is a pointer we use to
inform `audio_callback` the pts of the audio packet. This will be used next
time for synchronizing the audio with the video.

Now we can finally implement our `get_audio_clock` function. It's not as
simple as getting the `is->audio_clock` value, thought. Notice that we set the
audio PTS every time we process it, but if you look at the `audio_callback`
function, it takes time to move all the data from our audio packet into our
output buffer. That means that the value in our audio clock could be too far
ahead. So we have to check how much we have left to write. Here's the complete
code:

    
    
    double get_audio_clock(VideoState *is) {
      double pts;
      int hw_buf_size, bytes_per_sec, n;
      
      pts = is->audio_clock; /* maintained in the audio thread */
      hw_buf_size = is->audio_buf_size - is->audio_buf_index;
      bytes_per_sec = 0;
      n = is->audio_st->codec->channels * 2;
      if(is->audio_st) {
        bytes_per_sec = is->audio_st->codec->sample_rate * n;
      }
      if(bytes_per_sec) {
        pts -= (double)hw_buf_size / bytes_per_sec;
      }
      return pts;
    }
    

You should be able to tell why this function works by now ;)

So that's it! Go ahead and compile it:

    
    
    gcc -o tutorial05 tutorial05.c -lavutil -lavformat -lavcodec -lswscale -lz -lm \
    `sdl-config --cflags --libs`
    

and **finally!** you can watch a movie on your own movie player. Next time
we'll look at audio synching, and then the tutorial after that we'll talk
about seeking.

_**>>** Synching Audio_

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

