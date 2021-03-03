

extern rw::Light *BaseAmbientLight;
extern bool BaseAmbientLightOn;

extern rw::Light *CurrentLight;
extern rw::Light *AmbientLight;
extern rw::Light *PointLight;
extern rw::Light *DirectLight;
extern rw::Light *SpotLight;
extern rw::Light *SpotSoftLight;

extern float LightRadius;
extern float LightConeAngle;
extern rw::RGBAf LightColor;

extern bool LightOn;
extern bool LightDrawOn;
extern rw::V3d LightPos;
extern rw::int32 LightTypeIndex;

extern rw::BBox RoomBBox;

rw::Light *CreateBaseAmbientLight(void);
rw::Light *CreateAmbientLight(void);
rw::Light *CreateDirectLight(void);
rw::Light *CreatePointLight(void);
rw::Light *CreateSpotLight(void);
rw::Light *CreateSpotSoftLight(void);

void LightsDestroy(void);
void LightsUpdate(void);
void DrawCurrentLight(void);

void LightRotate(float xAngle, float yAngle);
void LightTranslateXY(float xDelta, float yDelta);
void LightTranslateZ(float zDelta);
