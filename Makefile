DISTRIBUTABLES += $(wildcard LICENSE*) res
RACK_DIR ?= ../..

SHELL:=/bin/bash -O extglob

FLAGS = \
    -Wno-deprecated-declarations \
	-O0 \
	-Werror=implicit-function-declaration \
	-Isrc \
	-Ilib/oscpack \

SOURCES = \
	$(wildcard lib/oscpack/ip/*.cpp) \
	$(wildcard lib/oscpack/osc/*.cpp) \
	$(wildcard src/*.cpp) \

include ../../arch.mk

ifeq ($(ARCH), win)
	SOURCES += $(wildcard lib/oscpack/ip/win32/*.cpp) 
	LDFLAGS += -lws2_32 -lwinmm
else
	SOURCES += $(wildcard lib/oscpack/ip/posix/*.cpp) 
endif

FLAGS := $(filter-out -MMD,$(FLAGS))

include $(RACK_DIR)/plugin.mk
