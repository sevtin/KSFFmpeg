## An ffmpeg and SDL Tutorial

Page 1 2 3 4 5 6 7 End Prev Home Next &nbsp_place_holder;

Text version

## Tutorial 03: Playing Sound

Code: tutorial03.c

### Audio

So now we want to play sound. SDL also gives us methods for outputting sound.
The `SDL_OpenAudio()` function is used to open the audio device itself. It
takes as arguments an `SDL_AudioSpec` struct, which contains all the
information about the audio we are going to output.

Before we show how you set this up, let's explain first about how audio is
handled by computers. Digital audio consists of a long stream of **samples**.
Each sample represents a value of the audio waveform. Sounds are recorded at a
certain **sample rate**, which simply says how fast to play each sample, and
is measured in number of samples per second. Example sample rates are 22,050
and 44,100 samples per second, which are the rates used for radio and CD
respectively. In addition, most audio can have more than one channel for
stereo or surround, so for example, if the sample is in stereo, the samples
will come 2 at a time. When we get data from a movie file, we don't know how
many samples we will get, but ffmpeg will not give us partial samples - that
also means that it will not split a stereo sample up, either.

SDL's method for playing audio is this: you set up your audio options: the
sample rate (called "freq" for **frequency** in the SDL struct), number of
channels, and so forth, and we also set a callback function and userdata. When
we begin playing audio, SDL will continually call this callback function and
ask it to fill the audio buffer with a certain number of bytes. After we put
this information in the `SDL_AudioSpec` struct, we call `SDL_OpenAudio()`,
which will open the audio device and give us back _another_ AudioSpec struct.
These are the specs we will _actually_ be using -- we are not guaranteed to
get what we asked for!

### Setting Up the Audio

Keep that all in your head for the moment, because we don't actually have any
information yet about the audio streams yet! Let's go back to the place in our
code where we found the video stream and find which stream is the audio
stream.

    
    
    // Find the first video stream
    videoStream=-1;
    audioStream=-1;
    for(i=0; i < pFormatCtx->nb_streams; i++) {
      if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO
         &&
           videoStream < 0) {
        videoStream=i;
      }
      if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO &&
         audioStream < 0) {
        audioStream=i;
      }
    }
    if(videoStream==-1)
      return -1; // Didn't find a video stream
    if(audioStream==-1)
      return -1;
    

From here we can get all the info we want from the `AVCodecContext` from the
stream, just like we did with the video stream:

    
    
    AVCodecContext *aCodecCtxOrig;
    AVCodecContext *aCodecCtx;
    
    aCodecCtxOrig=pFormatCtx->streams[audioStream]->codec;
    

If you remember from the previous tutorials, we still need to open the audio
codec itself. This is straightforward:

    
    
    AVCodec         *aCodec;
    
    aCodec = avcodec_find_decoder(aCodecCtx->codec_id);
    if(!aCodec) {
      fprintf(stderr, "Unsupported codec!\n");
      return -1;
    }
    // Copy context
    aCodecCtx = avcodec_alloc_context3(aCodec);
    if(avcodec_copy_context(aCodecCtx, aCodecCtxOrig) != 0) {
      fprintf(stderr, "Couldn't copy codec context");
      return -1; // Error copying codec context
    }
    /* set up SDL Audio here */
    
    avcodec_open2(aCodecCtx, aCodec, NULL);
    

Contained within the codec context is all the information we need to set up
our audio:

    
    
    wanted_spec.freq = aCodecCtx->sample_rate;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = aCodecCtx->channels;
    wanted_spec.silence = 0;
    wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;
    wanted_spec.callback = audio_callback;
    wanted_spec.userdata = aCodecCtx;
    
    if(SDL_OpenAudio(&wanted;_spec, &spec;) < 0) {
      fprintf(stderr, "SDL_OpenAudio: %s\n", SDL_GetError());
      return -1;
    }
    

Let's go through these:

  * `freq`: The sample rate, as explained earlier.
  * `format`: This tells SDL what format we will be giving it. The "S" in "S16SYS" stands for "signed", the 16 says that each sample is 16 bits long, and "SYS" means that the endian-order will depend on the system you are on. This is the format that `avcodec_decode_audio2` will give us the audio in.
  * `channels`: Number of audio channels.
  * `silence`: This is the value that indicated silence. Since the audio is signed, 0 is of course the usual value.
  * `samples`: This is the size of the audio buffer that we would like SDL to give us when it asks for more audio. A good value here is between 512 and 8192; ffplay uses 1024.
  * `callback`: Here's where we pass the actual callback function. We'll talk more about the callback function later.
  * `userdata`: SDL will give our callback a void pointer to any user data that we want our callback function to have. We want to let it know about our codec context; you'll see why.
Finally, we open the audio with `SDL_OpenAudio`.

### Queues

There! Now we're ready to start pulling audio information from the stream. But
what do we do with that information? We are going to be continuously getting
packets from the movie file, but at the same time SDL is going to call the
callback function! The solution is going to be to create some kind of global
structure that we can stuff audio packets in so our `audio_callback` has
something to get audio data from! So what we're going to do is to create a
**queue** of packets. ffmpeg even comes with a structure to help us with this:
`AVPacketList`, which is just a linked list for packets. Here's our queue
structure:

    
    
    typedef struct PacketQueue {
      AVPacketList *first_pkt, *last_pkt;
      int nb_packets;
      int size;
      SDL_mutex *mutex;
      SDL_cond *cond;
    } PacketQueue;
    

First, we should point out that `nb_packets` is not the same as `size` --
`size` refers to a byte size that we get from `packet->size`. You'll notice
that we have a mutex and a condtion variable in there. This is because SDL is
running the audio process as a separate thread. If we don't lock the queue
properly, we could really mess up our data. We'll see how in the
implementation of the queue. Every programmer should know how to make a queue,
but we're including this so you can learn the SDL functions.

First we make a function to initialize the queue:

    
    
    void packet_queue_init(PacketQueue *q) {
      memset(q, 0, sizeof(PacketQueue));
      q->mutex = SDL_CreateMutex();
      q->cond = SDL_CreateCond();
    }
    

Then we will make a function to put stuff in our queue:

    
    
    int packet_queue_put(PacketQueue *q, AVPacket *pkt) {
    
      AVPacketList *pkt1;
      if(av_dup_packet(pkt) < 0) {
        return -1;
      }
      pkt1 = av_malloc(sizeof(AVPacketList));
      if (!pkt1)
        return -1;
      pkt1->pkt = *pkt;
      pkt1->next = NULL;
      
      
      SDL_LockMutex(q->mutex);
      
      if (!q->last_pkt)
        q->first_pkt = pkt1;
      else
        q->last_pkt->next = pkt1;
      q->last_pkt = pkt1;
      q->nb_packets++;
      q->size += pkt1->pkt.size;
      SDL_CondSignal(q->cond);
      
      SDL_UnlockMutex(q->mutex);
      return 0;
    }
    

`SDL_LockMutex()` locks the mutex in the queue so we can add something to it,
and then `SDL_CondSignal()` sends a signal to our get function (if it is
waiting) through our condition variable to tell it that there is data and it
can proceed, then unlocks the mutex to let it go.

Here's the corresponding get function. Notice how `SDL_CondWait()` makes the
function **block** (i.e. pause until we get data) if we tell it to.

    
    
    int quit = 0;
    
    static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block) {
      AVPacketList *pkt1;
      int ret;
      
      SDL_LockMutex(q->mutex);
      
      for(;;) {
        
        if(quit) {
          ret = -1;
          break;
        }
    
        pkt1 = q->first_pkt;
        if (pkt1) {
          q->first_pkt = pkt1->next;
          if (!q->first_pkt)
    	q->last_pkt = NULL;
          q->nb_packets--;
          q->size -= pkt1->pkt.size;
          *pkt = pkt1->pkt;
          av_free(pkt1);
          ret = 1;
          break;
        } else if (!block) {
          ret = 0;
          break;
        } else {
          SDL_CondWait(q->cond, q->mutex);
        }
      }
      SDL_UnlockMutex(q->mutex);
      return ret;
    }
    

As you can see, we've wrapped the function in a forever loop so we will be
sure to get some data if we want to block. We avoid looping forever by making
use of SDL's SDL_CondWait()` function. Basically, all CondWait does is wait
for a signal from `SDL_CondSignal()` (or `SDL_CondBroadcast()`) and then
continue. However, it looks as though we've trapped it within our mutex -- if
we hold the lock, our put function can't put anything in the queue! However,
what `SDL_CondWait()` also does for us is to unlock the mutex we give it and
then attempt to lock it again once we get the signal.

### In Case of Fire

