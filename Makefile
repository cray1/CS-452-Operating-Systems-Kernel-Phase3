# This is a sample Makefile for Phase 3. Update the variables below as appropriate. It
# provides the following targets (assume test source files are named testN.c in this
# description).
#
#	make		(makes libphase3.a and all tests)
#        make phase1     ditto
#
#	make testN 	(makes testN)
#	make testN.out	(runs testN and puts output in testN.out)
#	make tests	(makes all testN.out files, i.e. runs all tests)
#	make tests_bg	(runs all tests in the background)
#
#	make testN.v	(runs valgrind on testN and puts output in testN.v)
#	make valgrind	(makes all testN.v files, i.e. runs valgrind on all tests)
#
#	make clean	(removes all files created by this Makefile)

ifndef CS452_STUDENTS
	452-STUDENTS = $(HOME)/Dropbox/452-students
else
	452-STUDENTS = $(CS452_STUDENTS)
endif

# Set to version of USLOSS you want to use.
USLOSS_VERSION = 2.12

# Set to version of Phase 1 you want to use.
PHASE1_VERSION = 1.5

# Set to version of Phase 2 you want to use.
PHASE2_VERSION = 2.0

#Set to the version of the user library you want to use.
USER_VERSION = 1.3

SRCS = $(wildcard *.c)

# Add any tests here. If the test is named test0 then the source file is assumed to be test0.c.
TESTS = $(patsubst %.c,%,$(wildcard tests/*.c))

# Change this if you want to change the arguments to valgrind.
VGFLAGS = --track-origins=yes --leak-check=full --max-stackframe=100000

# Change this if you need to link against additional libraries (probably not).
LIBS = -lphase$(PHASE2_VERSION) -lphase$(PHASE1_VERSION) -lphase3 -lusloss$(USLOSS_VERSION) -luser$(USER_VERSION)

# Change this if you want change which flags are passed to the C compiler.
CFLAGS += -Wall -g -DPHASE_3
CFLAGS += -DDEBUG

# You shouldn't need to change anything below here. 

PHASE = phase3

TARGET = lib$(PHASE).a

INCLUDES = -I$(452-STUDENTS)/include -I.

ifeq ($(shell uname),Darwin)
	DEFINES += -D_XOPEN_SOURCE
	OS = macosx
	CFLAGS += -Wno-int-to-void-pointer-cast -Wno-extra-tokens -Wno-unused-label -Wno-unused-function
else
	OS = linux
	CFLAGS += -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-unused-but-set-variable
endif


CC=gcc
LD=gcc
AR=ar    
CFLAGS += $(INCLUDES) $(DEFINES)
LDFLAGS = -L$(452-STUDENTS)/lib/$(OS) -L.
COBJS = ${SRCS:.c=.o}
DEPS = ${COBJS:.o=.d}
TSRCS = {$TESTS:=.c}
TOBJS = ${TESTS:=.o}
TOUTS = ${TESTS:=.out}
TVS = ${TESTS:=.v}

# The following is to deal with circular dependencies between the USLOSS and phase1
# libraries. Unfortunately the linkers handle this differently on the two OSes.
# Also add flags to warn about duplicate global variables and to make warnings fatal.

ifeq ($(OS), macosx)
	LIBFLAGS = -Wl,-all_load $(LIBS)
	# The following only works for dynamic libraries. TODO make it work with static.
	LDFLAGS += -Wl,-warn_commons -Wl,-fatal_warnings
else
	LIBFLAGS = -Wl,--start-group $(LIBS) -Wl,--end-group
	LDFLAGS +=  -Wl,--warn-common -Wl,--fatal-warnings
endif

%.d: %.c
	$(CC) -c $(CFLAGS) -MM -MF $@ $<

all: $(PHASE)

$(PHASE): $(TARGET) $(TESTS)


$(TARGET):  $(COBJS)
	$(AR) -r $@ $^

tests: $(TOUTS)

# Remove implicit rules so that "make phaseN" doesn't try to build it from phaseN.c or phaseN.o
% : %.c

% : %.o

%.out: %
	./$< 1> $@ 2>&1

$(TESTS):   %: $(TARGET) %.o Makefile
	- $(LD) $(LDFLAGS) -o $@ $@.o $(LIBFLAGS)

clean:
	rm -f $(COBJS) $(TARGET) $(TOBJS) $(TESTS) $(DEPS) $(TVS) *.out tests/*.out

%.d: %.c
	$(CC) -c $(CFLAGS) -MM -MF $@ $<

valgrind: $(TVS)

%.v: %
	valgrind $(VGFLAGS) ./$< 1> $@ 2>&1

-include $(DEPS)
