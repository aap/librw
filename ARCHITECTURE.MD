This document gives an overview of the architecture of librw.

Disclaimer: Some of these design decision were taken over from original RW,
some are my own. I only take partial responsibility.

Differently from original RW, librw has no neat separation into modules.
Some things could be made optional, but in particular RW's RpWorld
plugin is integrated with everything else.

# Plugins

To extend structs with custom data,
RW (and librw) provides a plugin mechanism
for certain structs.
This can be used to tack more data onto a struct
and register custom streaming functions.
Plugins must be registered before any instance
of that struct is allocated.

# Pipelines

RW's pipeline architecture was designed for very flexible data flow.
Unfortunately for RW most of the rendering pipeline moved into the GPU
causing RW's pipeline architecture to be severely overengineered.

librw's pipeline architecture is therefore much simplified
and only implements what is actually needed,
but the name *pipeline* is retained.

Three pipelines are implemented in librw itself:
Default, Skin, MatFX (only env map so far).
Others can be implemented by applications using librw.

# RW Objects

## Frame

A Frame is an orientation in space, arranged in a hierarchy.
Camera, Lights and Atomics can be attached to it.
It has two matrices: a (so called) model matrix,
which is relative to its parent,
and a local transformation matrix (LTM) which is relative to the world.
The LTM is updated automatically as needed whenever the hierarchy gets dirty.

## Camera

A Camera is attached to a Frame to position it in space
and has a framebuffer and a z-buffer attached to render to.
Rendering is started by `beginUpdate` and ended by `endUpdate`.
This sets up things like framebuffers and matrices
so that the Camera's raster can be rendered to.

## Light

Lights are attached to a Frame to position it in space.
They are used to light Atomics for rendering.
Different types of light are possible.

## Geometry

A Geometry contains the raw geometry data that can be rendered.
It has a list of materials that are applied to its triangles.
The latter are sorted by materials into meshes for easier instancing.

## Atomic

An Atomic is attached to a Frame to position it in space
and references a Geometry.
Atomics are the objects that are rendered by pipelines.

## Clump

A Clump is a container of Atomics, Lights and Cameras.
Clumps can be read from and written to DFF files.
Rendering a Clump will be render all of its Atomics.

# Engine

Due to the versatility of librw,
there are three levels of code:
Platform indpendent code,
platform specific code,
and render device specific code.

The second category does not exist in original RW,
but because librw is supposed to be able to
convert between all sorts of platform specific files,
the code for that has to be available no matter
the render platform used.
The common interface for all device-independent
platform code is the `Driver` struct.
The `Engine` struct has an array with one for each platform.

The render device specific code
is used for actually rendering something to the screen.
The common interface for the device-dependent
code is the `Device` struct and the `Engine`
struct only has a single one, as there can only be one render device 
(i.e. you cannot select D3D or OpenGL at runtime).

Thus when implementing a new backend
you have to implement the `Driver` and `Device` interfaces.
But do note that the `Driver` can be extended with plugins!

# Driver

The driver is mostly concerned with conversion
between platform independent data to platform dependent one, and vice versa.
This concerns the following two cases.

## Raster, Images

Images contain platform independent uncompressed pixel data.
Rasters contain platform dependent (and possibly compressed) pixel data.
A driver has to be able to convert an image to a raster for the purposes of loading textures
from files or having them converted from foreign rasters.
Converting from rasters to images is not absolutely necessary but it's needed e.g. for taking screenshots.
librw has a set of default raster formats that the platform is
expected to implement for the most part, however not all have to be supported necessarily.
A driver is also free to implement its own formats;
this is done for texture compression.

Rasters have different types,
`TEXTURE` and `CAMERATEXTURE` rasters can be used as textures,
`CAMERA` and `CAMERATEXTURE` can be used as render targets,
`ZBUFFER` is used as a depth-buffer.

## Pipelines

A librw ObjPipeline implements essentially
an instance stage which converts platform independent geometry
to a format that can efficiently be rendered,
and a render stage that actually renders it.
(There is also an uninstance function,
but this is only used to convert platform specific geometry back to the generic format
and hence is not necessary.)

# Device

The device implements everything that is actually needed for rendering.

This includes starting, handling and closing the rendering device,
setting render states,
allocating and destroying buffers and textures,
im2d and im3d rendering,
and the render functions of the pipelines.

## System

The `system` function implements certain device requests
from the engine (why these aren't separate functions I don't know, RW design).

The `Engine` is started in different stages, at various points of which
the render device gets different requests.
At the end the device is initialized and ready for rendering.
A similar but reverse sequence happens on shutdown.

Subsystems (screens) and video modes are queried through
the `system` by the application before the device is started.

## Immediate mode

Im2d and im3d are immediate-mode style rendering interface.

## Pipelines

For instancing the typical job is to allocate and fill
a struct to hold some data about an atomic,
an array of structs for all the meshes,
and vertex and index buffers to hold geometry for rendering.

The render function will render the previously instanced
data by doing a per-object setup and then iterating over
and rendering all the meshes.
