uniform vec2 u_alphaRef;

uniform vec4  u_fogData;
uniform vec4  u_fogColor;

#define u_fogStart (u_fogData.x)
#define u_fogEnd (u_fogData.y)
#define u_fogRange (u_fogData.z)
#define u_fogDisable (u_fogData.w)

void DoAlphaTest(float a)
{
	if(a < u_alphaRef.x || a >= u_alphaRef.y)
		discard;
}
