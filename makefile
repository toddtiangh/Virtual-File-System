override CFLAGS := -Wall -Werror -std=gnu99 -O0 -g $(CFLAGS) -I.
#CC = gcc

all: fs.o disk.o

# Build the threads.o file

fs.o: fs.c fs.h 

disk.o: disk.c disk.h


#test_filesystem.o: test_filesystem.c fs.h disk.h
#      $(CC) -c ./test_filesystem.c
      
#test_filesystem.o: test_filesystem.o fs.o disk.o
#      $(CC) test_filesystem.o disk.o fs.o -o test_filesystem
           
#test_files = ./test_filesystem

.PHONY: clean check checkprogs all

# Build all of the test programs                                                                             
checkprogs: $(test_files)

# Run the test programs                                                                                      
#check: checkprogs	
#	/bin/bash run_tests.sh $(test_files)

clean:	
	rm -f fs.o
