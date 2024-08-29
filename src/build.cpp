#include "build.h"

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

#include <sch/Netlist.h>
#include <interpret_sch/import.h>
#include <interpret_sch/export.h>

#include <phy/Tech.h>
#include <phy/Script.h>
#include <phy/Layout.h>
#include <phy/Library.h>
#include <interpret_phy/import.h>
#include <interpret_phy/export.h>

//#include <parse_expression/expression.h>
//#include <parse_expression/assignment.h>
//#include <parse_expression/composition.h>
#include <interpret_boolean/export.h>
#include <interpret_boolean/import.h>
#include <interpret_arithmetic/export.h>
#include <interpret_arithmetic/import.h>
#include <ucs/variable.h>

//printf(" -c             check for state conflicts that occur regardless of sense\n");
//	printf(" -cu            check for state conflicts that occur due to up-going transitions\n");
//	printf(" -cd            check for state conflicts that occur due to down-going transitions\n");


#ifdef GRAPHVIZ_SUPPORTED
namespace graphviz
{
	#include <graphviz/cgraph.h>
	#include <graphviz/gvc.h>
}
#endif

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
			hse::elaborate(g, v, true, true);
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

void save_bubble(string filename, const prs::bubble &bub, const ucs::variable_set &vars)
{
	string dot = export_bubble(bub, vars).to_string();
	size_t pos = filename.find_last_of(".");
	string format = "png";
	if (pos != string::npos) {
		format = filename.substr(pos+1);
		filename = filename.substr(0, pos);
	}
	
	if (format == "dot") {
		FILE *file = fopen((filename+".dot").c_str(), "w");
		fprintf(file, "%s\n", dot.c_str());
		fclose(file);
	} else {
#ifdef GRAPHVIZ_SUPPORTED
		graphviz::Agraph_t* G = graphviz::agmemread(dot.c_str());
		graphviz::GVC_t* gvc = graphviz::gvContext();
		graphviz::gvLayout(gvc, G, "dot");
		graphviz::gvRenderFilename(gvc, G, format.c_str(), (filename+"."+format).c_str());
		graphviz::gvFreeLayout(gvc, G);
		graphviz::agclose(G);
		graphviz::gvFreeContext(gvc);
#else
		string tfilename = filename;
		FILE *temp = NULL;
		int num = 0;
		for (; temp == NULL; num++)
			temp = fopen((tfilename + (num > 0 ? to_string(num) : "") + ".dot").c_str(), "wx");
		num--;
		tfilename += (num > 0 ? to_string(num) : "") + ".dot";

		fprintf(temp, "%s\n", dot.c_str());
		fclose(temp);

		if (system(("dot -T" + format + " " + tfilename + " > " + filename + "." + format).c_str()) != 0)
			error("", "Graphviz DOT not supported", __FILE__, __LINE__);
		else if (system(("rm -f " + tfilename).c_str()) != 0)
			warning("", "Temporary files not cleaned up", __FILE__, __LINE__);
#endif
	}
}

