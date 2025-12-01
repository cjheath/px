#
# Vanilla makefile for px
#

CXX	=	g++
CXXFLAGS =	-std=c++11

STRPP	=	../strpp

COPT	=	-DHAVE_PTHREADS # -DPEG_TRACE

DEBUG	=	-O2 $(COPT)
# DEBUG	=	-g $(COPT) -DUTF8_ASSERT
# DEBUG	=	-O2 $(COPT) -lprofiler
# DEBUG	=	-g -DTRACK_RESULTS $(COPT)

HDRS	=	\
		px_pegexp.h \
		px_railroad.h \
		px_cpp.h

SRCS	=	\
		px_pegexp.cpp \
		px_railroad.cpp \
		px_cpp.cpp \
		px.cpp

OBJS	=	$(patsubst %,%,$(SRCS:.cpp=.o))

LIBS	=	-L$(STRPP) -lstrpp

# vpath	%.cpp	../src
vpath	%.h	$(STRPP)/include
vpath	%.a	$(STRPP)

all:	strpp px doc

px:	Makefile peg_ast.h $(HDRS) $(SRCS) px_parser.cpp
	$(CXX) $(DEBUG) $(CXXFLAGS) -I. -I$(STRPP)/include -o $@ $(SRCS) $(LIBS)

# Regenerate px_parser from px.px:
px_parser.cpp: px.px
	px px.px >px_parser.cpp

# make grammar documentation:
doc:	px-rr.html
px-rr.html: px px.px
	px -r -x s px.px > px-rr.html

strpp:	
	cd $(STRPP); make lib

test:

tests:

clean:
	rm -f px $(OBJS)
	rm -rf *.dSYM

clobber:	clean

.PHONY:	all clean test tests clean clobber strpp
