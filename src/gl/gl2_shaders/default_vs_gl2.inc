const char *default_vert_src =
"attribute vec3 in_pos;\n"
"attribute vec3 in_normal;\n"
"attribute vec4 in_color;\n"
"attribute vec2 in_tex0;\n"

"varying vec4 v_color;\n"
"varying vec2 v_tex0;\n"
"varying float v_fog;\n"

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
