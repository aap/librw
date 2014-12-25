# null, opengl
BUILD=null

# e.g. null -> -DRW_NULL
BUILDDEF:=$(shell echo $(BUILD) | tr a-z A-Z | sed 's/^/-DRW_/')
BUILDDIR=build-$(BUILD)
SRCDIR=src
SRC := $(patsubst %.cpp,$(SRCDIR)/%.cpp, rwbase.cpp clump.cpp\
                                         geometry.cpp plugins.cpp ps2.cpp\
                                         ogl.cpp image.cpp)
OBJ := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SRC))
DEP := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.d,$(SRC))
CFLAGS=-Wall -Wextra -g $(BUILDDEF) #-Wno-parentheses #-Wconversion
LIB=librw-$(BUILD).a

$(LIB): $(OBJ)
	ar scr $@ $(OBJ)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%.d: $(SRCDIR)/%.cpp
	$(CXX) -MM -MT '$(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$<)' $(CFLAGS) $< > $@

clean:
	rm -f $(BUILDDIR)/*.[od]

-include $(DEP)
