# This makefile is used to compile the three steps in the assignment
# Use `make` to compile all the steps, `make debug` to compile with debug flag, `make clean` to clean the executables
# For details, you could check the makefile in each problem folder

.PHONY: all debug clean

all:
	$(MAKE) -C step1
	$(MAKE) -C step2
	$(MAKE) -C step3

debug:
	$(MAKE) -C step1 debug
	$(MAKE) -C step2 debug
	$(MAKE) -C step3 debug

clean:
	$(MAKE) -C step1 clean
	$(MAKE) -C step2 clean
	$(MAKE) -C step3 clean