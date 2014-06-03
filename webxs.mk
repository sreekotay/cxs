CC   := gcc
CCP  := g++
LD	 := g++
RM	 := rm

# The output executable.
BIN   := webxs

# Toolchain arguments.
CCFLAGS   := -Os
//CCFLAGS   := -g -ggdb
CCPLAGS   := $(CCFLAGS)

ifndef OS

UNAME=$(shell uname)
OS=$(UNAME)

#$(info OS = $(OS))
endif

ifeq ($(OS),Windows_NT)
	LDFLAGS = -lpthread -lws2_32 -lcomdlg32 -static-libgcc
else ifeq ($(OS),Darwin)
	CCFLAGS   := $(CCFLAGS) -Wunused-value
	LDFLAGS   :=  -lpthread -ldl -flto
else
	LDFLAGS   :=  -lpthread -ldl -lrt -flto
	//LDFLAGS   := -lpthread -ldl -lrt -g -ggdb
endif

ifneq ($(OS),Darwin)
  DEADCODESTRIP := -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,-s
else
  DEADCODESTRIP := -fdata-sections -ffunction-sections
endif

# Project sources.
C_SOURCE_FILES := webxs.c
//CPP_SOURCE_FILES := webxs_core.cpp
C_OBJECT_FILES := $(patsubst %.c,%.o,$(C_SOURCE_FILES))
CPP_OBJECT_FILES := $(patsubst %.cpp,%.o,$(CPP_SOURCE_FILES))

# The dependency file names.
DEPS := $(C_OBJECT_FILES:.o=.d), $(CPP_OBJECT_FILES:.o=.d)

all: $(BIN)

clean:  
	rm -f $(C_OBJECT_FILES) $(CPP_OBJECT_FILES) $(DEPS) $(BIN)

rebuild: clean all

$(BIN): $(C_OBJECT_FILES) $(CPP_OBJECT_FILES) 
	$(LD) $(C_OBJECT_FILES) $(CPP_OBJECT_FILES) $(DEADCODESTRIP) $(LDFLAGS) -o $@

%.o: %.c 
	$(CC) -c -MMD -MP $< -o $@ $(DEADCODESTRIP) $(CCFLAGS) 

%.o: %.cpp 
	$(CCP) -c -MMD -MP $< -o $@ $(DEADCODESTRIP) $(CCPLAGS)

# Let make read the dependency files and handle them.
-include $(DEPS)