#/bin/sh

TOP_PATH = $(shell pwd)

SRCPTH = $(TOP_PATH)
INPTH = $(TOP_PATH)

SDK_PATH = ./

CXX  = gcc
INLPTH = -g -Wall -O3 #-fgnu89-inline
INLPTH += -I$(INPTH)		
	   
LPTH :=  -lpthread  -ldl	
OBJ  = $(SDK_PATH)obj/ 

TARG = tavi
DIRS = $(SRCPTH) 

FILES = $(foreach dir, $(DIRS),  $(wildcard $(dir)/*.c))
OBJS  = $(patsubst %.c, %.o, $(FILES))

all:$(TARG)

#链接
$(TARG):$(OBJS)
	$(CXX) -o $(TARG)  $^ $(LPTH)   

#编译
.c.o:
	$(CXX) -c $(INLPTH) $< -o $@    
	
clean:
	rm -rf $(OBJS) 
	rm -rf $(TARG)
