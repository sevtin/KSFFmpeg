## An ffmpeg and SDL Tutorial

Page 1 2 3 4 5 6 7 End Prev Home Next &nbsp_place_holder;

Text version

## Tutorial 06: Synching Audio

Code: tutorial06.c

### Synching Audio

So now we have a decent enough player to watch a movie, so let's see what kind
of loose ends we have lying around. Last time, we glossed over synchronization
a little bit, namely sychronizing audio to a video clock rather than the other
way around. We're going to do this the same way as with the video: make an
internal video clock to keep track of how far along the video thread is and
sync the audio to that. Later we'll look at how to generalize things to sync
both audio and video to an external clock, too.

### Implementing the video clock

Now we want to implement a video clock similar to the audio clock we had last
time: an internal value that gives the current time offset of the video
currently being played. At first, you would think that this would be as simple
as updating the timer with the current PTS of the last frame to be shown.
However, don't forget that the time between video frames can be pretty long
when we get down to the millisecond level. The solution is to keep track of
another value, the time at which we set the video clock to the PTS of the last
frame. That way the current value of the video clock will be
`PTS_of_last_frame + (current_time - time_elapsed_since_PTS_value_was_set)`.
This solution is very similar to what we did with `get_audio_clock`.

So, in our big struct, we're going to put a `double video_current_pts` and a
`int64_t video_current_pts_time`. The clock updating is going to take place in
the `video_refresh_timer` function:

    
    
    void video_refresh_timer(void *userdata) {
    
      /* ... */
    
      if(is->video_st) {
        if(is->pictq_size == 0) {
          schedule_refresh(is, 1);
        } else {
          vp = &is-;>pictq[is->pictq_rindex];
    
          is->video_current_pts = vp->pts;
          is->video_current_pts_time = av_gettime();
    

Don't forget to initialize it in `stream_component_open`:

    
    
        is->video_current_pts_time = av_gettime();
    

And now all we need is a way to get the information:

    
    
    double get_video_clock(VideoState *is) {
      double delta;
    
      delta = (av_gettime() - is->video_current_pts_time) / 1000000.0;
      return is->video_current_pts + delta;
    }
    

### Abstracting the clock

But why force ourselves to use the video clock? We'd have to go and alter our
video sync code so that the audio and video aren't trying to sync to each
other. Imagine the mess if we tried to make it a command line option like it
is in ffplay. So let's abstract things: we're going to make a new wrapper
function, `get_master_clock` that checks an av_sync_type variable and then
call `get_audio_clock`, `get_video_clock`, or whatever other clock we want to
use. We could even use the computer clock, which we'll call
`get_external_clock`:

    
    
    enum {
      AV_SYNC_AUDIO_MASTER,
      AV_SYNC_VIDEO_MASTER,
      AV_SYNC_EXTERNAL_MASTER,
    };
    
    #define DEFAULT_AV_SYNC_TYPE AV_SYNC_VIDEO_MASTER
    
    double get_master_clock(VideoState *is) {
      if(is->av_sync_type == AV_SYNC_VIDEO_MASTER) {
        return get_video_clock(is);
      } else if(is->av_sync_type == AV_SYNC_AUDIO_MASTER) {
        return get_audio_clock(is);
      } else {
        return get_external_clock(is);
      }
    }
    main() {
    ...
      is->av_sync_type = DEFAULT_AV_SYNC_TYPE;
    ...
    }
    

### Synchronizing the Audio

Now the hard part: synching the audio to the video clock. Our strategy is
going to be to measure where the audio is, compare it to the video clock, and
then figure out how many samples we need to adjust by, that is, do we need to
speed up by dropping samples or do we need to slow down by adding them?

We're going to run a `synchronize_audio` function each time we process each
set of audio samples we get to shrink or expand them properly. However, we
don't want to sync every single time it's off because process audio a lot more
often than video packets. So we're going to set a minimum number of
consecutive calls to the `synchronize_audio` function that have to be out of
sync before we bother doing anything. Of course, just like last time, "out of
sync" means that the audio clock and the video clock differ by more than our
sync threshold.

**Note:** What the heck is going on here? This equation looks like magic! Well, it's basically a weighted mean using a geometric series as weights. I don't know if there's a name for this (I even checked Wikipedia!) but for more info, here's an explanation (or at weightedmean.txt) So we're going to use a fractional coefficient, say `c`, and So now let's say we've gotten N audio sample sets that have been out of sync. The amount we are out of sync can also vary a good deal, so we're going to take an average of how far each of those have been out of sync. So for example, the first call might have shown we were out of sync by 40ms, the next by 50ms, and so on. But we're not going to take a simple average because the most recent values are more important than the previous ones. So we're going to use a fractional coefficient, say `c`, and sum the differences like this: `diff_sum = new_diff + diff_sum*c`. When we are ready to find the average difference, we simply calculate `avg_diff = diff_sum * (1-c)`. 

Here's what our function looks like so far:

    
    
    /* Add or subtract samples to get a better sync, return new
       audio buffer size */
    int synchronize_audio(VideoState *is, short *samples,
    		      int samples_size, double pts) {
      int n;
      double ref_clock;
      
      n = 2 * is->audio_st->codec->channels;
      
      if(is->av_sync_type != AV_SYNC_AUDIO_MASTER) {
        double diff, avg_diff;
        int wanted_size, min_size, max_size, nb_samples;
        
        ref_clock = get_master_clock(is);
        diff = get_audio_clock(is) - ref_clock;
    
        if(diff < AV_NOSYNC_THRESHOLD) {
          // accumulate the diffs
          is->audio_diff_cum = diff + is->audio_diff_avg_coef
    	* is->audio_diff_cum;
          if(is->audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
    	is->audio_diff_avg_count++;
          } else {
    	avg_diff = is->audio_diff_cum * (1.0 - is->audio_diff_avg_coef);
    
           /* Shrinking/expanding buffer code.... */
    
          }
        } else {
          /* difference is TOO big; reset diff stuff */
          is->audio_diff_avg_count = 0;
          is->audio_diff_cum = 0;
        }
      }
      return samples_size;
    }
    

So we're doing pretty well; we know approximately how off the audio is from
the video or whatever we're using for a clock. So let's now calculate how many
samples we need to add or lop off by putting this code where the
"Shrinking/expanding buffer code" section is:

    
    
    if(fabs(avg_diff) >= is->audio_diff_threshold) {
      wanted_size = samples_size + 
      ((int)(diff * is->audio_st->codec->sample_rate) * n);
      min_size = samples_size * ((100 - SAMPLE_CORRECTION_PERCENT_MAX)
                                 / 100);
      max_size = samples_size * ((100 + SAMPLE_CORRECTION_PERCENT_MAX) 
                                 / 100);
      if(wanted_size < min_size) {
        wanted_size = min_size;
      } else if (wanted_size > max_size) {
        wanted_size = max_size;
      }
    

Remember that `audio_length * (sample_rate * # of channels * 2)` is the number
of samples in `audio_length` seconds of audio. Therefore, number of samples we
want is going to be the number of samples we already have plus or minus the
number of samples that correspond to the amount of time the audio has drifted.
We'll also set a limit on how big or small our correction can be because if we
change our buffer too much, it'll be too jarring to the user.

### Correcting the number of samples

Now we have to actually correct the audio. You may have noticed that our
`synchronize_audio` function returns a sample size, which will then tell us
how many bytes to send to the stream. So we just have to adjust the sample
size to the `wanted_size`. This works for making the sample size smaller. But
if we want to make it bigger, we can't just make the sample size larger
because there's no more data in the buffer! So we have to add it. But what
should we add? It would be foolish to try and extrapolate audio, so let's just
use the audio we already have by padding out the buffer with the value of the
last sample.

    
    
    if(wanted_size < samples_size) {
      /* remove samples */
      samples_size = wanted_size;
    } else if(wanted_size > samples_size) {
      uint8_t *samples_end, *q;
      int nb;
    
      /* add samples by copying final samples */
      nb = (samples_size - wanted_size);
      samples_end = (uint8_t *)samples + samples_size - n;
      q = samples_end + n;
      while(nb > 0) {
        memcpy(q, samples_end, n);
        q += n;
        nb -= n;
      }
      samples_size = wanted_size;
    }
    

Now we return the sample size, and we're done with that function. All we need
to do now is use it:

    
    
    void audio_callback(void *userdata, Uint8 *stream, int len) {
    
      VideoState *is = (VideoState *)userdata;
      int len1, audio_size;
      double pts;
    
      while(len > 0) {
        if(is->audio_buf_index >= is->audio_buf_size) {
          /* We have already sent all our data; get more */
          audio_size = audio_decode_frame(is, is->audio_buf, sizeof(is->audio_buf), &pts;);
          if(audio_size < 0) {
    	/* If error, output silence */
    	is->audio_buf_size = 1024;
    	memset(is->audio_buf, 0, is->audio_buf_size);
          } else {
    	audio_size = synchronize_audio(is, (int16_t *)is->audio_buf,
    				       audio_size, pts);
    	is->audio_buf_size = audio_size;
    

All we did is inserted the call to `synchronize_audio`. (Also, make sure to
check the source code where we initalize the above variables I didn't bother
to define.)

One last thing before we finish: we need to add an if clause to make sure we
don't sync the video if it is the master clock:

    
    
    if(is->av_sync_type != AV_SYNC_VIDEO_MASTER) {
      ref_clock = get_master_clock(is);
      diff = vp->pts - ref_clock;
    
      /* Skip or repeat the frame. Take delay into account
         FFPlay still doesn't "know if this is the best guess." */
      sync_threshold = (delay > AV_SYNC_THRESHOLD) ? delay :
                        AV_SYNC_THRESHOLD;
      if(fabs(diff) < AV_NOSYNC_THRESHOLD) {
        if(diff <= -sync_threshold) {
          delay = 0;
        } else if(diff >= sync_threshold) {
          delay = 2 * delay;
        }
      }
    }
    

And that does it! Make sure you check through the source file to initialize
any variables that I didn't bother defining or initializing. Then compile it:

    
    
    gcc -o tutorial06 tutorial06.c -lavutil -lavformat -lavcodec -lswscale -lz -lm \
    `sdl-config --cflags --libs`
    

and you'll be good to go.

Next time we'll make it so you can rewind and fast forward your movie.

_**>>** Seeking_

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

