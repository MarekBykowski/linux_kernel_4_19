GCC=gcc 
GCCFLAGS=-g

OBJS=user_addr.o

SRCS = $(OBJS:.o=.c)

all: user_addr

$(OBJS): $(SRCS)
	$(GCC) $(GCCFLAGS) -c $(SRCS)

main: $(OBJS) 
	$(GCC) $(GCCFLAGS) $(OBJS) -o $@

.PHONY: clean
clean:
	find ./ -name "*.o" -exec rm {} \;
	rm -rf user_addr
