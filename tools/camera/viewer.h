rw::Camera *ViewerCreate(rw::World *world);
void ViewerDestroy(rw::Camera *camera, rw::World *world);
void ViewerMove(rw::Camera *camera, rw::V3d *offset);
void ViewerRotate(rw::Camera *camera, float deltaX, float deltaY);
void ViewerTranslate(rw::Camera *camera, float deltaX, float deltaY);
void ViewerSetPosition(rw::Camera *camera, rw::V3d *position);
