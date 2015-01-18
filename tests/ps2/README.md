PS2 test renderer
=================

This is a basic RW renderer for PS2.
There is a bunch of unrelated code since I adapted an earlier program of mine.

To compile you need the ps2toolchain and ee-dvp-as from the Sony SDK
(dvp-as from the ps2toolchain segfaults when encountering MPG,
this should be fixed).

So far the program can render PS2 native geometry (no generic geometry atm)
in the default or skin format (no PDS pipes as used in San Andreas supported yet).
The files are read from host: (see main.cpp)
so you can use e.g. ps2link on a real PS2 or pcsx2 with the host device enabled.
