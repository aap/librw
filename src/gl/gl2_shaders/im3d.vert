attribute vec3 in_pos;
attribute vec4 in_color;
attribute vec2 in_tex0;

varying vec4 v_color;
varying vec2 v_tex0;
varying float v_fog;

void
main(void)
{
	vec4 V = u_world * vec4(in_pos, 1.0);
	vec4 cV = u_view * V;
	gl_Position = u_proj * cV;
	v_color = in_color;
	v_tex0 = in_tex0;
	v_fog = DoFog(gl_Position.w);
}
