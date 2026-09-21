/*
 * TextMate grammar generator for a parser (IDE syntax highlighting)
 *
 * Only rules tagged with a "(as scope.name)" annotation (see px.px: scope_annotation)
 * produce output. An untagged rule is transparent: whenever a tagged rule calls it,
 * its own expression is flattened (inlined) into the caller's regex. A tagged rule
 * that calls another *tagged* rule can't be flattened that way (the callee needs its
 * own distinct colouring), so it is instead represented as a TextMate begin/end
 * pattern with the callee included via the repository.
 *
 * This is necessarily a heuristic, best-effort translation: TextMate's model (a
 * lexer with a stack of line-scoped regexes) is much weaker than a PEG. Rules that
 * don't fit are skipped with a warning on stderr, in the spirit of px.cpp's own
 * check_rules() warnings.
 */
#include	<px_parser.h>

#include	<strval.h>
#include	<variant.h>
#include	<char_encoding.h>

#include	<cctype>
#include	<cstring>

#include	<px_textmate.h>

typedef	CowMap<bool>	StringSet;

/*
 * Interpreting Px's literal/class backslash escapes.
 * This mirrors (deliberately, not by sharing code - see px_pegexp.cpp/px_railroad.cpp
 * for the same duplication) the escape handling already used by the other generators.
 */
static UCS4 interpret_backslash(const UTF8*& cp)
{
	ErrNum		e;
	ErrBuf::MsgSequence	at;	// Where the last tolerated report began
	unsigned int	i;
	UCS4		ch = UTF8Get(cp);

	if (ch == '0')				// Octal
	{
		StrBody	temp_body(cp, StrStatic, 3);
		at = ErrCheckpoint();
		UCS4	v = StrVal(&temp_body).asInt32(&e, 8, &i);
		if (STRERR_TRAIL_TEXT == e)
		{
			ErrRollback(at);
			e = 0;		// The fixed-width slice ran out, as it should
		}
		if (e == 0)
		{ cp += i; return v; }
		return ch;
	}
	if (ch == 'x')				// Hex, \xHH or \x{...}
	{
		if (*cp == '{')
		{
			StrBody	temp_body(cp+1, StrStatic, 8);
			at = ErrCheckpoint();
			UCS4	v = StrVal(&temp_body).asInt32(&e, 16, &i);
			if (STRERR_TRAIL_TEXT == e)
			{
				ErrRollback(at);
				e = 0;		// The fixed-width slice ran out, as it should
			}
			if (e == 0)
				cp += i + 2;
			return v;
		}
		StrBody	temp_body(cp, StrStatic, 2);
		at = ErrCheckpoint();
		UCS4	v = StrVal(&temp_body).asInt32(&e, 16, &i);
		if (STRERR_TRAIL_TEXT == e)
		{
			ErrRollback(at);
			e = 0;		// The fixed-width slice ran out, as it should
		}
		if (e == 0)
			cp += i;
		return v;
	}
	if (ch == 'u')				// Unicode, ሴ or \u{...}
	{
		if (*cp == '{')
		{
			StrBody	temp_body(cp+1, StrStatic, 8);
			at = ErrCheckpoint();
			UCS4	v = StrVal(&temp_body).asInt32(&e, 16, &i);
			if (STRERR_TRAIL_TEXT == e)
			{
				ErrRollback(at);
				e = 0;		// The fixed-width slice ran out, as it should
			}
			if (e == 0)
				cp += i + 2;
			return v;
		}
		StrBody	temp_body(cp, StrStatic, 4);
		at = ErrCheckpoint();
		UCS4	v = StrVal(&temp_body).asInt32(&e, 16, &i);
		if (STRERR_TRAIL_TEXT == e)
		{
			ErrRollback(at);
			e = 0;		// The fixed-width slice ran out, as it should
		}
		if (e == 0)
			cp += i;
		return v;
	}

	switch (ch)
	{
	case 'n': return '\n';
	case 't': return '\t';
	case 'r': return '\r';
	case 'b': return '\b';
	case 'f': return '\f';
	}
	return ch;				// A backslashed char otherwise means itself
}

/*
 * Turn the raw text of a Px 'literal' into Oniguruma/PCRE regex source text.
 * Px allows a character property escape inside a string literal, not just in a
 * class or as a standalone atom (see README: "Strings and classes may use: a
 * character property escape") - e.g. 'x\h' means the letter x followed by one hex
 * digit, not the three literal characters x, \, h.
 */
