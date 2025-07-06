#include "show.h"

#include <common/standard.h>
#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>

#include <chp/graph.h>
//#include <chp/simulator.h>
//#include <parse_chp/composition.h>
#include <interpret_chp/import.h>
#include <interpret_chp/export.h>

#include <hse/graph.h>
#include <hse/simulator.h>
#include <hse/encoder.h>
#include <hse/elaborator.h>
#include <hse/synthesize.h>
#include <interpret_hse/import.h>
#include <interpret_hse/export.h>

#include <prs/production_rule.h>
#include <prs/bubble.h>
//#include <parse_prs/production_rule_set.h>
#include <interpret_prs/import.h>
#include <interpret_prs/export.h>

//#include <parse_expression/expression.h>
//#include <parse_expression/assignment.h>
//#include <parse_expression/composition.h>
#include <interpret_boolean/export.h>
#include <interpret_boolean/import.h>
#include <interpret_arithmetic/export.h>
#include <interpret_arithmetic/import.h>

#include "format/dot.h"

void show_help() {
	printf("Usage: lm show [options] file...\n");
	printf("Create visual representations of the circuit or behavior.\n");
	printf("\nOptions:\n");
	printf(" -o              Specify the output file name, formats other than 'dot'\n");
	printf("                 are passed onto graphviz dot for rendering\n");
	printf(" -l,--labels     Show the IDs for each place, transition, and arc\n");
	printf(" -lr,--leftright Render the graph from left to right\n");
	printf(" -e,--effective  Show the effective encoding of each place\n");
	printf(" -p,--predicate  Show the predicate of each place\n");
	printf(" -g,--ghost      Show the state annotations for the conditional branches\n");
	printf(" -r,--raw        Do not post-process the graph\n");
	printf(" -s,--sync       Render half synchronization actions\n");
}

void loadPath(string path, tokenizer &tokens) {
	ifstream fin;
	fin.open(path.c_str(), ios::binary | ios::in);
	fin.seekg(0, ios::end);
	int size = (int)fin.tellg();
	string buffer(size, ' ');
	fin.seekg(0, ios::beg);
	fin.read(&buffer[0], size);
	fin.clear();
	tokens.insert(path, buffer);
}