int build_command(configuration &config, int argc, char **argv, bool progress) {
	tokenizer tokens;
	
	string filename = "";
	string prefix = "";
	string format = "";
	string techPath = "";

	bool hasPrefix = false;
	bool cmos = true;
	int stage = -1;

	bool doElab = false;
	bool doConflicts = false;
	bool doEncode = false;
	bool doRules = false;
	bool doBubble = false;
	bool doSize = false;
	bool doNets = false;
	bool doCells = false;
	bool doPlace = false;
	bool doRoute = false;

	for (int i = 0; i < argc; i++) {
		string arg = argv[i];

		if (arg == "--no-cmos") {
			cmos = false;
		} else if (arg == "--all") {
			doElab = true;
			doConflicts = true;
			doEncode = true;
			doRules = true;
			doBubble = true;
			doSize = true;
			doNets = true;
			doCells = true;
			doPlace = true;
			doRoute = true;
		} else if (arg == "-g" or arg == "--graph") {
			doElab = true;
			stage = stage < 0 ? 0 : stage;
		} else if (arg == "-c" or arg == "--conflict") {
			doConflicts = true;
			stage = stage < 1 ? 1 : stage;
		} else if (arg == "-e" or arg == "--encode") {
			doEncode = true;
			stage = stage < 2 ? 2 : stage;
		} else if (arg == "-r" or arg == "--rules") {
			doRules = true;
			stage = stage < 3 ? 3 : stage;
		} else if (arg == "-b" or arg == "--bubble") {
			doBubble = true;
			stage = stage < 4 ? 4 : stage;
		} else if (arg == "-s" or arg == "--size") {
			doSize = true;
			stage = stage < 5 ? 5 : stage;
		} else if (arg == "-n" or arg == "--nets") {
			doNets = true;
			stage = stage < 6 ? 6 : stage;
		} else if (arg == "-l" or arg == "--cells") {
			doCells = true;
			stage = stage < 7 ? 7 : stage;
		} else if (arg == "-p" or arg == "--place") {
			doPlace = true;
			stage = stage < 8 ? 8 : stage;
		} else if (arg == "-x" or arg == "--route") {
			doRoute = true;
			stage = stage < 9 ? 9 : stage;
		} else if (arg == "-o" or arg == "--out") {
			if (++i >= argc) {
				printf("expected output prefix\n");
			}

			hasPrefix = true;
			prefix = argv[i];
		} else {
			size_t dot = arg.find_last_of(".");
			if (dot == string::npos) {
				printf("unrecognized file format\n");
				return 0;
			}
			string ext = arg.substr(dot+1);
			if (ext == "spice"
				or ext == "sp"
				or ext == "s") {
				ext = "spi";
			}

			if (ext == "py") {
				techPath = arg;
			} else {
				filename = arg;
				format = ext;

				if (prefix == "") {
					prefix = filename.substr(0, dot);
				}
			}

			if (ext != "chp"
				and ext != "hse"
				and ext != "astg"
				and ext != "prs"
				and ext != "spi"
				and ext != "gds"
				and ext != "py") {
				printf("unrecognized file format '%s'\n", ext.c_str());
				return 0;
			}
		}
	}

	if (filename == "") {
		printf("expected filename\n");
		return 0;
	}

	ucs::variable_set v;
	chp::graph cg;
	hse::graph hg;
	prs::production_rule_set pr;
	prs::bubble bub;

	if (format == "chp") {
		tokens.register_token<parse::block_comment>(false);
		tokens.register_token<parse::line_comment>(false);
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
	}

	if (!is_clean()) {
		complete();
		return 1;
	}

	if (format == "chp") {

		if (doElab) {
			FILE *fout = stdout;
			if (hasPrefix and prefix != "") {
				fout = fopen((prefix+".astg").c_str(), "w");
			}
			fprintf(fout, "%s", export_astg(cg, v).to_string().c_str());
			fclose(fout);
		}

	
		// TODO(edward.bingham) implement handshaking expansion
		// TODO(edward.bingham) implement handshaking reshuffling
		return 0;
	}

	if (format == "hse") {
		tokens.register_token<parse::block_comment>(false);
		tokens.register_token<parse::line_comment>(false);
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
		tokens.register_token<parse::block_comment>(false);
		tokens.register_token<parse::line_comment>(false);
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

	if (!is_clean()) {
		complete();
		return 1;
	}

	if (format == "chp"
		or format == "hse"
		or format == "astg") {
		hg.post_process(v, true);
		hg.check_variables(v);

		hse::elaborate(hg, v, true, progress);
		if (not is_clean()) {
			complete();
			return 1;
		}

		if (doElab) {
			FILE *fout = stdout;
			if (hasPrefix and prefix != "") {
				fout = fopen((prefix+"_predicate.astg").c_str(), "w");
			}
			fprintf(fout, "%s", export_astg(hg, v).to_string().c_str());
			fclose(fout);
		}

		if (stage >= 0 and stage < 1) {
			complete();
			return is_clean();
		}

		hse::encoder enc;
		enc.base = &hg;
		enc.variables = &v;

		enc.check(!cmos, progress);

		if (doConflicts) {
			if (!cmos) {
				print_conflicts(enc, hg, v, -1);
			} else {
				print_conflicts(enc, hg, v, 0);
				print_conflicts(enc, hg, v, 1);
			}
		}

		if (stage >= 0 and stage < 2) {
			complete();
			return is_clean();
		}

		for (int i = 0; i < 10 and enc.conflicts.size() > 0; i++) {
			//if (!cmos) {
			//	print_conflicts(enc, g, v, -1);
			//} else {
			//	print_conflicts(enc, g, v, 0);
			//	print_conflicts(enc, g, v, 1);
			//}

			enc.insert_state_variables();

			hse::elaborate(hg, v, true, progress);
			if (not is_clean()) {
				complete();
				return 1;
			}

			enc.check(!cmos, progress);
		}

		if (enc.conflicts.size() > 0) {
			// state variable insertion failed
			if (!cmos) {
				print_conflicts(enc, hg, v, -1);
			} else {
				print_conflicts(enc, hg, v, 0);
				print_conflicts(enc, hg, v, 1);
			}

			complete();
			return is_clean();
		}

		if (doEncode) {
			FILE *fout = stdout;
			if (hasPrefix and prefix != "") {
				fout = fopen((prefix+"_complete.astg").c_str(), "w");
			}
			fprintf(fout, "%s", export_astg(hg, v).to_string().c_str());
			fclose(fout);
		}

		if (stage >= 0 and stage < 3) {
			complete();
			return is_clean();
		}

		hse::gate_set gates(&hg, &v);
		gates.load(!cmos);
		gates.weaken();
		gates.build_reset();
		gates.save(&pr);

		if (doRules) {
			FILE *fout = stdout;
			if (hasPrefix and prefix != "") {
				fout = fopen((prefix+"_simple.prs").c_str(), "w");
			}
			fprintf(fout, "%s", export_production_rule_set(pr, v).to_string().c_str());
			fclose(fout);
		}
	}

	if (stage >= 0 and stage < 4) {
		complete();
		return is_clean();
	}

	if (format == "prs") {
		tokens.register_token<parse::block_comment>(false);
		tokens.register_token<parse::line_comment>(false);
		parse_prs::production_rule_set::register_syntax(tokens);
		config.load(tokens, filename, "");

		tokens.increment(false);
		tokens.expect<parse_prs::production_rule_set>();
		if (tokens.decrement(__FILE__, __LINE__))
		{
			parse_prs::production_rule_set syntax(tokens);
			pr = prs::import_production_rule_set(syntax, v, 0, &tokens, true);
		}
	}

	if (!is_clean()) {
		complete();
		return 1;
	}

	if (format == "chp"
		or format == "hse"
		or format == "astg"
		or format == "prs") {	
		pr.post_process(v);

		if (not pr.cmos_implementable()) {
			bub.load_prs(pr, v);

			if (doBubble and hasPrefix) {
				save_bubble(prefix+"_bubble.png", bub, v);
			}

			//int step = 0;
			//if (render_steps) {
			//	save_bubble(oformat, step++, bub, v);
			//}
			for (auto i = bub.net.begin(); i != bub.net.end(); i++) {
				bub.step(i);
				//if (render_steps && result.second) {
				//	save_bubble(oformat, step++, bub, v);
				//}
			}

			bub.save_prs(&pr, v);

			if (doBubble) {
				FILE *fout = stdout;
				if (hasPrefix and prefix != "") {
					fout = fopen((prefix+"_bubbled.prs").c_str(), "w");
				}
				fprintf(fout, "%s", export_production_rule_set(pr, v).to_string().c_str());
				fclose(fout);

				// TODO(edward.bingham) emit bubble reshuffled production rules
			}
		}

		if (stage >= 0 and stage < 5) {
			complete();
			return is_clean();
		}

		// TODO(edward.bingham) Do prs sizing

		FILE *fout = stdout;
		if (hasPrefix and prefix != "") {
			fout = fopen((prefix+".prs").c_str(), "w");
		}
		fprintf(fout, "%s", export_production_rule_set(pr, v).to_string().c_str());
		fclose(fout);
	}

	if (stage >= 0 and stage < 6) {
		complete();
		return is_clean();
	}

	if (techPath == "") {
		if (stage >= 6 or format == "spi" or format == "gds") {
			cout << "please provide a python techfile." << endl;
		}
		if (!is_clean()) {
			complete();
			return 1;
		}
		return 0;
	}

	phy::Tech tech;
	phy::loadTech(tech, techPath);
	sch::Netlist net(tech);


	// TODO(edward.bingham) remove this when we have a way to convert production
	// rules into a spice netlist
	if (format == "chp"
		or format == "hse"
		or format == "astg"
		or format == "prs") {
		if (!is_clean()) {
			complete();
			return 1;
		}
		return 0;
	}

	if (stage >= 0 and stage < 7) {
		complete();
		return is_clean();
	}

	if (format == "spi") {
		parse_spice::netlist::register_syntax(tokens);
		config.load(tokens, filename, "");

		tokens.increment(false);
		tokens.expect<parse_spice::netlist>();
		if (tokens.decrement(__FILE__, __LINE__))
		{
			parse_spice::netlist syntax(tokens);
			sch::import_netlist(syntax, net, &tokens);
		}
	}

	phy::Library lib(&tech);
	
	if (format == "chp"
		or format == "hse"
		or format == "astg"
		or format == "prs"
		or format == "spi") {
		net.build();
		for (auto ckt = net.subckts.begin(); ckt != net.subckts.end(); ckt++) {
			lib.cells.push_back(Layout(tech));
			ckt->draw(lib.cells.back());
		}

		phy::export_library(prefix, prefix+".gds", lib);
	}

	if (stage >= 0 and stage < 8) {
		complete();
		return is_clean();
	}
		

	if (!is_clean()) {
		complete();
		return 1;
	}

	return 0;
}

