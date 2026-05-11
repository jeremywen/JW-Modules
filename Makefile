OSC_ON=1

DISTRIBUTABLES += $(wildcard LICENSE*) res
RACK_DIR ?= ../..

SHELL:=/bin/bash -O extglob

MACHINE = $(shell $(CC) -dumpmachine)
ifdef OSC_ON
	FLAGS = \
		-DOSC_ON=$(OSC_ON) \
		-Wno-deprecated-declarations \
		-O0 \
		-Werror=implicit-function-declaration \
		-Isrc \
		-Ilib/oscpack
	SOURCES = \
		$(wildcard lib/oscpack/ip/*.cpp) \
		$(wildcard lib/oscpack/osc/*.cpp) \
		$(wildcard src/*.cpp)
	ifneq (, $(findstring mingw, $(MACHINE)))
		SOURCES += $(wildcard lib/oscpack/ip/win32/*.cpp) 
		LDFLAGS += -lws2_32 -lwinmm
	else
		SOURCES += $(wildcard lib/oscpack/ip/posix/*.cpp) 
	endif
endif
ifndef OSC_ON
	FLAGS = \
		-Wno-deprecated-declarations \
		-O0 \
		-Werror=implicit-function-declaration \
		-Isrc
	SOURCES = \
		$(wildcard src/*.cpp)
	ifneq (, $(findstring mingw, $(MACHINE)))
		LDFLAGS += -lws2_32 -lwinmm
	endif
endif

FLAGS := $(filter-out -MMD,$(FLAGS))

include $(RACK_DIR)/plugin.mk
