#include "sim.h"

#include <common/standard.h>
#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>

#include <parse_chp/composition.h>
#include <chp/graph.h>
#include <chp/simulator.h>
#include <interpret_chp/import.h>
#include <interpret_chp/export.h>
#include <interpret_arithmetic/export.h>
#include <interpret_arithmetic/import.h>

#include <hse/graph.h>
#include <hse/simulator.h>
#include <hse/elaborator.h>
#include <interpret_hse/import.h>
#include <interpret_hse/export.h>

#include <parse_expression/expression.h>
#include <parse_expression/assignment.h>
#include <parse_expression/composition.h>
#include <parse_prs/production_rule_set.h>
#include <prs/production_rule.h>
#include <prs/simulator.h>

#include <interpret_prs/import.h>
#include <interpret_prs/export.h>
#include <interpret_boolean/export.h>
#include <interpret_boolean/import.h>
#include <ucs/variable.h>

#include <unistd.h>

const bool debug = false;

void verify_help() {
	printf("Usage: ckt verify [options] <ckt-file>\n");
	printf("Do a full state space exploration to look for errors.\n");

	printf("\nSupported file formats:\n");
	printf(" *.chp           communicating hardware processes\n");
	printf(" *.hse           handshaking expansions\n");
	printf(" *.prs           production rule set\n");
	printf(" *.astg          asynchronous signal transition graph\n");
}

int verify_command(configuration &config, int argc, char **argv) {
	tokenizer tokens;
	tokens.register_token<parse::block_comment>(false);
	tokens.register_token<parse::line_comment>(false);
	
	string prefix = "";
	string filename = "";
	string format = "";

	string sfilename = "";

	bool backflow = true;

	for (int i = 0; i < argc; i++) {
		string arg = argv[i];

		if (arg == "--no-backflow") {
			backflow = false;
		} else if (filename == "") {
			filename = argv[i];
			size_t dot = filename.find_last_of(".");
			if (dot == string::npos) {
				printf("unrecognized file format\n");
				return 0;
			}
			format = filename.substr(dot+1);
			prefix = filename.substr(0, dot);
			if (format != "chp"
				and format != "hse"
				and format != "astg"
				and format != "prs") {
				printf("unrecognized file format '%s'\n", format.c_str());
				return 0;
			}
		} else {
			sfilename = argv[i];
			size_t dot = sfilename.find_last_of(".");
			if (dot == string::npos) {
				printf("unrecognized file format\n");
				return 0;
			}
			string sformat = sfilename.substr(dot+1);
			if (sformat != "sim") {
				printf("unrecognized file format '%s'\n", sformat.c_str());
				return 0;
			}
		}
	}

	if (filename == "") {
		printf("expected filename\n");
		return 0;
	}

	ucs::variable_set v;

	if (format == "chp") {
		chp::graph cg;

		parse_chp::composition::register_syntax(tokens);
		config.load(tokens, filename, "");

		tokens.increment(false);
		tokens.expect<parse_chp::composition>();
		while (tokens.decrement(__FILE__, __LINE__))
		{
			parse_chp::composition syntax(tokens);
			cg.merge(chp::parallel, chp::import_chp(syntax, v, 0, &tokens, true));

			tokens.increment(false);
			tokens.expect<parse_chp::composition>();
		}
		cg.post_process(v, true);

	} else if (format == "hse" or format == "astg") {
		hse::graph hg;

		if (format == "hse") {
			parse_chp::composition::register_syntax(tokens);
			config.load(tokens, filename, "");

			tokens.increment(false);
			tokens.expect<parse_chp::composition>();
			while (tokens.decrement(__FILE__, __LINE__))
			{
				parse_chp::composition syntax(tokens);
				hg.merge(hse::parallel, hse::import_hse(syntax, v, 0, &tokens, true));

				tokens.increment(false);
				tokens.expect<parse_chp::composition>();
			}
		} else if (format == "astg") {
			parse_astg::graph::register_syntax(tokens);
			config.load(tokens, filename, "");
			
			tokens.increment(false);
			tokens.expect<parse_astg::graph>();
			while (tokens.decrement(__FILE__, __LINE__))
			{
				parse_astg::graph syntax(tokens);
				hg.merge(hse::parallel, hse::import_hse(syntax, v, &tokens));

				tokens.increment(false);
				tokens.expect<parse_astg::graph>();
			}
		}
		hg.post_process(v, true);
		hg.check_variables(v);

		hse::elaborate(hg, v, false, true);
	} else if (format == "prs") {
		prs::production_rule_set pr;

		parse_prs::production_rule_set::register_syntax(tokens);
		config.load(tokens, filename, "");

		tokens.increment(false);
		tokens.expect<parse_prs::production_rule_set>();
		if (tokens.decrement(__FILE__, __LINE__))
		{
			parse_prs::production_rule_set syntax(tokens);
			map<int, int> nodemap;
			prs::import_production_rule_set(syntax, pr, -1, -1, prs::attributes(), v, nodemap, 0, &tokens, true);
		}

		if (debug) {
			printf("\n\n%s\n\n", export_production_rule_set(pr, v).to_string().c_str());
			pr.print(v);
			printf("\n\n");
		}

	}

	complete();
	return is_clean();
}
