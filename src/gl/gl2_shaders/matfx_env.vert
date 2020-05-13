uniform mat4 u_texMatrix;

attribute vec3 in_pos;
attribute vec3 in_normal;
attribute vec4 in_color;
attribute vec2 in_tex0;

varying vec4 v_color;
varying vec2 v_tex0;
varying vec2 v_tex1;
varying float v_fog;

void
main(void)
{
	vec4 V = u_world * vec4(in_pos, 1.0);
	gl_Position = u_proj * u_view * V;
	vec3 N = mat3(u_world) * in_normal;

	v_tex0 = in_tex0;
	v_tex1 = (u_texMatrix * vec4(N, 1.0)).xy;

	v_color = in_color;
	v_color.rgb += u_ambLight.rgb*surfAmbient;
	v_color.rgb += DoDynamicLight(V.xyz, N)*surfDiffuse;
	v_color = clamp(v_color, 0.0f, 1.0);
	v_color *= u_matColor;

	v_fog = DoFog(gl_Position.w);
}
