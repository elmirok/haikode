APP := Haikode
SRC_DIR := src
BUILD_DIR := build
TEST_DIR := tests
AI_NETWORK ?= 0
CODEX_BRIDGE ?= 0

CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
CPPFLAGS += -I$(SRC_DIR)
LIBS := -lbe -ltracker

CORE_SRCS := $(filter-out \
	$(SRC_DIR)/core/ChatClient.cpp \
	$(SRC_DIR)/core/OAuthCallbackServer.cpp \
	$(SRC_DIR)/core/OAuthHttpClient.cpp \
	$(SRC_DIR)/core/OpenAICompatibleProvider.cpp, \
	$(wildcard $(SRC_DIR)/core/*.cpp))
NETWORK_SRCS := \
	$(SRC_DIR)/core/ChatClient.cpp \
	$(SRC_DIR)/core/OAuthCallbackServer.cpp \
	$(SRC_DIR)/core/OAuthHttpClient.cpp \
	$(SRC_DIR)/core/OpenAICompatibleProvider.cpp

ifeq ($(AI_NETWORK),1)
	PKG_CFLAGS := $(shell pkg-config --cflags libcurl jsoncpp openssl 2>/dev/null)
	PKG_LIBS := $(shell pkg-config --libs libcurl jsoncpp openssl 2>/dev/null)
	CPPFLAGS += -DHAIKODE_AI_NETWORK=1 $(PKG_CFLAGS)
	EXTERNAL_LIBS := $(if $(PKG_LIBS),$(PKG_LIBS),-lcurl -ljsoncpp -lssl -lcrypto)
	LIBS += $(EXTERNAL_LIBS)
	LIBS += -lnetwork
	CORE_SRCS += $(NETWORK_SRCS)
endif

ifeq ($(CODEX_BRIDGE),1)
	CPPFLAGS += -DHAIKODE_CODEX_BRIDGE=1
endif

APP_SRCS := $(wildcard $(SRC_DIR)/*.cpp) $(CORE_SRCS)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(APP_SRCS))
TARGET := $(BUILD_DIR)/$(APP)
TEST_TARGET := $(BUILD_DIR)/core_tests
TEST_SRCS := $(TEST_DIR)/core_tests.cpp \
	$(SRC_DIR)/core/CodexBridge.cpp \
	$(SRC_DIR)/core/CommandPolicy.cpp \
	$(SRC_DIR)/core/CommandSafety.cpp \
	$(SRC_DIR)/core/Crypto.cpp \
	$(SRC_DIR)/core/DiffProposal.cpp \
	$(SRC_DIR)/core/IgnoreRules.cpp \
	$(SRC_DIR)/core/OAuthClient.cpp \
	$(SRC_DIR)/core/PatchManager.cpp \
	$(SRC_DIR)/core/PromptBuilder.cpp \
	$(SRC_DIR)/core/ProcessRunner.cpp \
	$(SRC_DIR)/core/ProjectMemory.cpp \
	$(SRC_DIR)/core/ProjectModel.cpp \
	$(SRC_DIR)/core/ProjectScanner.cpp \
	$(SRC_DIR)/core/TimeUtils.cpp \
	$(SRC_DIR)/core/ChatRequest.cpp \
	$(SRC_DIR)/core/EditProposal.cpp
INSTALL_DIR ?= $(HOME)/config/non-packaged/apps

.PHONY: all clean distclean run install test check-network-deps

all: $(TARGET)

ifeq ($(AI_NETWORK),1)
$(TARGET): | check-network-deps
$(OBJS): | check-network-deps
endif

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

check-network-deps:
	@printf '#include <curl/curl.h>\nint main(){return 0;}\n' | $(CXX) $(CPPFLAGS) $(CXXFLAGS) -x c++ - -c -o /dev/null >/dev/null 2>&1 || { echo "Haikode AI_NETWORK=1 requires curl_devel. Run: pkgman install curl_devel jsoncpp_devel openssl_devel"; exit 1; }
	@printf '#if __has_include(<json/json.h>)\n#include <json/json.h>\n#elif __has_include(<jsoncpp/json/json.h>)\n#include <jsoncpp/json/json.h>\n#else\n#error missing jsoncpp\n#endif\nint main(){return 0;}\n' | $(CXX) $(CPPFLAGS) $(CXXFLAGS) -x c++ - -c -o /dev/null >/dev/null 2>&1 || { echo "Haikode AI_NETWORK=1 requires jsoncpp_devel. Run: pkgman install curl_devel jsoncpp_devel openssl_devel"; exit 1; }

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	mkdir -p "$(dir $@)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TEST_TARGET): $(TEST_SRCS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $(TEST_SRCS)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

run: $(TARGET)
	./$(TARGET)

install: $(TARGET)
	mkdir -p "$(INSTALL_DIR)"
	cp "$(TARGET)" "$(INSTALL_DIR)/$(APP)"

clean:
	rm -rf -- "$(BUILD_DIR)"

distclean: clean
	rm -f -- "$(APP)"
