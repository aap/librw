BUILDDIR = build
SRC = main.cpp glshader.cpp math.cpp camera.cpp
OBJ = $(patsubst %.cpp, $(BUILDDIR)/%.o, $(SRC))
DEP = $(patsubst %.cpp, $(BUILDDIR)/%.d, $(SRC))
RWDIR=$(HOME)/src/librw
LDLIBS=-pthread -lX11 -lXrandr -lXi -lXxf86vm -lGL -lGLEW -lm
CFLAGS=-g -I$(RWDIR) -Wall -Wextra

rwtest: $(OBJ) $(RWDIR)/librw-opengl.a /usr/local/lib/libglfw3.a
	$(CXX) $^ $(LDLIBS) -o $@

$(BUILDDIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%.d: %.cpp
	@mkdir -p $(@D)
	$(CXX) -MM -MT '$(patsubst %.cpp,$(BUILDDIR)%.o,$<)' $(CFLAGS) $< > $@

clean:
	rm $(BUILDDIR)/* rwtest

-include $(DEP)
