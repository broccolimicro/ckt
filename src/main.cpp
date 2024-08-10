/*
 * main.cpp
 *
 *  Created on: Jan 16, 2015
 *      Author: nbingham
 */

#include <common/standard.h>
#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse_chp/composition.h>
#include <hse/graph.h>
#include <hse/simulator.h>
#include <hse/encoder.h>
#include <hse/elaborator.h>
#include <hse/synthesize.h>
#include <prs/production_rule.h>
#include <interpret_hse/import.h>
#include <interpret_hse/export.h>
#include <interpret_boolean/export.h>
#include <interpret_boolean/import.h>
#include <ucs/variable.h>
#include <interpret_prs/export.h>

void print_help()
{
	printf("Usage: hseenc [options] file...\n");
	printf("A state encoder and conflict solver for HSE\n");
	printf("\nGeneral Options:\n");
	printf(" -h,--help      Display this information\n");
	printf("    --version   Display version information\n");
	printf(" -v,--verbose   Display verbose messages\n");
	printf(" -d,--debug     Display internal debugging messages\n");
	printf("\nConflict Checking:\n");
	printf(" -c             check for state conflicts that occur regardless of sense\n");
	printf(" -cu            check for state conflicts that occur due to up-going transitions\n");
	printf(" -cd            check for state conflicts that occur due to down-going transitions\n");
}

