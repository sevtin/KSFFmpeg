## An ffmpeg and SDL Tutorial

Page 1 2 3 4 5 6 7 End Prev Home Next &nbsp_place_holder;

Text version

## Tutorial 07: Seeking

Code: tutorial07.c

### Handling the seek command

Now we're going to add some seeking capabilities to our player, because it's
really annoying when you can't rewind a movie. Plus, this will show you how
easy the `av_seek_frame` function is to use.

We're going to make the left and right arrows go back and forth in the movie
by a little and the up and down arrows a lot, where "a little" is 10 seconds,
and "a lot" is 60 seconds. So we need to set up our main loop so it catches
the keystrokes. However, when we do get a keystroke, we can't call
`av_seek_frame` directly. We have to do that in our main decode loop, the
`decode_thread` loop. So instead, we're going to add some values to the big
struct that will contain the new position to seek to and some seeking flags:

    
    
      int             seek_req;
      int             seek_flags;
      int64_t         seek_pos;
    

Now we need to set up our main loop to catch the key presses:

    
    
      for(;;) {
        double incr, pos;
    
        SDL_WaitEvent(&event;);
        switch(event.type) {
        case SDL_KEYDOWN:
          switch(event.key.keysym.sym) {
          case SDLK_LEFT:
    	incr = -10.0;
    	goto do_seek;
          case SDLK_RIGHT:
    	incr = 10.0;
    	goto do_seek;
          case SDLK_UP:
    	incr = 60.0;
    	goto do_seek;
          case SDLK_DOWN:
    	incr = -60.0;
    	goto do_seek;
          do_seek:
    	if(global_video_state) {
    	  pos = get_master_clock(global_video_state);
    	  pos += incr;
    	  stream_seek(global_video_state, 
                          (int64_t)(pos * AV_TIME_BASE), incr);
    	}
    	break;
          default:
    	break;
          }
          break;
    

To detect keypresses, we first look and see if we get an `SDL_KEYDOWN` event.
Then we check and see which key got hit using `event.key.keysym.sym`. Once we
know which way we want to seek, we calculate the new time by adding the
increment to the value from our new `get_master_clock` function. Then we call
a `stream_seek` function to set the seek_pos, etc., values. We convert our new
time to avcodec's internal timestamp unit. Recall that timestamps in streams
are measured in frames rather than seconds, with the formula `seconds = frames
* time_base (fps)`. avcodec defaults to a value of 1,000,000 fps (so a `pos`
of 2 seconds will be timestamp of 2000000). We'll see why we need to convert
this value later.

Here's our `stream_seek` function. Notice we set a flag if we are going
backwards:

    
    
    void stream_seek(VideoState *is, int64_t pos, int rel) {
    
      if(!is->seek_req) {
        is->seek_pos = pos;
        is->seek_flags = rel < 0 ? AVSEEK_FLAG_BACKWARD : 0;
        is->seek_req = 1;
      }
    }
    

Now let's go over to our `decode_thread` where we will actually perform our
seek. You'll notice in the source files that we've marked an area "seek stuff
goes here". Well, we're going to put it there now.

Seeking centers around the `av_seek_frame` function. This function takes a
format context, a stream, a timestamp, and a set of flags as an argument. The
function will seek to the timestamp you give it. The unit of the timestamp is
the `time_base` of the stream you pass the function. _However_, you do not
have to pass it a stream (indicated by passing a value of -1). If you do that,
the `time_base` will be in avcodec's internal timestamp unit, or 1000000fps.
This is why we multiplied our position by `AV_TIME_BASE` when we set
`seek_pos`.

However, sometimes you can (rarely) run into problems with some files if you
pass `av_seek_frame` -1 for a stream, so we're going to pick the first stream
in our file and pass it to `av_seek_frame`. Don't forget we have to rescale
our timestamp to be in the new unit too.

    
    
    if(is->seek_req) {
      int stream_index= -1;
      int64_t seek_target = is->seek_pos;
    
      if     (is->videoStream >= 0) stream_index = is->videoStream;
      else if(is->audioStream >= 0) stream_index = is->audioStream;
    
      if(stream_index>=0){
        seek_target= av_rescale_q(seek_target, AV_TIME_BASE_Q,
                          pFormatCtx->streams[stream_index]->time_base);
      }
      if(av_seek_frame(is->pFormatCtx, stream_index, 
                        seek_target, is->seek_flags) < 0) {
        fprintf(stderr, "%s: error while seeking\n",
                is->pFormatCtx->filename);
      } else {
         /* handle packet queues... more later... */
    
    

`av_rescale_q(a,b,c)` is a function that will rescale a timestamp from one
base to another. It basically computes `a*b/c` but this function is required
because that calculation could overflow. `AV_TIME_BASE_Q` is the fractional
version of `AV_TIME_BASE`. They're quite different: `AV_TIME_BASE *
time_in_seconds = avcodec_timestamp` and `AV_TIME_BASE_Q * avcodec_timestamp =
time_in_seconds` (but note that `AV_TIME_BASE_Q` is actually an `AVRational`
object, so you have to use special q functions in avcodec to handle it).

### Flushing our buffers

So we've set our seek correctly, but we aren't finished quite yet. Remember
that we have a queue set up to accumulate packets. Now that we're in a
different place, we have to flush that queue or the movie ain't gonna seek!
Not only that, but avcodec has its own internal buffers that need to be
flushed too by each thread.

To do this, we need to first write a function to clear our packet queue. Then,
we need to have some way of instructing the audio and video thread that they
need to flush avcodec's internal buffers. We can do this by putting a special
packet on the queue after we flush it, and when they detect that special
packet, they'll just flush their buffers.

Let's start with the flush function. It's really quite simple, so I'll just
show you the code:

    
    
    static void packet_queue_flush(PacketQueue *q) {
      AVPacketList *pkt, *pkt1;
    
      SDL_LockMutex(q->mutex);
      for(pkt = q->first_pkt; pkt != NULL; pkt = pkt1) {
        pkt1 = pkt->next;
        av_free_packet(&pkt-;>pkt);
        av_freep(&pkt;);
      }
      q->last_pkt = NULL;
      q->first_pkt = NULL;
      q->nb_packets = 0;
      q->size = 0;
      SDL_UnlockMutex(q->mutex);
    }
    

Now that the queue is flushed, let's put on our "flush packet." But first
we're going to want to define what that is and create it:

    
    
    AVPacket flush_pkt;
    
    main() {
      ...
      av_init_packet(&flush;_pkt);
      flush_pkt.data = "FLUSH";
      ...
    }
    

Now we put this packet on the queue:

    
    
      } else {
        if(is->audioStream >= 0) {
          packet_queue_flush(&is-;>audioq);
          packet_queue_put(&is-;>audioq, &flush;_pkt);
        }
        if(is->videoStream >= 0) {
          packet_queue_flush(&is-;>videoq);
          packet_queue_put(&is-;>videoq, &flush;_pkt);
        }
      }
      is->seek_req = 0;
    }
    

(This code snippet also continues the code snippet above for `decode_thread`.)
We also need to change `packet_queue_put` so that we don't duplicate the
special flush packet:

    
    
    int packet_queue_put(PacketQueue *q, AVPacket *pkt) {
    
      AVPacketList *pkt1;
      if(pkt != &flush;_pkt && av_dup_packet(pkt) < 0) {
        return -1;
      }
    

And then in the audio thread and the video thread, we put this call to
`avcodec_flush_buffers` immediately after `packet_queue_get`:

    
    
        if(packet_queue_get(&is-;>audioq, pkt, 1) < 0) {
          return -1;
        }
        if(pkt->data == flush_pkt.data) {
          avcodec_flush_buffers(is->audio_st->codec);
          continue;
        }
    

The above code snippet is exactly the same for the video thread, with "audio"
being replaced by "video".

That's it! We're done! Go ahead and compile your player:

    
    
    gcc -o tutorial07 tutorial07.c -lavutil -lavformat -lavcodec -lswscale -lz -lm \
    `sdl-config --cflags --libs`
    

and enjoy your movie player made in less than 1000 lines of C!

Of course, there's a lot of things we glanced over that we could add.

_**>>** What's Left?_

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

