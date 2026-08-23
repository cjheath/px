---
layout: default
title: Syntax Highlighting
description: Tagging a grammar with (as scope.name) and generating a TextMate grammar with -t.
---

`-t` compiles a grammar into a [TextMate
grammar](https://macromates.com/manual/en/language_grammars) &mdash; the JSON format
VS Code and many other editors use for syntax highlighting &mdash; from whichever
rules you've tagged with `(as scope.name)`, as described in [The Px
Language](language.html#rules-and-scope-annotations). Untagged rules are pure grammar,
invisible to `-t`, transparently inlined wherever a tagged rule calls them. When a
tagged rule calls *another* tagged rule, the callee keeps its own colour for its own
span &mdash; that's the only way to get more than one colour out of a single rule; Px
deliberately has no way to tag part of a rule's body.

```
px -t px.px > px.tmLanguage.json
```

## How a rule becomes a pattern

A tagged rule that doesn't call another tagged rule (inside a repetition, at least)
becomes a flat `match` pattern &mdash; its whole compiled regex, one colour. A rule
that does needs a `begin`/`end` region instead, so the callee can be `include`d with
its own colour in between.

The interesting case is a rule like Px's own `literal` &mdash; content only, no
delimiters of its own (see [pitfalls.md](pitfalls.html#3-unbounded-repetition-needs-its-own-stop-condition)
again: *any* unanchored content pattern will happily swallow text it shouldn't). Used
bare, `literal`'s pattern has no way to know where a real Px string literal starts.
The generator solves this the same way you would by hand: it scans the rest of the
grammar for how `literal` is actually *called* &mdash; finds `'\'' literal '\''` in
`atom` &mdash; and anchors it as `begin: "'"`, `end: "'"` instead of a bare match.
`class` gets the same treatment from `'[' class ']'`. This is exactly the lookahead
lesson from the pitfalls page, just applied by the generator instead of by you.

## The actual output

Here's what `px -t px.px` produces right now &mdash; eight tagged rules, none of them
warned-about or skipped:

```json
{
  "scopeName": "source.px",
  "patterns": [
    { "include": "#space" }, { "include": "#scope_annotation" },
    { "include": "#scope_name" }, { "include": "#label" },
    { "include": "#name" }, { "include": "#literal" },
    { "include": "#property" }, { "include": "#class" }
  ],
  "repository": {
    "literal": { "begin": "'", "end": "'", "name": "string.quoted.single.px" },
    "class":   { "begin": "\\[", "end": "\\]", "name": "constant.other.character-class.px" },
    "name":    { "match": "[A-Za-z_][\\w_]*", "name": "entity.name.function.px" }
  }
}
```

(trimmed for space &mdash; the full repository has all eight rules.)

## Live demo

Below is `px.px` highlighting *itself*, and `grammars/json.px` highlighting with the
exact same grammar (the tags live entirely in `px.px`; every `.px` file benefits for
free). The renderer is a small hand-written tokenizer implementing just the two
pattern shapes above &mdash; not a full Oniguruma engine, but enough to show precisely
what a real editor would do with this output.

<ul class="legend">
<li><span class="rule tok-comment">space</span><span class="scope">comment.line.double-slash.px</span></li>
<li><span class="rule tok-annotation">scope_annotation</span><span class="scope">keyword.other.annotation.px &mdash; whole (as ...) span, one colour</span></li>
<li><span class="rule tok-attr">scope_name</span><span class="scope">entity.other.attribute-name.px</span></li>
<li><span class="rule tok-annotation">label</span><span class="scope">keyword.other.annotation.px &mdash; shares scope_annotation's colour</span></li>
<li><span class="rule tok-function">name</span><span class="scope">entity.name.function.px &mdash; every identifier</span></li>
<li><span class="rule tok-string">literal</span><span class="scope">string.quoted.single.px</span></li>
<li><span class="rule tok-escape">property</span><span class="scope">constant.character.escape.px</span></li>
<li><span class="rule tok-class">class</span><span class="scope">constant.other.character-class.px</span></li>
</ul>

<script type="application/json" id="grammar-px">
{
  "fileTypes": [],
  "name": "Px",
  "patterns": [
    { "include": "#space" },
    { "include": "#scope_annotation" },
    { "include": "#scope_name" },
    { "include": "#label" },
    { "include": "#name" },
    { "include": "#literal" },
    { "include": "#property" },
    { "include": "#class" }
  ],
  "repository": {
    "class": {
      "begin": "\\[",
      "end": "\\]",
      "name": "constant.other.character-class.px"
    },
    "label": {
      "match": ":(?:[A-Za-z_][\\w_]*)",
      "name": "keyword.other.annotation.px"
    },
    "literal": {
      "begin": "'",
      "end": "'",
      "name": "string.quoted.single.px"
    },
    "name": {
      "match": "[A-Za-z_][\\w_]*",
      "name": "entity.name.function.px"
    },
    "property": {
      "match": "[adhswLU]",
      "name": "constant.character.escape.px"
    },
    "scope_annotation": {
      "match": "\\((?:(?:(?!(?:\\n[ \\t\\r]*(?:(?:\\n|(?!.)))))(?:[ \\t\\r\\n]|\\/\\/[^\\n]*))*)as(?:(?:(?!(?:\\n[ \\t\\r]*(?:(?:\\n|(?!.)))))(?:[ \\t\\r\\n]|\\/\\/[^\\n]*))*)(?:[A-Za-z_][\\w_.\\-]*)(?:(?:(?!(?:\\n[ \\t\\r]*(?:(?:\\n|(?!.)))))(?:[ \\t\\r\\n]|\\/\\/[^\\n]*))*)\\)(?:(?:(?!(?:\\n[ \\t\\r]*(?:(?:\\n|(?!.)))))(?:[ \\t\\r\\n]|\\/\\/[^\\n]*))*)",
      "name": "keyword.other.annotation.px"
    },
    "scope_name": {
      "match": "[A-Za-z_][\\w_.\\-]*",
      "name": "entity.other.attribute-name.px"
    },
    "space": {
      "match": "(?:[ \\t\\r\\n]|\\/\\/[^\\n]*)",
      "name": "comment.line.double-slash.px"
    }
  },
  "scopeName": "source.px"
}
</script>

<script type="text/plain" id="sample-pxpx">// Px grammar for the Px grammar-specification language.
// Px is a Backus-Naur form with operators in prefix position

EOF =					// End of file is where nothing follows
	!.

// space may include an end-of-line, or a //comment to eol
space (as comment.line.double-slash.px) =
	| [ \t\r\n]			// Any single whitespace
	| '//' *[^\n]			// including a comment to end-of-line

// A blankline is defined as two newlines separated by nothing but whitespace
blankline =
	'\n' *[ \t\r] (| '\n' | EOF)

// optional embedded whitespace is any amount of space not including a blank line
s =
	*(!blankline space)		// Any space not including a blankline

TOP =					// This grammar expects some rules
	?(BOM:bom)			// optional byte-order mark
	*space rule			// parse one rule at a time
	-> rule

BOM	=
	  '\uFEFF'

rule =
	name s ?scope_annotation '=' s	// A named rule matches...
	alternates ?action		// alternates perhaps followed by an action
	blankline s			// and ends in a blank line
	-> name, scope_annotation, alternates, action

scope_annotation (as keyword.other.annotation.px) =	// Assigns a TextMate scope name to colour this rule's matches
	'(' s 'as' s scope_name s ')' s
	-> scope_name

scope_name (as entity.other.attribute-name.px) =	// A dotted TextMate scope name, e.g. keyword.control.px
	[\a_] *[\w_.\-]</script>

<script type="text/plain" id="sample-json">// JSON grammar in Px. See http://json.org/ and ECMA-262 Ed.5
// Unicode uses 16-bit format, so Emoji's require a surrogate pair, see ISO/IEC 10646
// High surrogate first: D800 to DBFF, Low surrogate: D800 to DBFF
// https://www.ecma-international.org/wp-content/uploads/ECMA-404_2nd_edition_december_2017.pdf

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

number	= ?'-' (|'0' | [1-9] *[0-9]) ?('.' +[0-9]) ?([eE] ?[-+] +[0-9])</script>

<div class="demo">
<div class="demo-code" id="out-pxpx"></div>
</div>

<div class="demo">
<div class="demo-code" id="out-json"></div>
</div>

<script>
(function () {
  "use strict";

  function readJSON(id) { return JSON.parse(document.getElementById(id).textContent); }
  function readText(id) { return document.getElementById(id).textContent; }
  function escapeHtml(s) {
    return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
  }

  var SCOPE_CLASS = {
    space: "tok-comment",
    scope_annotation: "tok-annotation",
    scope_name: "tok-attr",
    label: "tok-annotation",
    literal: "tok-string",
    "class": "tok-class",
    property: "tok-escape",
    name: "tok-function"
  };

  // Implements exactly the two pattern shapes px -t can emit: a flat "match", or a
  // "begin"/"end" region with nested "patterns" includes. Zero-length matches are
  // skipped so a pattern like literal's (?:...)* can never stall the scan.
  function buildTokenizer(grammar) {
    var repo = grammar.repository;
    var rootNames = grammar.patterns.map(function (p) { return p.include.slice(1); });

    function earliest(names, text, from) {
      var best = null;
      for (var i = 0; i < names.length; i++) {
        var name = names[i], pat = repo[name], src = pat.match || pat.begin, re;
        try { re = new RegExp(src, "g"); } catch (e) { continue; }
        re.lastIndex = from;
        var m = re.exec(text);
        while (m && m[0].length === 0) {
          re.lastIndex = m.index + 1;
          m = re.lastIndex <= text.length ? re.exec(text) : null;
        }
        if (m && (!best || m.index < best.m.index)) best = { name: name, pat: pat, m: m };
      }
      return best;
    }

    return function tokenize(text) {
      var tokens = [], pos = 0, stack = [];
      while (pos < text.length) {
        if (stack.length) {
          var region = stack[stack.length - 1], endRe, endM = null;
          try { endRe = new RegExp(region.pat.end, "g"); } catch (e) { endRe = null; }
          if (endRe) {
            endRe.lastIndex = pos; endM = endRe.exec(text);
            while (endM && endM[0].length === 0) {
              endRe.lastIndex = endM.index + 1;
              endM = endRe.lastIndex <= text.length ? endRe.exec(text) : null;
            }
          }
          var nested = null;
          if (region.pat.patterns) {
            nested = earliest(region.pat.patterns.map(function (p) { return p.include.slice(1); }), text, pos);
          }
          if (nested && (!endM || nested.m.index < endM.index)) {
            if (nested.m.index > pos) tokens.push([pos, nested.m.index, region.name]);
            tokens.push([nested.m.index, nested.m.index + nested.m[0].length, nested.name]);
            pos = nested.m.index + nested.m[0].length;
            continue;
          }
          if (endM) {
            if (endM.index > pos) tokens.push([pos, endM.index, region.name]);
            tokens.push([endM.index, endM.index + endM[0].length, region.name]);
            pos = endM.index + endM[0].length;
            stack.pop();
            continue;
          }
          tokens.push([pos, text.length, region.name]);
          pos = text.length;
          continue;
        }
        var best = earliest(rootNames, text, pos);
        if (!best) break;
        if (best.m.index > pos) tokens.push([pos, best.m.index, null]);
        var start = best.m.index, end = best.m.index + best.m[0].length;
        tokens.push([start, end, best.name]);
        pos = end;
        if (best.pat.begin) stack.push({ name: best.name, pat: best.pat });
      }
      if (pos < text.length) tokens.push([pos, text.length, null]);
      return tokens;
    };
  }

  function render(elId, text, grammar) {
    var tokens = buildTokenizer(grammar)(text), html = "";
    for (var i = 0; i < tokens.length; i++) {
      var t = tokens[i], chunk = escapeHtml(text.slice(t[0], t[1])), cls = t[2] ? SCOPE_CLASS[t[2]] : null;
      html += cls ? '<span class="' + cls + '">' + chunk + "</span>" : chunk;
    }
    document.getElementById(elId).innerHTML = html;
  }

  var GRAMMAR = readJSON("grammar-px");
  render("out-pxpx", readText("sample-pxpx"), GRAMMAR);
  render("out-json", readText("sample-json"), GRAMMAR);
})();
</script>

## Using the output

The generated `.tmLanguage.json` is a standard TextMate grammar. VS Code loads it
through the smallest possible kind of extension &mdash; no code, just a
`package.json` declaring the language and pointing at the grammar file. Many other
editors (Sublime Text, JetBrains via a plugin, the CodeMirror/Monaco
`vscode-textmate` library) consume the same format directly.

### Building the extension folder

```
mkdir -p px-syntax/syntaxes
px -t px.px > px-syntax/syntaxes/px.tmLanguage.json
```

`px-syntax/package.json`:

```json
{
  "name": "px-syntax",
  "displayName": "Px",
  "version": "0.0.1",
  "engines": { "vscode": "^1.60.0" },
  "contributes": {
    "languages": [
      {
        "id": "px",
        "extensions": [".px"],
        "aliases": ["Px"]
      }
    ],
    "grammars": [
      {
        "language": "px",
        "scopeName": "source.px",
        "path": "./syntaxes/px.tmLanguage.json"
      }
    ]
  }
}
```

`scopeName` here has to match the `"scopeName"` field *inside* the generated JSON
exactly &mdash; `-t` derives it from the grammar's base filename (`px.px` &rarr;
`source.px`), so if you rename the `.px` file, regenerate before re-testing.

### Installing it for local testing

The fastest loop while you're iterating &mdash; no packaging step, no marketplace
&mdash; is to drop the folder straight into VS Code's extensions directory and
reload:

```
ln -s "$(pwd)/px-syntax" ~/.vscode/extensions/px-syntax-0.0.1
```

(`%USERPROFILE%\.vscode\extensions` on Windows; VS Code Insiders and other variants
use their own sibling folder.) Then in VS Code: **Cmd/Ctrl+Shift+P** &rarr;
**Developer: Reload Window**, open any `.px` file, and the colours above should be
exactly what you see. Every time you regenerate `px.tmLanguage.json`, reload the
window again to pick up the change &mdash; no reinstall needed, since it's a symlink.

### Packaging it properly

Once it's worth sharing, package it into a real `.vsix` with Microsoft's own
extension CLI:

```
npm install -g @vscode/vsce
cd px-syntax
vsce package                              # writes px-syntax-0.0.1.vsix
code --install-extension px-syntax-0.0.1.vsix
```

That `.vsix` is also what you'd upload to the VS Code Marketplace, or hand to someone
else to install the same way.
