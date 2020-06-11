## Data List

Text version

AVCodecContext

     All the codec information from a stream, from AVStream->codec. Some important data members: 

**AVRational time_base**: frames per sec  
**int sample_rate**: samples per sec  
**int channels**: number of channels  

For a full list (this thing is huge), see
http://www.irisa.fr/texmex/people/dufouil/ffmpegdoxy/structAVCodecContext.html
Many of the options are used mostly for encoding rather than decoding.

AVFormatContext

     Data fields: 

**const AVClass * av_class**   
**AVInputFormat * iformat**   
**AVOutputFormat * oformat**   
**void * priv_data**:   
**ByteIOContext pb**: ByteIOContext of the file, used for the low level file manipulation   
**unsigned int nb_streams**: Number of streams in the file  
**AVStream * streams [MAX_STREAMS]**: Stream data for each stream is stored here.  
**char filename [1024]**: duh  
  
_File Information_

**int64_t timestamp**:   
**char title [512]**:   
**char author [512]**:   
**char copyright [512]**:   
**char comment [512]**:   
**char album [512]**:   
**int year**:   
**int track**:   
**char genre [32]**:   
  
**int ctx_flags**:   
Any of AVFMT_NOFILE, AVFMT_NEEDNUMBER, AVFMT_SHOW_IDS, AVFMT_RAWPICTURE,
AVFMT_GLOBALHEADER, AVFMT_NOTIMESTAMPS, AVFMT_GENERIC_INDEX **AVPacketList *
packet_buffer**: This buffer is only needed when packets were already buffered
but not decoded, for example to get the codec parameters in mpeg streams

**int64_t start_time**: decoding: position of the first frame of the component, in AV_TIME_BASE fractional seconds. NEVER set this value directly: it is deduced from the AVStream values.  
**int64_t duration**: decoding: duration of the stream, in AV_TIME_BASE fractional seconds. NEVER set this value directly: it is deduced from the AVStream values.  
**int64_t file_size**: total file size, 0 if unknown  
**int bit_rate**: decoding: total stream bitrate in bit/s, 0 if not available. Never set it directly if the file_size and the duration are known as ffmpeg can compute it automatically.  
**AVStream * cur_st**   
**const uint8_t * cur_ptr**   
**int cur_len**   
**AVPacket cur_pkt**:   
**int64_t data_offset**:   
**int index_built**: offset of the first packet  
**int mux_rate**:   
**int packet_size**:   
**int preload**:   
**int max_delay**:   
**int loop_output**: number of times to loop output in formats that support it  
**int flags**:   
**int loop_input**:   
**unsigned int probesize**: decoding: size of data to probe; encoding unused  
**int max_analyze_duration**: maximum duration in AV_TIME_BASE units over which the input should be analyzed in av_find_stream_info()  
**const uint8_t * key**:   
**int keylen**:   

AVIOContext

     An IO Context to access resources. 

**const AVClass * av_class** A class for private options.  
**unsigned char * buffer** Start of the buffer.  
**int buffer_size** Maximum buffer size.  
**unsigned char * buf_ptr** Current position in the buffer.  
**unsigned char * buf_end** End of the data, may be less than buffer+buffer_size if the read function returned less data than requested, e.g.   
**void * opaque** A private pointer, passed to the read/write/seek/...   
**int(* read_packet )(void *opaque, uint8_t *buf, int buf_size)**   
**int(* write_packet )(void *opaque, uint8_t *buf, int buf_size)**   
**int64_t(* seek )(void *opaque, int64_t offset, int whence)**   
**int64_t pos** position in the file of the current buffer   
**int must_flush** true if the next seek should flush   
**int eof_reached** true if eof reached   
**int write_flag** true if open for writing   
**int max_packet_size**   
**unsigned long checksum**   
**unsigned char * checksum_ptr**   
**unsigned long(* update_checksum )(unsigned long checksum, const uint8_t *buf, unsigned int size)**   
**int error** contains the error code or 0 if no error happened   
**int(* read_pause )(void *opaque, int pause)** Pause or resume playback for network streaming protocols - e.g.   
**int64_t(* read_seek )(void *opaque, int stream_index, int64_t timestamp, int flags)** Seek to a given timestamp in stream with the specified stream_index.   
**int seekable** A combination of AVIO_SEEKABLE_ flags or 0 when the stream is not seekable.   
**int64_t maxsize** max filesize, used to limit allocations This field is internal to libavformat and access from outside is not allowed.   
**int direct** avio_read and avio_write should if possible be satisfied directly instead of going through a buffer, and avio_seek will always call the underlying seek function directly.   
**int64_t bytes_read** Bytes read statistic This field is internal to libavformat and access from outside is not allowed.   
**int seek_count** seek statistic This field is internal to libavformat and access from outside is not allowed.   
**int writeout_count** writeout statistic This field is internal to libavformat and access from outside is not allowed.   
**int orig_buffer_size** Original buffer size used internally after probing and ensure seekback to reset the buffer size This field is internal to libavformat and access from outside is not allowed. 

