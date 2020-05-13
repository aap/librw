const char *im3d_vert_src =
"attribute vec3 in_pos;\n"
"attribute vec4 in_color;\n"
"attribute vec2 in_tex0;\n"

"varying vec4 v_color;\n"
"varying vec2 v_tex0;\n"
"varying float v_fog;\n"

"void\n"
"main(void)\n"
"{\n"
"	vec4 V = u_world * vec4(in_pos, 1.0);\n"
"	vec4 cV = u_view * V;\n"
"	gl_Position = u_proj * cV;\n"
"	v_color = in_color;\n"
"	v_tex0 = in_tex0;\n"
"	v_fog = DoFog(gl_Position.w);\n"
"}\n"
;
