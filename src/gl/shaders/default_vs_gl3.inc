const char *default_vert_src =
"layout(location = 0) in vec3 in_pos;\n"
"layout(location = 1) in vec3 in_normal;\n"
"layout(location = 2) in vec4 in_color;\n"
"layout(location = 3) in vec2 in_tex0;\n"

"out vec4 v_color;\n"
"out vec2 v_tex0;\n"
"out float v_fog;\n"

"void\n"
"main(void)\n"
"{\n"
"	vec4 V = u_world * vec4(in_pos, 1.0);\n"
"	gl_Position = u_proj * u_view * V;\n"
"	vec3 N = mat3(u_world) * in_normal;\n"

"	v_tex0 = in_tex0;\n"

"	v_color = in_color;\n"
"	v_color.rgb += u_ambLight.rgb*surfAmbient;\n"
"	v_color.rgb += DoDynamicLight(V.xyz, N)*surfDiffuse;\n"
"	v_color = clamp(v_color, 0.0f, 1.0);\n"
"	v_color *= u_matColor;\n"

"	v_fog = DoFog(gl_Position.w);\n"
"}\n"
;