AVDictionary

     This is used to pass parameters to ffmpeg. 

**int count**   
**AVDictionaryEntry *elems**   

AVDictionaryEntry

     This is used to hold dictionary entries in AVDictionary. 

**char *ket**   
**char *value**   

AVFrame

     This struct is dependent upon the type of codec, and therefore is dynamically defined. There are common data members for this struct, though: 

**uint8_t *data[4]**:   
**int linesize[4]**: Stride information  
**uint8_t *base[4]**:   
**int key_frame**:   
**int pict_type**:   
**int64_t pts**: This is not the pts you want when decoding.  
**int coded_picture_number**:   
**int display_picture_number**:   
**int quality**:   
**int age**:   
**int reference**:   
**int8_t *qscale_table**:   
**int qstride**:   
**uint8_t *mbskip_table**:   
**int16_t (*motion_val[2])[2]**:   
**uint32_t *mb_type**:   
**uint8_t motion_subsample_log2**:   
**void *opaque**: User-defined data  
**uint64_t error[4]**:   
**int type**:   
**int repeat_pict**: Indicates we should repeat this picture this number of times   
**int qscale_type**:   
**int interlaced_frame**:   
**int top_field_first**:   
**AVPanScan *pan_scan**:   
**int palette_has_changed**:   
**int buffer_hints**:   
**short *dct_coeff**:   
**int8_t *ref_index[2]**:   

AVPacket

     The struct in which raw packet data is stored. This data should be given to avcodec_decode_audio2 or avcodec_decode_video to to get a frame. 

**int64_t pts**: presentation time stamp in time_base units  
**int64_t dts**: decompression time stamp in time_base units  
**uint8_t * data**: the raw data  
**int size**: size of data  
**int stream_index**: the stream it came from, based on the number in the AVFormatContext  
**int flags**: set to PKT_FLAG_KEY if packet is a key frame  
**int duration**: presentation duration in time_base units (0 if not available)  
**void(* destruct )(struct AVPacket *)**: deallocation function for this packet (defaults to av_destruct_packet)   
**void * priv**:  
**int64_t pos**: byte position in stream, -1 if unknown 

AVPacketList

     A simple linked list for packets. 

**AVPacket pkt**  
**AVPacketList * next**

AVPicture

     This struct is the exact same as the first two data members of AVFrame, so it is often casted from that. This is usually used in sws functions. 

**uint8_t * data [4]**  
**int linesize [4]**: number of bytes per line 

AVRational

     Simple struct to represent rational numbers. 

**int num**: numerator  
**int den**: denominator 

AVStream

     Struct for the stream. You will probably use the information in **codec** most often. 

**int index**:  
**int id**:  
**AVCodecContext * codec**:  
**AVRational r_frame_rate**:  
**void * priv_data**:  
**int64_t codec_info_duration**:  
**int codec_info_nb_frames**:  
**AVFrac pts**:  
**AVRational time_base**:  
**int pts_wrap_bits**:  
**int stream_copy**:  
**enum AVDiscard discard**: selects which packets can be discarded at will and dont need to be demuxed  
**float quality**:  
**int64_t start_time**:  
**int64_t duration**:  
**char language [4]**:  
**int need_parsing**: 1->full parsing needed, 2->only parse headers dont repack  
**AVCodecParserContext * parser**:  
**int64_t cur_dts**:  
**int last_IP_duration**:  
**int64_t last_IP_pts**:  
**AVIndexEntry * index_entries**:  
**int nb_index_entries**:  
**unsigned int index_entries_allocated_size**:  
**int64_t nb_frames**: number of frames in this stream if known or 0  
**int64_t pts_buffer [MAX_REORDER_DELAY+1]**:  

