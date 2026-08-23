---
layout: default
title: What is a PEG?
description: Ordered choice and greedy repetition, and why they change how you write grammars.
---

A **Parsing Expression Grammar** looks a lot like a BNF grammar &mdash; rules, made of
alternatives, made of sequences of other rules and terminals. The difference that
matters is in how a PEG *decides between alternatives*, and it's a small difference
with large consequences.

## Ordered choice, not ambiguity resolution

In a context-free grammar, `A | B` means "any string derivable from A or from B" &mdash;
if a string matches both, the grammar is ambiguous, and something outside the grammar
(a precedence table, a disambiguation rule, a GLR parser trying every path) has to
pick one.

In a PEG, `A | B` means something operationally simpler: **try A; if A succeeds, use
it, and never even look at B.** Only if A fails does the parser try B. There's no
ambiguity to resolve, because there's no choice left once the first alternative
matches &mdash; but that also means **the order of your alternatives is part of the
grammar's meaning**, not just a style preference. Write `A | B` when you meant `B | A`
and you get a different language, silently.

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

`?`, `*` and `+` behave the way you'd expect from a regular expression &mdash; zero-or-one,
zero-or-more, one-or-more &mdash; and like a regex, they're **greedy**: `*X` matches as many
`X`s in a row as it can, and once it stops (because the next `X` failed), it does not
go back and try consuming fewer. A PEG engine, in general, does not backtrack *into* a
repetition or *across* a choice that already succeeded. What you write is what runs.

## Unlimited lookahead, without consuming input

The one thing a PEG *can* do that a plain regex can't is call arbitrary rules
&mdash; including itself, recursively &mdash; as a **lookahead assertion** that checks
whether something matches *without consuming any input*:

- `&A` succeeds (consuming nothing) if `A` would match here, and fails otherwise.
- `!A` succeeds (consuming nothing) if `A` would *not* match here, and fails otherwise.

Because `A` can be any rule in the grammar &mdash; not just a character class &mdash;
this is far more powerful than a regex lookahead. It's also the main tool for working
around ordered choice and greedy repetition once they start causing trouble, which is
exactly what the [next page](pitfalls.html) works through with real examples.

## Recursive descent, so left recursion doesn't work

Px compiles a grammar into a straightforward recursive-descent parser: matching rule
`R` calls the function for whatever `R` is made of, which may call `R` again for
something nested *inside* it. What it can't do is call `R` again as the very first
thing, before consuming any input &mdash; `R = R 'x'` would call itself forever with no
progress. Px detects this at parse time and reports it rather than hanging; the
[pitfalls page](pitfalls.html#left-recursion) shows the fix.
