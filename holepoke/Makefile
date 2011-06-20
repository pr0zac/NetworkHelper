targets = holepoked peer

# Put holepoke.pb.o first so it gets built before the other objects, which need it's .h file
holepoked_objs = holepoke.pb.o network.o uuid.o holepoked.o
peer_objs = holepoke.pb.o endpoint.o sender.o receiver.o network.o fsm.o peer.o

UNAME = $(shell uname)

ifdef DEBUG
CFLAGS = -Wall -Werror -g -O0 -fno-inline -DDEBUG
else
CFLAGS = -Wall -O3
endif

ifeq ($(UNAME), Darwin)
macports_prefix=/opt/local
INCLUDES = -I$(macports_prefix)/include
LDFLAGS=/opt/local/lib/libprotobuf.a -L$(macports_prefix)/lib -framework CoreFoundation -arch i386 -arch x86_64
CFLAGS+=$(INCLUDES) -arch i386 -arch x86_64
CC = clang
CXXFLAGS=$(CFLAGS)
CXX = clang++
LD = clang++
endif

ifeq ($(UNAME), Linux)
INCLUDES=-I/usr/include
LDFLAGS=/usr/lib/libprotobuf.a -L/usr/lib -lpthread -luuid
CFLAGS+=$(INCLUDES)
CC = gcc
CXXFLAGS=$(CFLAGS)
CXX = g++
LD = g++
endif

all: $(targets)

%.pb.cc: %.proto
	protoc -I=. --cpp_out=. $<

holepoked: $(holepoked_objs)
	$(LD) -v -o $@ $(holepoked_objs) $(LDFLAGS)

peer: $(peer_objs)
	$(LD) -o $@ $(peer_objs) $(LDFLAGS)

clean:
	$(RM) $(targets) $(holepoked_objs) $(peer_objs) holepoke.pb.h
