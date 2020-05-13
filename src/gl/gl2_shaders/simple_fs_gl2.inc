const char *simple_frag_src =
"uniform vec2 u_alphaRef;\n"

"uniform float u_fogStart;\n"
"uniform float u_fogEnd;\n"
"uniform float u_fogRange;\n"
"uniform float u_fogDisable;\n"
"uniform vec4  u_fogColor;\n"

"uniform sampler2D tex0;\n"

"varying vec4 v_color;\n"
"varying vec2 v_tex0;\n"
"varying float v_fog;\n"

"void\n"
"main(void)\n"
"{\n"
"	vec4 color;\n"
"	color = v_color*texture2D(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));\n"
"	color.rgb = mix(u_fogColor.rgb, color.rgb, v_fog);\n"
"	if(color.a < u_alphaRef.x || color.a >= u_alphaRef.y)\n"
"		discard;\n"
"/*\n"
"	switch(u_alphaTest){\n"
"	default:\n"
"	case 0: break;\n"
"	case 1:\n"
"		if(color.a < u_alphaRef)\n"
"			discard;\n"
"		break;\n"
"	case 2:\n"
"		if(color.a >= u_alphaRef)\n"
"			discard;\n"
"		break;\n"
"	}\n"
"*/\n"

"	gl_FragColor = color;\n"
"}\n"

;