StrVal regex_escape_literal(StrVal literal)
{
	static	char	ubuf[24];
	static	auto	pcre_special = ".^$*+?()[]{}|\\/";

	literal.transform(
		[&](const UTF8*& cp, const UTF8* ep) -> StrVal
		{
			UCS4	ch = UTF8Get(cp);
			if (ch == '\\')
			{
				const UTF8*	save = cp;
				UCS4		esc = UTF8Get(cp);
				switch (esc)
				{
				case 'a': return "[A-Za-z]";
				case 'h': return "[0-9A-Fa-f]";
				case 'L': return "[a-z]";
				case 'U': return "[A-Z]";
				case 'd': case 's': case 'w':
					return StrVal("\\")+esc;
				}
				cp = save;
				ch = interpret_backslash(cp);
			}

			switch (ch)
			{
			case '\n': return "\\n";
			case '\t': return "\\t";
			case '\r': return "\\r";
			case '\f': return "\\f";
			case '\b': return "\\x08";	// Outside a class, PCRE's \b means word-boundary
			}
			if (ch < ' ' || ch == 0x7F)
			{
				return StrVal::format("\\x{1:X02}", VariantArray() << ch);
			}
			if (ch < 0x80 && strchr(pcre_special, (char)ch))
				return StrVal("\\")+ch;

			return ch;
		}
	);
	return literal;
}

/*
 * Turn the raw text of a Px 'class' body (already stripped of the enclosing [ ]) into
 * an Oniguruma/PCRE character class body, translating Px's own property escapes.
 */
StrVal regex_translate_class(StrVal body)
{
	StrVal	prefix;
	if (body.length() > 0 && body[0] == '^')
	{
		prefix = "^";
		body = body.substr(1);
	}

	static	char	ubuf[24];
	static	auto	pcre_class_special = "]\\^-";

	body.transform(
		[&](const UTF8*& cp, const UTF8* ep) -> StrVal
		{
			UCS4	ch = UTF8Get(cp);
			if (ch != '\\')
			{
				if (ch < ' ' || ch == 0x7F)
					return StrVal::format("\\x{1:X02}", VariantArray() << ch);
				return ch;
			}

			const UTF8*	save = cp;
			UCS4		esc = UTF8Get(cp);
			switch (esc)		// Px-specific properties; PCRE doesn't have these letters spare
			{
			case 'a': return "A-Za-z";
			case 'h': return "0-9A-Fa-f";
			case 'L': return "a-z";
			case 'U': return "A-Z";
			case 'd': case 's': case 'w':	// same meaning in PCRE, keep as-is
				return StrVal("\\")+esc;
			}

			cp = save;		// Not a property; interpret as an ordinary escape
			UCS4	ch2 = interpret_backslash(cp);
			switch (ch2)
			{
			case '\n': return "\\n";
			case '\t': return "\\t";
			case '\r': return "\\r";
			case '\f': return "\\f";
			case '\b': return "\\b";	// Inside a class, PCRE's \b IS backspace
			}
			if (ch2 < ' ' || ch2 == 0x7F)
				return StrVal::format("\\x{1:X02}", VariantArray() << ch2);
			if (ch2 < 0x80 && strchr(pcre_class_special, (char)ch2))
				return StrVal("\\")+ch2;
			return ch2;
		}
	);
	return StrVal("[")+prefix+body+"]";
}

/*
 * Flattening a Px expression into a single Oniguruma/PCRE regex.
 * Only used once the caller has established (via scan_shape, below) that the
 * expression contains no call to a *tagged* rule and isn't recursive.
 */
static StrVal flatten_re(Variant re, StrVariantMap& rule_by_name, StringSet& visiting);
static StrVal flatten_atom(StrVariantMap atom, StrVariantMap& rule_by_name, StringSet& visiting);

static bool regex_is_atomic(const StrVal& r)		// Would a repeat suffix apply to all of r unambiguously?
{
	int	len = r.length();
	if (len == 1)
		return true;
	if (len == 2 && r[0] == '\\')
		return true;				// \d \s \w \n ...
	if (len >= 2 && r[0] == '[' && r[len-1] == ']')
		return true;
	if (len >= 2 && r[0] == '(' && r[len-1] == ')')
		return true;				// Already a group, from flatten_atom below
	return false;
}

static StrVal regex_group_if_needed(const StrVal& r)
{
	if (regex_is_atomic(r))
		return r;
	return StrVal("(?:")+r+")";
}

