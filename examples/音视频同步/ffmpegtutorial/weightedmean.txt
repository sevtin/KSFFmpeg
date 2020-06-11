## An ffmpeg and SDL Tutorial

Printable version Text version

The function we are using is a weighted mean using a geometric series as its
weights. A weighted mean is defined like this:

    
    
    w_0*x_0 + w_1*x_1 + ... + w_n*x_n
    ---------------------------------
         w_0 + w_1 + ... + w_n
    

If you substitute 1 in for each `w_n`, you get your normal everyday arithmetic
mean (a.k.a. an _average_).

Our function is basically a repetition of:

    
    
    total = d_x + c*avg;
    

However, you can also look at it like this:

    
    
    total = c^n*d_0 + c^(n-1)*d_1 + ... + c*d_(n-1) + d_n
    

in which case, this is just the top part of a weighted mean with `c^n,
c^(n-1)...` as the weights. That means the bottom half is `c^n+c^(n-1)...`,
which, as you may have guessed, is a simple geometric sum which works out to
`1/(1-c)` as n approaches infinity.

So, by approximation, the weighted mean of our sequence of diffs is simply:

    
    
     total     
    -------  = total * (1-c)
       1        
     -----
     (1-c)
    

So when we get the final total and want to know the average, we just multiply
it by 1-c and get the answer! There is probably a name for this way of taking
the mean of a sequence, but I'm pretty ignorant and I don't know it. If you
know it, please email me.

* * *

Function Reference

Data Reference

email:

dranger at gmail dot com

This work is licensed under the Creative Commons Attribution-Share Alike 2.5
License. To view a copy of this license, visit
http://creativecommons.org/licenses/by-sa/2.5/ or send a letter to Creative
Commons, 543 Howard Street, 5th Floor, San Francisco, California, 94105, USA.

  
Code examples are based off of FFplay, Copyright (c) 2003 Fabrice Bellard, and
a tutorial by Martin Bohme.

