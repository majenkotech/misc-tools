TARGETS=dmmvideo
CFLAGS=
LDFLAGS=-lgd

all: $(TARGETS)


dmmvideo: dmmvideo.o
	${CC} -o $@ $< -lgd -ggdb3


clean:
	rm -f dmmvideo dmmvideo.o
