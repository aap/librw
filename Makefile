# null, gl3, ps2, d3d9
BUILD := null
TARGET := librw-$(BUILD).a
CFLAGS := -Wall -Wextra -g -fno-diagnostics-show-caret \
	-Wno-parentheses -Wno-invalid-offsetof \
	-Wno-unused-parameter -Wno-sign-compare

include Make.common

$(TARGET): $(OBJ)
	ar scr $@ $(OBJ)
