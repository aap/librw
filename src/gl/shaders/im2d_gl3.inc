const char *im2d_vert_src =
"#version 330\n"

"layout(std140) uniform Im2DState\n"
"{\n"
"	int   u_alphaTest;\n"
"	float u_alphaRef;\n"
"	mat4  u_xform;\n"
"};\n"

"layout(location = 0) in vec3 in_pos;\n"
"layout(location = 2) in vec4 in_color;\n"
"layout(location = 3) in vec2 in_tex0;\n"

"out vec4 v_color;\n"
"out vec2 v_tex0;\n"

"void\n"
"main(void)\n"
"{\n"
"	gl_Position = vec4(in_pos, 1.0);\n"
"	v_color = in_color;\n"
"	v_tex0 = in_tex0;\n"
"}\n"
;
const char *im2d_frag_src =
"#version 330\n"

"uniform sampler2D tex;\n"

"in vec4 v_color;\n"
"in vec2 v_tex0;\n"

"out vec4 color;\n"

"void\n"
"main(void)\n"
"{\n"
"	color = v_color*texture(tex, vec2(v_tex0.x, v_tex0.y));\n"
"}\n"

;
