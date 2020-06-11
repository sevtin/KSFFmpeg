## An ffmpeg and SDL Tutorial

Page 1 2 3 4 5 6 7 End  Prev Home Next  &nbsp_place_holder;

### What now?

So we have a working player, but it's certainly not as nice as it could be. We
did a lot of handwaving, and there are a lot of other features we could add:

  * Let's face it, this player sucks. The version of ffplay.c it is based on is totally outdated, and this tutorial needs a major rehaul as a result. If you want to move on to more serious projects using ffmpeg libraries, I implore you to check out the most recent version of ffplay.c as your next task. 
  * Error handling. The error handling in our code is abysmal, and could be handled a lot better.
  * Pausing. We can't pause the movie, which is admittedly a useful feature. We can do this by making use of an internal paused variable in our big struct that we set when the user pauses. Then our audio, video, and decode threads check for it so they don't output anything. We also use `av_read_play` for network support. It's pretty simple to explain, but it's not obvious to figure out for yourself, **so consider this homework** if you want to try more. For hints, check out ffplay.c.
  * Support for video hardware.
  * Seeking by bytes. If you calculate the seek position by bytes instead of seconds, it is more accurate on video files that have discontiguous timestamps, like VOB files.
  * Frame dropping. If the video falls too far behind, we should drop the next frame instead of setting a short refresh.
  * Network support. This video player can't play network streaming video.
  * Support for raw video like YUV files. There are some options we have to set if our player is to support raw video like YUV files, as we cannot guess the size or time_base.
  * Fullscreen
  * Various options, e.g. different pic formats; see ffplay.c for all the command line switches.
If you want to know more about ffmpeg, we've only covered a portion of it. The
next step would be to study how to _encode_ multimedia. A good place to start
would be the output_example.c file in the ffmpeg distribution. I may write
another tutorial on that, but I might not get around to it.

**UPDATE** It has been a long time since I updated this, and the world of video software has gotten a lot more mature. This tutorial has only required simple API updates; very little has actually changed in terms of the basic concepts. Most of those updates have actually simplified the code. However, while I have gone through and updated the code here, ffplay still totally outperforms this toy player. Let's be frank, it's pretty unusable as an actual movie player. So if you or your future self wants to improve this tutorial, go into ffplay and find out what we're missing. My guess is it's mostly taking advantage of video hardware, but maybe I'm missing some obvious things. It's possible ffplay might have drastically changed some things; I haven't looked yet. 

But I'm very proud that it has still helped a lot of people out over the
years, even if you had to go get code elsewhere. My absolute gratitude to
chelyaev who did the work in replacing all the functions that were deprecated
since I wrote this 8 (!) years ago.

Well, I hope this tutorial was instructive and fun. If you have any
suggestions, bugs, complaints, accolades, etc., about this tutorial, please
email me at dranger at gmail dot com. Please do not ask me for help in your
other ffmpeg projects. I get way too many of those emails.

Links:

ffmpeg home page

Martin Bohme's original tutorial

libSDL

SDL Documentation

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

