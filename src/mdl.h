rw::Clump *clumpStreamReadRsl(rw::Stream *stream);
bool clumpStreamWriteRsl(rw::Stream *stream, rw::Clump *c);
rw::uint32 clumpStreamGetSizeRsl(rw::Clump *c);

rw::Atomic *atomicStreamReadRsl(rw::Stream *stream, rw::Frame **frameList);
rw::uint32 atomicStreamGetSizeRsl(rw::Atomic *a);

rw::Geometry *geometryStreamReadRsl(rw::Stream *stream);
rw::uint32 geometryStreamGetSizeRsl(rw::Geometry *g);

void convertRslGeometry(rw::Geometry *g);

void registerRslPlugin(void);