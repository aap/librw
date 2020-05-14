const char *header_frag_src =
"uniform vec2 u_alphaRef;\n"

"uniform float u_fogStart;\n"
"uniform float u_fogEnd;\n"
"uniform float u_fogRange;\n"
"uniform float u_fogDisable;\n"
"uniform vec4  u_fogColor;\n"

"void DoAlphaTest(float a)\n"
"{\n"
"	if(a < u_alphaRef.x || a >= u_alphaRef.y)\n"
"		discard;\n"
"}\n"
;
