uniform sampler2D tex0;

varying vec4 v_color;
varying vec2 v_tex0;
varying float v_fog;

void
main(void)
{
	vec4 color;
	color = v_color*texture2D(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));
	color.rgb = mix(u_fogColor.rgb, color.rgb, v_fog);
	DoAlphaTest(color.a);
	gl_FragColor = color;
}

