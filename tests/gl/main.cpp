#include "rwtest.h"

using namespace std;

int screenWidth = 640, screenHeight= 480;
bool running = true;
Camera *camera;

GLint program;
GLuint vbo;

Rw::Clump *clump;

char *filename;

void
renderAtomic(Rw::Atomic *atomic)
{
	using namespace Rw;
	static GLenum prim[] = {
		GL_TRIANGLES, GL_TRIANGLE_STRIP
	};
	Geometry *geo = atomic->geometry;
	if(!(geo->geoflags & Geometry::NATIVE))
		Gl::Instance(atomic);
	Gl::InstanceDataHeader *inst = (Gl::InstanceDataHeader*)geo->instData;
	MeshHeader *meshHeader = geo->meshHeader;

	Frame *frm = atomic->frame;
	frm->updateLTM();
	glUniformMatrix4fv(glGetUniformLocation(program, "worldMat"),
	                   1, GL_FALSE, frm->ltm);

	glVertexAttrib4f(3, 1.0f, 1.0f, 1.0f, 1.0f);
	if(inst->vbo == 0 && inst->ibo == 0)
		Gl::UploadGeo(geo);
	glBindBuffer(GL_ARRAY_BUFFER, inst->vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, inst->ibo);
	Gl::SetAttribPointers(inst);

	uint64 offset = 0;
	for(uint32 i = 0; i < meshHeader->numMeshes; i++){
		Mesh *mesh = &meshHeader->mesh[i];
		glDrawElements(prim[meshHeader->flags], mesh->numIndices,
		               GL_UNSIGNED_SHORT, (void*)offset);
		offset += mesh->numIndices*2;
	}
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(2);
	glDisableVertexAttribArray(3);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void
render(void)
{
	glUseProgram(program);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	camera->look();
	glUniformMatrix4fv(glGetUniformLocation(program, "projMat"),
	                   1, GL_FALSE, camera->projMat.cr);
	glUniformMatrix4fv(glGetUniformLocation(program, "viewMat"),
	                   1, GL_FALSE, camera->viewMat.cr);

	glVertexAttrib3f(2, -0.5f, 0.5f, 0.70710f);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 28, (GLvoid*)0);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 28, (GLvoid*)12);
	glDrawArrays(GL_LINES, 0, 6);
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(3);

	for(Rw::uint32 i = 0; i < clump->numAtomics; i++){
		char *name = PLUGINOFFSET(char, clump->atomicList[i]->frame,
		                          Rw::NodeNameOffset);
		if(strstr(name, "_dam") || strstr(name, "_vlo"))
			continue;
		renderAtomic(clump->atomicList[i]);
	}
}

void
init(void)
{
	glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
	glEnable(GL_DEPTH_TEST);
	const char *shadersrc =
		"#version 120\n"
		"#ifdef VERTEX\n"
		"uniform mat4 projMat;"
		"uniform mat4 viewMat;"
		"uniform mat4 worldMat;"
		"attribute vec3 in_vertex;"
		"attribute vec3 in_texColor;"
		"attribute vec3 in_normal;"
		"attribute vec4 in_color;"
		"varying vec4 v_color;"
		"vec3 lightdir = vec3(0.5, -0.5, -0.70710);"
		"void main()"
		"{"
		"	gl_Position = projMat * viewMat * worldMat * vec4(in_vertex, 1.0);"
		"	vec3 n = mat3(worldMat) * in_normal;"
		"	float l = max(0.0, dot(n, -lightdir));"
		"	v_color = in_color*l;"
		"}\n"
		"#endif\n"
		"#ifdef FRAGMENT\n"
		"varying vec4 v_color;"
		"void main()"
		"{"
		"	gl_FragColor = v_color;"
		"}\n"
		"#endif\n";
	const char *srcarr[] = { "#define VERTEX", shadersrc };
	GLint vertshader = gl::compileShader(srcarr, 2, GL_VERTEX_SHADER);
	assert(vertshader != 0);
	srcarr[0] = "#define FRAGMENT";
	GLint fragshader = gl::compileShader(srcarr, 2, GL_FRAGMENT_SHADER);
	assert(fragshader != 0);
	program = gl::linkProgram(vertshader, fragshader);
	assert(program != 0);

	GLfloat vertarray[] = {
		0.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f, 1.0f,

		0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 1.0f,

		0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
	};
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertarray),
	             vertarray, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	camera = new Camera;
	camera->setAspectRatio(1.0f*screenWidth/screenHeight);
	camera->setNearFar(0.1f, 450.0f);
	camera->setTarget(Vec3(0.0f, 0.0f, 0.0f));
	camera->setPosition(Vec3(0.0f, 5.0f, 0.0f));

	Rw::RegisterMaterialRightsPlugin();
	Rw::RegisterMatFXPlugin();
	Rw::RegisterAtomicRightsPlugin();
	Rw::RegisterHAnimPlugin();
	Rw::RegisterNodeNamePlugin();
	Rw::RegisterBreakableModelPlugin();
	Rw::RegisterExtraVertColorPlugin();
	Rw::Ps2::RegisterADCPlugin();
	Rw::RegisterSkinPlugin();
	Rw::RegisterNativeDataPlugin();
