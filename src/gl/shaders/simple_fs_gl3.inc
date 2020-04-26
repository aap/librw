const char *simple_frag_src =
"#version 330\n"

"layout(std140) uniform State\n"
"{\n"
"	int   u_alphaTest;\n"
"	float u_alphaRef;\n"

"	float u_fogStart;\n"
"	float u_fogEnd;\n"
"	float u_fogRange;\n"
"	float u_fogDisable;\n"
"	vec4  u_fogColor;\n"
"};\n"

"uniform sampler2D tex0;\n"

"in vec4 v_color;\n"
"in vec2 v_tex0;\n"
"in float v_fog;\n"

"out vec4 color;\n"

"void\n"
"main(void)\n"
"{\n"
"	color = v_color*texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));\n"
"	color.rgb = mix(u_fogColor.rgb, color.rgb, v_fog);\n"
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
"}\n"

;
