#if !defined(PX_PARSER_H)
#define PX_PARSER_H

#include	<strval.h>
#include	<variant.h>
#include	<peg.h>
#include	<peg_ast.h>

class PxParser
: public Peg<PegMemorySource, PegMatch, PegContext>
{
	static	Rule	rules[];
	static	int	num_rule;
public:
	PxParser() : Peg(rules, num_rule) {}
};

#endif // PX_PARSER_H
