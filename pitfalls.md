---
layout: default
title: Pitfalls & Lookahead
description: The mistakes ordered choice and greedy repetition invite, and how to fix them with & and !.
---

Every pitfall below follows directly from the two mechanics on the [PEG
primer](peg-primer.html) page: ordered choice never backtracks once an alternative
succeeds, and repetition is greedy with no backtracking either. Each one is shown with
a naive rule that goes wrong, and the fix using `&`/`!` lookahead. Several are quoted
directly from Px's own grammar, [`px.px`](https://github.com/cjheath/px/blob/main/px.px)
&mdash; Px is self-hosting, so its own grammar has had to solve every one of these.

## 1. Alternative order decides the winner

```
keyword = | 'if' | 'else' | 'while'

name    = [\a_] *[\w_]
```

If some other rule tries `name | keyword`, it will *never* reach `keyword`: `name`'s
pattern matches `if` perfectly well as a generic identifier, ordered choice takes that
match and moves on, and `keyword` never even gets tried. The fix is just to list the
more specific alternative first: `keyword | name`. This isn't a special case to
remember &mdash; it's *always* true that PEG alternatives should go from most to least
specific, because that's the only way ordered choice can express "prefer this."

## 2. A keyword can accidentally match a prefix of a longer word

Even with the ordering fixed, `'if'` alone will happily match the first two characters
of `iffy`, leaving `fy` to be parsed as whatever comes next &mdash; almost certainly a
syntax error, and a confusing one. The fix is a negative lookahead asserting the
keyword isn't immediately followed by another word character:

```
keyword = (|'if' |'else' |'while') !\w
```

`!\w` consumes nothing; it just checks that the next character (if any) isn't a letter
or digit, then lets the rest of the sequence continue from the same position.

## 3. Unbounded repetition needs its own stop condition {#3-unbounded-repetition-needs-its-own-stop-condition}

This one is quoted verbatim from `px.px`, because it's the exact bug this project's
own TextMate generator ran into (see [Syntax Highlighting](textmate-generator.html)).
The rule for what's inside a Px `'...'` literal is:

```
literal =
	*(!['] literal_char)
```

`literal_char`'s fallback alternative is `[^\\\n]` &mdash; "any character except
backslash or newline." Read `*(!['] literal_char)` carefully: the `![']` isn't
decoration, it's load-bearing. Without it &mdash; `*literal_char` on its own &mdash;
the repetition would happily keep matching `[^\\\n]` for every character from here to
the end of the line (or the file), because nothing ever tells it to stop. `!['']`
supplies the stop condition: "keep going *only while* the next character isn't a
closing quote." Any time you write `*X` where `X` can match almost anything, ask what
makes it stop &mdash; if the answer isn't already built into `X`, it needs a lookahead.

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
identical.

## 5. Lookahead content is never consumed

`px.px` defines end-of-file as:

```
EOF = !.
```

`.` is "any character" &mdash; so `!.` means "succeed here only if there is *no* next
character." It's tempting to read a `.` (or any atom) inside `!`/`&` as if it were
part of what the rule matches, but it isn't: **whatever is inside a lookahead is
checked, never consumed, no matter what it is.** `s` (Px's "skip embedded whitespace"
rule) leans on this to avoid swallowing a semantically significant blank line:

```
s = *(!blankline space)
```

`s` repeats "a space character or comment, as long as we're not looking at a
blankline" &mdash; `blankline` itself is never part of what gets consumed; it's purely
a boundary check run fresh at every position.

## 6. `&`, the mirror image of `!`

`!A` fails if `A` matches; `&A` fails if `A` *doesn't*. Both consume nothing either
way. `&` is the right tool whenever you need to confirm the *next* thing without
letting a different rule end up consuming it:

```
decimal_point = '.' &[0-9]	// a '.' immediately followed by a digit

member_dot    = '.' !\d	// a '.' NOT immediately followed by a digit
```

In a language mixing numeric literals (`3.14`) with member access (`obj.field`), this
pair lets a single `.` character route to the right rule based on nothing but a
one-character lookahead, without either rule accidentally consuming what belongs to
the other.

## 7. Left recursion {#left-recursion}

Px compiles grammars into recursive-descent parsers, so a rule can't be the very first
thing it calls on itself:

```
expr = | expr '+' term | term      // wrong: infinite regress, no input consumed first
```

Matching `expr` immediately tries to match `expr` again at the *same* position, before
consuming anything &mdash; recursive descent has no way to make progress here. Px's
`Peg::recurse()` (in `peg.h`) detects this at parse time by tracking each rule's start
position against its calling ancestors, and turns it into a parse failure instead of
an infinite loop. The fix is the same one every recursive-descent grammar uses: move
the repetition from recursion into an explicit loop, matching the first term before
repeating the rest:

```
expr = term *('+' term)
```
