# null, opengl
BUILD=null

# e.g. null -> -DRW_NULL
BUILDDEF:=$(shell echo $(BUILD) | tr a-z A-Z | sed 's/^/-DRW_/')
SRC := insttest.cpp
OUT := insttest
INC := -I/usr/local/include -I../..
CFLAGS=-Wall -Wextra -g $(BUILDDEF) -Wno-parentheses -Wno-write-strings #-Wconversion
LIB=../../librw-$(BUILD).a

$(OUT): $(SRC) $(LIB)
	$(CXX) $(CFLAGS) $(INC) $(SRC) $(LIB) -o $(OUT)

clean:
	rm -f $(OUT)
