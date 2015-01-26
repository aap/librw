CC=ee-gcc
CXX=ee-g++
AS=ee-g++
LD=ee-g++
DVPAS=ee-dvp-as

LINK=-T$(PS2SDK)/ee/startup/linkfile
LIBPATH=-L$(PS2SDK)/ee/lib
INCPATH=-I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I../..
LIBS=../../librw-ps2.a -lc -lc -lkernel -lmf # g++ throws one -lc away, why? (unless -nostdlib)

CFLAGS = -c -Wall -nostdlib -fno-common -DPS2_EE $(INCPATH)
ASFLAGS = -c -xassembler-with-cpp
LDFLAGS = -mno-crt0 $(LIBPATH)
OUT=test

C_SRC=main.cpp gs.cpp dma.cpp math.cpp mesh.cpp
#S_SRC=crt0.s low.s
HEADER=dma.h  ee_regs.h  gif.h  gs.h  mips_regs.h  ps2.h math.h mesh.h
OBJ=$(C_SRC:.cpp=.o) $(S_SRC:.s=.o) vu.o defaultpipe.o skinpipe.o

$(OUT).elf: $(OBJ) $(HEADER)
	$(LD) $(LDFLAGS) $(LINK) $(PS2SDK)/ee/startup/crt0.o \
	 $(OBJ) $(LIBS) -o $(OUT).elf

.cpp.o: $(HEADER)
	$(CXX) $(CFLAGS) $< -o $@

.s.o: $(HEADER)
	$(AS) $(ASFLAGS) $< -o $@

%.o: %.dsm
	$(DVPAS) $< -o $@

$(C_SRC) $(S_SRC): $(HEADER)

clean:
	rm -f $(OBJ)

