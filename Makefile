SRCDIR=src
BUILDDIR=build
SRC := $(patsubst %.cpp,$(SRCDIR)/%.cpp, rwbase.cpp clump.cpp\
                                         geometry.cpp plugins.cpp ps2.cpp\
                                         ogl.cpp)
OBJ := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SRC))
OBJ := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.d,$(SRC))
CFLAGS=-Wall -Wextra -g #-Wno-parentheses #-Wconversion

librw.a: $(OBJ)
	ar scr $@ $(OBJ)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%.d: $(SRCDIR)/%.cpp
	$(CXX) -MM -MT '$(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$<)' $(CFLAGS) $< > $@

-include $(DEP)
