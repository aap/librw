layout(location = 0) in vec3 in_pos;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec2 in_tex0;

out vec4 v_color;
out vec2 v_tex0;
out float v_fog;

void
main(void)
{
	vec4 Vertex = u_world * vec4(in_pos, 1.0);
	vec4 CamVertex = u_view * Vertex;
	gl_Position = u_proj * CamVertex;
	v_color = in_color;
	v_tex0 = in_tex0;
	v_fog = DoFog(gl_Position.w);
}