static StrVal flatten_repeat_suffix(Variant repeat_count, StrVal atom_regex)
{
	if (repeat_count.type() == Variant::None)
		return atom_regex;

	Variant	limit_v = repeat_count.as_variant_map()["limit"];

	if (limit_v.type() == Variant::String)
	{
		UCS4	op = limit_v.as_strval()[0];
		switch (op)
		{
		case '?': return regex_group_if_needed(atom_regex)+"?";
		case '*': return regex_group_if_needed(atom_regex)+"*";
		case '+': return regex_group_if_needed(atom_regex)+"+";
		case '&': return StrVal("(?=")+atom_regex+")";
		case '!': return StrVal("(?!")+atom_regex+")";
		}
		return atom_regex;
	}

	/*
	 * A bounded {n} count; only the literal-digit form is representable in a static regex.
	 * A named/variable count (a Px feature with no static regex equivalent) is approximated
	 * as unbounded.
	 */
	StrVal	val = limit_v.as_variant_map()["val"].as_strval();
	bool	numeric = val.length() > 0;
	for (int i = 0; numeric && i < val.length(); i++)
		if (!isdigit((int)val[i]))
			numeric = false;
	if (numeric)
		return regex_group_if_needed(atom_regex)+"{"+val+"}";
	return regex_group_if_needed(atom_regex)+"*";
}

static StrVal flatten_repetition(StrVariantMap repetition, StrVariantMap& rule_by_name, StringSet& visiting)
{
	StrVariantMap	atom = repetition["atom"].as_variant_map();
	Variant		repeat_count = repetition["repeat_count"];
	StrVal		atom_regex = flatten_atom(atom, rule_by_name, visiting);
	return flatten_repeat_suffix(repeat_count, atom_regex);
}

static StrVal flatten_sequence(VariantArray repetitions, StrVariantMap& rule_by_name, StringSet& visiting)
{
	StrVal	ret;
	for (int i = 0; i < repetitions.length(); i++)
		ret += flatten_repetition(repetitions[i].as_variant_map(), rule_by_name, visiting);
	return ret;
}

static StrVal flatten_atom(StrVariantMap atom, StrVariantMap& rule_by_name, StringSet& visiting)
{
	auto	entry = atom.begin();
	StrVal	node_type = entry->first;
	Variant	element = entry->second;

	if (node_type == "any")
		return ".";
	else if (node_type == "call")
	{
		StrVal	callee = element.as_strval();
		if (visiting[callee] || !rule_by_name.contains(callee))
			return "";		// Guarded against by the caller; defensive only
		StrVariantMap	crule = rule_by_name[callee].as_variant_map();
		visiting.put(callee, true);
		StrVal	inlined = flatten_re(crule["alternates"], rule_by_name, visiting);
		visiting.put(callee, false);
		return regex_group_if_needed(inlined);
	}
	else if (node_type == "property")
	{
		switch ((UCS4)element.as_strval()[0])
		{
		case 'a': return "[A-Za-z]";
		case 'd': return "\\d";
		case 'h': return "[0-9A-Fa-f]";
		case 's': return "\\s";
		case 'w': return "\\w";
		case 'L': return "[a-z]";
		case 'U': return "[A-Z]";
		}
		return "";
	}
	else if (node_type == "literal")
		return regex_escape_literal(element.as_strval());
	else if (node_type == "class")
		return regex_translate_class(element.as_strval());
	else if (node_type == "group")
	{
		VariantArray	alternates = element.as_variant_map()["alternates"].as_variant_array();
		return StrVal("(?:")+flatten_re(alternates[0], rule_by_name, visiting)+")";
	}
	return "";
}

static StrVal flatten_re(Variant re, StrVariantMap& rule_by_name, StringSet& visiting)
{
	if (re.type() == Variant::StrVarMap)
	{
		StrVariantMap	map = re.as_variant_map();
		auto		entry = map.begin();
		StrVal		node_type = entry->first;
		Variant		element = entry->second;

		if (node_type == "sequence")
			return flatten_re(element, rule_by_name, visiting);
		else if (node_type == "repetition")
			return flatten_sequence(element.as_variant_array(), rule_by_name, visiting);
		return "";	// Not reached: any/call/property/literal/class/group go via flatten_atom
	}
	else if (re.type() == Variant::VarArray)	// Multiple |alternates
	{
		VariantArray	va = re.as_variant_array();
		StrVal	ret;
		for (int i = 0; i < va.length(); i++)
		{
			if (i)
				ret += "|";
			ret += flatten_re(va[i], rule_by_name, visiting);
		}
		return StrVal("(?:")+ret+")";
	}
	return "";
}

