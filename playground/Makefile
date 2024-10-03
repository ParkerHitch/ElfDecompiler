CC = gcc
INC_DIR = /opt/homebrew/Cellar/capstone/5.0.3/include
LIB_DIR = /opt/homebrew/Cellar/capstone/5.0.3/lib
LIBNAME = capstone

all: test1

test1: test1.o
	${CC} $< -O3 -Wall -L$(LIB_DIR) -l$(LIBNAME) -o $@

%.o: %.c
	${CC} -c $< -o $@ -I$(INC_DIR)

