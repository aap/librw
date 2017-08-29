#version 330

layout(std140) uniform State
{
	int   u_alphaTest;
	float u_alphaRef;

	int   u_fogEnable;
	float u_fogStart;
	float u_fogEnd;
	vec4  u_fogColor;
};

layout(std140) uniform Scene
{
	mat4 u_proj;
	mat4 u_view;
};

layout(std140) uniform Object
{
	mat4  u_world;
};

layout(location = 0) in vec3 in_pos;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec2 in_tex0;

out vec4 v_color;
out vec2 v_tex0;
out float v_fog;

void
main(void)
{
	vec4 V = u_world * vec4(in_pos, 1.0);
	vec4 cV = u_view * V;   
	gl_Position = u_proj * cV;
	v_color = in_color;
	v_tex0 = in_tex0;
	v_fog = clamp((cV.z - u_fogEnd)/(u_fogStart - u_fogEnd), 0.0, 1.0);
}