/*
 * Deciding whether a tagged rule can be a flat "match", or needs "begin"/"end".
 */
struct ShapeInfo {
	bool	has_tagged_call = false;	// Calls (directly, or via inlined untagged rules) a tagged rule
	bool	has_unbounded_any = false;	// Contains '.' (which, per Px, matches a newline) under */+
	bool	inexpressible = false;		// Uses a feature with no static-regex equivalent
};

static void scan_shape(
	Variant		re,
	StrVariantMap&	rule_by_name,
	StringSet&	tagged,
	StringSet&	visiting,
	ShapeInfo&	info,
	bool		under_unbounded)
{
	if (re.type() == Variant::StrVarMap)
	{
		StrVariantMap	map = re.as_variant_map();
		auto		entry = map.begin();
		StrVal		node_type = entry->first;
		Variant		element = entry->second;

		if (node_type == "sequence")
			scan_shape(element, rule_by_name, tagged, visiting, info, under_unbounded);
		else if (node_type == "repetition")
		{
			VariantArray	repetitions = element.as_variant_array();
			for (int i = 0; i < repetitions.length(); i++)
			{
				StrVariantMap	repetition = repetitions[i].as_variant_map();
				Variant		atom = repetition["atom"];
				Variant		repeat_count = repetition["repeat_count"];
				bool		unbounded = under_unbounded;
				if (repeat_count.type() != Variant::None)
				{
					Variant	limit = repeat_count.as_variant_map()["limit"];
					if (limit.type() == Variant::String)
					{
						UCS4	op = limit.as_strval()[0];
						if (op == '*' || op == '+')
							unbounded = true;
						else if (op == '!' || op == '&')
							unbounded = false;	// A lookahead's content is never itself consumed/spanned
					}
					else
					{
						StrVal	val = limit.as_variant_map()["val"].as_strval();
						bool	numeric = val.length() > 0;
						for (int k = 0; numeric && k < val.length(); k++)
							if (!isdigit((int)val[k]))
								numeric = false;
						if (!numeric)
							info.inexpressible = true;
					}
				}
				scan_shape(atom, rule_by_name, tagged, visiting, info, unbounded);
			}
		}
		else if (node_type == "group")
		{
			VariantArray	alternates = element.as_variant_map()["alternates"].as_variant_array();
			scan_shape(alternates[0], rule_by_name, tagged, visiting, info, under_unbounded);
		}
		else if (node_type == "any")
		{
			if (under_unbounded)
				info.has_unbounded_any = true;
		}
		else if (node_type == "call")
		{
			/*
			 * A tagged callee occurring under an unbounded repetition is a genuinely
			 * distinguishable repeating token (e.g. `escape` inside `string`'s `*(...)`)
			 * and gets its own colour via a nested include. A tagged callee at a FIXED,
			 * non-repeated position (e.g. `name` inside `label`'s `':' name`) is just one
			 * component of a fixed-shape construct: it's inlined like any other call, so
			 * the whole construct takes the containing rule's own colour uniformly.
			 */
			StrVal	callee = element.as_strval();
			if (tagged.contains(callee) && under_unbounded)
				info.has_tagged_call = true;
			if (!visiting[callee] && rule_by_name.contains(callee))
			{
				visiting.put(callee, true);
				StrVariantMap	crule = rule_by_name[callee].as_variant_map();
				scan_shape(crule["alternates"], rule_by_name, tagged, visiting, info, under_unbounded);
				visiting.put(callee, false);
			}
		}
		// "property", "literal", "class": nothing to check
	}
	else if (re.type() == Variant::VarArray)
	{
		VariantArray	va = re.as_variant_array();
		for (int i = 0; i < va.length(); i++)
			scan_shape(va[i], rule_by_name, tagged, visiting, info, under_unbounded);
	}
}

// Gather the names of all rules called (directly) anywhere within `re`.
static void collect_calls(Variant re, StringSet& calls)
{
	if (re.type() == Variant::StrVarMap)
	{
		StrVariantMap	map = re.as_variant_map();
		auto		entry = map.begin();
		StrVal		node_type = entry->first;
		Variant		element = entry->second;
		if (node_type == "sequence")
			collect_calls(element, calls);
		else if (node_type == "repetition")
		{
			VariantArray	reps = element.as_variant_array();
			for (int i = 0; i < reps.length(); i++)
				collect_calls(reps[i].as_variant_map()["atom"], calls);
		}
		else if (node_type == "group")
		{
			VariantArray	alternates = element.as_variant_map()["alternates"].as_variant_array();
			for (int i = 0; i < alternates.length(); i++)
				collect_calls(alternates[i], calls);
		}
		else if (node_type == "call")
			calls.put(element.as_strval(), true);
	}
	else if (re.type() == Variant::VarArray)
	{
		VariantArray	va = re.as_variant_array();
		for (int i = 0; i < va.length(); i++)
			collect_calls(va[i], calls);
	}
}

