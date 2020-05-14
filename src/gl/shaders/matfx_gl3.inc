const char *matfx_env_vert_src =
"uniform mat4 u_texMatrix;\n"

"layout(location = 0) in vec3 in_pos;\n"
"layout(location = 1) in vec3 in_normal;\n"
"layout(location = 2) in vec4 in_color;\n"
"layout(location = 3) in vec2 in_tex0;\n"

"out vec4 v_color;\n"
"out vec2 v_tex0;\n"
"out vec2 v_tex1;\n"
"out float v_fog;\n"

"void\n"
"main(void)\n"
"{\n"
"	vec4 V = u_world * vec4(in_pos, 1.0);\n"
"	gl_Position = u_proj * u_view * V;\n"
"	vec3 N = mat3(u_world) * in_normal;\n"

"	v_tex0 = in_tex0;\n"
"	v_tex1 = (u_texMatrix * vec4(N, 1.0)).xy;\n"

"	v_color = in_color;\n"
"	v_color.rgb += u_ambLight.rgb*surfAmbient;\n"
"	v_color.rgb += DoDynamicLight(V.xyz, N)*surfDiffuse;\n"
"	v_color = clamp(v_color, 0.0f, 1.0);\n"
"	v_color *= u_matColor;\n"

"	v_fog = DoFog(gl_Position.w);\n"
"}\n"
;
const char *matfx_env_frag_src =
"uniform sampler2D tex0;\n"
"uniform sampler2D tex1;\n"

"uniform float u_coefficient;\n"
"uniform vec4 u_colorClamp;\n"

"in vec4 v_color;\n"
"in vec2 v_tex0;\n"
"in vec2 v_tex1;\n"
"in float v_fog;\n"

"out vec4 color;\n"

"void\n"
"main(void)\n"
"{\n"
"	vec4 pass1 = v_color;\n"
"	vec4 envColor = pass1;	// TODO: colorClamp\n"
"	pass1 *= texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));\n"

"	vec4 pass2 = envColor*u_coefficient*texture(tex1, vec2(v_tex1.x, 1.0-v_tex1.y));\n"

"	pass1.rgb = mix(u_fogColor.rgb, pass1.rgb, v_fog);\n"
"	pass2.rgb = mix(vec3(0.0, 0.0, 0.0), pass2.rgb, v_fog);\n"

"	color.rgb = pass1.rgb*pass1.a + pass2.rgb;\n"
"	color.a = pass1.a;\n"

"	DoAlphaTest(color.a);\n"
"}\n"
;
