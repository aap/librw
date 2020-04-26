#version 330

layout(std140) uniform State
{
	int   u_alphaTest;
	float u_alphaRef;

	float u_fogStart;
	float u_fogEnd;
	float u_fogRange;
	float u_fogDisable;
	vec4  u_fogColor;
};

uniform sampler2D tex0;
uniform sampler2D tex1;

uniform float u_coefficient;

in vec4 v_color;
in vec2 v_tex0;
in vec2 v_tex1;
in float v_fog;

out vec4 color;

void
main(void)
{
	vec4 pass1 = v_color;
	vec4 envColor = pass1;	// TODO: colorClamp

	pass1 *= texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));
	vec4 pass2 = envColor*u_coefficient*texture(tex1, vec2(v_tex1.x, 1.0-v_tex1.y));

	color.rgb = pass1.rgb*pass1.a + pass2.rgb;
	color.a = pass1.a;

	switch(u_alphaTest){
	default:
	case 0: break;
	case 1:
		if(color.a < u_alphaRef)
			discard;
		break;
	case 2:
		if(color.a >= u_alphaRef)
			discard;
		break;
	}
}