// Is `target` reachable (directly or indirectly) by following calls from `from`?
static bool can_reach(const StrVal& target, const StrVal& from, StrVariantMap& rule_by_name, StringSet& visited)
{
	if (visited.contains(from))
		return false;
	visited.put(from, true);
	if (from == target)
		return true;
	if (!rule_by_name.contains(from))
		return false;

	StrVariantMap	rule = rule_by_name[from].as_variant_map();
	StringSet	local_calls;
	collect_calls(rule["alternates"], local_calls);

	for (auto it = local_calls.begin(); it != local_calls.end(); it++)
		if (can_reach(target, it->first, rule_by_name, visited))
			return true;
	return false;
}

// Is `name`, directly or indirectly, one of its own callees?
static bool rule_is_recursive(const StrVal& name, StrVariantMap& rule_by_name)
{
	StrVariantMap	rule = rule_by_name[name].as_variant_map();
	StringSet	local_calls;
	collect_calls(rule["alternates"], local_calls);

	for (auto it = local_calls.begin(); it != local_calls.end(); it++)
	{
		StringSet	visited;
		if (can_reach(name, it->first, rule_by_name, visited))
			return true;
	}
	return false;
}

/*
 * Auto-inferring begin/end delimiters for a rule that can't be a flat "match"
 */
struct DelimSearch {
	bool	found;
	StrVal	end_regex;
};

/*
 * Recognise the idiomatic PEG "read until delimiter" shape: *(!X ...) - X is already exactly
 * the closing delimiter, no positional guessing required.
 */
static DelimSearch find_guard_delimiter(StrVariantMap outer_repetition, StrVariantMap& rule_by_name, StringSet& visiting)
{
	DelimSearch	result{false, StrVal()};

	StrVariantMap	atom = outer_repetition["atom"].as_variant_map();
	auto		entry = atom.begin();
	StrVal		node_type = entry->first;
	Variant		atom_element = entry->second;
	if (node_type != "group")
		return result;

	VariantArray	alternates = atom_element.as_variant_map()["alternates"].as_variant_array();
	if (alternates.length() != 1)
		return result;			// Only the simple single-alternative group is handled

	Variant		seq = alternates[0];
	if (seq.type() != Variant::StrVarMap || seq.as_variant_map().begin()->first != "sequence")
		return result;
	Variant		seq_body = seq.as_variant_map().begin()->second;
	if (seq_body.type() != Variant::StrVarMap)
		return result;			// Multiple |alternatives inside the group: not handled here

	VariantArray	inner_reps = seq_body.as_variant_map()["repetition"].as_variant_array();
	if (inner_reps.length() == 0)
		return result;

	StrVariantMap	first = inner_reps[0].as_variant_map();
	Variant		rc = first["repeat_count"];
	if (rc.type() == Variant::None)
		return result;
	Variant		limit = rc.as_variant_map()["limit"];
	if (limit.type() != Variant::String || limit.as_strval()[0] != '!')
		return result;

	result.found = true;
	result.end_regex = flatten_atom(first["atom"].as_variant_map(), rule_by_name, visiting);
	return result;
}

// Collect (in order, deduplicated) the names of tagged rules called anywhere within `re`.
static void collect_tagged_calls(Variant re, StringSet& tagged, StringArray& found)
{
	if (re.type() == Variant::StrVarMap)
	{
		StrVariantMap	map = re.as_variant_map();
		auto		entry = map.begin();
		StrVal		node_type = entry->first;
		Variant		element = entry->second;

		if (node_type == "sequence")
			collect_tagged_calls(element, tagged, found);
		else if (node_type == "repetition")
		{
			VariantArray	reps = element.as_variant_array();
			for (int i = 0; i < reps.length(); i++)
				collect_tagged_calls(reps[i].as_variant_map()["atom"], tagged, found);
		}
		else if (node_type == "group")
		{
			VariantArray	alternates = element.as_variant_map()["alternates"].as_variant_array();
			for (int i = 0; i < alternates.length(); i++)
				collect_tagged_calls(alternates[i], tagged, found);
		}
		else if (node_type == "call")
		{
			StrVal	name = element.as_strval();
			if (tagged.contains(name))
			{
				bool	already = false;
				for (int i = 0; i < found.length() && !already; i++)
					already = found[i] == name;
				if (!already)
					found.append(name);
			}
		}
	}
	else if (re.type() == Variant::VarArray)
	{
		VariantArray	va = re.as_variant_array();
		for (int i = 0; i < va.length(); i++)
			collect_tagged_calls(va[i], tagged, found);
	}
}

