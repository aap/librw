uniform vec2 u_alphaRef;

uniform float u_fogStart;
uniform float u_fogEnd;
uniform float u_fogRange;
uniform float u_fogDisable;
uniform vec4  u_fogColor;

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
	if(color.a < u_alphaRef.x || color.a >= u_alphaRef.y)
		discard;
/*
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
*/

	gl_FragColor = color;
}

