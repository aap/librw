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

in vec4 v_color;
in vec2 v_tex0;
in float v_fog;

out vec4 color;

void
main(void)
{
	color = v_color*texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));
	color.rgb = mix(u_fogColor.rgb, color.rgb, v_fog);
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