ByteIOContext

     Struct that stores the low-level file information about our movie file. 

**unsigned char * buffer**:   
**int buffer_size**:   
**unsigned char * buf_ptr**:   
**unsigned char * buf_end**:   
**void * opaque**:   
**int(* read_packet )(void *opaque, uint8_t *buf, int buf_size)**:   
**int(* write_packet )(void *opaque, uint8_t *buf, int buf_size)**:   
**offset_t(* seek )(void *opaque, offset_t offset, int whence)**:   
**offset_t pos**:   
**int must_flush**:   
**int eof_reached**:   
**int write_flag**:   
**int is_streamed**:   
**int max_packet_size**:   
**unsigned long checksum**:   
**unsigned char * checksum_ptr**:   
**unsigned long(* update_checksum )(unsigned long checksum,**:   
**const uint8_t *buf, unsigned int size)**:   
**int error**: contains the error code or 0 if no error happened   

SDL_AudioSpec

     The SDL_AudioSpec structure is used to describe the format of some audio data. Data members: 

**freq**: Audio frequency in samples per second  
**format**: Audio data format  
**channels**: Number of channels: 1 mono, 2 stereo, 4 surround, 6 surround with center and lfe  
**silence**: Audio buffer silence value (calculated)  
**samples**: Audio buffer size in samples  
**size**: Audio buffer size in bytes (calculated)  
**callback(..)**: Callback function for filling the audio buffer  
**userdata**: Pointer the user data which is passed to the callback function 

The following **format** values are acceptable:

AUDIO_U8 Unsigned 8-bit samples.

AUDIO_S8 Signed 8-bit samples.

AUDIO_U16 or AUDIO_U16LSB not supported by all hardware (unsigned 16-bit
little-endian)

AUDIO_S16 or AUDIO_S16LSBnot supported by all hardware (signed 16-bit little-
endian)

AUDIO_U16MSB not supported by all hardware (unsigned 16-bit big-endian)

AUDIO_S16MSBnot supported by all hardware (signed 16-bit big-endian)

AUDIO_U16SYS Either AUDIO_U16LSB or AUDIO_U16MSB depending on hardware CPU
endianness

AUDIO_S16SYS Either AUDIO_S16LSB or AUDIO_S16MSB depending on hardware CPU
endianness

SDL_Event

     General event structure. Data members: 

**type**: The type of event  
**active**: Activation event (see SDL_ActiveEvent)  
**key**: Keyboard event (see SDL_KeyboardEvent)  
**motion**: Mouse motion event (see SDL_MouseMotionEvent)  
**button**: Mouse button event (see SDL_MouseButtonEvent)  
**jaxis**: Joystick axis motion event (see SDL_JoyAxisEvent)  
**jball**: Joystick trackball motion event (see SDL_JoyBallEvent)  
**jhat**: Joystick hat motion event (see SDL_JoyHatEvent)  
**jbutton**: Joystick button event (see SDL_JoyButtonEvent)  
**resize**: Application window resize event (see SDL_ResizeEvent)  
**expose**: Application window expose event (see SDL_ExposeEvent)  
**quit**: Application quit request event (see SDL_QuitEvent)  
**user**: User defined event (see SDL_UserEvent)  
**syswm**: Undefined window manager event (see SDL_SysWMEvent)  

Here are the event types. See the SDL documentation for more info:

  
SDL_ACTIVEEVENT SDL_ActiveEvent

SDL_KEYDOWN/UP SDL_KeyboardEvent

SDL_MOUSEMOTION SDL_MouseMotionEvent

SDL_MOUSEBUTTONDOWN/UP SDL_MouseButtonEvent

SDL_JOYAXISMOTION SDL_JoyAxisEvent

SDL_JOYBALLMOTION SDL_JoyBallEvent

SDL_JOYHATMOTION SDL_JoyHatEvent

