---
layout: default
title: The C++ Generator
description: Turning a grammar into a working Peg<>-based parser, and reading the AST it produces.
---

With no flags, `px` compiles a grammar straight into a C++ source file defining a
table of rules for the `Peg<>` template (declared in `peg.h`, in the same repo):

```
px grammars/json.px > json_parser.cpp
```

## What comes out

Each rule becomes one `Rule` table entry: its name, its body compiled into a compact
runtime pattern language ("Pegexp", but using `<name>` for a rule call, everything else
is close to what you wrote), and, if it captures anything, an array of the capture names:

```cpp
const char*	TOP_captures[] = { "object", "array", "string", "v", "number", 0 };

JsonParser::Rule	JsonParser::rules[] =
{
	{ "s",
	  "*[ \\t\\r\\n]",
	  0
	},
	{ "TOP",
	  "<s>(|<object>|<array>|<string>|true:v:|false:v:|null:v:|<number>)<s>",
	  TOP_captures
	},
	...
};

int	JsonParser::num_rule = sizeof(JsonParser::rules)/sizeof(JsonParser::rules[0]);
```

That's just data, not code &mdash; [`Peg<>`][Peg] is what turns a `Rule[]` table
into a parser. A program can have many different parsers without any additional
code, and could in principle even create new parsers on the fly.

## Declaring the parser class

The generated file expects `JsonParser` to already be declared, by instantiating
`Peg<>` yourself &mdash; exactly the way `px_parser.h` declares `PxParser` for Px's
own self-hosted grammar. All `JsonParser` supplies is the `Rule[]` table and its
length, to [`Peg<>`'s own constructor][Peg-ctor]:

```cpp
// json_parser.h
#include <strval.h>
#include <variant.h>
#include <peg.h>
#include <peg_ast.h>

class JsonParser
: public Peg<PegMemorySource, PegMatch, PegContext>
{
	static	Rule	rules[];
	static	int	num_rule;
public:
	JsonParser() : Peg(rules, num_rule) {}
};
```

[`PegMemorySource`][PegMemorySource], [`PegMatch`][PegMatch] and
[`PegContext`][PegContext] (all in `peg_ast.h`) are the ready-made implementation
that parses into a `Variant` abstract syntax tree &mdash; matched text becomes a
`Variant::String`, and each rule's captures become entries in a `Variant::StrVarMap`,
with repeated captures collected into a `Variant::VarArray` (that's how `object`'s
`k, v` capture ends up as two parallel arrays &mdash; see [The Px
Language](language.html#captures)).

If you need different results or more optimised implementation, you can define your
own `Context` type to expand the Peg<> template.

## Parsing something

[`Peg::parse()`][parse] does the work, [`PegMatch::is_failure()`][is_failure] says
whether it worked, and on failure the `Source` itself (here `PegMemorySource`, via
[`current_byte()`][current_byte] and its `current_line()`/`current_column()` siblings)
says how far it got:

```cpp
#include <json_parser.h>
#include <json_parser.cpp>   // the generated Rule[] table
#include <cstdio>

int main()
{
	const char*     text = "{\"answer\": 42, \"ok\": true}";
	JsonParser      parser;
	JsonParser::Source source(text);

	JsonParser::Match match = parser.parse(source);
	if (match.is_failure())
	{
		printf("parse failed at byte %lld\n", (long long)source.current_byte());
		return 1;
	}

	printf("%s\n", match.var.as_json(2).asUTF8());
	return 0;
}
```

[`match.var`][PegMatch-var] is the parsed `Variant` AST &mdash; [`Variant::as_json()`][as_json]
(used above with 2-space indenting) is a quick way to inspect any `Variant`, turned
into a plain C string with [`StrVal::asUTF8()`][asUTF8]. That's exactly what `px -j`
uses to dump a *grammar's own* parsed structure when you want to see how Px itself
parsed a `.px` file, before it's compiled further.

On failure, [`match.furthermost_success`][furthermost_success] is how far the parser
got, and [`match.failures`][failures] lists which atoms it tried and failed at that
point &mdash; `px`'s own error reporting (what you see when a `.px` file fails to
parse) is built from exactly these two fields.

## A grammar describing a sequence of independent items

The TOP grammar for Px defined in `px.px` matches just one rule at a time,
although a Px grammar normally requires multiple rules. `px.cpp` calls the
parser repeatedly, progressing through the input one rule at a time.  Each
parse starts from `match.furthermost_success` (where the previous parse reached),
until the whole file is consumed.  This means that the entire abstract syntax
tree doesn't need to exist at one time.

<!-- Everything above lives in strpp (https://github.com/cjheath/strpp), pinned to the
     commit this page was written against so the line numbers stay accurate. -->
[Peg]: https://github.com/cjheath/strpp/blob/d7982b249095f20becdb5dbeefd54df6fa363390/include/peg.h#L237
[Peg-ctor]: https://github.com/cjheath/strpp/blob/d7982b249095f20becdb5dbeefd54df6fa363390/include/peg.h#L246
[parse]: https://github.com/cjheath/strpp/blob/d7982b249095f20becdb5dbeefd54df6fa363390/include/peg.h#L256
[PegMemorySource]: https://github.com/cjheath/strpp/blob/d7982b249095f20becdb5dbeefd54df6fa363390/include/peg_ast.h#L21
[PegMatch]: https://github.com/cjheath/strpp/blob/d7982b249095f20becdb5dbeefd54df6fa363390/include/peg_ast.h#L41
[is_failure]: https://github.com/cjheath/strpp/blob/d7982b249095f20becdb5dbeefd54df6fa363390/include/peg_ast.h#L77
[PegMatch-var]: https://github.com/cjheath/strpp/blob/d7982b249095f20becdb5dbeefd54df6fa363390/include/peg_ast.h#L80
[furthermost_success]: https://github.com/cjheath/strpp/blob/d7982b249095f20becdb5dbeefd54df6fa363390/include/peg_ast.h#L83
[failures]: https://github.com/cjheath/strpp/blob/d7982b249095f20becdb5dbeefd54df6fa363390/include/peg_ast.h#L84
[PegContext]: https://github.com/cjheath/strpp/blob/d7982b249095f20becdb5dbeefd54df6fa363390/include/peg_ast.h#L119
[as_json]: https://github.com/cjheath/strpp/blob/d7982b249095f20becdb5dbeefd54df6fa363390/include/variant.h#L168
[asUTF8]: https://github.com/cjheath/strpp/blob/d7982b249095f20becdb5dbeefd54df6fa363390/include/strval.h#L432
[current_byte]: https://github.com/cjheath/strpp/blob/d7982b249095f20becdb5dbeefd54df6fa363390/include/pegexp.h#L159
