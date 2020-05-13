uniform vec4 u_xform;

attribute vec4 in_pos;
attribute vec4 in_color;
attribute vec2 in_tex0;

varying vec4 v_color;
varying vec2 v_tex0;
varying float v_fog;

void
main(void)
{
	gl_Position = in_pos;
	gl_Position.xy = gl_Position.xy * u_xform.xy + u_xform.zw;
	v_fog = DoFog(gl_Position.z);
	gl_Position.xyz *= gl_Position.w;
	v_color = in_color;
//v_color.r = 0.0;
//v_color.g = 0.0;
//v_color.b = 0.0;
//v_color.a = 1.0;
//v_fog = 1.0;
	v_tex0 = in_tex0;
}