//      Rw::Ps2::RegisterNativeDataPlugin();
	Rw::RegisterMeshPlugin();

	Rw::StreamFile in;
//	in.open("player-vc-ogl.dff", "rb");
//	in.open("player-iii.dff", "rb");
//	in.open("admiral-ogl.dff", "rb");
	in.open(filename, "rb");
	Rw::FindChunk(&in, Rw::ID_CLUMP, NULL, NULL);
	clump = Rw::Clump::streamRead(&in);
	assert(clump);
	in.close();
}

void
keypress(GLFWwindow *w, int key, int scancode, int action, int mods)
{
	if(action != GLFW_PRESS)
		return;

	switch(key){
	case 'Q':
	case GLFW_KEY_ESCAPE:
		running = false;
		break;
	}
}

static int lastX, lastY;
static int clickX, clickY;
static bool isLDown, isMDown, isRDown;
static bool isShiftDown, isCtrlDown, isAltDown;

void
mouseButton(GLFWwindow *w, int button, int action, int mods)
{
	double x, y;
	glfwGetCursorPos(w, &x, &y);
	if(action == GLFW_PRESS){
		lastX = clickX = x;
		lastY = clickY = y;
		if(button == GLFW_MOUSE_BUTTON_LEFT)
			isLDown = true;
		if(button == GLFW_MOUSE_BUTTON_MIDDLE)
			isMDown = true;
		if(button == GLFW_MOUSE_BUTTON_RIGHT)
			isRDown = true;
	}else if(action == GLFW_RELEASE){
		if(button == GLFW_MOUSE_BUTTON_LEFT)
			isLDown = false;
		if(button == GLFW_MOUSE_BUTTON_MIDDLE)
			isMDown = false;
		if(button == GLFW_MOUSE_BUTTON_RIGHT)
			isRDown = false;
	}
}

void
mouseMotion(GLFWwindow *w, double x, double y)
{
	GLfloat dx, dy;
	static int xoff = 0, yoff = 0;
	static bool wrappedLast = false;
	int width, height;

	glfwGetWindowSize(w, &width, &height);

	dx = float(lastX - x) / float(width);
	dy = float(lastY - y) / float(height);
	/* Wrap the mouse if it goes over the window border.
	 * Unfortunately, after glfwSetMousePos is done, there can be old
	 * events with an old mouse position,
	 * hence the check if the pointer was wrapped the last time. */
	if((isLDown || isMDown || isRDown) &&
	   (x < 0 || y < 0 || x >= width || y >= height)){
		if(wrappedLast){
			dx = float(lastX-xoff - x) / float(width);
			dy = float(lastY-yoff - y) / float(height);
		}
		xoff = yoff = 0;
		while (x+xoff >= width) xoff -= width;
		while (y+yoff >= height) yoff -= height;
		while (x+xoff < 0) xoff += width;
		while (y+yoff < 0) yoff += height;
		glfwSetCursorPos(w, x+xoff, y+yoff);
		wrappedLast = true;
	}else{
		wrappedLast = false;
		xoff = yoff = 0;
	}
	lastX = x+xoff;
	lastY = y+yoff;
	if(isLDown){
		if(isShiftDown)
			camera->turn(dx*2.0f, dy*2.0f);
		else
			camera->orbit(dx*2.0f, -dy*2.0f);
	}
	if(isMDown){
		if(isShiftDown)
			;
		else
			camera->pan(dx*8.0f, -dy*8.0f);
	}
	if(isRDown){
		if(isShiftDown)
			;
		else
			camera->zoom(dx*12.0f);
	}
}

void
resize(GLFWwindow*, int width, int height)
{
	screenWidth = width;
	screenHeight = height;
	glViewport(0, 0, screenWidth, screenHeight);
	camera->setAspectRatio(1.0f*screenWidth/screenHeight);
}

void
closewindow(GLFWwindow*)
{
	running = false;
}

int
main(int argc, char *argv[])
{
	if(argc < 2)
		return 1;
	filename = argv[1];

	if(!glfwInit()){
		fprintf(stderr, "Error: could not initialize GLFW\n");
		return 1;
	}
	GLFWwindow *window = glfwCreateWindow(screenWidth, screenHeight,
	                                      "OpenGL", 0, 0);
	if(!window){
		fprintf(stderr, "Error: could not create GLFW window\n");
		glfwTerminate();
		return 1;
	}
	glfwMakeContextCurrent(window);
	GLenum status = glewInit();
	if(status != GLEW_OK){
		fprintf(stderr, "Error: %s\n", glewGetErrorString(status));
		return 1;
	}
	if(!GLEW_VERSION_2_0){
		fprintf(stderr, "Error: OpenGL 2.0 needed\n");
		return 1;
	}

	init();

	glfwSetWindowSizeCallback(window, resize);
	glfwSetWindowCloseCallback(window, closewindow);
	glfwSetMouseButtonCallback(window, mouseButton);
	glfwSetCursorPosCallback(window, mouseMotion);
	glfwSetKeyCallback(window, keypress);
	while(running){
		glfwPollEvents();
		isShiftDown = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
		isCtrlDown = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
		isAltDown = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS;

		render();
		glfwSwapBuffers(window);
	}

	glfwTerminate();
	return 0;
}
