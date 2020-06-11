## An ffmpeg and SDL Tutorial

### or

## How to Write a Video Player in

Less Than 1000 Lines

Page 1 2 3 4 5 6 7 End Prev Home Next &nbsp_place_holder;

# UPDATE: This tutorial is up to date as of February 2015.

ffmpeg is a wonderful library for creating video applications or even general
purpose utilities. ffmpeg takes care of all the hard work of video processing
by doing all the decoding, encoding, muxing and demuxing for you. This can
make media applications much simpler to write. It's simple, written in C,
fast, and can decode almost any codec you'll find in use today, as well as
encode several other formats.

The only problem is that documentation was basically nonexistent. There is a
single tutorial that shows the basics of ffmpeg and auto-generated doxygen
documents. That's it. So, when I decided to learn about ffmpeg, and in the
process about how digital video and audio applications work, I decided to
document the process and present it as a tutorial.

There is a sample program that comes with ffmpeg called ffplay. It is a simple
C program that implements a complete video player using ffmpeg. This tutorial
will begin with an updated version of the original tutorial, written by Martin
Bohme (I have stolen liberally borrowed from that work), and work from there
to developing a working video player, based on Fabrice Bellard's ffplay.c. In
each tutorial, I'll introduce a new idea (or two) and explain how we implement
it. Each tutorial will have a C file so you can download it, compile it, and
follow along at home. The source files will show you how the real program
works, how we move all the pieces around, as well as showing you the technical
details that are unimportant to the tutorial. By the time we are finished, we
will have a working video player written in less than 1000 lines of code!

In making the player, we will be using SDL to output the audio and video of
the media file. SDL is an excellent cross-platform multimedia library that's
used in MPEG playback software, emulators, and many video games. You will need
to download and install the SDL development libraries for your system in order
to compile the programs in this tutorial.

This tutorial is meant for people with a decent programming background. At the
very least you should know C and have some idea about concepts like queues,
mutexes, and so on. You should know some basics about multimedia; things like
waveforms and such, but you don't need to know a lot, as I explain a lot of
those concepts in this tutorial.

There are also old school ASCII files of the tutorials. You can also get a
tarball of the text files and source code or just the source.

Please feel free to email me with bugs, questions, comments, ideas, features,
whatever, at _dranger at gmail dot com_.

_**>>** Proceed with the tutorial!_

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

