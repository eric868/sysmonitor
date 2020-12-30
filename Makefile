SRCS = main.cpp system_monitor.cpp 
GCC = g++

all:
	$(GCC) $(SRCS)

.PHONY:all
