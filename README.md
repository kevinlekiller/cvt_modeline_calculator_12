# cvt_modeline_calculator_12
CVT (Coordinated Video Timings) modeline calculator with CVT v1.2 timings.

This is a modified CVT modeline calculator based on [cvt](http://www.uruk.org/~erich/projects/cvt/)  
by erich@uruk.org, which is based on [GTF modeline calculator](http://gtf.sourceforge.net/) by Andy Ritger.

This modified version adds support for CVT v1.2 (VESA-2013-3 v1.2).

Changes:
 * Add support for CVT v1.2 reduced blanking timings.
 * Add support for 1000/1001 reducing of refresh rate for better movie support (ie 24hz * 1000 / 1001 = 23.976hz).
 * Add check for reduce blanking on CVT v1.1 for refresh rates not a multiple of 60hz.
 * Add checks for bad x y and refresh values.
 * Display 3 digits after decimal point on CLI output for the commented out part of the xf86 modeline. (ie 23.98hz -> 23.976hz)
 * Change xf86 modeline printout to be similar to [x.org](http://www.x.org/wiki/)'s cvt.
 * Fix some default timing values on v1.1 to be in line with the Vesa Public Standards CVT Generator.
 * Various minor changes.

Compilation:

`$ gcc cvt12.c -O2 -o cvt -lm -Wall`

License is in the source at the top.
