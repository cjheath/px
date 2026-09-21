/*
 * Px PEG parser generator defined using pegular expression rules
 *
 * Copyright 2026 Clifford Heath. ALL RIGHTS RESERVED SUBJECT TO ATTACHED LICENSE.
 */
#include	<strval.h>
#include	<variant.h>
#include	<char_encoding.h>

#include	<cctype>
#include	<sys/stat.h>
#include	<unistd.h>
#include	<fcntl.h>

#include	<px_parser.h>
#include	<px_pegexp.h>
#include	<px_cpp.h>
#include	<px_railroad.h>
#include	<px_textmate.h>
#include	<px_parser.cpp>

typedef	CowMap<bool>	StringSet;

void usage()
{
	PX_WRITE_ERR(StrVal(
		"Usage: px [-c|-j|-t|-r [-x rule ...]] peg.px\n"
		"\t-c\tEmit the C++ rules for a parser (default case)\n"
		"\t-t\tEmit a TextMate grammar\n"
		"\t-r\tEmit railroad diagrams as HTML\n"
		"\t-x rule\tWith -r, a rule to omit from the diagrams\n"
		"\t-j\tEmit the parsed rules as JSON\n"
		));
	exit(1);
}

char* slurp_file(const char* filename, off_t* size_p)
{
	// Open the file and get its size
	int		fd;
	struct	stat	stat;
	char*		px;
	if ((fd = open(filename, O_RDONLY)) < 0		// Can't open
	 || fstat(fd, &stat) < 0			// Can't fstat
	 || (stat.st_mode&S_IFMT) != S_IFREG		// Not a regular file
	 || (px = new char[stat.st_size+1]) == 0	// Can't get memory
	 || read(fd, px, stat.st_size) < stat.st_size)	// Can't read entire file
	{
		perror(filename);
		usage();
	}
	if (size_p)
		*size_p = stat.st_size;
	px[stat.st_size] = '\0';

	return px;
}

/*
 * Descend into the AST of a regular expression, saving any new rule names that are called
 */
void accumulate_called_rules(StringSet& called, Variant re)
{
	if (re.type() == Variant::StrVarMap)
	{
		StrVariantMap	map = re.as_variant_map();
		assert(map.size() == 1);

		auto		entry = map.begin();
		StrVal		node_type = entry->first;	// Extract the node_type and value
		Variant		element = entry->second;

		if (node_type == "call")
			called.put(element.as_strval(), true);
		else if (node_type == "sequence")	// Process the VarArray
			accumulate_called_rules(called, element);
		else if (node_type == "repetition")
		{		// Each repetition has {optional repeat_count, atom, optional label}
			VariantArray	repetitions = element.as_variant_array();

			for (int i = 0; i < repetitions.length(); i++)
			{
				StrVariantMap	repetition = repetitions[i].as_variant_map();
				Variant		atom = repetition["atom"];
				accumulate_called_rules(called, atom);
			}
		}
		else if (node_type == "group")
		{
			VariantArray	alternates = element.as_variant_map()["alternates"].as_variant_array();
			return accumulate_called_rules(called, alternates[0]);
		}
		else if (node_type == "alternates")
		{
			VariantArray	repetitions = element.as_variant_array();

			for (int i = 0; i < repetitions.length(); i++)
			{
				StrVariantMap	repetition = repetitions[i].as_variant_map();
				Variant		atom = repetition["atom"];
				accumulate_called_rules(called, atom);
			}
		}
	}
	else if (re.type() == Variant::VarArray)
	{
		VariantArray	va = re.as_variant_array();
		for (int i = 0; i < va.length(); i++)
			accumulate_called_rules(called, va[i]);
	}
}

