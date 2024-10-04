CC = gcc
LIB_SUBMOD = ./capstone
LIB_INSTALL_PREFIX = install
INC_DIR = $(LIB_SUBMOD)/include
LIBNAME = capstone
LIB_DIR = $(LIB_SUBMOD)/$(LIB_INSTALL_PREFIX)/lib64
LIB = $(LIB_SUBMOD)/$(LIB_INSTALL_PREFIX)/lib64/lib$(LIBNAME).a

EXENAME = decomp

OUT_DIR = ./out
SRC_DIR = ./src

SRCS = main.c elfParser.c
HEADERS = elfParser.h
OBJS = $(addprefix $(OUT_DIR)/,$(SRCS:%.c=%.o))

# Keep compile-commands.json updated for clangd
# https://github.com/rizsotto/Bear
bear:
	bear -- make all

all: $(OBJS) $(LIB)
	$(CC) -o $(EXENAME) $(OBJS) -L$(LIB_DIR) -l$(LIBNAME)

$(OUT_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) -o $@ -c $< -I$(INC_DIR)

$(LIB): 
	cd $(LIB_SUBMOD); \
	cmake -B build -DCMAKE_BUILD_TYPE=Release; \
	cmake --build build; \
	cmake --install build --prefix "$(LIB_INSTALL_PREFIX)"; \

clean:
	rm -rf out/*
	rm $(EXENAME)