SDL_JOYBUTTONDOWN/UP SDL_JoyButtonEvent

SDL_VIDEORESIZE SDL_ResizeEvent

SDL_VIDEOEXPOSE SDL_ExposeEvent

SDL_QUIT SDL_QuitEvent

SDL_USEREVENT SDL_UserEvent

SDL_SYSWMEVENT SDL_SysWMEvent

SDL_Overlay

    A YUV Overlay. Data members: 

**format**: Overlay format (see below)  
**w, h**: Width and height of overlay  
**planes**: Number of planes in the overlay. Usually either 1 or 3  
**pitches**: An array of pitches, one for each plane. Pitch is the length of a row in bytes.  
**pixels**: An array of pointers to the data of each plane. The overlay be locked before these pointers are used.  
**hw_overlay**: This will be set to 1 if the overlay is hardware accelerated.  
**

SDL_Rect

     Defines a rectangular area. 

**Sint16 x, y** **Uint16 w, h**

_x, y_: Position of the upper-left corner of the rectangle _w, h_: The width
and height of the rectangle

A SDL_Rect defines a rectangular area of pixels. It is used by SDL_BlitSurface
to define blitting regions and by several other video functions.

SDL_Surface

     Graphical Surface Structure 

**Uint32 flags** (Read-only): Surface flags  
**SDL_PixelFormat *format** (Read-only)  
**int w, h** (Read-only): Width and height  
**Uint16 pitch** (Read-only): stride  
**void *pixels** (Read-write): pointer to actual pixel data  
**SDL_Rect clip_rect** (Read-only): Surface clip rectangle  
**int refcount** (Read-mostly): used for mem allocation  
This structure also contains private fields not shown here.

An SDL_Surface represents an area of "graphical" memory, memory that can be
drawn to. The video framebuffer is returned as a SDL_Surface by
SDL_SetVideoMode and SDL_GetVideoSurface. The **w** and **h** fields are
values representing the width and height of the surface in pixels. The
**pixels** field is a pointer to the actual pixel data. Note: the surface
should be locked (via SDL_LockSurface) before accessing this field. The
**clip_rect** field is the clipping rectangle as set by SDL_SetClipRect.

The flags field supports the following OR-ed values:

SDL_SWSURFACE Surface is stored in system memory

SDL_HWSURFACE Surface is stored in video memory

SDL_ASYNCBLIT Surface uses asynchronous blits if possible

SDL_ANYFORMAT Allows any pixel-format (Display surface)

SDL_HWPALETTE Surface has exclusive palette

SDL_DOUBLEBUF Surface is double buffered (Display surface)

SDL_FULLSCREEN Surface is full screen (Display Surface)

SDL_OPENGL Surface has an OpenGL context (Display Surface)

SDL_OPENGLBLIT Surface supports OpenGL blitting (Display Surface). NOTE: This
option is kept for compatibility only, and is not recommended for new code.

SDL_RESIZABLE Surface is resizable (Display Surface)

SDL_HWACCEL Surface blit uses hardware acceleration

SDL_SRCCOLORKEY Surface use colorkey blitting

SDL_RLEACCEL Colorkey blitting is accelerated with RLE

SDL_SRCALPHA Surface blit uses alpha blending

SDL_PREALLOC Surface uses preallocated memory

SDL_Thread

     This struct is is system independent and you probably don't have to use it. For more info, see src/thread/sdl_thread_c.h in the source code. 

SDL_cond

     This struct is is system independent and you probably don't have to use it. For more info, see src/thread/<system&rt;/SDL_syscond.c in the source code.

SDL_mutex

    This struct is is system independent and you probably don't have to use it. For more info, see src/thread/<system&rt;/SDL_sysmutex.c in the source code. 

* * *

email:

dranger at gmail dot com

Back to dranger.com

This work is licensed under the Creative Commons Attribution-Share Alike 2.5
License. To view a copy of this license, visit
http://creativecommons.org/licenses/by-sa/2.5/ or send a letter to Creative
Commons, 543 Howard Street, 5th Floor, San Francisco, California, 94105, USA.

  
Code examples are based off of FFplay, Copyright (c) 2003 Fabrice Bellard, and
a tutorial by Martin Bohme.

