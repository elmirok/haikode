## Genio - The Haiku IDE Makefile ##############################################
COMPILER_FLAGS = -Werror -std=c++20
WARNINGS = ALL
HAIKODE_AI_NETWORK ?= 1

TARGET_DIR := app
TYPE := APP

APP_MIME_SIG := "application/x-vnd.Haikode"

# Debug configuration
debug ?= 0
ifneq ($(debug), 0)
	DEBUGGER := TRUE
endif

# check if DEBUGGER is 1 or TRUE
ifeq ($(DEBUGGER),1)
	COMPILER_FLAGS += -gdwarf-3
	# for ASSERT()
	COMPILER_FLAGS += -DDEBUG=1
	# for scintilla
	COMPILER_FLAGS += -DGDEBUG
	NAME = Haikode_debug
else ifeq ($(DEBUGGER),TRUE)
	COMPILER_FLAGS += -gdwarf-3
	# for ASSERT()
	COMPILER_FLAGS += -DDEBUG=1
	# for scintilla
	COMPILER_FLAGS += -DGDEBUG
	NAME = Haikode_debug
else
	NAME := Haikode
endif

arch := $(shell getarch)
platform := $(shell uname -p)

# All directories under src
SRC_DIRS := $(shell find src -type d)

# All cpp files in SRC_DIRS
SRCS := $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.cpp))

RDEFS := Genio.rdef Spinner.rdef

LIBS  = be localestub game shared translation tracker $(STDCPPLIBS)
LIBS += columnlistview crypto editorconfig lsp git2 yaml-cpp
LIBS += libs/scintilla/bin/libscintilla.a

ifeq ($(HAIKODE_AI_NETWORK), 1)
	COMPILER_FLAGS += -DHAIKODE_AI_NETWORK=1
	LIBS += curl network
endif

PRIVATE_HEADERS = interface locale shared storage support tracker
SYSTEM_INCLUDE_PATHS  = $(foreach header,$(PRIVATE_HEADERS),$(shell findpaths -e B_FIND_PATH_HEADERS_DIRECTORY private/$(header)))
SYSTEM_INCLUDE_PATHS += $(shell findpaths -a $(platform) -e B_FIND_PATH_HEADERS_DIRECTORY lexilla)
SYSTEM_INCLUDE_PATHS += $(shell findpaths -a $(platform) -e B_FIND_PATH_HEADERS_DIRECTORY lsp)
SYSTEM_INCLUDE_PATHS += libs libs/json libs/scintilla/haiku libs/scintilla/include


## clang build flag ############################################################
BUILD_WITH_CLANG ?= 0
ifeq ($(BUILD_WITH_CLANG), 1)
	# clang build
	CC  := clang
	CXX := clang++
	LD  := clang++
else
	COMPILER_FLAGS += -Wno-maybe-uninitialized
endif


LOCALES := ca de en_AU en_GB en es_419 es fr fur it nb tr

## Include the Makefile-Engine
BUILDHOME := \
	$(shell findpaths -r "makefile_engine" B_FIND_PATH_DEVELOP_DIRECTORY)
include $(BUILDHOME)/etc/makefile-engine

# Rules to compile the resource definition files.
# Taken from makefile_engine and removed  CFLAGS because
# clang doesn't like if we pass -std=c++20 to it
$(OBJ_DIR)/%.rsrc : %.rdef
	cat $< | $(CC) -E $(INCLUDES) - | grep -av '^#' | $(RESCOMP) -I $(dir $<) -o "$@" -
$(OBJ_DIR)/%.rsrc : %.RDEF
	cat $< | $(CC) -E $(INCLUDES)  - | grep -av '^#' | $(RESCOMP) -I $(dir $<) -o "$@" -

deps:
	$(MAKE) -C libs/scintilla/haiku
	$(MAKE) -C libs/terminal -f Makefile.addon DEBUGGER=$(DEBUGGER)

.PHONY: clean cleanall deps

cleanall: clean
	$(MAKE) clean -C libs/scintilla/haiku
	$(MAKE) clean -C libs/terminal -f Makefile.addon
	rm -f txt2header
	rm -f Changelog.h

$(TARGET): deps

GenioApp.cpp : Changelog.h

Changelog.h : Changelog.txt txt2header
	txt2header < Changelog.txt > Changelog.h

txt2header :
	$(CXX) txt2header.cpp -o txt2header

compiledb:
	compiledb make -Bnwk
