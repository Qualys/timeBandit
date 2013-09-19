SRCS=crwlr.cc
CFLAGS=-ggdb -I/usr/include/libxml2
CURLOPTS=$(shell curl-config --libs)
LIBS=-lxml2

crwlr:	$(SRCS) Makefile
	g++ -o $@ $(CFLAGS) $(SRCS) $(CURLOPTS) $(LIBS)
