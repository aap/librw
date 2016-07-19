const char *matfx_env_vert_src =
"#version 330\n"

"layout(std140) uniform Scene\n"
"{\n"
"	mat4 u_proj;\n"
"	mat4 u_view;\n"
"};\n"

"#define MAX_LIGHTS 8\n"
"struct Light {\n"
"	vec4  position;\n"
"	vec4  direction;\n"
"	vec4  color;\n"
"	float radius;\n"
"	float minusCosAngle;\n"
"};\n"

"layout(std140) uniform Object\n"
"{\n"
"	mat4  u_world;\n"
"	vec4  u_ambLight;\n"
"	int   u_numLights;\n"
"	Light u_lights[MAX_LIGHTS];\n"
"};\n"

"uniform vec4 u_matColor;\n"
"uniform vec4 u_surfaceProps;	// amb, spec, diff, extra\n"

"uniform mat4 u_texMatrix;\n"
"uniform float u_coefficient;\n"

"layout(location = 0) in vec3 in_pos;\n"
"layout(location = 1) in vec3 in_normal;\n"
"layout(location = 2) in vec4 in_color;\n"
"layout(location = 3) in vec2 in_tex0;\n"

"out vec4 v_color;\n"
"out vec2 v_tex0;\n"

"void\n"
"main(void)\n"
"{\n"
"	vec4 V = u_world * vec4(in_pos, 1.0);\n"
"	gl_Position = u_proj * u_view * V;\n"
"	vec3 N = mat3(u_world) * in_normal;\n"

"	v_color = in_color;\n"
"	for(int i = 0; i < u_numLights; i++){\n"
"		float L = max(0.0, dot(N, -normalize(u_lights[i].direction.xyz)));\n"
"		v_color.rgb += u_lights[i].color.rgb*L*u_surfaceProps.z;\n"
"	}\n"
"	v_color.rgb += u_ambLight.rgb*u_surfaceProps.x;\n"
"	v_color *= u_matColor;\n"

"	v_color *= u_coefficient;\n"
"	vec3 cN = mat3(u_view) * in_normal;\n"
"	v_tex0 = (u_texMatrix * vec4(cN, 1.0)).xy;\n"
"}\n"
;
const char *matfx_env_frag_src =
"#version 330\n"

"layout(std140) uniform State\n"
"{\n"
"	int   u_alphaTest;\n"
"	float u_alphaRef;\n"
"	// fog etc.\n"
"};\n"

"uniform sampler2D tex;\n"

"in vec4 v_color;\n"
"in vec2 v_tex0;\n"

"out vec4 color;\n"

"void\n"
"main(void)\n"
"{\n"
"	color = v_color*texture2D(tex, vec2(v_tex0.x, v_tex0.y));\n"
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
"}\n"

;
