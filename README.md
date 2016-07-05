librw
=====

This library is supposed to be a re-implementation of RenderWare graphics,
or a good part of it anyway.

It is intended to be cross-platform in two senses eventually:
support rendering on different platforms similar to RW;
supporting all file formats for all platforms at all times and provide
way to convert to all other platforms.

File formats are already supported rather well, although rasters
as used by TXD files still need some work, especially for PS2.

As for rendering, D3D9 and OpenGL 3 work somewhat well but both still need
work. Rendering some things on the PS2 worked in the past but the code
is not maintained, it was only a test.

# Roadmap

* Work on platform independent rendering functions (setting render states etc.)

* Get a solid GL3 driver working

* Make building everything a bit easier

# Building

Edit the makefile(s) and type 'make BUILD=gl3'
