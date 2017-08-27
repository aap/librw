#version 330

//layout(std140) uniform Im2DState
//{
//	int   u_alphaTest;
//	float u_alphaRef;
//	mat4  u_xform;
//};

uniform vec4 u_xform;

layout(location = 0) in vec4 in_pos;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec2 in_tex0;

out vec4 v_color;
out vec2 v_tex0;

void
main(void)
{
	gl_Position = in_pos;
	gl_Position.xy = gl_Position.xy * u_xform.xy + u_xform.zw;
	gl_Position.xyz *= gl_Position.w;
	v_color = in_color;
	v_tex0 = in_tex0;
}