int show_command(string workingDir, string techPath, string cellsDir, int argc, char **argv) {
	tokenizer tokens;
	tokens.register_token<parse::block_comment>(false);
	tokens.register_token<parse::line_comment>(false);

	string filename = "";
	string format = "";

	string ofilename = "a.png";

	int encodings = -1;

	bool proper = true;
	bool aggressive = false;
	bool process = true;
	bool labels = false;
	bool notations = false;
	bool horiz = false;
	bool states = false;
	bool petri = false;
	bool ghost = false;
	
	for (int i = 0; i < argc; i++) {
		string arg = argv[i];
		if (arg == "--labels" || arg == "-l") {
			labels = true;
		} else if (arg == "--notations" || arg == "-nt") {
			notations = true;
		} else if (arg == "--leftright" || arg == "-lr") {
			horiz = true;
		} else if (arg == "--effective" || arg == "-e") {
			encodings = 1;
		} else if (arg == "--predicate" || arg == "-p") {
			encodings = 0;
		} else if (arg == "--ghost" || arg == "-g") {
			ghost = true;
		} else if (arg == "--raw" || arg == "-r") {
			process = false;
		} else if (arg == "--nest" || arg == "-n") {
			proper = false;
		} else if (arg == "--aggressive" || arg == "-ag") {
			aggressive = true;
		} else if (arg == "-sg" or arg == "--states") {
			states = true;
		} else if (arg == "-pn" or arg == "--petri") {
			petri = true;
		} else if (arg == "-o") {
			if (++i >= argc) {
				error("", "expected output filename", __FILE__, __LINE__);
				return 1;
			}

			ofilename = argv[i];
		} else {
			filename = argv[i];
			size_t dot = filename.find_last_of(".");
			if (dot == string::npos) {
				printf("unrecognized file format\n");
				return 1;
			}
			format = filename.substr(dot+1);
			if (format != "chp"
				and format != "hse"
				and format != "cog"
				and format != "astg"
				and format != "prs") {
				printf("unrecognized file format '%s'\n", format.c_str());
				return 0;
			}
		}
	}

	if (filename == "") {
		printf("expected filename\n");
		return 0;
	}

	if (format == "chp" or format == "cog") {
		chp::graph cg;
		cout << "here1" << endl;

		if (format == "chp") {
			parse_chp::composition::register_syntax(tokens);
			loadPath(filename, tokens);

			tokens.increment(false);
			tokens.expect<parse_chp::composition>();
			while (tokens.decrement(__FILE__, __LINE__))
			{
				parse_chp::composition syntax(tokens);
				chp::import_chp(cg, syntax, &tokens, true);

				tokens.increment(false);
				tokens.expect<parse_chp::composition>();
			}
		} else {
			cout << "here2" << endl;
			parse_cog::composition::register_syntax(tokens);
			loadPath(filename, tokens);

			tokens.increment(false);
			tokens.expect<parse_cog::composition>();
			while (tokens.decrement(__FILE__, __LINE__))
			{
				parse_cog::composition syntax(tokens);
				cout << syntax.to_string() << endl;
				chp::import_chp(cg, syntax, &tokens, true);
				cg.print();

				tokens.increment(false);
				tokens.expect<parse_cog::composition>();
			}
		}
		
		if (process) {
			cg.post_process();
		}

		if (is_clean()) {
			gvdot::render(ofilename, export_graph(cg, labels).to_string());
		}
	} else if (format == "hse" or format == "astg" or format == "cog") {
		hse::graph hg;

		if (format == "hse") {
			parse_chp::composition::register_syntax(tokens);
			loadPath(filename, tokens);

			tokens.increment(false);
			tokens.expect<parse_chp::composition>();
			while (tokens.decrement(__FILE__, __LINE__))
			{
				parse_chp::composition syntax(tokens);
				hse::import_hse(hg, syntax, &tokens, true);

				tokens.increment(false);
				tokens.expect<parse_chp::composition>();
			}
		} else if (format == "astg") {
			parse_astg::graph::register_syntax(tokens);
			loadPath(filename, tokens);
			
			tokens.increment(false);
			tokens.expect<parse_astg::graph>();
			while (tokens.decrement(__FILE__, __LINE__))
			{
				parse_astg::graph syntax(tokens);
				hse::import_hse(hg, syntax, &tokens);

				tokens.increment(false);
				tokens.expect<parse_astg::graph>();
			}
		} else if (format == "cog") {
			parse_cog::composition::register_syntax(tokens);
			loadPath(filename, tokens);

			tokens.increment(false);
			tokens.expect<parse_cog::composition>();
			while (tokens.decrement(__FILE__, __LINE__))
			{
				parse_cog::composition syntax(tokens);
				hse::import_hse(hg, syntax, &tokens, true);

				tokens.increment(false);
				tokens.expect<parse_cog::composition>();
			}
		}
		if (process) {
			hg.post_process(true);
		}
		hg.check_variables();

		if (is_clean()) {
			if (states) {
				hse::graph sg = hse::to_state_graph(hg, true);
				gvdot::render(ofilename, export_graph(sg, horiz, labels, notations, ghost, encodings).to_string());
			} else if (petri) {
				hse::graph pn = hse::to_petri_net(hg, true);
				gvdot::render(ofilename, export_graph(hg, horiz, labels, notations, ghost, encodings).to_string());
			} else {
				gvdot::render(ofilename, export_graph(hg, horiz, labels, notations, ghost, encodings).to_string());
			}
		}
	} else if (format == "prs") {
		prs::production_rule_set pr;

		parse_prs::production_rule_set::register_syntax(tokens);
		loadPath(filename, tokens);

		tokens.increment(false);
		tokens.expect<parse_prs::production_rule_set>();
		if (tokens.decrement(__FILE__, __LINE__))
		{
			parse_prs::production_rule_set syntax(tokens);
			prs::import_production_rule_set(syntax, pr, -1, -1, prs::attributes(), 0, &tokens, true);
		}
	}

	complete();
	return is_clean();
}
