## Function List

Text version

int **avformat_open_input**(AVFormatContext **ptr, const char * filename,
AVInputFormat *fmt, AVDictionary **options)

     Opens a media file **filename**, stores the format context in the address pointed to by **ptr**. 

**fmt** forces file format if not NULL. 

**buf_size**: optional buffer size. 

**options**: An AVDictionary filled with AVFormatContext and demuxer-private options. 

void **avformat_close_input**(AVFormatContext **s)

    Closes a media file. It does not close its codecs, however. 

int avio_open2 (AVIOContext **s, const char *url, int flags, const
AVIOInterruptCB *int_cb, AVDictionary **options)

    Creates an IO context to use the resource specified by **url**. 

**s** is the pointer to the place the AVIOContext will be created. In case of failure the pointed to value is set to NULL. 

**url** is the name of the resource to access. 

**flags** flags which control how the resource indicated by url is to be opened. 

**int_cb** an interrupt callback to be used at the protocols level. 

**options** A dictionary filled with protocol-private options. On return this parameter will be destroyed and replaced with a dict containing options that were not found. May be NULL. 

int **av_dup_packet**(AVPacket *pkt)

     This apparently is a hack: if this packet wasn't allocated, we allocate it here. It returns 0 on success or AVERROR_NOMEM on failure. 

int **av_find_stream_info**(AVFormatContext *s, AVDictionary **options)

    This function searches the stream to find information about it that might not have been obvious like frame rate. This is useful for file formats without headers like MPEG. It's recommended that you call this after opening a file. It returns >= 0 on success, AVERROR_* on error. 

AVFrame ***avcodec_free_frame**()

    Old name for av_frame_free. Changed in lavc 55.28.1.

void **av_frame_free** (AVFrame **frame)

    Free the frame and any dynamically allocated objects in it, e.g. extended_data. If the frame is reference counted, it will be unreferenced first. 

void **av_free**(void *ptr)

    Frees memory allocated by av_malloc() or av_realloc(). You are allowed to call this function with ptr == NULL. It is recommended you call av_freep() instead.

void **av_freep**(void *ptr)

    Frees memory and sets the pointer to NULL. Uses av_free() internally. 

void **av_free_packet**(AVPacket *pkt)

    A wrapper around the packet's destruct method (**pkt->destruct**). 

int64_t **av_gettime**()

    Gets the current time in microseconds.

void **av_init_packet**(AVPacket *pkt)

    Initialize optional fields of a packet. 

void ***av_malloc**(unsigned int size)

    Memory allocation of size byte with alignment suitable for all memory accesses (including vectors if available on the CPU). av_malloc(0) must return a non NULL pointer. 

void ***av_mallocz**(unsigned int size)

    Same as av_malloc() but it initializes the memory to zero. 

double **av_q2d**(AVRational a)

    Converts an AVRational to a double.

int **av_read_frame**(AVFormatContext *s, AVPacket *pkt)

    

Return the next frame of a stream. The information is stored as a packet in
**pkt**.

The returned packet is valid until the next av_read_frame() or until
av_close_input_file() and must be freed with av_free_packet. For video, the
packet contains exactly one frame. For audio, it contains an integer number of
frames if each frame has a known fixed size (e.g. PCM or ADPCM data). If the
audio frames have a variable size (e.g. MPEG audio), then it contains one
frame.

pkt->pts, pkt->dts and pkt->duration are always set to correct values in
AVStream.timebase units (and guessed if the format cannot provided them).
pkt->pts can be AV_NOPTS_VALUE if the video format has B frames, so it is
better to rely on pkt->dts if you do not decompress the payload.

**Returns:** 0 if OK, < 0 if error or end of file. 

void **av_register_all**();

    Registers all codecs with the library.

int64_t **av_rescale_q**(int64_t a, AVRational bq, AVRational cq)

    Returns a * bq / cq.

int **av_seek_frame**(AVFormatContext *s, int stream_index, int64_t timestamp,
int flags)

    Seeks to the key frame at **timestamp**. 

**stream_index**: If _stream_index_ is -1, a default stream is selected, and timestamp is automatically converted from AV_TIME_BASE units to the stream specific time_base. 

