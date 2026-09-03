---
layout: default
title: Pitfalls & Lookahead
description: The mistakes ordered choice and greedy repetition invite, and how to fix them with & and !.
---

The pitfalls below follow directly from the two processes listed on the [PEG
primer](peg-primer.html) page: ordered choice never backtracks once an alternative
succeeds, and repetition is greedy with no backtracking either. An example of each of
these is given here, with a rule that goes wrong, and the correct rule using lookahead.
(Some of these examples are quoted directly from Px's own grammar,
[`px.px`](https://github.com/cjheath/px/blob/main/px.px)
&mdash; Px is self-hosting, so its own grammar has had to solve every one of these.)

## 1. Alternative order decides the winner

```
keyword = | 'if' | 'else' | 'while'

name    = [\a_] *[\w_]
```

If a parent rule tries `| name | keyword`, it will *never* match `keyword`, because the
pattern for `name` matches any keyword, so the `keyword` alternative will never be tried.
Instead, list the more specific alternative first: `keyword | name`.

## 2. A keyword can accidentally match a prefix of a longer word

Even with the ordering fixed, `'if'` alone will happily match the first two characters of
`iffy`, leaving `fy` to be parsed as whatever comes next &mdash, which is almost certainly
a syntax error. Instead, add a negative lookahead to ensure that the keyword isn't followed
by another word character:

```
keyword = (|'if' |'else' |'while') !\w
```

The lookahead `!\w` consumes nothing; it just checks that the next character (if any) isn't
a letter or digit, then lets the rest of the sequence continue from the same position.

## 3. Unbounded repetition needs its own stop condition {#3-unbounded-repetition-needs-its-own-stop-condition}

This one is quoted verbatim from `px.px`.
The rule for what's inside a Px `'...'` literal is:

```
literal =
	*(!['] literal_char)
```

`literal_char`'s final alternative is `[^\\\n]` &mdash; "any character except
backslash or newline." Read `*(!['] literal_char)` carefully: the `![']` makes
sure that the repetition stops when it finds a closing ' after any number of
complete `literal_char`s. The repetition would happily keep matching `[^\\\n]`
for every character from here to the end of the line (or the file), because
nothing ever tells it to stop. `!['']` supplies the stop condition: "keep going
*only while* the next character isn't a closing quote."
Any time you write `*X` where `X` can match almost anything, ask what makes it
stop &mdash; if the answer isn't already built into `X`, it needs a lookahead.

## 4. "Read until a delimiter"

The same shape shows up constantly and is worth naming on its own &mdash; a block
comment, for instance:

```
blockComment = '/*' *(!'*/' .) '*/'
```

`*(!'*/' .)` reads as "keep consuming any character, one at a time, for as long as the
next two characters aren't `*/`." This is the standard PEG idiom for "read until you
see this delimiter": guard the repeated atom with a negative lookahead for the
terminator, then consume one unit of content. It's different from pitfall 3 only in
that here the stop condition is a specific string rather than "not the boundary
character" &mdash; the underlying shape (lookahead first, consuming atom second) is
identical. In fact, you can use any PEG grammar rules in a lookahead, which makes
these grammars very powerful.

## 5. Lookahead content is never consumed

`px.px` defines end-of-file as:

```
EOF = !.
```

`.` is "any character", so `!.` means "succeed here only if there is *no* next
character." It's tempting to read a `.` (or any atom) inside `!`/`&` as if it were
part of what the rule matches, but it isn't: **whatever is inside a lookahead is
checked, never consumed, no matter what it is.** `s` (Px's "skip embedded whitespace"
rule) leans on this to avoid swallowing a semantically significant blank line:

```
s = *(!blankline space)
```

`s` repeats "a space character or comment, as long as we're not looking at a
blankline"; `blankline` itself is never part of what gets consumed; it's purely
a boundary check before every other kind of `space`.

## 6. `&`, the mirror image of `!`

`!A` fails if `A` matches; `&A` fails if `A` *doesn't*. Neither rule consumes any input,
but they can cause the calling rule to fail. Use this to prevent the parser proceeding
too far, when a different rule should match that input instead:

```
decimal_point = '.' &[0-9]	// a '.' immediately followed by a digit

member_dot    = '.' !\d	// a '.' NOT immediately followed by a digit
```

This example of a language mixing numeric literals (`3.14`) with member access (`obj.field`),
this lets a single `.` character choose the correct rule based on a one-character
lookahead, without either rule accidentally consuming what belongs to the other.

## 7. Left recursion {#left-recursion}

PEG parsers are implemented using recursive descent, which usually means that a rule
which calls itself must consume some input before that happens, or you get infinite
recursion.  This isn't a problem for Px. Every time a rule is invoked, Px ensures that
this same rule is not already active at the same position in the input. If it detects
that recursion, the rule path fails and may try other options.

```
expr = | expr '+' term | term      // wrong: infinite regress, no input consumed first
```

This grammar cannot ever match the '+', because the `expr` before it is recursive and
will fail. You can avoid this by implementing the repetition as an explicit loop, matching
the first term before repeating the rest:

```
expr = term *('+' term)
```
