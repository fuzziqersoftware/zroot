OBJECTS=Complex.o Iterate.o JuliaSet.o Main.o
CXXFLAGS=-DMACOSX -O3 -g -Wall -std=c++14 -I/opt/local/include
LDFLAGS=-g -std=c++14 -L/opt/local/lib -lphosg
EXECUTABLES=zroot

UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),x86_64)
	OBJECTS += Iterate-amd64.o
	CXXFLAGS += -DAMD64
endif

all: zroot

zroot: $(OBJECTS)
	g++ $(LDFLAGS) -o zroot $^

clean:
	-rm -rf *.o $(EXECUTABLES)

.PHONY: clean
