const char *im3d_vert_src =
"layout(location = 0) in vec3 in_pos;\n"
"layout(location = 2) in vec4 in_color;\n"
"layout(location = 3) in vec2 in_tex0;\n"

"out vec4 v_color;\n"
"out vec2 v_tex0;\n"
"out float v_fog;\n"

"void\n"
"main(void)\n"
"{\n"
"	vec4 Vertex = u_world * vec4(in_pos, 1.0);\n"
"	vec4 CamVertex = u_view * Vertex;\n"
"	gl_Position = u_proj * CamVertex;\n"
"	v_color = in_color;\n"
"	v_tex0 = in_tex0;\n"
"	v_fog = DoFog(gl_Position.w);\n"
"}\n"
;