/*
 * Discovering delimiters from how a rule is CALLED elsewhere.
 *
 * A rule like `literal` (content: *(!['] literal_char)) describes only what's INSIDE
 * a Px string literal - the quotes themselves belong to whichever rule calls it
 * (atom: '\'' literal '\''). Used as a bare root-level "match" pattern, such a rule
 * has no anchor requiring an actual quote nearby: it's the earliest match at almost
 * any position not otherwise claimed, and greedily swallows everything up to the next
 * quote - including whatever else should have been separately coloured in between.
 *
 * Fix: scan every rule in the grammar (tagged or not) for direct calls to a tagged
 * rule, and whenever that call is immediately preceded/followed by a plain, unrepeated
 * literal or class in the SAME sequence, treat those as the rule's begin/end delimiters
 * instead of exposing it as an unanchored match. Only the first call site found for a
 * given callee is used - a rule invoked in genuinely different delimited contexts is a
 * known limitation of this heuristic.
 */
static StrVal fixed_delimiter_regex(Variant repetition, StrVariantMap& rule_by_name, StringSet& visiting)
{
	if (repetition.type() != Variant::StrVarMap)
		return "";
	StrVariantMap	rep = repetition.as_variant_map();
	if (rep["repeat_count"].type() != Variant::None)
		return "";	// Only a plain, unrepeated neighbour counts as a fixed delimiter
	StrVariantMap	atom = rep["atom"].as_variant_map();
	StrVal		node_type = atom.begin()->first;
	if (node_type != "literal" && node_type != "class")
		return "";
	return flatten_atom(atom, rule_by_name, visiting);
}

static void scan_for_call_delimiters(
	Variant		re,
	StrVariantMap&	rule_by_name,
	StringSet&	visiting,
	StrVariantMap&	delim_begin,
	StrVariantMap&	delim_end,
	bool		under_unbounded)
{
	if (re.type() == Variant::StrVarMap)
	{
		StrVariantMap	map = re.as_variant_map();
		auto		entry = map.begin();
		StrVal		node_type = entry->first;
		Variant		element = entry->second;

		if (node_type == "sequence")
			scan_for_call_delimiters(element, rule_by_name, visiting, delim_begin, delim_end, under_unbounded);
		else if (node_type == "repetition")
		{
			VariantArray	reps = element.as_variant_array();
			for (int i = 0; i < reps.length(); i++)
			{
				StrVariantMap	rep = reps[i].as_variant_map();
				StrVariantMap	atom = rep["atom"].as_variant_map();
				auto		atom_entry = atom.begin();
				StrVal		atom_type = atom_entry->first;
				Variant		atom_element = atom_entry->second;

				/*
				 * A call site inside a repetition is recurring (e.g. a comma-separated
				 * list): its neighbours are structural separators, not true anchoring
				 * delimiters for the callee. Only a call at a FIXED, non-repeated
				 * position (like '\'' literal '\'' in atom) can contribute a pair.
				 */
				bool		unbounded = under_unbounded;
				Variant		repeat_count = rep["repeat_count"];
				if (repeat_count.type() != Variant::None)
				{
					Variant	limit = repeat_count.as_variant_map()["limit"];
					if (limit.type() == Variant::String)
					{
						UCS4	op = limit.as_strval()[0];
						if (op == '*' || op == '+')
							unbounded = true;
						else if (op == '!' || op == '&')
							unbounded = false;
					}
				}

				if (atom_type == "call")
				{
					/*
					 * A begin and an end must come from the SAME call site: a preceding
					 * delimiter from one call and a following delimiter from an unrelated
					 * call elsewhere would combine into a meaningless pair.
					 */
					StrVal	callee = atom_element.as_strval();
					if (!unbounded && !delim_begin.contains(callee) && i > 0 && i+1 < reps.length())
					{
						StrVal	b = fixed_delimiter_regex(reps[i-1], rule_by_name, visiting);
						StrVal	e = fixed_delimiter_regex(reps[i+1], rule_by_name, visiting);
						if (b != "" && e != "")
						{
							delim_begin.insert(callee, Variant(b));
							delim_end.insert(callee, Variant(e));
						}
					}
				}
				else if (atom_type == "group")
				{
					VariantArray	alternates = atom_element.as_variant_map()["alternates"].as_variant_array();
					for (int j = 0; j < alternates.length(); j++)
						scan_for_call_delimiters(alternates[j], rule_by_name, visiting, delim_begin, delim_end, unbounded);
				}
			}
		}
	}
	else if (re.type() == Variant::VarArray)
	{
		VariantArray	va = re.as_variant_array();
		for (int i = 0; i < va.length(); i++)
			scan_for_call_delimiters(va[i], rule_by_name, visiting, delim_begin, delim_end, under_unbounded);
	}
}

