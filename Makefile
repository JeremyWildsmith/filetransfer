# tool macros
CC ?= gcc
CXX ?= g++
CFLAGS := -I./include/
CXXFLAGS := # FILL: compile flags

SERVERFLAGS_COMPILE := -DMODE_SERVER
CLIENTFLAGS_COMPILE :=

SERVERFLAGS_LINK := -g -DMODE_SERVER
CLIENTFLAGS_LINK := -g
COBJFLAGS := $(CFLAGS) -c

# path macros
CLIENT_PATH := bin/client
SRC_PATH := src
SERVER_PATH := bin/server

# compile macros
TARGET_NAME := tcp
ifeq ($(OS),Windows_NT)
	TARGET_NAME := $(addsuffix .exe,$(TARGET_NAME))
endif
TARGET_CLIENT := $(CLIENT_PATH)/client
TARGET_SERVER := $(SERVER_PATH)/server

export CLIENT_TEST := $(shell readlink -f $(TARGET_CLIENT))
export SERVER_TEST := $(shell readlink -f $(TARGET_SERVER))

# src files & OBJ_CLIENT files
SRC := $(foreach x, $(SRC_PATH), $(wildcard $(addprefix $(x)/*,.c*)))
OBJ_CLIENT := $(addprefix $(CLIENT_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC)))))
OBJ_SERVER := $(addprefix $(SERVER_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC)))))

# clean files list
DISTCLEAN_LIST := $(OBJ_CLIENT) \
                  $(OBJ_SERVER)
CLEAN_LIST := $(TARGET_CLIENT) \
			  $(TARGET_SERVER) \
			  $(DISTCLEAN_LIST)

# default rule
default: makedir all

# non-phony targets
$(TARGET_CLIENT): $(OBJ_CLIENT)
	$(CC) -o $@ $(OBJ_CLIENT) $(CLIENTFLAGS_COMPILE) $(CFLAGS)

$(CLIENT_PATH)/%.o: $(SRC_PATH)/%.c*
	$(CC) $(COBJFLAGS) $(CLIENTFLAGS_LINK) -o $@ $<

$(SERVER_PATH)/%.o: $(SRC_PATH)/%.c*
	$(CC) $(COBJFLAGS) $(SERVERFLAGS_LINK) -o $@ $<

$(TARGET_SERVER): $(OBJ_SERVER)
	$(CC) $(CFLAGS) $(SERVERFLAGS_COMPILE) $(OBJ_SERVER) -o $@

# phony rules
.PHONY: makedir
makedir:
	@mkdir -p $(CLIENT_PATH) $(SERVER_PATH)

.PHONY: all
all: client server

.PHONY: client
client: makedir $(TARGET_CLIENT)

.PHONY: server
server: makedir $(TARGET_SERVER)

.PHONY: clean
clean:
	@echo CLEAN $(CLEAN_LIST)
	@rm -f $(CLEAN_LIST)

.PHONY: distclean
distclean:
	@echo CLEAN $(DISTCLEAN_LIST)
	@rm -f $(DISTCLEAN_LIST)

.PHONY: distclean
test: all
	cd tests && bats .