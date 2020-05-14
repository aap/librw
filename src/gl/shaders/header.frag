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
//uniform int   u_alphaTest;
//uniform float u_alphaRef;
uniform vec2 u_alphaRef;

uniform float u_fogStart;
uniform float u_fogEnd;
uniform float u_fogRange;
uniform float u_fogDisable;
uniform vec4  u_fogColor;
#endif

void DoAlphaTest(float a)
{
	if(a < u_alphaRef.x || a >= u_alphaRef.y)
		discard;
}