bool check_rules(VariantArray rules)
{
	StringSet	defined_rules;
	StringSet	called_rules;

	// Find names of all rules that are called:
	for (int i = 0; i < rules.length(); i++)
	{
		StrVariantMap	rule = rules[i].as_variant_map()["rule"].as_variant_map();
		StrVal		rule_name = rule["name"].as_strval();
		Variant		va = rule["alternates"];	// Definition of the rule's RE

		// record that this rule is defined:
		defined_rules.put(rule_name, true);
#if 0
	// REVISIT: Ensure that actions only request available captures
		// record which rules this rule calls and what labels it has, for checking actions:
		StringSet	local_called_rules;
		accumulate_called_rules(local_called_rules, va);
		Variant		vact = rule["action"];		// Ensure that capture names are defined
#endif

		// accumulate all called rules:
		accumulate_called_rules(called_rules, va);
	}


	// Ensure that all called rules exist:
	bool ok = true;
	for (auto i = called_rules.begin(); i != called_rules.end(); i++)
	{
		if (!defined_rules[i->first])
		{
			ok = false;
			PX_WRITE_ERR(StrVal::format("Rule {1} is called but not defined\n",
					VariantArray() << StrVal(i->first)));
		}
	}

	// Report defined rules that are not called:
	for (auto i = defined_rules.begin(); i != defined_rules.end(); i++)
	{
		if (i->first != "TOP" && !called_rules[i->first])
			PX_WRITE_ERR(StrVal::format("Rule {1} is defined but not called\n",
					VariantArray() << StrVal(i->first)));
	}

	return ok;
}

typedef void (*Emitter)(const char* parser_name, VariantArray rules);

bool
parse_and_emit(const char* filename, VariantArray& rules, Emitter emit)
{
	off_t		file_size;
	char*		text = slurp_file(filename, &file_size);
	char*		basename;
	PxParser::Source source(text);
	int		bytes_parsed = 0;
	int		rules_parsed = 0;

	// Figure out the basename, isolate and null-terminate it:
	if ((basename = (char*)strrchr(filename, '/')) != 0)
		basename++;
	else
		basename = (char*)filename;
	basename = strdup(basename);
	if (strchr(basename, '.'))
		*strchr(basename, '.') = '\0';

	PxParser	peg;

	do {
		PxParser::Match match = peg.parse(source);

		if (match.is_failure())
		{
			PX_WRITE(StrVal::format(
				"Parse failed at line {1} column {2} (byte {3} of {4}) after {5} rules. Possible next {6} tokens were:\n",
				VariantArray()
					<< (source.current_line()+match.furthermost_success.current_line()-1)
					<< (source.current_column()+match.furthermost_success.current_column()-1)
					<< (source.current_byte()+match.furthermost_success.current_byte())
					<< file_size
					<< rules_parsed
					<< match.failures.length()));

			for (int i = 0; i < match.failures.length(); i++)
			{
				PegFailure	f = match.failures[i];
				PX_WRITE(StrVal("\t")+StrVal(f.atom, f.atom_len)+"\n");
				PX_WRITE(StrVal("\t... from ")+StringArray(f.path).join("->")+"\n");
			}
			break;
		}

		bytes_parsed = match.furthermost_success.peek() - text;


		rules.append(match.var);

		// Start again at the next rule:
		source = match.furthermost_success;
		rules_parsed++;
	} while (bytes_parsed < file_size);


	delete text;

	bool	success = bytes_parsed == file_size;

	if (!success)
	{
		rules.clear();
		return false;
	}

	if (!check_rules(rules))
		return false;

	emit(basename, rules);

	return true;
}

void emit_json(const char* base_name, VariantArray rules)
{
	PX_WRITE(StrVal(Variant(rules).as_json(0))+"\n");
}

int
main(int argc, const char** argv)
{
	Emitter emit = emit_cpp;

	if (argc-- < 2)
		usage();
	argv++;

	if (argc > 1)
	{
		if (0 == strcmp("-c", argv[0]))
		{
			argc--, argv++;
			emit = emit_cpp;
		}
		else if (0 == strcmp("-r", argv[0]))
		{
			argc--, argv++;
			while (argc > 2 && 0 == strcmp("-x", argv[0]))
			{
				argc-=2, argv++;
				omitted_rules.append(*argv++);
			}
			emit = emit_railroad;
		}
		else if (0 == strcmp("-j", argv[0]))
		{
			argc--, argv++;
			emit = emit_json;
		}
		else if (0 == strcmp("-t", argv[0]))
		{
			argc--, argv++;
			emit = emit_textmate;
		}
		else if (argv[0][0] == '-')
			usage();		// An option we do not have, rather than a file
	}

	/*
	start_recording_allocations();
	*/

	VariantArray	rules;
	if (!parse_and_emit(argv[0], rules, emit))
		exit(1);

	/*
	if (allocation_growth_count() > 0)	// No allocation should remain unfreed
		report_allocation_growth();
	*/

	return 0;
}