/*
 * Building the TextMate pattern object for one tagged rule
 */
static Variant build_pattern_for_rule(
	StrVal		name,
	StrVal		scope_name,
	StrVariantMap&	rule_by_name,
	StringSet&	tagged,
	StrVariantMap&	delim_begin,
	StrVariantMap&	delim_end)
{
	if (delim_begin.contains(name) && delim_end.contains(name))
	{
		StrVariantMap	pattern;
		pattern.insert("name", Variant(scope_name));
		pattern.insert("begin", delim_begin[name]);
		pattern.insert("end", delim_end[name]);
		return Variant(pattern);
	}

	Variant	alternates = rule_by_name[name].as_variant_map()["alternates"];

	ShapeInfo	info;
	StringSet	scan_visiting;
	scan_shape(alternates, rule_by_name, tagged, scan_visiting, info, false);
	bool	recursive = rule_is_recursive(name, rule_by_name);

	if (!info.has_tagged_call && !info.has_unbounded_any && !info.inexpressible && !recursive)
	{
		StringSet	visiting;
		StrVal		regex = flatten_re(alternates, rule_by_name, visiting);

		StrVariantMap	pattern;
		pattern.insert("name", Variant(scope_name));
		pattern.insert("match", Variant(regex));
		return Variant(pattern);
	}

	/*
	 * Only has_tagged_call is blocking (not an actual multi-line/recursive/inexpressible
	 * risk): if no clean begin/end split can be found below, prefer a flat match that just
	 * inlines the nested tagged content (losing its own distinct colour) over skipping the
	 * rule entirely. The other reasons genuinely risk a wrong or non-terminating regex, so
	 * those still fall back to warn-and-skip.
	 */
	bool	must_split = info.has_unbounded_any || info.inexpressible || recursive;
	auto	flat_fallback = [&]() -> Variant
	{
		if (must_split)
			return Variant();
		StringSet	visiting;
		StrVal		regex = flatten_re(alternates, rule_by_name, visiting);
		StrVariantMap	pattern;
		pattern.insert("name", Variant(scope_name));
		pattern.insert("match", Variant(regex));
		return Variant(pattern);
	};

	/*
	 * Needs begin/end. Only a single top-level sequence (no top-level |alternatives) with
	 * an unbounded (star or plus) element is auto-splittable; anything else falls back per must_split.
	 */
	Variant	seq_list = alternates.as_variant_map()["sequence"];
	if (seq_list.type() != Variant::StrVarMap)
	{
		Variant	fallback = flat_fallback();
		if (fallback.type() != Variant::None)
			return fallback;
		PX_WRITE_ERR(StrVal::format(
			"textmate: rule {1} has multiple top-level alternatives; can't auto-infer begin/end, skipping\n",
			VariantArray() << name));
		return Variant();
	}
	VariantArray	top_reps = seq_list.as_variant_map()["repetition"].as_variant_array();

	int	mid = -1;
	for (int i = 0; i < top_reps.length() && mid < 0; i++)
	{
		Variant	rc = top_reps[i].as_variant_map()["repeat_count"];
		if (rc.type() == Variant::None)
			continue;
		Variant	limit = rc.as_variant_map()["limit"];
		if (limit.type() == Variant::String)
		{
			UCS4	op = limit.as_strval()[0];
			if (op == '*' || op == '+')
				mid = i;
		}
	}

	if (mid <= 0)
	{
		Variant	fallback = flat_fallback();
		if (fallback.type() != Variant::None)
			return fallback;
		PX_WRITE_ERR(StrVal::format(
			"textmate: rule {1} needs a fixed prefix before its repeated content; "
			"can't auto-infer begin/end, skipping\n",
			VariantArray() << name));
		return Variant();
	}

	StringSet	visiting;
	VariantArray	begin_reps;
	for (int i = 0; i < mid; i++)
		begin_reps.append(top_reps[i]);
	StrVal	begin_regex = flatten_sequence(begin_reps, rule_by_name, visiting);

	StrVariantMap	mid_rep = top_reps[mid].as_variant_map();
	DelimSearch	guard = find_guard_delimiter(mid_rep, rule_by_name, visiting);

	StrVal	end_regex;
	if (guard.found)
		end_regex = guard.end_regex;
	else
	{
		VariantArray	end_reps;
		for (int i = mid+1; i < top_reps.length(); i++)
			end_reps.append(top_reps[i]);
		if (end_reps.length() == 0)
		{
			Variant	fallback = flat_fallback();
			if (fallback.type() != Variant::None)
				return fallback;
			PX_WRITE_ERR(StrVal::format(
				"textmate: rule {1}: can't auto-infer an end delimiter, skipping\n",
				VariantArray() << name));
			return Variant();
		}
		end_regex = flatten_sequence(end_reps, rule_by_name, visiting);
	}

	StringArray	nested;
	collect_tagged_calls(mid_rep["atom"], tagged, nested);

	VariantArray	patterns;
	for (int i = 0; i < nested.length(); i++)
	{
		StrVariantMap	inc;
		inc.insert("include", Variant(StrVal("#")+nested[i]));
		patterns.append(Variant(inc));
	}

	StrVariantMap	pattern;
	pattern.insert("name", Variant(scope_name));
	pattern.insert("begin", Variant(begin_regex));
	pattern.insert("end", Variant(end_regex));
	if (patterns.length() > 0)
		pattern.insert("patterns", Variant(patterns));
	return Variant(pattern);
}

