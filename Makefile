PRG_NAME	= pwmctl
SRCDIR		= .
OBJDIR		= obj
OBJS		= $(addprefix $(OBJDIR)/, $(patsubst %.cpp, %.o, $(wildcard *.cpp)))
CXX			= g++
LD			= g++
LIB			= -lrt
CFLAGS		= -g -Wall -O2

.PHONY:clean
.PHONY:all

all:$(PRG_NAME).out

$(PRG_NAME).out:$(OBJS)
	$(LD) -o $@ $(LIB) $(OBJS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CFLAGS) -o $@ -c $<

pwmctl.o:	Packet.h

clean:
	rm -rf $(PRG_NAME).out $(OBJS)

