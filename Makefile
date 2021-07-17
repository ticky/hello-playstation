EE_BIN = hello-playstation.elf
EE_OBJS = hello-playstation.o

# We want the debug library so we can print text to the debug console
EE_LIBS = -lpad -ldebug -lgskit -ldmakit

# Include gsKit
EE_INCS := -I$(GSKIT)/include $(EE_INCS)
EE_LDFLAGS := -L$(GSKIT)/lib $(EE_LDFLAGS)

all: $(EE_BIN)

run: $(EE_BIN)
	ps2client execee host:$(EE_BIN)

reset:
	ps2client reset

clean:
	rm -f *.elf *.o *.a

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
