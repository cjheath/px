#
# Vanilla makefile for px
#

SHELL	=	/bin/bash	# `test` target uses diff <(...) process substitution

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
		px_cpp.h \
		px_textmate.h

SRCS	=	\
		px_pegexp.cpp \
		px_railroad.cpp \
		px_cpp.cpp \
		px_textmate.cpp \
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

# Regression tests for the -t (TextMate grammar) generator. Each NAME.px has matching
# NAME.expected.json (stdout) and NAME.expected.stderr (stderr): frozen fixtures live
# in tests/, plus px.px itself (the live language grammar, checked against
# tests/px.expected.*). Regenerate expectations after an intentional change with:
#   px -t NAME.px >NAME.expected.json 2>NAME.expected.stderr
test:	px
	@check() { \
		grammar=$$1; name=$$2; \
		out=$$(./px -t "$$grammar" 2>/tmp/px_test_stderr.$$$$); \
		err=$$(cat /tmp/px_test_stderr.$$$$); rm -f /tmp/px_test_stderr.$$$$; \
		ok=1; \
		if [ "$$out" != "$$(cat "$$name.expected.json")" ]; then \
			echo "FAIL: $$name (stdout differs from $$name.expected.json)"; \
			diff <(echo "$$out") "$$name.expected.json" | head -10; \
			ok=0; \
		fi; \
		if [ "$$err" != "$$(cat "$$name.expected.stderr")" ]; then \
			echo "FAIL: $$name (stderr differs from $$name.expected.stderr)"; \
			diff <(echo "$$err") "$$name.expected.stderr" | head -10; \
			ok=0; \
		fi; \
		[ $$ok -eq 1 ] && echo "PASS: $$name" || fail=1; \
	}; \
	fail=0; \
	check px.px tests/px; \
	for grammar in tests/*.px; do \
		name=$${grammar%.px}; \
		[ -f "$$name.expected.json" ] || continue; \
		check "$$grammar" "$$name"; \
	done; \
	exit $$fail

tests:	test

clean:
	rm -f px $(OBJS)
	rm -rf *.dSYM

clobber:	clean

.PHONY:	all clean test tests clean clobber strpp
