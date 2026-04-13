## Genio - The Haiku IDE Makefile ##############################################
COMPILER_FLAGS = -Werror -std=c++20
WARNINGS = ALL

TARGET_DIR := app
TYPE := APP

APP_MIME_SIG := "application/x-vnd.Genio"

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
	NAME = Genio_debug
else ifeq ($(DEBUGGER),TRUE)
	COMPILER_FLAGS += -gdwarf-3
	# for ASSERT()
	COMPILER_FLAGS += -DDEBUG=1
	# for scintilla
	COMPILER_FLAGS += -DGDEBUG
	NAME = Genio_debug
else
	NAME := Genio
endif

arch := $(shell getarch)
platform := $(shell uname -p)

# All directories under src
SRC_DIRS := $(shell find src -type d)

# All cpp files in SRC_DIRS
SRCS := $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.cpp))

RDEFS := Genio.rdef Spinner.rdef

LIBS  = be shared translation tracker game localestub $(STDCPPLIBS)
LIBS += columnlistview
LIBS += editorconfig
LIBS += git2
LIBS += libs/scintilla/bin/libscintilla.a
LIBS += yaml-cpp
LIBS += lsp

SYSTEM_INCLUDE_PATHS  = $(shell findpaths -e B_FIND_PATH_HEADERS_DIRECTORY private/interface)
SYSTEM_INCLUDE_PATHS += $(shell findpaths -e B_FIND_PATH_HEADERS_DIRECTORY private/shared)
SYSTEM_INCLUDE_PATHS += $(shell findpaths -e B_FIND_PATH_HEADERS_DIRECTORY private/storage)
SYSTEM_INCLUDE_PATHS += $(shell findpaths -e B_FIND_PATH_HEADERS_DIRECTORY private/support)
SYSTEM_INCLUDE_PATHS += $(shell findpaths -e B_FIND_PATH_HEADERS_DIRECTORY private/tracker)
SYSTEM_INCLUDE_PATHS += $(shell findpaths -e B_FIND_PATH_HEADERS_DIRECTORY private/locale)
SYSTEM_INCLUDE_PATHS += $(shell findpaths -a $(platform) -e B_FIND_PATH_HEADERS_DIRECTORY lexilla)
SYSTEM_INCLUDE_PATHS += $(shell findpaths -e B_FIND_PATH_HEADERS_DIRECTORY lsp)
SYSTEM_INCLUDE_PATHS += libs
SYSTEM_INCLUDE_PATHS += libs/json
SYSTEM_INCLUDE_PATHS += libs/scintilla/haiku
SYSTEM_INCLUDE_PATHS += libs/scintilla/include


## clang build flag ############################################################
BUILD_WITH_CLANG ?= 0
################################################################################
ifeq ($(BUILD_WITH_CLANG), 1)
	# clang build
	CC  := clang
	CXX := clang++
	LD  := clang++
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

.PHONY: clean deps

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
