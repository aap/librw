const char *im2d_vert_src =
"uniform vec4 u_xform;\n"

"attribute vec4 in_pos;\n"
"attribute vec4 in_color;\n"
"attribute vec2 in_tex0;\n"

"varying vec4 v_color;\n"
"varying vec2 v_tex0;\n"
"varying float v_fog;\n"

"void\n"
"main(void)\n"
"{\n"
"	gl_Position = in_pos;\n"
"	gl_Position.xy = gl_Position.xy * u_xform.xy + u_xform.zw;\n"
"	v_fog = DoFog(gl_Position.z);\n"
"	gl_Position.xyz *= gl_Position.w;\n"
"	v_color = in_color;\n"
"	v_tex0 = in_tex0;\n"
"}\n"
;
