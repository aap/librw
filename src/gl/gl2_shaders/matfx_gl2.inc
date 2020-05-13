const char *matfx_env_vert_src =
"uniform mat4 u_texMatrix;\n"

"attribute vec3 in_pos;\n"
"attribute vec3 in_normal;\n"
"attribute vec4 in_color;\n"
"attribute vec2 in_tex0;\n"

"varying vec4 v_color;\n"
"varying vec2 v_tex0;\n"
"varying vec2 v_tex1;\n"
"varying float v_fog;\n"

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

"#ifdef DIRECTIONALS\n"
"	for(int i = 0; i < MAX_LIGHTS; i++){\n"
"		if(u_directLights[i].enabled == 0.0)\n"
"			break;\n"
"		v_color.rgb += DoDirLight(u_directLights[i], N)*surfDiffuse;\n"
"	}\n"
"#endif\n"
"#ifdef POINTLIGHTS\n"
"	for(int i = 0; i < MAX_LIGHTS; i++){\n"
"		if(u_pointLights[i].enabled == 0.0)\n"
"			break;\n"
"		v_color.rgb += DoPointLight(u_pointLights[i], V.xyz, N)*surfDiffuse;\n"
"	}\n"
"#endif\n"
"#ifdef SPOTLIGHTS\n"
"	for(int i = 0; i < MAX_LIGHTS; i++){\n"
"		if(u_spotLights[i].enabled == 0.0)\n"
"			break;\n"
"		v_color.rgb += DoSpotLight(u_spotLights[i], V.xyz, N)*surfDiffuse;\n"
"	}\n"
"#endif\n"

"	v_color = clamp(v_color, 0.0f, 1.0);\n"
"	v_color *= u_matColor;\n"

"	v_fog = DoFog(gl_Position.w);\n"
"}\n"
;
const char *matfx_env_frag_src =
"uniform vec2 u_alphaRef;\n"

"uniform float u_fogStart;\n"
"uniform float u_fogEnd;\n"
"uniform float u_fogRange;\n"
"uniform float u_fogDisable;\n"
"uniform vec4  u_fogColor;\n"

"uniform sampler2D tex0;\n"
"uniform sampler2D tex1;\n"

"uniform float u_coefficient;\n"
"uniform vec4 u_colorClamp;\n"

"varying vec4 v_color;\n"
"varying vec2 v_tex0;\n"
"varying vec2 v_tex1;\n"
"varying float v_fog;\n"

"void\n"
"main(void)\n"
"{\n"
"	vec4 color;\n"

"	vec4 pass1 = v_color;\n"
"	vec4 envColor = pass1;	// TODO: colorClamp\n"
"	pass1 *= texture2D(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));\n"

"	vec4 pass2 = envColor*u_coefficient*texture2D(tex1, vec2(v_tex1.x, 1.0-v_tex1.y));\n"

"	pass1.rgb = mix(u_fogColor.rgb, pass1.rgb, v_fog);\n"
"	pass2.rgb = mix(vec3(0.0, 0.0, 0.0), pass2.rgb, v_fog);\n"

"	color.rgb = pass1.rgb*pass1.a + pass2.rgb;\n"
"	color.a = pass1.a;\n"

"	if(color.a < u_alphaRef.x || color.a >= u_alphaRef.y)\n"
"		discard;\n"
"/*\n"
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
"*/\n"

"	gl_FragColor = color;\n"
"}\n"
;
