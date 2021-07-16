EE_BIN = hello-playstation.elf
EE_OBJS = hello-playstation.o version.o

# We want the debug library so we can print text to the debug console
EE_LIBS = -lpad -ldebug -lgskit -ldmakit

# Include gsKit
EE_INCS := -I$(GSKIT)/include $(EE_INCS)
EE_LDFLAGS := -L$(GSKIT)/lib $(EE_LDFLAGS)

all: $(EE_BIN)

version.c:
	echo "const char *git_version = \"$(shell git describe --tags --always --dirty)\";\nconst char *build_date = \"$(shell date -u +"%Y-%m-%dT%H:%M:%SZ")\";" > $@

clean:
	rm -f $(EE_BIN) $(EE_OBJS) version.c

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