**timestamp** is measured in AVStream.time_base units or if there is no stream specified then in AV_TIME_BASE units. 

**flags**: Set options regarding direction and seeking mode.  
AVSEEK_FLAG_ANY: Seek to any frame, not just keyframes

AVSEEK_FLAG_BACKWARD: Seek backward

AVSEEK_FLAG_BYTE: Seeking based on position in bytes

AVFrame ***avcodec_alloc_frame**()

    Old name for av_frame_alloc. Changed in lavc 55.28.1.

AVFrame ***av_frame_alloc**()

    Allocates an AVFrame and initializes it. This can be freed with av_frame_free(). 

int **avcodec_decode_audio4**(AVCodecContext *avctx, AVFrame *frame, int
*got_frame_ptr, const AVPacket *avpkt)

     Decodes an audio frame from **avpkt** into **frame**. The avcodec_decode_audio4() function decodes a frame of audio from the AVPacket. To decode it, it makes use of the audiocodec which was coupled with avctx using avcodec_open2(). The resulting decoded frame is stored in the given AVFrame. If a frame has been decompressed, it will set got_frame_ptr to 1. 

**Warning:** The input buffer, avpkt->data, must be FF_INPUT_BUFFER_PADDING_SIZE larger than the actual read bytes because some optimized bitstream readers read 32 or 64 bits at once and could read over the end. 

**avctx**: The codec context.  
**frame**: The target frame.  
**got_frame_ptr**: Target int that will be set if a frame has been decompressed.  
**avpkt**: The AVPacket containing the audio.  

**Returns**: On error a negative value is returned, otherwise the number of bytes consumed from the input AVPacket is returned. 

int **avcodec_decode_video2**(AVCodecContext *avctx, AVFrame *picture, int
*frameFinished, const AVPacket *avpkt)

     Decodes a video frame from buf into picture. The avcodec_decode_video2() function decodes a frame of video from the input buffer buf of size buf_size. To decode it, it makes use of the videocodec which was coupled with avctx using avcodec_open2(). The resulting decoded frame is stored in picture. 

**Warning**: The sample alignment and buffer problems that apply to avcodec_decode_audio4 apply to this function as well.

**avctx**: The codec context.  
**picture**: The AVFrame in which the decoded video will be stored.  
**frameFinished**: Zero if no frame could be decompressed, otherwise it is non-zero. **avpkt**: The input AVPacket containing the input buffer. You can create such packet with av_init_packet() and by then setting data and size, some decoders might in addition need other fields like flags&AV_PKT_FLAG_KEY. All decoders are designed to use the least fields possible. 

**Returns**: On error a negative value is returned, otherwise the number of bytes used or zero if no frame could be decompressed. 

int64_t av_frame_get_best_effort_timestamp (const AVFrame *frame)

    A simple accessor to get the best_effort_timestamp from the AVFrame object.

AVCodec ***avcodec_find_decoder**(enum CodecID id)

    Find the decoder with the CodecID **id**. Returns NULL on failure. This should be called after getting the desired AVCodecContext from a stream in AVFormatContext, using codecCtx->codec_id.

void **avcodec_flush_buffers**(AVCodecContetx *avctx)

    Flush buffers, should be called when seeking or when switching to a different stream. 

AVCodecContext * **avcodec_alloc_context3** (const AVCodec *codec)

    Allocate an AVCodecContext and set its fields to default values. 

int **avcodec_copy_context** (AVCodecContext *dest, const AVCodecContext *src)

    

Copy the settings of the source AVCodecContext into the destination
AVCodecContext. The resulting destination codec context will be unopened, i.e.
you are required to call avcodec_open2() before you can use this
AVCodecContext to decode/encode video/audio data.

**dest** should be initialized with avcodec_alloc_context3(NULL), but otherwise uninitialized.

int **avcodec_open2**(AVCodecContext *avctx, AVCodec *codec, AVDictionary
**options)

     Initializes **avctx** to use the codec given in **codec**. This should be used after avcodec_find_decoder. Returns zero on success, and a negative value on error.

