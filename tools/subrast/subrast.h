extern rw::Camera *Camera;
extern rw::Camera *SubCameras[4];

void CreateCameras(rw::World *world);
void DestroyCameras(rw::World *world);
void UpdateSubRasters(rw::Camera *mainCamera, rw::int32 mainWidth, rw::int32 mainHeight);

extern rw::V3d Xaxis;
extern rw::V3d Yaxis;
extern rw::V3d Zaxis;
