# null, opengl
BUILD=null

# e.g. null -> -DRW_NULL
BUILDDEF:=$(shell echo $(BUILD) | tr a-z A-Z | sed 's/^/-DRW_/')
BUILDDIR=build-$(BUILD)
SRCDIR=src
#SRC := $(patsubst %.cpp,$(SRCDIR)/%.cpp, $(wildcard *.cpp))
SRC := $(wildcard $(SRCDIR)/*.cpp $(SRCDIR)/*/*.cpp)
OBJ := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SRC))
DEP := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.d,$(SRC))
INC := -I/usr/local/include
CFLAGS=-Wall -Wextra -g $(BUILDDEF) -fno-diagnostics-show-caret \
	-Wno-parentheses -Wno-invalid-offsetof \
	-Wno-unused-parameter -Wno-sign-compare
LIB=librw-$(BUILD).a

$(LIB): $(OBJ)
	ar scr $@ $(OBJ)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CFLAGS) $(INC) -c $< -o $@

$(BUILDDIR)/%.d: $(SRCDIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) -MM -MT '$(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$<)' $(CFLAGS) $(INC) $< > $@

clean:
	echo $(SRC)
	rm -rf $(BUILDDIR)/*

-include $(DEP)
