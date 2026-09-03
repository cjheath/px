---
layout: default
title: What is a PEG?
description: Ordered choice and greedy repetition, and why they change how you write grammars.
---

A **Parsing Expression Grammar** in Px is like a [BNF grammar](https://en.wikipedia.org/wiki/Backus–Naur_form).
A set of rules, each made of alternatives, each alternative made of sequences of other rules
and terminal expressions (literals). A BNF doesn't indicate how to decide between ambiguous alternatives,
but PEGs do, a small difference with large consequences.

## Ordered choice, not ambiguity resolution

In a context-free grammar, `A | B` means "any string derivable from A or from B" &mdash;
if a string matches both, the grammar is ambiguous, and something outside the grammar
(a precedence table, a disambiguation rule, a GLR parser trying every path) has to
pick one.

In a PEG, `A | B` means something much simpler: **try A; if A succeeds, use it,
and never even look at B.** Only if A fails does the parser try B. There's no
ambiguity to resolve, because there's no choice left once the first alternative
matches &mdash; but that also means **the order of your alternatives is part of the
grammar's meaning**, not just a style preference. If you write `A | B` when you meant
`B | A`, it matches a different language.

Px writes this the same way BNF would, just with the `|` moved to prefix position and
repeated before every alternative:

```
atom =
	| '.':any			// The "Any" character
	| name:call			// call to another rule
	| '\\' property			// A character property
	| '\'' literal '\''		// A literal
	| '[' class ']'			// A character class
	| '(' group ')'
	-> any, call, property, literal, class, group
```

Six alternatives, tried top to bottom, first match wins.

## Greedy repetition, no backtracking

`?`, `*` and `+` behave the way you'd expect from a regular expression; zero-or-one,
zero-or-more, one-or-more; and like a regex, they're **greedy**: `*X` matches as many
`X`s in a row as it can, and once it stops (because the next `X` failed), it does not
go back and try consuming fewer. A PEG engine, in general, does not backtrack *into* a
repetition or *across* a choice that already succeeded. What you write is what runs.

## Non-regular grammars

A regular expression cannot include recursive rules, because recursion produces
non-regular grammars. Although some `regular expression` libraries do support
recursion, this extension makes them non-regular. Px allows recursive calls,
either directly or indirectly, with a caveat around left recursion.

## Unlimited lookahead

Because PEGs are greedy, it is necessary sometimes to *look ahead* to prevent a rule
from consuming too much input. In Px, lookahead expressions can be arbitrary parsing
rules, including recursive ones. These grammars are therefore more powerful than any
conventional parser using limited lookahead, and more powerful than parsers like ANTLR
which supports unlimited lookahead but requires it to be strictly regular.

A **lookahead assertion** checks whether something matches *without consuming any input*:

- `&A` succeeds (consuming nothing) if `A` would match here, and fails otherwise.
- `!A` succeeds (consuming nothing) if `A` would *not* match here, and fails otherwise.

This is also the main tool for working around ordered choice and greedy repetition once
they start causing trouble, which is exactly what the [Pitfalls page](pitfalls.html) shows.

## Recursive descent, so left recursion doesn't work

Px compiles a grammar into a straightforward recursive-descent parser: matching rule
`R` calls the function for whatever `R` is made of, which may call `R` again for
something nested *inside* it. Px defends against infinite recursion (when the same
rule is caused before other progress) by causing that rule to fail.
The [pitfalls page](pitfalls.html#left-recursion) shows how to work around this.
