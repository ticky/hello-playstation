# This is the binary we will ultimately produce
EE_BIN = hello-playstation.elf

# The object files which, combined, make up the binary above;
# their names will automatically map to correspondingly named C files
EE_OBJS = hello-playstation.o

# ps2sdk libraries we want to link to:
# - pad: for handling gamepad input
# - debug: so we can print text to the debug console
# - gskit: for fancy graphics
# - dmakit: required for gskit
EE_LIBS = -lpad -ldebug -lgskit -ldmakit

# Make sure to add gsKit to the include and lib paths
EE_INCS := -I$(GSKIT)/include $(EE_INCS)
EE_LDFLAGS := -L$(GSKIT)/lib $(EE_LDFLAGS)

# build the binary we've specified at the top
all: $(EE_BIN)

# send the binary over to the PS2, and run it
run: $(EE_BIN)
	ps2client execee host:$(EE_BIN)

# exit to ps2client (if the program hasn't crashed?)
reset:
	ps2client reset

# Note: "run" and "reset" each require `PS2HOSTNAME` to be set
# to your PS2's IP address or host name

# delete all the files generated by the build process
clean:
	rm -f *.elf *.o *.a

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