int **avpicture_fill**(AVPicture *picture, uint8_t *ptr, int pix_fmt, int
width, int height)

    Sets up the struct pointed to by **picture** with the buffer **ptr**, pic format **pix_fmt** and the given width and height. Returns the size of the image data in bytes.

int **avpicture_get_size**(int pix_fmt, int width, int height)

    Calculates how many bytes will be required for a picture of the given width, height, and pic format.

struct SwsContext* sws_getContext(int srcW, int srcH, int srcFormat, int dstW,
int dstH, int dstFormat, int flags, SwsFilter *srcFilter, SwsFilter
*dstFilter, double *param)

    Returns an SwsContext to be used in sws_scale. 

**Params:**  
**srcW, srcH, srcFormat**: source width, height, and pix format  
**dstW, dstH, dstFormat**: destination width, height, and pix format  
**flags**: Method of scaling to use. Choices are SWS_FAST_BILINEAR, SWS_BILINEAR, SWS_BICUBIC, SWS_X, SWS_POINT, SWS_AREA, SWS_BICUBLIN, SWS_GAUSS, SWS_SINC, SWS_LANCZOS, SWS_SPLINE. Other flags include CPU capability flags: SWS_CPU_CAPS_MMX, SWS_CPU_CAPS_MMX2, SWS_CPU_CAPS_3DNOW, SWS_CPU_CAPS_ALTIVEC. Other flags include (currently not completely implemented) SWS_FULL_CHR_H_INT, SWS_FULL_CHR_H_INP, and SWS_DIRECT_BGR. Finally we have SWS_ACCURATE_RND and perhaps the most useful for beginners, SWS_PRINT_INFO. I have no idea what most of these do. Maybe email me?  
**srcFilter, dstFilter**: SwsFilter for source and destination. SwsFilter involves chroma/luminsence filtering. A value of NULL sets these to the default.  
**param**: should be a pointer to an int[2] buffer with coefficients. Not documented. Looks like it's used to alter the default scaling algorithms slightly. A value of NULL sets this to the default. Experts only! 

int **sws_scale**(SwsContext *c, uint8_t *src, int srcStride[], int srcSliceY,
int srcSliceH, uint8_t dst[], int dstStride[]

sws_scale(sws_ctx, pFrame->data, pFrame->linesize, 0,
is->video_st->codec->height, pict.data, pict.linesize);     Scales the data in
**src** according to our settings in our SwsContext **c**. **srcStride** and
**dstStride** are the source and destination linesize.

SDL_TimerID SDL_AddTimer(Uint32 interval, SDL_NewTimerCallback callback, void
*param)

     Adds a callback function to be run after the specified number of milliseconds has elapsed. The callback function is passed the current timer interval and the user supplied parameter from the SDL_AddTimer call and returns the next timer interval. (If the returned value from the callback is the same as the one passed in, the timer continues at the same rate.) If the returned value from the callback is 0, the timer is cancelled. 

Another way to cancel a currently-running timer is by calling SDL_RemoveTimer
with the timer's ID (which was returned from SDL_AddTimer).

The timer callback function may run in a different thread than your main
program, and so shouldn't call any functions from within itself. However, you
may always call SDL_PushEvent.

The granularity of the timer is platform-dependent, but you should count on it
being at least 10 ms as this is the most common number. This means that if you
request a 16 ms timer, your callback will run approximately 20 ms later on an
unloaded system. If you wanted to set a flag signaling a frame update at 30
frames per second (every 33 ms), you might set a timer for 30 ms (see example
below). If you use this function, you need to pass SDL_INIT_TIMER to SDL_Init.

Returns an ID value for the added timer or NULL if there was an error.

Format for **callback** is:

Uint32 callback(Uint32 interval, void *param).

int **SDL_CondSignal**(SDL_cond *cond)

     Restart one of the threads that are waiting on the condition variable, cond. Returns 0 on success of -1 on an error.

int **SDL_CondWait**(SDL_cond *cond, SDL_mutex *mut);

    Unlock the provided mutex and wait for another thread to call SDL_CondSignal or SDL_CondBroadcast on the condition variable cond, then re-lock the mutex and return. The mutex must be locked before entering this function. Returns 0 when it is signalled, or -1 on an error.

SDL_cond ***SDL_CreateCond**(void);

     Creates a condition variable. 

SDL_Thread ***SDL_CreateThread**(int (*fn)(void *), void *data);

    SDL_CreateThread creates a new thread of execution that shares all of its parent's global memory, signal handlers, file descriptors, etc, and runs the function fn passing it the void pointer data. The thread quits when fn returns. 

void **SDL_Delay**(Uint32 ms);

     Wait a specified number of milliseconds before returning. SDL_Delay will wait at least the specified time, but possible longer due to OS scheduling. 

**Note**: Count on a delay granularity of at least 10 ms. Some platforms have shorter clock ticks but this is the most common. 

SDL_Overlay ***SDL_CreateYUVOverlay**(int width, int height, Uint32 format,
SDL_Surface *display);

     SDL_CreateYUVOverlay creates a YUV overlay of the specified width, height and format (see SDL_Overlay for a list of available formats), for the provided display. A SDL_Overlay structure is returned. 

**display** needs to actually be the surface gotten from SDL_SetVideoMode otherwise this function will segfault. 

The term 'overlay' is a misnomer since, unless the overlay is created in
hardware, the contents for the display surface underneath the area where the
overlay is shown will be overwritten when the overlay is displayed.

int **SDL_LockYUVOverlay**(SDL_Overlay *overlay)

     SDL_LockYUVOverlay locks the overlay for direct access to pixel data. Returns 0 on success, or -1 on an error. 

void **SDL_UnlockYUVOverlay**(SDL_Overlay *overlay)

     Unlocks a previously locked overlay. An overlay must be unlocked before it can be displayed. 

int **SDL_DisplayYUVOverlay**(SDL_Overlay *overlay, SDL_Rect *dstrect)

     Blit the overlay to the surface specified when it was created. The SDL_Rect structure, dstrect, specifies the position and size of the destination. If the dstrect is a larger or smaller than the overlay then the overlay will be scaled, this is optimized for 2x scaling. Returns 0 on success. 

void **SDL_FreeYUVOverlay**(SDL_Overlay *overlay)      Frees an overlay
created by SDL_CreateYUVOverlay.

int **SDL_Init**(Uint32 flags);

     Initializes SDL. This should be called before all other SDL functions. The flags parameter specifies what part(s) of SDL to initialize. 

SDL_INIT_TIMER Initializes the timer subsystem.

SDL_INIT_AUDIO Initializes the audio subsystem.

SDL_INIT_VIDEO Initializes the video subsystem.

SDL_INIT_CDROM Initializes the cdrom subsystem.

SDL_INIT_JOYSTICK Initializes the joystick subsystem.

SDL_INIT_EVERYTHING Initialize all of the above.

SDL_INIT_NOPARACHUTE Prevents SDL from catching fatal signals.

SDL_INIT_EVENTTHREAD Run the event manager in a separate thread.

Returns -1 on an error or 0 on success. You can get extended error message by
calling SDL_GetError. Typical cause of this error is using a particular
display without having according subsystem support, such as missing mouse
driver when using with framebuffer device. In this case you can either compile
SDL without mouse device, or set "SDL_NOMOUSE=1" environment variable before
running your application.

SDL_mutex ***SDL_CreateMutex**(void);

    Creates a new, unlocked mutex.

int **SDL_LockMutex**(SDL_mutex *mutex)

     SDL_LockMutex is an alias for SDL_mutexP. It locks **mutex**, which was previously created with SDL_CreateMutex. If the mutex is already locked by another thread then SDL_mutexP will not return until the thread that locked it unlocks it (with SDL_mutexV). If called repeatedly on a mutex, SDL_mutexV (a.k.a. SDL_UnlockMutex) must be called equal amount of times to return the mutex to unlocked state. Returns 0 on success, or -1 on an error. 

int **SDL_UnlockMutex**(SDL_Mutex *mutex)

    Unlocks **mutex**.

int **SDL_OpenAudio**(SDL_AudioSpec *desired, SDL_AudioSpec *obtained)

     This function opens the audio device with the desired parameters, and returns 0 if successful, placing the actual hardware parameters in the structure pointed to by obtained. If obtained is NULL, the audio data passed to the callback function will be guaranteed to be in the requested format, and will be automatically converted to the hardware audio format if necessary. This function returns -1 if it failed to open the audio device, or couldn't set up the audio thread. 

To open the audio device a desired SDL_AudioSpec must be created. You must
then fill this structure with your desired audio specifications.

**desired->freq**: The desired audio frequency in samples-per-second.  
**desired->format**: The desired audio format (see SDL_AudioSpec)  
**desired->channels**: The desired channels (1 for mono, 2 for stereo, 4 for surround, 6 for surround with center and lfe).  
**desired->samples**: The desired size of the audio buffer in samples. This number should be a power of two, and may be adjusted by the audio driver to a value more suitable for the hardware. Good values seem to range between 512 and 8192 inclusive, depending on the application and CPU speed. Smaller values yield faster response time, but can lead to underflow if the application is doing heavy processing and cannot fill the audio buffer in time. A stereo sample consists of both right and left channels in LR ordering. Note that the number of samples is directly related to time by the following formula: ms = (samples*1000)/freq  
**desired->callback**: This should be set to a function that will be called when the audio device is ready for more data. It is passed a pointer to the audio buffer, and the length in bytes of the audio buffer. This function usually runs in a separate thread, and so you should protect data structures that it accesses by calling SDL_LockAudio and SDL_UnlockAudio in your code. The callback prototype is `void callback(void *userdata, Uint8 *stream, int len)`. userdata is the pointer stored in the userdata field of the SDL_AudioSpec. stream is a pointer to the audio buffer you want to fill with information and len is the length of the audio buffer in bytes.  
**desired->userdata**: This pointer is passed as the first parameter to the callback function. 

SDL_OpenAudio reads these fields from the desired SDL_AudioSpec structure
passed to the function and attempts to find an audio configuration matching
your desired. As mentioned above, if the obtained parameter is NULL then SDL
with convert from your desired audio settings to the hardware settings as it
plays.

If obtained is NULL then the desired SDL_AudioSpec is your working
specification, otherwise the obtained SDL_AudioSpec becomes the working
specification and the desired specification can be deleted. The data in the
working specification is used when building SDL_AudioCVT's for converting
loaded data to the hardware format.

SDL_OpenAudio calculates the size and silence fields for both the desired and
obtained specifications. The size field stores the total size of the audio
buffer in bytes, while the silence stores the value used to represent silence
in the audio buffer

The audio device starts out playing silence when it's opened, and should be
enabled for playing by calling SDL_PauseAudio(0) when you are ready for your
audio callback function to be called. Since the audio driver may modify the
requested size of the audio buffer, you should allocate any local mixing
buffers after you open the audio device.

void **SDL_PauseAudio**(int pause_on)

     This function pauses and unpauses the audio callback processing. It should be called with pause_on=0 after opening the audio device to start playing sound. This is so you can safely initialize data for your callback function after opening the audio device. Silence will be written to the audio device during the pause. 

int **SDL_PushEvent**(SDL_Event *event)

     The event queue can actually be used as a two way communication channel. Not only can events be read from the queue, but the user can also push their own events onto it. event is a pointer to the event structure you wish to push onto the queue. The event is copied into the queue, and the caller may dispose of the memory pointed to after SDL_PushEvent returns. This function is thread safe, and can be called from other threads safely. Returns 0 on success or -1 if the event couldn't be pushed. 

int **SDL_WaitEvent**(SDL_Event *event)

    Waits indefinitely for the next available event, returning 0 if there was an error while waiting for events, 1 otherwise. If event is not NULL, the next event is removed from the queue and stored in that area. 

void **SDL_Quit**()

     SDL_Quit shuts down all SDL subsystems and frees the resources allocated to them. This should always be called before you exit. 

SDL_Surface ***SDL_SetVideoMode**(int width, int height, int bitsperpixel,
Uint32 flags)

     Set up a video mode with the specified width, height and bitsperpixel. As of SDL 1.2.10, if **width** and **height** are both 0, it will use the width and height of the current video mode (or the desktop mode, if no mode has been set). If **bitsperpixel** is 0, it is treated as the current display bits per pixel. The **flags** parameter is the same as the flags field of the SDL_Surface structure. OR'd combinations of the following values are valid: 

SDL_SWSURFACE Create the video surface in system memory

SDL_HWSURFACE Create the video surface in video memory

SDL_ASYNCBLIT Enables the use of asynchronous updates of the display surface.
This will usually slow down blitting on single CPU machines, but may provide a
speed increase on SMP systems.

SDL_ANYFORMAT Normally, if a video surface of the requested bits-per-pixel
(bpp) is not available, SDL will emulate one with a shadow surface. Passing
SDL_ANYFORMAT prevents this and causes SDL to use the video surface,
regardless of its pixel depth.

SDL_HWPALETTE Give SDL exclusive palette access. Without this flag you may not
always get the the colors you request with SDL_SetColors or SDL_SetPalette.

SDL_DOUBLEBUF Enable hardware double buffering; only valid with SDL_HWSURFACE.
Calling SDL_Flip will flip the buffers and update the screen. All drawing will
take place on the surface that is not displayed at the moment. If double
buffering could not be enabled then SDL_Flip will just perform a
SDL_UpdateRect on the entire screen.

SDL_FULLSCREEN SDL will attempt to use a fullscreen mode. If a hardware
resolution change is not possible (for whatever reason), the next higher
resolution will be used and the display window centered on a black background.

SDL_OPENGL Create an OpenGL rendering context. You should have previously set
OpenGL video attributes with SDL_GL_SetAttribute. (IMPORTANT: Please see this
page for more.)

SDL_OPENGLBLIT Create an OpenGL rendering context, like above, but allow
normal blitting operations. The screen (2D) surface may have an alpha channel,
and SDL_UpdateRects must be used for updating changes to the screen surface.
NOTE: This option is kept for compatibility only, and will be removed in next
versions. Is not recommended for new code.

SDL_RESIZABLECreate a resizable window. When the window is resized by the user
a SDL_VIDEORESIZE event is generated and SDL_SetVideoMode can be called again
with the new size.

SDL_NOFRAME If possible, SDL_NOFRAME causes SDL to create a window with no
title bar or frame decoration. Fullscreen modes automatically have this flag
set.

_Note:_ Whatever flags SDL_SetVideoMode could satisfy are set in the flags
member of the returned surface.

_Note:_ A bitsperpixel of 24 uses the packed representation of 3 bytes/pixel.
For the more common 4 bytes/pixel mode, use a bitsperpixel of 32. Somewhat
oddly, both 15 and 16 will request a 2 bytes/pixel mode, but different pixel
formats.

_Note:_ Use SDL_SWSURFACE if you plan on doing per-pixel manipulations, or
blit surfaces with alpha channels, and require a high framerate. When you use
hardware surfaces (SDL_HWSURFACE), SDL copies the surfaces from video memory
to system memory when you lock them, and back when you unlock them. This can
cause a major performance hit. (Be aware that you may request a hardware
surface, but receive a software surface. Many platforms can only provide a
hardware surface when using SDL_FULLSCREEN.) SDL_HWSURFACE is best used when
the surfaces you'll be blitting can also be stored in video memory.

_Note:_ If you want to control the position on the screen when creating a
windowed surface, you may do so by setting the environment variables
"SDL_VIDEO_CENTERED=center" or "SDL_VIDEO_WINDOW_POS=x,y". You can set them
via SDL_putenv.

**Return Value**: The framebuffer surface, or NULL if it fails. The surface returned is freed by SDL_Quit and should not be freed by the caller. Note: This rule includes consecutive calls to SDL_SetVideoMode (i.e. resize or rez change) - the pre-existing surface will be released automatically. 

* * *

email:

dranger at gmail dot com

Back to dranger.com

This work is licensed under the Creative Commons Attribution-Share Alike 2.5
License. To view a copy of this license, visit
http://creativecommons.org/licenses/by-sa/2.5/ or send a letter to Creative
Commons, 543 Howard Street, 5th Floor, San Francisco, California, 94105, USA.

  
Much of this documentation is based off of FFmpeg, Copyright (c) 2003 Fabrice
Bellard, and from the SDL Documentation Wiki.

