#include "plot.h"

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
#include <ucs/variable.h>

#ifdef GRAPHVIZ_SUPPORTED
namespace graphviz
{
	#include <graphviz/cgraph.h>
	#include <graphviz/gvc.h>
}
#endif

void plot_help() {
	printf("Usage: ckt plot [options] file...\n");
	printf("Create visual representations of the circuit or behavior.\n");
	printf("\nOptions:\n");
	printf(" -o              Specify the output file name, formats other than 'dot'\n");
	printf("                 are passed onto graphviz dot for rendering\n");
	printf(" -l,--labels     Show the IDs for each place, transition, and arc\n");
	printf(" -lr,--leftright Render the graph from left to right\n");
	printf(" -e,--effective  Show the effective encoding of each place\n");
	printf(" -p,--predicate  Show the predicate of each place\n");
	printf(" -r,--raw        Do not post-process the graph\n");
	printf(" -s,--sync       Render half synchronization actions\n");
}

void render(string filename, string format, string content) {
	if (format == "dot") {
		FILE *file = fopen(filename.c_str(), "w");
		fprintf(file, "%s\n", content.c_str());
		fclose(file);
	} else {
#ifdef GRAPHVIZ_SUPPORTED
		graphviz::Agraph_t* G = graphviz::agmemread(content.c_str());
		graphviz::GVC_t* gvc = graphviz::gvContext();
		graphviz::gvLayout(gvc, G, "dot");
		graphviz::gvRenderFilename(gvc, G, format.c_str(), filename.c_str());
		graphviz::gvFreeLayout(gvc, G);
		graphviz::agclose(G);
		graphviz::gvFreeContext(gvc);
#else
		
		string tfilename = filename.substr(0, filename.find_last_of("."));
		FILE *temp = NULL;
		int num = 0;
		for (; temp == NULL; num++)
			temp = fopen((tfilename + (num > 0 ? to_string(num) : "") + ".dot").c_str(), "wx");
		num--;
		tfilename += (num > 0 ? to_string(num) : "") + ".dot";

		fprintf(temp, "%s\n", content.c_str());
		fclose(temp);

		if (system(("dot -T" + format + " " + tfilename + " > " + filename).c_str()) != 0)
			error("", "Graphviz DOT not supported", __FILE__, __LINE__);
		else if (system(("rm -f " + tfilename).c_str()) != 0)
			warning("", "Temporary files not cleaned up", __FILE__, __LINE__);
#endif
	}
}

int plot_command(configuration &config, int argc, char **argv) {
	tokenizer tokens;
	tokens.register_token<parse::block_comment>(false);
	tokens.register_token<parse::line_comment>(false);

	string filename = "";
	string format = "";

	string ofilename = "a.png";
	string oformat = "png";

	int encodings = -1;

	bool proper = true;
	bool aggressive = false;
	bool process = true;
	bool labels = false;
	bool notations = false;
	bool horiz = false;
	bool states = false;
	bool petri = false;

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
			size_t dot = ofilename.find_last_of(".");
			if (dot == string::npos) {
				printf("unrecognized file format\n");
				return 1;
			}
			oformat = ofilename.substr(dot+1);
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
		if (process) {
			cg.post_process(v, true);
		}

		if (is_clean()) {
			render(ofilename, oformat, export_graph(cg, v, labels).to_string());
		}
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
		if (process) {
			hg.post_process(v, true);
		}
		hg.check_variables(v);

		if (is_clean()) {
			if (states) {
				hse::graph sg = hse::to_state_graph(hg, v, true);
				render(ofilename, oformat, export_graph(sg, v, horiz, labels, notations, encodings).to_string());
			} else if (petri) {
				hse::graph pn = hse::to_petri_net(hg, v, true);
				render(ofilename, oformat, export_graph(hg, v, horiz, labels, notations, encodings).to_string());
			} else {
				render(ofilename, oformat, export_graph(hg, v, horiz, labels, notations, encodings).to_string());
			}
		}
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
	}

	complete();
	return is_clean();
}
