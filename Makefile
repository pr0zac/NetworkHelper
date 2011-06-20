TARGETS=network_helper

HOLEPOKEOBJS=./holepoke/holepoke.pb.o ./holepoke/endpoint.o ./holepoke/sender.o ./holepoke/receiver.o ./holepoke/network.o ./holepoke/fsm.o ./holepoke/uuid.o

OBJS=cc.o socket_list_item.o network_receiver.o network_sender.o network_helper.o

UNAME = $(shell uname)

ifdef DEBUG
CFLAGS = -Wall -Werror -g -O0 -fno-inline -DDEBUG
HOLEPOKEFLAGS=DEBUG=1
else
CFLAGS = -Wall -O3
endif

UDTLIB=udt4/src/libudt.a

ifeq ($(UNAME), Darwin)
UDT_MAKE_ARGS+=os=OSX arch=all
INCLUDES=-I/opt/local/include
LDFLAGS=/opt/local/lib/libprotobuf.a -L/opt/local/lib -framework CoreFoundation -lstdc++ -lpthread -lm -arch i386 -arch x86_64
CFLAGS+=-Iudt4/src $(INCLUDES) -arch i386 -arch x86_64
CXXFLAGS=$(CFLAGS)
CC = clang
CXX = clang++
LD = clang++ $(LDFLAGS)
endif

ifeq ($(UNAME), Linux)
INCLUDES=-I/usr/include
LDFLAGS=/usr/lib/libprotobuf.a -L/usr/lib -luuid -lstdc++ -lpthread -lm -lbsd
CFLAGS+=-Iudt4/src $(INCLUDES)
CXXFLAGS=$(CFLAGS)
CC = gcc
CXX = g++
LD = g++ -v $(LDFLAGS)
endif

network_helper: $(HOLEPOKEOBJS) $(OBJS) $(UDTLIB)
	$(LD) -o network_helper $(HOLEPOKEOBJS) $(OBJS) $(UDTLIB) $(LDFLAGS)

$(HOLEPOKEOBJS):
	cd holepoke && make $(HOLEPOKEFLAGS)

$(UDTLIB):
	cd udt4 && make -e $(UDT_MAKE_ARGS)

d:
	cd holepoke && make clean
	$(RM) $(OBJS) $(TARGETS)
	make DEBUG=1

clean:
	cd udt4 && make -e $(UDT_MAKE_ARGS) clean
	cd holepoke && make clean
	$(RM) $(OBJS) $(TARGETS)