You'll also notice that we have a global `quit` variable that we check to make
sure that we haven't set the program a quit signal (SDL automatically handles
TERM signals and the like). Otherwise, the thread will continue forever and
we'll have to `kill -9` the program.

    
    
      SDL_PollEvent(&event;);
      switch(event.type) {
      case SDL_QUIT:
        quit = 1;
    

We make sure to set the `quit` flag to 1.

### Feeding Packets

The only thing left is to set up our queue:

    
    
    PacketQueue audioq;
    main() {
    ...
      avcodec_open2(aCodecCtx, aCodec, NULL);
    
      packet_queue_init(&audioq;);
      SDL_PauseAudio(0);
    

`SDL_PauseAudio()` finally starts the audio device. It plays silence if it
doesn't get data; which it won't right away.

So, we've got our queue set up, now we're ready to start feeding it packets.
We go to our packet-reading loop:

    
    
    while(av_read_frame(pFormatCtx, &packet;)>=0) {
      // Is this a packet from the video stream?
      if(packet.stream_index==videoStream) {
        // Decode video frame
        ....
        }
      } else if(packet.stream_index==audioStream) {
        packet_queue_put(&audioq;, &packet;);
      } else {
        av_free_packet(&packet;);
      }
    

Note that we don't free the packet after we put it in the queue. We'll free it
later when we decode it.

### Fetching Packets

Now let's finally make our `audio_callback` function to fetch the packets on
the queue. The callback has to be of the form `void callback(void *userdata,
Uint8 *stream, int len)`, where `userdata` of course is the pointer we gave to
SDL, `stream` is the buffer we will be writing audio data to, and `len` is the
size of that buffer. Here's the code:

    
    
    void audio_callback(void *userdata, Uint8 *stream, int len) {
    
      AVCodecContext *aCodecCtx = (AVCodecContext *)userdata;
      int len1, audio_size;
    
      static uint8_t audio_buf[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2];
      static unsigned int audio_buf_size = 0;
      static unsigned int audio_buf_index = 0;
    
      while(len > 0) {
        if(audio_buf_index >= audio_buf_size) {
          /* We have already sent all our data; get more */
          audio_size = audio_decode_frame(aCodecCtx, audio_buf,
                                          sizeof(audio_buf));
          if(audio_size < 0) {
    	/* If error, output silence */
    	audio_buf_size = 1024;
    	memset(audio_buf, 0, audio_buf_size);
          } else {
    	audio_buf_size = audio_size;
          }
          audio_buf_index = 0;
        }
        len1 = audio_buf_size - audio_buf_index;
        if(len1 > len)
          len1 = len;
        memcpy(stream, (uint8_t *)audio_buf + audio_buf_index, len1);
        len -= len1;
        stream += len1;
        audio_buf_index += len1;
      }
    }
    

This is basically a simple loop that will pull in data from another function
we will write, `audio_decode_frame()`, store the result in an intermediary
buffer, attempt to write `len` bytes to `stream`, and get more data if we
don't have enough yet, or save it for later if we have some left over. The
size of `audio_buf` is 1.5 times the size of the largest audio frame that
ffmpeg will give us, which gives us a nice cushion.

### Finally Decoding the Audio

Let's get to the real meat of the decoder, `audio_decode_frame`:

    
    
    int audio_decode_frame(AVCodecContext *aCodecCtx, uint8_t *audio_buf,
                           int buf_size) {
    
      static AVPacket pkt;
      static uint8_t *audio_pkt_data = NULL;
      static int audio_pkt_size = 0;
      static AVFrame frame;
    
      int len1, data_size = 0;
    
      for(;;) {
        while(audio_pkt_size > 0) {
          int got_frame = 0;
          len1 = avcodec_decode_audio4(aCodecCtx, &frame;, &got;_frame, &pkt;);
          if(len1 < 0) {
    	/* if error, skip frame */
    	audio_pkt_size = 0;
    	break;
          }
          audio_pkt_data += len1;
          audio_pkt_size -= len1;
          data_size = 0;
          if(got_frame) {
    	data_size = av_samples_get_buffer_size(NULL, 
    					       aCodecCtx->channels,
    					       frame.nb_samples,
    					       aCodecCtx->sample_fmt,
    					       1);
    	assert(data_size <= buf_size);
    	memcpy(audio_buf, frame.data[0], data_size);
          }
          if(data_size <= 0) {
    	/* No data yet, get more frames */
    	continue;
          }
          /* We have data, return it and come back for more later */
          return data_size;
        }
        if(pkt.data)
          av_free_packet(&pkt;);
    
        if(quit) {
          return -1;
        }
    
        if(packet_queue_get(&audioq;, &pkt;, 1) < 0) {
          return -1;
        }
        audio_pkt_data = pkt.data;
        audio_pkt_size = pkt.size;
      }
    }
    

This whole process actually starts towards the end of the function, where we
call `packet_queue_get()`. We pick the packet up off the queue, and save its
information. Then, once we have a packet to work with, we call
`avcodec_decode_audio4()`, which acts a lot like its sister function,
`avcodec_decode_video()`, except in this case, a packet might have more than
one frame, so you may have to call it several times to get all the data out of
the packet. Once we have the frame, we simply copy it to our audio buffer,
making sure the data_size is smaller than our audio buffer. Also, remember the
cast to audio_buf, because SDL gives an 8 bit int buffer, and ffmpeg gives us
data in a 16 bit int buffer. You should also notice the difference between
`len1` and `data_size`. `len1` is how much of the packet we've used, and
`data_size` is the amount of raw data returned.

When we've got some data, we immediately return to see if we still need to get
more data from the queue, or if we are done. If we still had more of the
packet to process, we save it for later. If we finish up a packet, we finally
get to free that packet.

So that's it! We've got audio being carried from the main read loop to the
queue, which is then read by the `audio_callback` function, which hands that
data to SDL, which SDL beams to your sound card. Go ahead and compile:

    
    
    gcc -o tutorial03 tutorial03.c -lavutil -lavformat -lavcodec -lswscale -lz -lm \
    `sdl-config --cflags --libs`
    

Hooray! The video is still going as fast as possible, but the audio is playing
in time. Why is this? That's because the audio information has a sample rate
-- we're pumping out audio information as fast as we can, but the audio simply
plays from that stream at its leisure according to the sample rate.

We're almost ready to start syncing video and audio ourselves, but first we
need to do a little program reorganization. The method of queueing up audio
and playing it using a separate thread worked very well: it made the code more
managable and more modular. Before we start syncing the video to the audio, we
need to make our code easier to deal with. Next time: Spawning Threads!

_**>>** Spawning Threads_

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

