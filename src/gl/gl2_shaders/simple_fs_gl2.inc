const char *simple_frag_src =
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
"	DoAlphaTest(color.a);\n"
"	gl_FragColor = color;\n"
"}\n"

;
