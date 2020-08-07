#Makefile for MP2

CC=/usr/bin/gcc
CC_OPTS=-g3 -pthread
CC_LIBS=
CC_DEFINES=
CC_INCLUDES=
CC_ARGS=${CC_OPTS} ${CC_LIBS} ${CC_DEFINES} ${CC_INCLUDES}

# clean is not a file
.PHONY=clean


all: ls_router vec_router

ls_router: ls_router.c
	@${CC} ${CC_ARGS} -o ls_router ls_router.c

vec_router: vec_router.c
	@${CC} ${CC_ARGS} -o vec_router vec_router.c

clean:
	@rm -f ls_router vec_router output *.o logs/*.log
