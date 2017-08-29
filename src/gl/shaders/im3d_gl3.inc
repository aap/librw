const char *im3d_vert_src =
"#version 330\n"

"layout(std140) uniform State\n"
"{\n"
"	int   u_alphaTest;\n"
"	float u_alphaRef;\n"

"	int   u_fogEnable;\n"
"	float u_fogStart;\n"
"	float u_fogEnd;\n"
"	vec4  u_fogColor;\n"
"};\n"

"layout(std140) uniform Scene\n"
"{\n"
"	mat4 u_proj;\n"
"	mat4 u_view;\n"
"};\n"

"layout(std140) uniform Object\n"
"{\n"
"	mat4  u_world;\n"
"};\n"

"layout(location = 0) in vec3 in_pos;\n"
"layout(location = 2) in vec4 in_color;\n"
"layout(location = 3) in vec2 in_tex0;\n"

"out vec4 v_color;\n"
"out vec2 v_tex0;\n"
"out float v_fog;\n"

"void\n"
"main(void)\n"
"{\n"
"	vec4 V = u_world * vec4(in_pos, 1.0);\n"
"	vec4 cV = u_view * V;   \n"
"	gl_Position = u_proj * cV;\n"
"	v_color = in_color;\n"
"	v_tex0 = in_tex0;\n"
"	v_fog = clamp((cV.z - u_fogEnd)/(u_fogStart - u_fogEnd), 0.0, 1.0);\n"
"}\n"
;