void emit_textmate(const char* base_name, VariantArray rules)
{
	StrVariantMap	rule_by_name;
	StringSet	tagged;
	StringArray	order;			// Tagged rule names, in source (declaration) order

	for (int i = 0; i < rules.length(); i++)
	{
		StrVariantMap	rule = rules[i].as_variant_map()["rule"].as_variant_map();
		StrVal		name = rule["name"].as_strval();
		rule_by_name.insert(name, Variant(rule));

		if (rule["scope_annotation"].type() != Variant::None)
		{
			tagged.put(name, true);
			order.append(name);
		}
	}

	/*
	 * Find delimiters for tagged rules from how they're called anywhere in the grammar,
	 * so e.g. `literal` and `class` (content-only rules) get anchored between their
	 * enclosing quotes/brackets instead of exposed as unanchored root-level matches.
	 */
	StrVariantMap	delim_begin, delim_end;
	StringSet	delim_visiting;
	for (int i = 0; i < rules.length(); i++)
	{
		StrVariantMap	rule = rules[i].as_variant_map()["rule"].as_variant_map();
		scan_for_call_delimiters(rule["alternates"], rule_by_name, delim_visiting, delim_begin, delim_end, false);
	}

	StrVariantMap	repository;
	VariantArray	root_patterns;
	for (int i = 0; i < order.length(); i++)
	{
		StrVal	name = order[i];
		StrVal	scope_name = rule_by_name[name].as_variant_map()["scope_annotation"]
					.as_variant_map()["scope_name"].as_strval();

		Variant	pattern = build_pattern_for_rule(name, scope_name, rule_by_name, tagged, delim_begin, delim_end);
		if (pattern.type() == Variant::None)
			continue;		// Already warned in build_pattern_for_rule

		repository.insert(name, pattern);

		StrVariantMap	inc;
		inc.insert("include", Variant(StrVal("#")+name));
		root_patterns.append(Variant(inc));
	}

	StrVal	parser_name = StrVal((UCS4)base_name[0]).asUpper()+(base_name+1);
	StrVal	scope_name_root = StrVal("source.")+StrVal(base_name).asLower();

	StrVariantMap	grammar;
	grammar.insert("name", Variant(parser_name));
	grammar.insert("scopeName", Variant(scope_name_root));
	grammar.insert("fileTypes", Variant(VariantArray()));
	grammar.insert("patterns", Variant(root_patterns));
	grammar.insert("repository", Variant(repository));

	PX_WRITE(StrVal(Variant(grammar).as_json(0))+"\n");
}
