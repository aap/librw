
#define DIRECTIONALS
//#define POINTLIGHTS
//#define SPOTLIGHTS

#ifdef USE_UBOS
layout(std140) uniform State
{
	vec2 u_alphaRef;

	float u_fogStart;
	float u_fogEnd;
	float u_fogRange;
	float u_fogDisable;
	vec4  u_fogColor;
};
#else
uniform vec2 u_alphaRef;

uniform float u_fogStart;
uniform float u_fogEnd;
uniform float u_fogRange;
uniform float u_fogDisable;
uniform vec4  u_fogColor;
#endif

#ifdef USE_UBOS
layout(std140) uniform Scene
{
	mat4 u_proj;
	mat4 u_view;
};
#else
uniform mat4 u_proj;
uniform mat4 u_view;
#endif

#define MAX_LIGHTS 8

#ifdef USE_UBOS
layout(std140) uniform Object
{
	mat4  u_world;
	vec4  u_ambLight;
	vec4 u_lightParams[MAX_LIGHTS];	// type, radius, minusCosAngle, hardSpot
	vec4 u_lightPosition[MAX_LIGHTS];
	vec4 u_lightDirection[MAX_LIGHTS];
	vec4 u_lightColor[MAX_LIGHTS];
};
#else
uniform mat4 u_world;
uniform vec4 u_ambLight;
uniform vec4 u_lightParams[MAX_LIGHTS];	// type, radius, minusCosAngle, hardSpot
uniform vec4 u_lightPosition[MAX_LIGHTS];
uniform vec4 u_lightDirection[MAX_LIGHTS];
uniform vec4 u_lightColor[MAX_LIGHTS];
#endif

uniform vec4 u_matColor;
uniform vec4 u_surfProps;	// amb, spec, diff, extra

#define surfAmbient (u_surfProps.x)
#define surfSpecular (u_surfProps.y)
#define surfDiffuse (u_surfProps.z)

vec3 DoDynamicLight(vec3 V, vec3 N)
{
	vec3 color = vec3(0.0, 0.0, 0.0);
	for(int i = 0; i < MAX_LIGHTS; i++){
		if(u_lightParams[i].x == 0.0)
			break;
#ifdef DIRECTIONALS
		if(u_lightParams[i].x == 1.0){
			// direct
			float l = max(0.0, dot(N, -u_lightDirection[i].xyz));
			color += l*u_lightColor[i].rgb;
		}else
#endif
#ifdef POINTLIGHTS
		if(u_lightParams[i].x == 2.0){
			// point
			vec3 dir = V - u_lightPosition[i].xyz;
			float dist = length(dir);
			float atten = max(0.0, (1.0 - dist/u_lightParams[i].y));
			float l = max(0.0, dot(N, -normalize(dir)));
			color += l*u_lightColor[i].rgb*atten;
		}else
#endif
#ifdef SPOTLIGHTS
		if(u_lightParams[i].x == 3.0){
			// spot
			vec3 dir = V - u_lightPosition[i].xyz;
			float dist = length(dir);
			float atten = max(0.0, (1.0 - dist/u_lightParams[i].y));
			dir /= dist;
			float l = max(0.0, dot(N, -dir));
			float pcos = dot(dir, u_lightDirection[i].xyz);	// cos to point
			float ccos = -u_lightParams[i].z;
			float falloff = (pcos-ccos)/(1.0-ccos);
			if(falloff < 0.0)	// outside of cone
				l = 0.0;
			l *= max(falloff, u_lightParams[i].w);
			return l*u_lightColor[i].rgb*atten;
		}else
#endif
			;
	}
	return color;
}

float DoFog(float w)
{
	return clamp((w - u_fogEnd)*u_fogRange, u_fogDisable, 1.0);
}
