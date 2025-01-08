#include "build.h"
#include "cli.h"

#include <common/standard.h>
#include <common/timer.h>
#include <common/text.h>
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
#include <prs/synthesize.h>
#include <interpret_prs/import.h>
#include <interpret_prs/export.h>

#include <sch/Netlist.h>
#include <sch/Tapeout.h>
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

#include <filesystem>

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

void print_conflicts(const hse::encoder &enc) {
	for (int sense = -1; sense < 2; sense++) {
		for (auto i = enc.conflicts.begin(); i != enc.conflicts.end(); i++) {
			if (i->sense == sense) {
				printf("T%d.%d\t...%s...   conflicts with:\n", i->index.index, i->index.term, export_node(i->index.iter(), *enc.base, *enc.variables).c_str());

				for (auto j = i->region.begin(); j != i->region.end(); j++) {
					printf("\t%s\t...%s...\n", j->to_string().c_str(), export_node(*j, *enc.base, *enc.variables).c_str());
				}
				printf("\n");
			}
		}
		printf("\n");
	}
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
			hse::elaborate(g, v, true, true, true);
		else if ((strncmp(command, "conflicts", 9) == 0 && length == 9) || (strncmp(command, "c", 1) == 0 && length == 1))
		{
			enc.check(true, true);
			print_conflicts(enc);
		}
		else if ((strncmp(command, "conflicts up", 12) == 0 && length == 12) || (strncmp(command, "cu", 2) == 0 && length == 2))
		{
			enc.check(false, true);
			print_conflicts(enc);
		}
		else if ((strncmp(command, "conflicts down", 14) == 0 && length == 14) || (strncmp(command, "cd", 2) == 0 && length == 2))
		{
			enc.check(false, true);
			print_conflicts(enc);
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

void export_cells(const phy::Library &lib, const sch::Netlist &net) {
	if (not filesystem::exists(lib.tech->lib)) {
		filesystem::create_directory(lib.tech->lib);
	}
	for (int i = 0; i < (int)lib.macros.size(); i++) {
		if (lib.macros[i].name.rfind("cell_", 0) == 0) {
			string cellPath = lib.tech->lib + "/" + lib.macros[i].name;
			if (not filesystem::exists(cellPath+".gds")) {
				export_layout(cellPath+".gds", lib.macros[i]);
				export_lef(cellPath+".lef", lib.macros[i]);
				if (i < (int)net.subckts.size()) {
					export_spi(cellPath+".spi", net, net.subckts[i]);
				}
			}
		}
	}
}

// returns whether the cell was imported
bool loadCell(phy::Library &lib, sch::Netlist &lst, int idx, bool progress=false, bool debug=false) {
	if (idx >= (int)lib.macros.size()) {
		lib.macros.resize(idx+1, Layout(*lib.tech));
	}
	lib.macros[idx].name = lst.subckts[idx].name;
	string cellPath = lib.tech->lib + "/" + lib.macros[idx].name+".gds";
	if (progress) {
		printf("  %s...", lib.macros[idx].name.c_str());
		fflush(stdout);
		printf("[");
	}

	Timer tmr;
	float searchDelay = 0.0;
	float genDelay = 0.0;

	sch::Subckt spiNet = lst.subckts[idx];
	spiNet.cleanDangling(true);
	spiNet.combineDevices();
	spiNet.canonicalize();

	if (filesystem::exists(cellPath)) {
		bool imported = import_layout(lib.macros[idx], cellPath, lib.macros[idx].name);
		if (progress) {
			if (imported) {
				sch::Subckt gdsNet(true);
				extract(gdsNet, lib.macros[idx], true);
				gdsNet.cleanDangling(true);
				gdsNet.combineDevices();
				gdsNet.canonicalize();
				searchDelay = tmr.since();
				if (gdsNet.compare(spiNet) == 0) {
					printf("%sFOUND%s]\t%gs\n", KGRN, KNRM, searchDelay);
				} else {
					printf("%sFAILED LVS%s, ", KRED, KNRM);
					imported = false;
				}
			} else {
				searchDelay = tmr.since();
				printf("%sFAILED IMPORT%s, ", KRED, KNRM);
			}
		}
		if (imported) {
			return true;
		} else {
			lib.macros[idx].clear();
		}
	}

	tmr.reset();

	int result = sch::routeCell(lib, lst, idx);
	if (progress) {
		if (result == 1) {
			genDelay = tmr.since();
			printf("%sFAILED PLACEMENT%s]\t(%gs %gs)\n", KRED, KNRM, searchDelay, genDelay);
		} else if (result == 2) {
			genDelay = tmr.since();
			printf("%sFAILED ROUTING%s]\t(%gs %gs)\n", KRED, KNRM, searchDelay, genDelay);
		} else {
			sch::Subckt gdsNet(true);
			extract(gdsNet, lib.macros[idx], true);
			gdsNet.cleanDangling(true);
			gdsNet.combineDevices();
			gdsNet.canonicalize();

			genDelay = tmr.since();
			if (gdsNet.compare(spiNet) == 0) {
				printf("%sGENERATED%s]\t(%gs %gs)\n", KGRN, KNRM, searchDelay, genDelay);
			} else {
				printf("%sFAILED LVS%s]\t(%gs %gs)\n", KRED, KNRM, searchDelay, genDelay);
				if (debug) {
					gdsNet.print();
					spiNet.print();
				}
			}
		}
	}
	return false;
}

void loadCells(phy::Library &lib, sch::Netlist &lst, gdstk::GdsWriter *out=nullptr, bool progress=false, bool debug=false) {
	bool libFound = filesystem::exists(lib.tech->lib);
	if (progress) {
		printf("Load cell layouts:\n");
	}
	Timer tmr;
	lib.macros.reserve(lst.subckts.size()+lib.macros.size());
	for (int i = 0; i < (int)lst.subckts.size(); i++) {
		if (lst.subckts[i].isCell) {
			if (not loadCell(lib, lst, i, progress, debug)) {
				// We generated a new cell, save this to the cell library
				if (not libFound) {
					filesystem::create_directory(lib.tech->lib);
					libFound = true;
				}
				string cellPath = lib.tech->lib + "/" + lib.macros[i].name;
				export_layout(cellPath+".gds", lib.macros[i]);
				export_lef(cellPath+".lef", lib.macros[i]);
				export_spi(cellPath+".spi", lst, lst.subckts[i]);
			}
			if (out != nullptr) {
				out->write_cell(*phy::export_layout(lib.macros[i]));
			}
		}
	}
	if (progress) {
		printf("done\t%gs\n\n", tmr.since());
	}
}

void build_help() {
	printf("\nUsage: lm build [options] <file>\n");
	printf("Synthesize the production rules that implement the behavioral description.\n");

	printf("\nOptions:\n");
	printf(" --logic <family>      pick the logic family you want to synthesize to. The following logic families are supported:\n");
	printf("         raw           do not require inverting logic\n");
	printf("         cmos          require inverting logic (default)\n");
	printf("\n");
	printf(" --all          save all intermediate stages\n");
	printf(" -o,--out       set the filename prefix for the saved intermediate stages\n\n");
	printf(" -g,--graph     save the elaborated astg\n");
	printf(" -c,--conflicts print the conflicts to stdout\n");
	printf(" -e,--encode    save the complete state encoded astg\n");
	printf(" -r,--rules     save the synthesized production rules\n");
	printf(" -b,--bubble    save the bubble reshuffled production rules\n");
	printf(" -k,--keepers   save the production rules with added state-holding elements\n");
	printf(" -s,--size      save the sized production rules\n");
	printf(" -n,--nets      save the generated netlist\n");
	printf(" -l,--cells     save the netlist split into cells\n");

	printf("\nSupported file formats:\n");
	printf(" *.cog          a wire-level programming language\n");
	printf(" *.chp          a data-level process calculi called Communicating Hardware Processes\n");
	printf(" *.hse          a wire-level process calculi called Hand-Shaking Expansions\n");
	printf(" *.prs          production rules\n");
	printf(" *.astg         asynchronous signal transition graph\n");
}

int build_command(configuration &config, string techPath, string cellsDir, int argc, char **argv, bool progress, bool debug) {
	const int LOGIC_RAW = 0;
	const int LOGIC_CMOS = 1;
	const int LOGIC_ADIABATIC = 2;

	const int DO_ELAB = 0;
	const int DO_CONFLICTS = 1;
	const int DO_ENCODE = 2;
	const int DO_RULES = 3;
	const int DO_BUBBLE = 4;
	const int DO_KEEPERS = 5;
	const int DO_SIZE = 6;
	const int DO_NETS = 7;
	const int DO_CELLS = 8;
	const int DO_PLACE = 9;
	const int DO_ROUTE = 10;

	tokenizer tokens;
	
	string filename = "";
	string prefix = "";
	string format = "";
	
	int logic = LOGIC_CMOS;
	int stage = -1;

	bool doPreprocess = false;
	bool doPostprocess = false;

	bool doElab = false;
	bool doConflicts = false;
	bool doEncode = false;
	bool doRules = false;
	bool doBubble = false;
	bool doKeepers = false;
	bool doSize = false;
	bool doNets = false;
	bool doCells = false;
	bool doPlace = false;
	bool doRoute = false;

	bool noCells = false;
	bool noGhosts = false;

	for (int i = 0; i < argc; i++) {
		string arg = argv[i];

		if (arg == "--logic") {
			if (++i >= argc) {
				printf("expected logic family (raw, cmos)\n");
			}
			arg = argv[i];

			if (arg == "raw") {
				logic = LOGIC_RAW;
			} else if (arg == "cmos") {
				logic = LOGIC_CMOS;
			} else if (arg == "adiabatic") {
				// This is not an officially supported logic family
				// It is here for experimental purposes
				logic = LOGIC_ADIABATIC;
			} else {
				printf("unsupported logic family '%s'\n", argv[i]);
				return is_clean();
			}
		} else if (arg == "--all") {
			doElab = true;
			doConflicts = true;
			doEncode = true;
			doRules = true;
			doBubble = true;
			doKeepers = true;
			doSize = true;
			doNets = true;
			doCells = true;
			doPlace = true;
			doRoute = true;
		} else if (arg == "--pre") {
			doPreprocess = true;
		} else if (arg == "--post") {
			doPostprocess = true;
		} else if (arg == "-g" or arg == "--graph") {
			doElab = true;
			set_stage(stage, DO_ELAB);
		} else if (arg == "-c" or arg == "--conflict") {
			doConflicts = true;
			set_stage(stage, DO_CONFLICTS);
		} else if (arg == "-e" or arg == "--encode") {
			doEncode = true;
			set_stage(stage, DO_ENCODE);
		} else if (arg == "-r" or arg == "--rules") {
			doRules = true;
			set_stage(stage, DO_RULES);
		} else if (arg == "-b" or arg == "--bubble") {
			doBubble = true;
			set_stage(stage, DO_BUBBLE);
		} else if (arg == "-k" or arg == "--keepers") {
			doKeepers = true;
			set_stage(stage, DO_KEEPERS);
		} else if (arg == "-s" or arg == "--size") {
			doSize = true;
			set_stage(stage, DO_SIZE);
		} else if (arg == "-n" or arg == "--nets") {
			doNets = true;
			set_stage(stage, DO_NETS);
		} else if (arg == "-l" or arg == "--cells") {
			doCells = true;
			set_stage(stage, DO_CELLS);
		} else if (arg == "-p" or arg == "--place") {
			doPlace = true;
			set_stage(stage, DO_PLACE);
		} else if (arg == "-x" or arg == "--route") {
			doRoute = true;
			set_stage(stage, DO_ROUTE);
		} else if (arg == "+g" or arg == "++graph") {
			doElab = true;
		} else if (arg == "+c" or arg == "++conflict") {
			doConflicts = true;
		} else if (arg == "+e" or arg == "++encode") {
			doEncode = true;
		} else if (arg == "+r" or arg == "++rules") {
			doRules = true;
		} else if (arg == "+b" or arg == "++bubble") {
			doBubble = true;
		} else if (arg == "+k" or arg == "++keepers") {
			doKeepers = true;
		} else if (arg == "+s" or arg == "++size") {
			doSize = true;
		} else if (arg == "+n" or arg == "++nets") {
			doNets = true;
		} else if (arg == "+l" or arg == "++cells") {
			doCells = true;
		} else if (arg == "+p" or arg == "++place") {
			doPlace = true;
		} else if (arg == "+x" or arg == "++route") {
			doRoute = true;
		} else if (arg == "--no-cells") {
			noCells = true;
		} else if (arg == "--no-ghosts") {
			noGhosts = true;
		} else if (arg == "-o" or arg == "--out") {
			if (++i >= argc) {
				printf("expected output prefix\n");
			}

			prefix = argv[i];
		} else {
			string path = extractPath(arg);
			string opt = (arg.size() > path.size() ? arg.substr(path.size()+1) : "");

			size_t dot = path.find_last_of(".");
			string ext = "";
			if (dot != string::npos) {
				ext = path.substr(dot+1);
				if (ext == "spice"
					or ext == "sp"
					or ext == "s") {
					ext = "spi";
				}
			}

			filename = path;
			format = ext;

			if (prefix == "") {
				prefix = dot != string::npos ? filename.substr(0, dot) : filename;
			}

			if (ext != "chp"
				and ext != "hse"
				and ext != "cog"
				and ext != "astg"
				and ext != "prs"
				and ext != "spi"
				and ext != "gds") {
				printf("unrecognized file format '%s'\n", ext.c_str());
				return 0;
			}
		}
	}

	bool inverting = false;
	if (logic == LOGIC_CMOS) {
		inverting = true;
	}

	if (filename == "") {
		printf("expected filename\n");
		return 0;
	}

	ucs::variable_set v;
	chp::graph cg;
	hse::graph hg;
	hg.name = prefix;
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
			if (prefix != "") {
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
	} else if (format == "cog") {
		tokens.register_token<parse::block_comment>(false);
		tokens.register_token<parse::line_comment>(false);
		parse_cog::composition::register_syntax(tokens);
		config.load(tokens, filename, "");

		tokens.increment(false);
		tokens.expect<parse_cog::composition>();
		while (tokens.decrement(__FILE__, __LINE__))
		{
			parse_cog::composition syntax(tokens);
			boolean::cover covered;
			bool hasRepeat = false;
			hg.merge(hse::parallel, hse::import_hse(syntax, v, covered, hasRepeat, 0, &tokens, true));

			tokens.increment(false);
			tokens.expect<parse_cog::composition>();
		}
	}

	if (doPreprocess) {
		FILE *fout = stdout;
		if (prefix != "") {
			fout = fopen((prefix+"_pre.astg").c_str(), "w");
		}
		fprintf(fout, "%s", export_astg(hg, v).to_string().c_str());
		fclose(fout);
	}

	if (!is_clean()) {
		complete();
		return 1;
	}

	if (format == "chp"
		or format == "hse"
		or format == "cog"
		or format == "astg") {
		hg.post_process(v, true);
		hg.check_variables(v);

		if (doPostprocess) {
			FILE *fout = stdout;
			if (prefix != "") {
				fout = fopen((prefix+"_post.astg").c_str(), "w");
			}
			fprintf(fout, "%s", export_astg(hg, v).to_string().c_str());
			fclose(fout);
		}

		if (progress) printf("Elaborate state space:\n");
		hse::elaborate(hg, v, stage >= DO_ENCODE or not noGhosts, true, progress);
		if (progress) printf("done\n\n");

		if (not is_clean()) {
			complete();
			return 1;
		}

		if (doElab) {
			FILE *fout = stdout;
			if (prefix != "") {
				string suffix = stage == DO_ELAB ? "" : "_predicate";
				fout = fopen((prefix+suffix+".astg").c_str(), "w");
			}
			fprintf(fout, "%s", export_astg(hg, v).to_string().c_str());
			fclose(fout);
		}

		if (stage >= 0 and stage < DO_CONFLICTS) {
			complete();
			return is_clean();
		}

		hse::encoder enc;
		enc.base = &hg;
		enc.variables = &v;

		if (progress) {
			printf("Identify state conflicts:\n");
		}
		enc.check(!inverting, progress);
		if (progress) {
			printf("done\n\n");
		}

		if (doConflicts) {
			print_conflicts(enc);
		}

		if (stage >= 0 and stage < DO_ENCODE) {
			complete();
			return is_clean();
		}

		if (progress) printf("Insert state variables:\n");
		if (not enc.insert_state_variables(20, !inverting, progress, false)) {
			complete();
			return 1;
		}
		if (progress) printf("done\n\n");

		if (doEncode) {
			FILE *fout = stdout;
			if (prefix != "") {
				string suffix = stage == DO_ENCODE ? "" : "_complete";
				fout = fopen((prefix+suffix+".astg").c_str(), "w");
			}
			fprintf(fout, "%s", export_astg(hg, v).to_string().c_str());
			fclose(fout);
		}

		if (enc.conflicts.size() > 0) {
			// state variable insertion failed
			print_conflicts(enc);
			complete();
			return is_clean();
		}

		if (stage >= 0 and stage < DO_RULES) {
			complete();
			return is_clean();
		}

		if (progress) printf("Synthesize production rules:\n");
		hse::synthesize_rules(&pr, &hg, &v, !inverting, progress);
		if (progress) printf("done\n\n");

		if (doRules) {
			FILE *fout = stdout;
			if (prefix != "") {
				string suffix = stage == DO_RULES ? "" : "_simple";
				fout = fopen((prefix+suffix+".prs").c_str(), "w");
			}
			fprintf(fout, "%s", export_production_rule_set(pr, v).to_string().c_str());
			fclose(fout);
		}
	}

	if (stage >= 0 and stage < DO_BUBBLE) {
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
			map<int, int> nodemap;
			prs::import_production_rule_set(syntax, pr, -1, -1, prs::attributes(), v, nodemap, 0, &tokens, true);
		}
	}

	if (!is_clean()) {
		complete();
		return 1;
	}

	if (format == "chp"
		or format == "hse"
		or format == "cog"
		or format == "astg"
		or format == "prs") {
		if (inverting and not pr.cmos_implementable()) {
			if (progress) {
				printf("Bubble reshuffle production rules:\n");
				printf("  %s...", prefix.c_str());
				fflush(stdout);
			}
			bub.load_prs(pr, v);

			int step = 0;
			if (doBubble and debug) {
				save_bubble(prefix+"_bubble0.png", bub, v);
			}
			for (auto i = bub.net.begin(); i != bub.net.end(); i++) {
				auto result = bub.step(i);
				if (doBubble and debug and result.second) {
					save_bubble(prefix+"_bubble" + to_string(++step) + ".png", bub, v);
				}
			}
			auto result = bub.complete();
			if (doBubble and debug and result) {
				save_bubble(prefix+"_bubble" + to_string(++step) + ".png", bub, v);
			}

			bub.save_prs(&pr, v);
			if (progress) {
				printf("[%sDONE%s]\n", KGRN, KNRM);
				printf("done\n\n");
			}

			if (doBubble) {
				FILE *fout = stdout;
				if (prefix != "") {
					string suffix = stage == DO_BUBBLE ? "" : "_bubbled";
					fout = fopen((prefix+suffix+".prs").c_str(), "w");
				}
				fprintf(fout, "%s", export_production_rule_set(pr, v).to_string().c_str());
				fclose(fout);
			}
		}

		if (stage >= 0 and stage < DO_KEEPERS) {
			complete();
			return is_clean();
		}

		if (logic == LOGIC_CMOS or logic == LOGIC_RAW) {
			if (progress) printf("Insert keepers:\n");
			pr.add_keepers(v, true, false, 1, progress);
			if (progress) printf("done\n\n");

			if (doKeepers) {
				FILE *fout = stdout;
				if (prefix != "") {
					string suffix = stage == DO_KEEPERS ? "" : "_keep";
					fout = fopen((prefix+suffix+".prs").c_str(), "w");
				}
				fprintf(fout, "%s", export_production_rule_set(pr, v).to_string().c_str());
				fclose(fout);
			}
		}

		if (stage >= 0 and stage < DO_SIZE) {
			complete();
			return is_clean();
		}

		if (progress) printf("Size production rules:\n");
		pr.size_devices(0.1, progress);
		if (progress) printf("done\n\n");

		if (doSize) {
			FILE *fout = stdout;
			if (prefix != "") {
				string suffix = stage == DO_SIZE ? "" : "_sized";
				fout = fopen((prefix+suffix+".prs").c_str(), "w");
			}
			fprintf(fout, "%s", export_production_rule_set(pr, v).to_string().c_str());
			fclose(fout);
		}
	}

	if (stage >= 0 and stage < DO_NETS) {
		complete();
		return is_clean();
	}

	if (techPath.empty()) {
		if (stage >= DO_NETS or format == "spi" or format == "gds") {
			cout << "please provide a python techfile." << endl;
		} else {
			FILE *fout = stdout;
			if (prefix != "") {
				fout = fopen((prefix+".prs").c_str(), "w");
			}
			fprintf(fout, "%s", export_production_rule_set(pr, v).to_string().c_str());
			fclose(fout);
		}
		if (!is_clean()) {
			complete();
			return 1;
		}
		return 0;
	}

	phy::Tech tech;
	if (not phy::loadTech(tech, techPath, cellsDir)) {
		cout << "Unable to load techfile \'" + techPath + "\'." << endl;
		FILE *fout = stdout;
		if (prefix != "") {
			fout = fopen((prefix+".prs").c_str(), "w");
		}
		fprintf(fout, "%s", export_production_rule_set(pr, v).to_string().c_str());
		fclose(fout);
		complete();
		return 1;
	}
	sch::Netlist net(tech);

	if (format == "chp"
		or format == "hse"
		or format == "cog"
		or format == "astg"
		or format == "prs") {
		if (progress) printf("Build netlist:\n");
		net.subckts.push_back(prs::build_netlist(tech, pr, v, progress));
		if (progress) printf("done\n\n");
		if (debug) {
			net.subckts.back().print();
		}

		if (doNets) {
			FILE *fout = stdout;
			if (prefix != "") {
				string suffix = stage == DO_NETS ? "" : "_simple";
				fout = fopen((prefix+".spi").c_str(), "w");
			}
			fprintf(fout, "%s", sch::export_netlist(net).to_string().c_str());
			fclose(fout);
		}
	}

	if (stage >= 0 and stage < DO_CELLS) {
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

	if (noCells) {
		for (int i = 0; i < (int)net.subckts.size(); i++) {
			net.subckts[i].isCell = true;
		}
	}

	if (progress) printf("Break subckts into cells:\n");
	Timer cellsTmr;
	net.mapCells(progress);
	if (progress) printf("done\t%gs\n\n", cellsTmr.since());

	if (format != "spi") {
		FILE *fout = stdout;
		if (prefix != "") {
			fout = fopen((prefix+".spi").c_str(), "w");
		}
		fprintf(fout, "%s", sch::export_netlist(net).to_string().c_str());
		fclose(fout);
	}

	phy::Library lib(tech);
	gdstk::GdsWriter gds = {};
	gds = gdstk::gdswriter_init((prefix+".gds").c_str(), prefix.c_str(), ((double)tech.dbunit)*1e-6, ((double)tech.dbunit)*1e-6, 4, nullptr, nullptr);

	if (format == "chp"
		or format == "hse"
		or format == "cog"
		or format == "astg"
		or format == "prs"
		or format == "spi") {
		loadCells(lib, net, &gds, progress, debug);
	}

	if (stage >= 0 and stage < DO_PLACE) {
		gds.close();
		complete();
		return is_clean();
	}
	
	gds.close();

	if (!is_clean()) {
		complete();
		return 1;
	}

	return 0;
}

