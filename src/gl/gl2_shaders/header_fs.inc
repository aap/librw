const char *header_frag_src =
"uniform vec2 u_alphaRef;\n"

"uniform vec4  u_fogData;\n"
"uniform vec4  u_fogColor;\n"

"#define u_fogStart (u_fogData.x)\n"
"#define u_fogEnd (u_fogData.y)\n"
"#define u_fogRange (u_fogData.z)\n"
"#define u_fogDisable (u_fogData.w)\n"

"void DoAlphaTest(float a)\n"
"{\n"
"	if(a < u_alphaRef.x || a >= u_alphaRef.y)\n"
"		discard;\n"
"}\n"
;