void print_version()
{
	printf("hseenc 1.0.0\n");
	printf("Copyright (C) 2013 Sol Union.\n");
	printf("There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
	printf("\n");
}

void print_command_help()
{
	printf("<arg> specifies a required argument\n(arg=value) specifies an optional argument with a default value\n");
	printf("\nGeneral:\n");
	printf(" help, h                       print this message\n");
	printf(" quit, q                       exit the interactive simulation environment\n");
	printf(" load (filename)               load an hse, default is to reload current file\n");
	printf(" save (filename)               save an hse, default is to overwrite current file\n");
	printf("\nViewing and Manipulating State:\n");
	printf(" elaborate, e                  elaborate the predicates\n");
	printf(" conflicts, c                  check for state conflicts that occur regardless of sense\n");
	printf(" conflicts up, cu              check for state conflicts that occur due to up-going transitions\n");
	printf(" conflicts down, cd            check for state conflicts that occur due to down-going transitions\n");
	printf("\nViewing and Manipulating Structure:\n");
	printf(" print, p                      print the current hse\n");
	printf(" insert <expr>                 insert the transition <expr> into the hse\n");
	printf(" pinch <node>                  remove <node> and merge its input and output nodes to maintain token flow\n");
}

void print_location_help()
{
	printf("<arg> specifies a required argument\n(arg=value) specifies an optional argument with a default value\n");
	printf(" help                          print this message\n");
	printf(" done                          exit the interactive location selector\n");
	printf(" before <i>, b<i>              put the state variable before the selected hse block\n");
	printf(" after <i>, a<i>               put the state variable after the selected hse block\n");
	printf(" <i>                           go into the selected hse block\n");
	printf(" up, u                         back out of the current hse block\n");
}

void print_conflicts(hse::encoder &enc, hse::graph &g, ucs::variable_set &v, int sense)
{
	for (int i = 0; i < (int)enc.conflicts.size(); i++)
	{
		if (enc.conflicts[i].sense == sense)
		{
			printf("T%d.%d\t...%s...   conflicts with:\n", enc.conflicts[i].index.index, enc.conflicts[i].index.term, export_node(hse::iterator(hse::transition::type, enc.conflicts[i].index.index), g, v).c_str());

			for (int j = 0; j < (int)enc.conflicts[i].region.size(); j++) {
				printf("\t%s\t...%s...\n", enc.conflicts[i].region[j].to_string().c_str(), export_node(enc.conflicts[i].region[j], g, v).c_str());
			}
			printf("\n");
		}
	}
	printf("\n");
}

vector<pair<hse::iterator, int> > get_locations(FILE *script, hse::graph &g, ucs::variable_set &v)
{
	vector<pair<hse::iterator, int> > result;

	vector<hse::iterator> source;
	if (g.source.size() > 0)
		for (int i = 0; i < (int)g.source[0].tokens.size(); i++)
			source.push_back(hse::iterator(hse::place::type, g.source[0].tokens[i].index));
	parse_chp::composition p = export_sequence(source, g, v);

	vector<parse::syntax*> stack(1, &p);

	bool update = true;

	int n;
	char command[256];
	bool done = false;
	while (!done)
	{
		while (update)
		{
			if (stack.back()->is_a<parse_chp::composition>())
			{
				parse_chp::composition *par = (parse_chp::composition*)stack.back();

				if (par->branches.size() == 1 && par->branches[0].sub.valid)
					stack.back() = &par->branches[0].sub;
				else if (par->branches.size() == 1 && par->branches[0].ctrl.valid)
					stack.back() = &par->branches[0].ctrl;
				else if (par->branches.size() == 1 && par->branches[0].assign.valid)
					stack.back() = &par->branches[0].assign;
				else
				{
					for (int i = 0; i < (int)par->branches.size(); i++)
						printf("(%d) %s\n", i, par->branches[i].to_string(-1, "").c_str());
					update = false;
				}
			}
			else if (stack.back()->is_a<parse_chp::control>())
			{
				parse_chp::control *par = (parse_chp::control*)stack.back();
				if (par->branches.size() == 1 && par->branches[0].second.branches.size() == 0)
					stack.back() = &par->branches[0].first;
				else
				{
					for (int i = 0; i < (int)par->branches.size(); i++)
						printf("(%d) %s -> (%d) %s\n", i*2, par->branches[i].first.to_string().c_str(), i*2+1, par->branches[i].second.to_string().c_str());
					update = false;
				}
			}
			else
				update = false;
		}

		printf("%d %d\n", stack.back()->start, stack.back()->end);

		if (stack.back()->is_a<parse_expression::expression>() || stack.back()->is_a<parse_expression::assignment>())
			printf("before(b) or after(a)?");
		else
			printf("(location)");

		fflush(stdout);
		if (fgets(command, 255, script) == NULL && script != stdin)
		{
			fclose(script);
			script = stdin;
			if (fgets(command, 255, script) == NULL)
				exit(0);
		}
		int length = strlen(command);
		command[length-1] = '\0';
		length--;

		if (strncmp(command, "up", 2) == 0 || strncmp(command, "u", 1) == 0)
		{
			stack.pop_back();
			update = true;
		}
		else if (strncmp(command, "done", 4) == 0 || strncmp(command, "d", 1) == 0)
			done = true;
		else if (strncmp(command, "quit", 4) == 0 || strncmp(command, "q", 1) == 0)
			exit(0);
		else if (strncmp(command, "help", 4) == 0 || strncmp(command, "h", 1) == 0)
			print_location_help();
		if (strncmp(command, "before", 6) == 0 || strncmp(command, "b", 1) == 0)
		{
			if (stack.back()->is_a<parse_expression::expression>() || stack.back()->is_a<parse_expression::assignment>())
			{

			}
			else if (sscanf(command, "before %d", &n) == 1 || sscanf(command, "b%d", &n) == 1)
			{
			}
			else
				error("", "before what?", __FILE__, __LINE__);
		}
		else if (strncmp(command, "after", 5) == 0 || strncmp(command, "a", 1) == 0)
		{
			if (stack.back()->is_a<parse_expression::expression>() || stack.back()->is_a<parse_expression::assignment>())
			{

			}
			else if (sscanf(command, "after %d", &n) == 1 || sscanf(command, "a%d", &n) == 1)
			{
			}
			else
				error("", "after what?", __FILE__, __LINE__);
		}
		else if (sscanf(command, "%d", &n) == 1)
		{
			if (stack.back()->is_a<parse_expression::expression>() || stack.back()->is_a<parse_expression::assignment>())
			{

			}
			else
			{
				cout << n << endl;
				if (stack.back()->is_a<parse_chp::composition>())
				{
					parse_chp::composition *par = (parse_chp::composition*)stack.back();
					if (n < (int)par->branches.size())
					{
						if (par->branches[n].sub.valid)
							stack.push_back(&par->branches[n].sub);
						else if (par->branches[n].ctrl.valid)
							stack.push_back(&par->branches[n].ctrl);
						else if (par->branches[n].assign.valid)
							stack.push_back(&par->branches[n].assign);
						update = true;
					}
					else
						error("", "invalid option", __FILE__, __LINE__);
				}
				else if (stack.back()->is_a<parse_chp::control>())
				{
					parse_chp::control *par = (parse_chp::control*)stack.back();
					if (n < (int)par->branches.size()/2)
					{
						stack.push_back(n%2 == 0 ? (parse::syntax*)&par->branches[n/2].first : (parse::syntax*)&par->branches[n/2].second);
						update = true;
					}
					else
						error("", "invalid option", __FILE__, __LINE__);
				}
			}
		}

	}

	return result;
}

void real_time(hse::graph &g, ucs::variable_set &v, string filename)
{
	hse::encoder enc;
	enc.base = &g;
	enc.variables = &v;

	tokenizer assignment_parser(false);
	parse_expression::composition::register_syntax(assignment_parser);

	char command[256];
	bool done = false;
	FILE *script = stdin;
	while (!done)
	{
		printf("(hseenc)");
		fflush(stdout);
		if (fgets(command, 255, script) == NULL && script != stdin)
		{
			fclose(script);
			script = stdin;
			if (fgets(command, 255, script) == NULL)
				exit(0);
		}
		int length = strlen(command);
		command[length-1] = '\0';
		length--;

		if ((strncmp(command, "help", 4) == 0 && length == 4) || (strncmp(command, "h", 1) == 0 && length == 1))
			print_command_help();
		else if ((strncmp(command, "quit", 4) == 0 && length == 4) || (strncmp(command, "q", 1) == 0 && length == 1))
			done = true;
		else if ((strncmp(command, "elaborate", 9) == 0 && length == 9) || (strncmp(command, "e", 1) == 0 && length == 1))
			elaborate(g, v, true);
		else if ((strncmp(command, "conflicts", 9) == 0 && length == 9) || (strncmp(command, "c", 1) == 0 && length == 1))
		{
			enc.check(true, true);
			print_conflicts(enc, g, v, -1);
		}
		else if ((strncmp(command, "conflicts up", 12) == 0 && length == 12) || (strncmp(command, "cu", 2) == 0 && length == 2))
		{
			enc.check(false, true);
			print_conflicts(enc, g, v, 0);
		}
		else if ((strncmp(command, "conflicts down", 14) == 0 && length == 14) || (strncmp(command, "cd", 2) == 0 && length == 2))
		{
			enc.check(false, true);
			print_conflicts(enc, g, v, 1);
		}
		else if (strncmp(command, "insert", 6) == 0)
		{
			if (length <= 6)
				printf("error: expected expression\n");
			else
			{
				assignment_parser.insert("", string(command).substr(6));
				parse_expression::composition expr(assignment_parser);
				boolean::cover action = import_cover(expr, v, 0, &assignment_parser, true);
				if (assignment_parser.is_clean())
				{
					vector<pair<hse::iterator, int> > locations = get_locations(script, g, v);
					for (int i = 0; i < (int)locations.size(); i++)
						printf("%s %c%d\n", locations[i].second ? "after" : "before", locations[i].first.type == hse::place::type ? 'P' : 'T', locations[i].first.index);
				}
			}
		}
		else if (length > 0)
			printf("error: unrecognized command '%s'\n", command);
	}
}

int main(int argc, char **argv)
{
	configuration config;
	config.set_working_directory(argv[0]);
	tokenizer hse_tokens;
	tokenizer astg_tokens;
	parse_chp::composition::register_syntax(hse_tokens);
	parse_astg::graph::register_syntax(astg_tokens);
	hse_tokens.register_token<parse::block_comment>(false);
	hse_tokens.register_token<parse::line_comment>(false);

	string ofilename;
	string gfilename;
	bool cmos = true;
	bool force = false;
	bool progress = false;
	bool c = false;

	for (int i = 1; i < argc; i++)
	{
		string arg = argv[i];
		if (arg == "--help" || arg == "-h")			// Help
		{
			print_help();
			return 0;
		}
		else if (arg == "--version")	// Version Information
		{
			print_version();
			return 0;
		}
		else if (arg == "--verbose" || arg == "-v")
			set_verbose(true);
		else if (arg == "--debug" || arg == "-d")
			set_debug(true);
		else if (arg == "--no-cmos" || arg == "-n")
			cmos = false;
		else if (arg == "--force" || arg == "-f")
			force = true;
		else if (arg == "--progress" || arg == "-p")
			progress = true;
		else if (arg == "-c")
			c = true;
		else if (arg == "-g") {
			i++;
			if (i < argc)
				gfilename = argv[i];
			else {
				error("", "expected graph filename", __FILE__, __LINE__);
				return 1;
			}
		} else if (arg == "-o") {
			i++;
			if (i < argc)
				ofilename = argv[i];
			else {
				error("", "expected output filename", __FILE__, __LINE__);
				return 1;
			}
		} else {
			string filename = argv[i];
			size_t dot = filename.find_last_of(".");
			string format = "";
			if (dot != string::npos)
				format = filename.substr(dot+1);
			if (format == "hse")
				config.load(hse_tokens, filename, "");
			else if (format == "astg")
				config.load(astg_tokens, filename, "");
			else
				printf("unrecognized file format '%s'\n", format.c_str());
		}
	}

	if (is_clean() && (hse_tokens.segments.size() > 0 || astg_tokens.segments.size() > 0))
	{
		hse::graph g;
		ucs::variable_set v;

		hse_tokens.increment(false);
		hse_tokens.expect<parse_chp::composition>();
		while (hse_tokens.decrement(__FILE__, __LINE__))
		{
			parse_chp::composition syntax(hse_tokens);
			g.merge(hse::parallel, import_hse(syntax, v, 0, &hse_tokens, true));

			hse_tokens.increment(false);
			hse_tokens.expect<parse_chp::composition>();
		}

		astg_tokens.increment(false);
		astg_tokens.expect<parse_astg::graph>();
		while (astg_tokens.decrement(__FILE__, __LINE__))
		{
			parse_astg::graph syntax(astg_tokens);
			g.merge(hse::parallel, import_hse(syntax, v, &astg_tokens));

			astg_tokens.increment(false);
			astg_tokens.expect<parse_astg::graph>();
		}
		g.post_process(v, true);
		g.check_variables(v);
		
		elaborate(g, v, progress);
		if (not is_clean()) {
			complete();
			return false;
		}

		hse::encoder enc;
		enc.base = &g;
		enc.variables = &v;

		enc.check(!cmos, progress);

		if (c) {
			if (!cmos) {
				print_conflicts(enc, g, v, -1);
			} else {
				print_conflicts(enc, g, v, 0);
				print_conflicts(enc, g, v, 1);
			}
		}

		if (c) {
			complete();
			return is_clean();
		}

		for (int i = 0; i < 10 and enc.conflicts.size() > 0; i++) {
			/*if (!cmos) {
				print_conflicts(enc, g, v, -1);
			} else {
				print_conflicts(enc, g, v, 0);
				print_conflicts(enc, g, v, 1);
			}*/

			enc.insert_state_variables();

			elaborate(g, v, progress);
			if (not is_clean()) {
				complete();
				return false;
			}

			enc.check(!cmos, progress);
		}

		if (gfilename != "") {
			FILE *fg = fopen(gfilename.c_str(), "w");
			fprintf(fg, "%s", export_astg(g, v).to_string().c_str());
			fclose(fg);
		}

		if (enc.conflicts.size() > 0) {
			// state variable insertion failed
			if (!cmos) {
				print_conflicts(enc, g, v, -1);
			} else {
				print_conflicts(enc, g, v, 0);
				print_conflicts(enc, g, v, 1);
			}
			if (!force) {
				complete();
				return is_clean();
			}
		}

		prs::production_rule_set pr;
		hse::gate_set gates(&g, &v);
		gates.load(!cmos);
		gates.weaken();
		gates.build_reset();
		gates.save(&pr);
	
		FILE *fout = stdout;
	 	if (ofilename != "") {
			fout = fopen(ofilename.c_str(), "w");
		}
		fprintf(fout, "%s", export_production_rule_set(pr, v).to_string().c_str());
		fclose(fout);
	}

	complete();
	return is_clean();
}
