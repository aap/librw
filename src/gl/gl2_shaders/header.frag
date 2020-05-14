uniform vec2 u_alphaRef;

uniform float u_fogStart;
uniform float u_fogEnd;
uniform float u_fogRange;
uniform float u_fogDisable;
uniform vec4  u_fogColor;

void DoAlphaTest(float a)
{
	if(a < u_alphaRef.x || a >= u_alphaRef.y)
		discard;
}
