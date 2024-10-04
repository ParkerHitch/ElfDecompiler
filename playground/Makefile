CC = gcc
LIB_SUBMOD = ../capstone
LIB_INSTALL_PREFIX = install
INC_DIR = $(LIB_SUBMOD)/include
LIBNAME = capstone

UNAME_P = $(shell uname -p)
ifeq ($(UNAME_P),x86_64)
LIB_DIR = $(LIB_SUBMOD)/$(LIB_INSTALL_PREFIX)/lib
else
LIB_DIR = $(LIB_SUBMOD)/$(LIB_INSTALL_PREFIX)/lib64
endif
LIB = $(LIB_DIR)/lib$(LIBNAME).a

all: test1

test1: test1.o $(LIB)
	${CC} $< -O3 -Wall -L$(LIB_DIR) -l$(LIBNAME) -o $@

%.o: %.c
	${CC} -c $< -o $@ -I$(INC_DIR)

$(LIB): 
	cd $(LIB_SUBMOD); \
	cmake -B build -DCMAKE_BUILD_TYPE=Release; \
	cmake --build build; \
	cmake --install build --prefix "$(LIB_INSTALL_PREFIX)"; \
