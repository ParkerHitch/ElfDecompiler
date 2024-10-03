CC = gcc
INC_DIR = /opt/homebrew/Cellar/capstone/5.0.3/include
LIB_DIR = /opt/homebrew/Cellar/capstone/5.0.3/lib
LIBNAME = capstone

EXENAME = decomp

OUT_DIR = ./out
SRC_DIR = ./src

SRCS = main.c
OBJS = $(OUT_DIR)/$(SRCS:%.c=%.o)

# Keep compile-commands.json updated for clangd
# https://github.com/rizsotto/Bear
bear:
	bear -- make all

all: $(OBJS)
	$(CC) -o $(EXENAME) $^ -L$(LIB_DIR) -l$(LIBNAME)

$(OUT_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) -o $@ -c $< -I$(INC_DIR)

clean:
	rm -rf out/*
	rm $(EXENAME)
