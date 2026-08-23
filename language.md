---
layout: default
title: The Px Language
description: Rules, atoms, operators, captures, and the (as scope.name) annotation.
---

A Px grammar is a sequence of rules. Each rule is a name, an `=`, a PEG expression,
and an optional capture list, and is terminated by a blank line:

```
rule_name =
	expression
	-> captured, names
```

## Rules and scope annotations {#rules-and-scope-annotations}

Right after the name, before the `=`, a rule can optionally carry a parenthesised
`(as scope.name)` &mdash; this doesn't change what the rule matches at all; it's
metadata read only by the [syntax-highlighting generator](textmate-generator.html):

```
comment (as comment.line.px) =
	'//' *[^\n]
```

## Atoms {#atoms}

Every expression is built from these six kinds of atom:

| Atom | Meaning |
|---|---|
| `.` | any single character (including a newline) |
| `name` | call another rule |
| `\x` | a character property (see below) |
| `'text'` | a literal string |
| `[chars]` | a character class |
| `(expr)` | a parenthesised group |

## Operators

Operators go in *prefix* position, before the atom they apply to &mdash; the opposite
of a regex, where `x*` follows the `x`. In Px it's `*x`.

| Operator | Meaning |
|---|---|
| `?A` | zero or one of A |
| `*A` | zero or more of A |
| `+A` | one or more of A |
| `\|A\|B` | A, or if that fails, B (ordered choice &mdash; see the [PEG primer](peg-primer.html)) |
| `&A` | succeed here (consuming nothing) if A matches |
| `!A` | succeed here (consuming nothing) if A does *not* match |
| `{n}A` | exactly n of A; `{n,name}` and other count forms reference a captured value |

`&`/`!` are lookahead assertions, the main subject of [Pitfalls &
Lookahead](pitfalls.html).

## Character classes and properties

`[...]` matches one character from the set inside &mdash; `^` at the start negates it,
and `-` between two characters gives a range, exactly like a regex class. Inside a
class or a literal string, a backslash escape can be:

- a **character property**: `\a` alpha, `\d` digit, `\h` hex digit, `\s` whitespace,
  `\w` alpha-or-digit, `\L` lowercase, `\U` uppercase &mdash; these work as a class
  member, inside a literal, or as a standalone atom (`\d` on its own means "any
  digit");
- a **C-style escape**: `\n` `\t` `\r` `\b` `\f`, or `\'` `\"` `\\` for the character
  itself;
- a **numeric escape**: `\177` (octal), `\xHH` or `\x{H...}` (hex), `\uHHHH` or
  `\u{H...}` (Unicode).

## Captures {#captures}

A rule followed by `-> a, b, c` captures whatever matched `a`, `b` and `c` into an
ordered array in the parse tree, even when one of them appears more than once in the
rule (handy for a comma-separated list). Anything not named in the list is parsed but
discarded &mdash; only what you capture ends up in the AST.

A **label** (`:name` right after any atom, including a rule call) gives that specific
occurrence a name to capture by, independent of what rule it calls:

```
object = '{' *(|(string:k ':' TOP:v *(',' string:k ':' TOP:v)) | s) '}'
	-> k, v
```

Here `string:k` and `TOP:v` both appear twice (once, and again inside the repeated
`,`-separated tail) &mdash; every match of either gets appended to the `k` or `v`
array respectively, so `object`'s capture is two parallel arrays of keys and values.

## A worked example

Px's own [JSON grammar](https://github.com/cjheath/px/blob/main/grammars/json.px) uses
everything above in about twenty lines:

```
s	= *[ \t\r\n]				// Zero or more space characters

TOP	= s (| object |array |string |'true':v |'false':v |'null':v |number ) s
	-> object, array, string, v, number

object	= '{' *(|(string:k ':' TOP:v *(',' string:k ':' TOP:v)) | s) '}'
	-> k, v

array	= '[' (|(TOP:e *(',' TOP:e)) |s) ']'
	-> e

string	= s ["] *(|[^"\\\u{0}-\u{1F}]:c |escape:c) ["] s
	-> c

escape	= [\\] (|["\\/bfnrt] | 'u' [0-9a-fA-F] [0-9a-fA-F] [0-9a-fA-F] [0-9a-fA-F] )

number	= ?'-' (|'0' | [1-9] *[0-9]) ?('.' +[0-9]) ?([eE] ?[-+] +[0-9])
```

`TOP`'s ordered choice tries `object`, then `array`, then `string`, then the three
literal keywords, then `number` &mdash; and because a JSON value can only ever start
with one of those, order doesn't matter *here*. `string`'s body is exactly the
"unbounded repetition needs a stop condition" shape from
[pitfalls.md](pitfalls.html#3-unbounded-repetition-needs-its-own-stop-condition):
every iteration either takes a plain character that isn't `"`, `\` or a control
character, or an `escape`, until the closing `"` ends it. See this grammar turned into
a real parser on the [C++ generator](cpp-generator.html) page, and into a highlighter
on the [syntax highlighting](textmate-generator.html) page.
