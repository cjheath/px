---
layout: default
title: Railroad Diagrams
description: Generating visual grammar documentation with -r, and reading it.
railroad: true
---

`-r` compiles a grammar into a self-contained HTML page of railroad (syntax) diagrams,
one per rule, using [tabatkins/railroad-diagrams](https://github.com/tabatkins/railroad-diagrams):

```
px -r px.px > px-grammar.html
```

Whitespace-only helper rules (like Px's own `s`) clutter a diagram set without adding
anything worth reading &mdash; `-x rulename` excludes one, and can be repeated:

```
px -r -x s -x space -x blankline px.px > px-grammar.html
```

The output is a `<dl>` of `<dt>` rule names next to a `<dd>` containing a `<script>`
that draws that rule's diagram &mdash; it needs `railroad-diagrams.js` and its
stylesheet loaded on the page (this site vendors a copy under `assets/`, from
[`strpp/scripts`](https://github.com/cjheath/strpp/tree/main/scripts) &mdash; CC0,
by Tab Atkins Jr.; the generated HTML itself expects them at a relative `../scripts/`
path, which you can adjust to wherever you keep a local copy).

## Live, generated straight from `px.px`

This is the *actual* output of `px -r px.px` for four rules &mdash; a top-level `rule`,
the `scope_annotation` it can carry, the six kinds of `atom`, and `literal` (notice the
`![\']` terminal: that's the stop-condition lookahead from
[pitfalls.md](pitfalls.html#3-unbounded-repetition-needs-its-own-stop-condition), drawn
as a rail you can literally see the parser refusing to take).

<div class="demo">
<script>
var PxRailroads = {
  rule:
    ComplexDiagram(Sequence(NonTerminal('name', {href: '#'}), Optional(NonTerminal('scope_annotation', {href: '#scope_annotation'})), Terminal('='), NonTerminal('alternates', {href: '#'}), Optional(NonTerminal('action', {href: '#'})), NonTerminal('blankline', {href: '#'}))),
  scope_annotation:
    ComplexDiagram(Sequence(Terminal('('), Terminal('as'), NonTerminal('scope_name', {href: '#'}), Terminal(')'))),
  atom:
    ComplexDiagram(Choice(0, Terminal('.'), NonTerminal('name', {href: '#'}), Sequence(Terminal('\\'), NonTerminal('property', {href: '#'})), Sequence(Terminal('\''), NonTerminal('literal', {href: '#literal'}), Terminal('\'')), Sequence(Terminal('['), NonTerminal('class', {href: '#'}), Terminal(']')), Sequence(Terminal('('), NonTerminal('group', {href: '#'}), Terminal(')')))),
  literal:
    ComplexDiagram(ZeroOrMore(Sequence(Terminal('![\']'), NonTerminal('literal_char', {href: '#'}))))
};
</script>
<dl>
<dt id="rule"><code>rule</code></dt>
  <dd><script>PxRailroads.rule.addTo();</script></dd>
<dt id="scope_annotation"><code>scope_annotation</code></dt>
  <dd><script>PxRailroads.scope_annotation.addTo();</script></dd>
<dt id="atom"><code>atom</code></dt>
  <dd><script>PxRailroads.atom.addTo();</script></dd>
<dt id="literal"><code>literal</code></dt>
  <dd><script>PxRailroads.literal.addTo();</script></dd>
</dl>
</div>

Each diagram reads left to right along the main rail; a side loop is a repetition, a
fork is a choice, and a labelled box that links elsewhere is a call to another rule
&mdash; `atom`'s six side-by-side branches are the same six-way ordered choice from
[The Px Language](language.html#atoms), just drawn instead of tabulated.
