#version 330

layout(std140) uniform Scene
{
	mat4 u_proj;
	mat4 u_view;
};

#define MAX_LIGHTS 8
struct Light {
	vec4  position;
	vec4  direction;
	vec4  color;
	float radius;
	float minusCosAngle;
};

layout(std140) uniform Object
{
	mat4  u_world;
	vec4  u_ambLight;
	int   u_numLights;
	Light u_lights[MAX_LIGHTS];
};

uniform vec4 u_matColor;
uniform vec4 u_surfaceProps;	// amb, spec, diff, extra

uniform mat4 u_texMatrix;
uniform float u_coefficient;

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec2 in_tex0;

out vec4 v_color;
out vec2 v_tex0;

void
main(void)
{
	vec4 V = u_world * vec4(in_pos, 1.0);
	gl_Position = u_proj * u_view * V;
	vec3 N = mat3(u_world) * in_normal;

	v_color = in_color;
	for(int i = 0; i < u_numLights; i++){
		float L = max(0.0, dot(N, -normalize(u_lights[i].direction.xyz)));
		v_color.rgb += u_lights[i].color.rgb*L*u_surfaceProps.z;
	}
	v_color.rgb += u_ambLight.rgb*u_surfaceProps.x;
	v_color *= u_matColor;

	v_color *= u_coefficient;
	vec3 cN = mat3(u_view) * in_normal;
	v_tex0 = (u_texMatrix * vec4(cN, 1.0)).xy;
}
