all: im2d_gl3.inc im3d_gl3.inc simple_vs_gl3.inc simple_fs_gl3.inc matfx_gl3.inc skin_gl3.inc

im2d_gl3.inc: im2d.vert
	(echo 'const char *im2d_vert_src =';\
	 sed 's/..*/"&\\n"/' im2d.vert;\
	 echo ';') >im2d_gl3.inc

im3d_gl3.inc: im3d.vert
	(echo 'const char *im3d_vert_src =';\
	 sed 's/..*/"&\\n"/' im3d.vert;\
	 echo ';') >im3d_gl3.inc

simple_vs_gl3.inc: simple.vert
	(echo 'const char *simple_vert_src =';\
	 sed 's/..*/"&\\n"/' simple.vert;\
	 echo ';') >simple_vs_gl3.inc

simple_fs_gl3.inc: simple.frag
	(echo 'const char *simple_frag_src =';\
	 sed 's/..*/"&\\n"/' simple.frag;\
	 echo ';') >simple_fs_gl3.inc

matfx_gl3.inc: matfx_env.frag matfx_env.vert
	(echo 'const char *matfx_env_vert_src =';\
	 sed 's/..*/"&\\n"/' matfx_env.vert;\
	 echo ';';\
	 echo 'const char *matfx_env_frag_src =';\
	 sed 's/..*/"&\\n"/' matfx_env.frag;\
	 echo ';') >matfx_gl3.inc

skin_gl3.inc: skin.vert
	(echo 'const char *skin_vert_src =';\
	 sed 's/..*/"&\\n"/' skin.vert;\
	 echo ';') >skin_gl3.inc

