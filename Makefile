EE_BIN = hello-playstation.elf
EE_OBJS = hello-playstation.o

# We want the debug library so we can print text to the debug console
EE_LIBS = -ldebug

all: $(EE_BIN)

clean:
	rm -f *.elf *.o *.a

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal