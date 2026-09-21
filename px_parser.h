#if !defined(PX_PARSER_H)
#define PX_PARSER_H

#include	<strval.h>
#include	<variant.h>

/*
 * Where px writes. A Strpp program has no stdio, so this is write(2) - and a
 * platform with somewhere better to write defines PX_WRITE and PX_WRITE_ERR
 * before including this, as it does for strpp's panic dump.
 */
#if	!defined(PX_WRITE)
#include	<unistd.h>
inline void	px_write(int fd, StrVal text)
{
	(void)!write(fd, text.asUTF8(), (int)text.numBytes());
}
#define	PX_WRITE(text)		px_write(1, (text))
#define	PX_WRITE_ERR(text)	px_write(2, (text))
#endif
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
