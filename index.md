---
layout: default
title: A PEG parser generator language
description: Px is a grammar-description language and compiler for parsers and code generators.
---

Px is a grammar-description language and compiler for parsers and code generators. You
write one grammar, in Px, and Px compiles it into whichever of these you need:

- a **C++ recursive-descent parser**, built on a small `Peg<>` template, that parses
  text straight into an abstract syntax tree;
- **railroad diagrams** documenting the grammar visually;
- a **TextMate grammar** (the format VS Code and many other editors use) so files
  written in your language get syntax highlighting.

Px grammars are [PEGs](peg-primer.html) (Parsing Expression Grammars) written in a
[BNF](https://en.wikipedia.org/wiki/Backus%E2%80%93Naur_form)-like notation, but with
repetition operators (`?` `*` `+`) and lookahead assertions (`&` `!`) moved into
*prefix* position, ahead of what they apply to, rather than trailing it. This copies
the underlying Pegexp library, which doesn't need to pre-compile the matching expressions
in order to achieve good performance, as is the case with most regular expression
implementations.

An example of a rule is this `number` rule from Px's own [JSON grammar](https://github.com/cjheath/px/blob/main/grammars/json.px):

```
number	= ?'-' (|'0' | [1-9] *[0-9]) ?('.' +[0-9]) ?([eE] ?[-+] +[0-9])
```

Read left to right: an optional `-`, then either a bare `0` or a non-zero digit
followed by more digits, then an optional `.` followed by digits, then an optional
exponent. It's just a different way to write a regular expression. Or as we will
see, a non-regular one, because a rule can call itself directly or through other rules.

## Where to start

- **[PEG Primer](peg-primer.html)** &mdash; what a PEG actually is, and the one
  mechanic (ordered choice, no backtracking across alternatives) that explains almost
  everything else on this site.
- **[Pitfalls & Lookahead](pitfalls.html)** &mdash; the mistakes that ordered choice
  and greedy repetition invite, worked through with real rules from Px's own
  grammar, and how `&`/`!` lookahead assertions fix each one.
- **[The Px Language](language.html)** &mdash; the full reference: rules, atoms,
  operators, captures, and the `(as scope.name)` syntax-colouring annotation.
- **[C++ Generator](cpp-generator.html)** &mdash; turning a grammar into a working
  `Peg<>`-based parser, and walking the AST it produces.
- **[Railroad Diagrams](railroad-generator.html)** &mdash; generating and reading
  visual grammar documentation.
- **[Syntax Highlighting](textmate-generator.html)** &mdash; tagging a grammar so
  Px can generate a TextMate grammar for it, with a live demo.
