#include "sim.h"

#include <cinttypes>

#include <common/standard.h>
#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>

#include <parse_ucs/source.h>
#include <parse_astg/factory.h>
#include <parse_cog/factory.h>
#include <parse_chp/factory.h>
#include <parse_prs/factory.h>
#include <parse_spice/factory.h>

#include <chp/graph.h>
#include <chp/simulator.h>
#include <hse/graph.h>
#include <hse/simulator.h>
#include <prs/production_rule.h>
#include <prs/simulator.h>

#include "weaver/project.h"
#include "weaver/cli.h"

#include "format/dot.h"
#include "format/cog.h"
#include "format/spice.h"
#include "format/gds.h"
#include "format/verilog.h"
#include "format/prs.h"
#include "format/wv.h"
#include "format/astg.h"

#include "format/vcd.h"

#include <interpret_arithmetic/import.h>
#include <interpret_arithmetic/export.h>
#include <interpret_boolean/import.h>
#include <interpret_boolean/export.h>

#include <interpret_hse/export_cli.h>
#include <interpret_chp/export_cli.h>
#include <interpret_prs/export.h>

void sim_help() {
	printf("Usage: lm sim [options] <ckt-file> [sim-file]\n");
	printf("A simulation environment for various behavioral descriptions.\n");

	printf("\nSupported file formats:\n");
	printf(" *.cog           a wire-level programming language\n");
	printf(" *.chp           a data-level process calculi called Communicating Hardware Processes\n");
	printf(" *.hse           a wire-level process calculi called Hand-Shaking Expansions\n");
	printf(" *.prs           production rule set\n");
	printf(" *.astg          asynchronous signal transition graph\n");
	printf(" *.sim           Load a sequence of transitions to operate on\n");
}

void print_chpsim_help()
{
	printf("<arg> specifies a required argument\n(arg=value) specifies an optional argument with a default value\n");
	printf("\nGeneral:\n");
	printf(" help, h             print this message\n");
	printf(" seed <n>            set the random seed for the simulation\n");
	printf(" source <file>       source and execute a list of commands from a file\n");
	printf(" save <file>         save the sequence of fired transitions to a '.sim' file\n");
	printf(" load <file>         load a sequence of transitions from a '.sim' file\n");
	printf(" clear, c            clear any stored sequence and return to random stepping\n");
	printf(" quit, q             exit the interactive simulation environment\n");
	printf("\nRunning Simulation:\n");
	printf(" tokens, t           list the location and state information of every token\n");
	printf(" enabled, e          return the list of enabled transitions\n");
	printf(" fire <i>, f<i>      fire the i'th enabled transition\n");
	printf(" step (N=1), s(N=1)  step through N transitions (random unless a sequence is loaded)\n");
	printf(" reset (i), r(i)     reset the simulator to the initial marking and re-seed (does not clear)\n");
	printf("\nSetting/Viewing State:\n");
	printf(" set <i> <expr>      execute a transition as if it were local to the i'th token\n");
	printf(" set <expr>          execute a transition as if it were remote to all tokens\n");
	printf(" force <expr>        execute a transition as if it were local to all tokens\n");
}

void print_hsesim_help() {
	printf("<arg> specifies a required argument\n(arg=value) specifies an optional argument with a default value\n");
	printf("\nGeneral:\n");
	printf(" help, h             print this message\n");
	printf(" seed <n>            set the random seed for the simulation\n");
	printf(" source <file>       source and execute a list of commands from a file\n");
	printf(" save <file>         save the sequence of fired transitions to a '.sim' file\n");
	printf(" load <file>         load a sequence of transitions from a '.sim' file\n");
	printf(" clear, c            clear any stored sequence and return to random stepping\n");
	printf(" quit, q             exit the interactive simulation environment\n");
	printf("\nRunning Simulation:\n");
	printf(" tokens, t           list the location and state information of every token\n");
	printf(" enabled, e          return the list of enabled transitions\n");
	printf(" fire <i>, f<i>      fire the i'th enabled transition\n");
	printf(" step (N=1), s(N=1)  step through N transitions (random unless a sequence is loaded)\n");
	printf(" reset (i), r(i)     reset the simulator to the initial marking and re-seed (does not clear)\n");
	printf("\nSetting/Viewing State:\n");
	printf(" set <i> <expr>      execute a transition as if it were local to the i'th token\n");
	printf(" set <expr>          execute a transition as if it were remote to all tokens\n");
	printf(" force <expr>        execute a transition as if it were local to all tokens\n");
}

void print_prsim_help() {
	printf("<arg> specifies a required argument\n(arg=value) specifies an optional argument with a default value\n");
	printf("\nGeneral:\n");
	printf(" help, h             print this message\n");
	printf(" seed <n>            set the random seed for the simulation\n");
	printf(" source <file>       source and execute a list of commands from a file\n");
	printf(" save <file>         save the sequence of fired transitions to a '.sim' file\n");
	printf(" load <file>         load a sequence of transitions from a '.sim' file\n");
	printf(" clear, c            clear any stored sequence and return to random stepping\n");
	printf(" quit, q             exit the interactive simulation environment\n");
	printf("\nRunning Simulation:\n");
	printf(" tokens, t           list the location and state information of every token\n");
	printf(" enabled, e          return the list of enabled transitions\n");
	printf(" fire <i>, f<i>      fire the i'th enabled transition\n");
	printf(" step (N=1), s(N=1)  step through N transitions (random unless a sequence is loaded)\n");
	printf(" reset (i), r(i)     reset the simulator to the initial marking and re-seed (does not clear)\n");
	printf("\nSetting/Viewing State:\n");
	printf(" set <i> <expr>      execute a transition as if it were local to the i'th token\n");
	printf(" set <expr>          execute a transition as if it were remote to all tokens\n");
	printf(" force <expr>        execute a transition as if it were local to all tokens\n");
}

void chpsim(chp::graph &g, vector<chp::term_index> steps = vector<chp::term_index>()) {
	chp::simulator sim;
	sim.base = &g;

	tokenizer assignment_parser(false);
	parse_expression::composition::register_syntax(assignment_parser);

	// TODO(edward.bingham) use a minheap and random event times to implement a
	// discrete event simulator here based upon the set of enabled signals.
	//vector<pair<uint64_t, > > events;

	int seed = 0;
	srand(seed);
	int enabled = 0;
	bool uptodate = false;
	int step = 0;
	int n = 0, n1 = 0;
	char command[256];
	bool done = false;
	FILE *script = stdin;
	while (!done)
	{
		if (script == stdin)
		{
			printf("(chpsim)");
			fflush(stdout);
		}
		if (fgets(command, 255, script) == NULL && script != stdin)
		{
			fclose(script);
			script = stdin;
			printf("(chpsim)");
			fflush(stdout);
			if (fgets(command, 255, script) == NULL)
				exit(0);
		}
		int length = strlen(command);
		command[length-1] = '\0';
		length--;

		if ((strncmp(command, "help", 4) == 0 && length == 4) || (strncmp(command, "h", 1) == 0 && length == 1))
			print_chpsim_help();
		else if ((strncmp(command, "quit", 4) == 0 && length == 4) || (strncmp(command, "q", 1) == 0 && length == 1))
			done = true;
		else if (strncmp(command, "seed", 4) == 0)
		{
			if (sscanf(command, "seed %d", &n) == 1)
			{
				seed = n;
				srand(seed);
			}
			else
				printf("error: expected seed value\n");
		}
		else if ((strncmp(command, "clear", 5) == 0 && length == 5) || (strncmp(command, "c", 1) == 0 && length == 1))
			steps.resize(step);
		else if (strncmp(command, "source", 6) == 0 && length > 7)
		{
			script = fopen(&command[7], "r");
			if (script == NULL)
			{
				printf("error: file not found '%s'\n", &command[7]);
				script = stdin;
			}
		}
		else if (strncmp(command, "load", 4) == 0 && length > 5)
		{
			FILE *seq = fopen(&command[5], "r");
			if (seq != NULL)
			{
				while (fgets(command, 255, seq) != NULL)
				{
					if (sscanf(command, "%d.%d", &n, &n1) == 2)
						steps.push_back(chp::term_index(n, n1));
				}
				fclose(seq);
			}
			else
				printf("error: file not found '%s'\n", &command[5]);
		}
		else if (strncmp(command, "save", 4) == 0 && length > 5)
		{
			FILE *seq = fopen(&command[5], "w");
			if (seq != NULL)
			{
				for (int i = 0; i < (int)steps.size(); i++)
					fprintf(seq, "%d.%d\n", steps[i].index, steps[i].term);
				fclose(seq);
			}
		}
		else if (strncmp(command, "reset", 5) == 0 || strncmp(command, "r", 1) == 0)
		{
			if (sscanf(command, "reset %d", &n) == 1 || sscanf(command, "r%d", &n) == 1)
			{
				sim = chp::simulator(&g, g.reset[n]);
				uptodate = false;
				step = 0;
				srand(seed);
			}
			else
				for (int i = 0; i < (int)g.reset.size(); i++)
					printf("(%d) %s\n", i, g.reset[i].to_string(g).c_str());
		}
		else if ((strncmp(command, "tokens", 6) == 0 && length == 6) || (strncmp(command, "t", 1) == 0 && length == 1))
		{
			vector<vector<int> > tokens;
			for (int i = 0; i < (int)sim.tokens.size(); i++)
			{
				bool found = false;
				for (int j = 0; j < (int)tokens.size() && !found; j++)
					if (g.is_reachable(petri::iterator(chp::place::type, sim.tokens[i].index), petri::iterator(chp::place::type, sim.tokens[tokens[j][0]].index)))
					{
						tokens[j].push_back(i);
						found = true;
					}

				if (!found)
					tokens.push_back(vector<int>(1, i));
			}

			for (int i = 0; i < (int)tokens.size(); i++)
			{
				printf("%s {\n", export_composition(sim.encoding, g).to_string().c_str());
				for (int j = 0; j < (int)tokens[i].size(); j++) {
					int virt = -1;
					for (int k = 0; k < (int)sim.loaded.size() and virt < 0; k++) {
						if (find(sim.loaded[k].output_marking.begin(), sim.loaded[k].output_marking.end(), tokens[i][j]) != sim.loaded[k].output_marking.end()) {
							virt = k;
						}
					}
					if (virt >= 0) {
						printf("\t\t(%d) %s->%s\tP%d\t%s\n", tokens[i][j], export_expression(sim.tokens[tokens[i][j]].guard, g).to_string().c_str(), export_composition(g.transitions[sim.loaded[virt].index].action, g).to_string().c_str(), sim.tokens[tokens[i][j]].index, export_node(chp::iterator(chp::place::type, sim.tokens[tokens[i][j]].index), g).c_str());
					} else {
						printf("\t(%d) P%d\t%s\n", tokens[i][j], sim.tokens[tokens[i][j]].index, export_node(chp::iterator(chp::place::type, sim.tokens[tokens[i][j]].index), g).c_str());
					}
				}
				printf("}\n");
			}
		}
		else if ((strncmp(command, "enabled", 7) == 0 && length == 7) || (strncmp(command, "e", 1) == 0 && length == 1))
		{
			if (!uptodate)
			{
				enabled = sim.enabled();
				uptodate = true;
			}

			for (int i = 0; i < enabled; i++)
			{
				printf("(%d) T%d.%d:%s->%s : %s\n", i,
					sim.loaded[sim.ready[i].first].index,
					sim.ready[i].second,
					export_expression(sim.loaded[sim.ready[i].first].guard, g).to_string().c_str(), 
					export_composition(sim.loaded[sim.ready[i].first].local_action.states[sim.ready[i].second], g).to_string().c_str(),
					export_expression(sim.loaded[sim.ready[i].first].guard_action, g).to_string().c_str());
				if (sim.loaded[sim.ready[i].first].vacuous) {
					printf("\tvacuous");
				}
				if (!sim.loaded[sim.ready[i].first].stable) {
					printf("\tunstable");
				}
				printf("\n");
			}
			printf("\n");
		}
		else if ((strncmp(command, "disabled", 7) == 0 && length == 7) || (strncmp(command, "d", 1) == 0 && length == 1))
		{
			if (!uptodate)
			{
				enabled = sim.enabled();
				uptodate = true;
			}

			for (int i = 0; i < (int)sim.loaded.size(); i++)
			{
				printf("(%d) T%d:%s->%s\n", i, sim.loaded[i].index, export_expression(sim.loaded[i].guard_action, g).to_string().c_str(), export_composition(sim.loaded[i].local_action, g).to_string().c_str());
				if (sim.loaded[i].vacuous) {
					printf("\tvacuous");
				}
				if (!sim.loaded[i].stable) {
					printf("\tunstable");
				}
				printf("\n");
			}
			printf("\n");
		}

		else if (strncmp(command, "set", 3) == 0)
		{
			int i = 0;
			if (sscanf(command, "set %d ", &n) != 1)
			{
				n = -1;
				i = 4;
			}
			else
			{
				i = 5;
				while (i < length && command[i-1] != ' ')
					i++;
			}

			assignment_parser.insert("", string(command).substr(i));
			parse_expression::composition expr(assignment_parser);
			arithmetic::Parallel assign = arithmetic::import_parallel(expr, g, 0, &assignment_parser, false);
			if (assignment_parser.is_clean())
			{
				arithmetic::State local_action = assign.evaluate(sim.encoding);
				arithmetic::State remote_action = local_action.remote(g.remote_groups());
				sim.encoding = arithmetic::localAssign(sim.encoding, local_action, true);
				sim.global = arithmetic::localAssign(sim.global, remote_action, true);
				sim.encoding = arithmetic::remoteAssign(sim.encoding, sim.global, true);
			}
			assignment_parser.reset();
			uptodate = false;
		}
		else if (strncmp(command, "force", 5) == 0)
		{
			if (length <= 6)
				printf("error: expected expression\n");
			else
			{
				assignment_parser.insert("", string(command).substr(6));
				parse_expression::composition expr(assignment_parser);
				arithmetic::Parallel assign = arithmetic::import_parallel(expr, g, 0, &assignment_parser, false);
				if (assignment_parser.is_clean())
				{
					arithmetic::State local_action = assign.evaluate(sim.encoding);
					arithmetic::State remote_action = local_action.remote(g.remote_groups());
					sim.encoding = arithmetic::localAssign(sim.encoding, remote_action, true);
					sim.global = arithmetic::localAssign(sim.global, remote_action, true);
				}
				assignment_parser.reset();
				uptodate = false;
			}
		}
		else if (strncmp(command, "step", 4) == 0 || strncmp(command, "s", 1) == 0)
		{
			if (sscanf(command, "step %d", &n) != 1 && sscanf(command, "s%d", &n) != 1)
				n = 1;

			for (int i = 0; i < n && (enabled != 0 || !uptodate); i++)
			{
				if (!uptodate)
				{
					enabled = sim.enabled();
					uptodate = true;
				}

				if (enabled != 0)
				{
					int firing = rand()%enabled;
					bool vacuous = false;
					if (step < (int)steps.size())
					{
						for (firing = 0; firing < (int)sim.ready.size() &&
						(sim.loaded[sim.ready[firing].first].index != steps[step].index || sim.ready[firing].second != steps[step].term); firing++);

						if (firing == (int)sim.ready.size())
						{
							printf("error: loaded simulation does not match HSE, please clear the simulation to continue\n");
							break;
						}
					}
					else {
						vacuous = sim.loaded[sim.ready[firing].first].vacuous;
						steps.push_back(chp::term_index(sim.loaded[sim.ready[firing].first].index, sim.ready[firing].second));
					}

					/*for (auto h = sim.history.begin(); h != sim.history.end(); h++) {
						printf("%s->%s; ", export_expression(h->first, g).to_string().c_str(), export_composition(g.transitions[h->second.index].local_action[h->second.term], g).to_string().c_str());
					}
					printf("\n");*/

					string flags = "";
					if (vacuous) {
						flags = " [vacuous]";
					}
					printf("%d\tT%d.%d\t%s -> %s : %s%s\n", step,
						sim.loaded[sim.ready[firing].first].index,
						sim.ready[firing].second,
						export_expression(sim.loaded[sim.ready[firing].first].guard, g).to_string().c_str(),
						export_composition(sim.loaded[sim.ready[firing].first].local_action[sim.ready[firing].second], g).to_string().c_str(),
						export_expression(sim.loaded[sim.ready[firing].first].guard_action, g).to_string().c_str(),
						flags.c_str());

					sim.fire(firing);

					uptodate = false;
					sim.interference_errors.clear();
					sim.instability_errors.clear();
					sim.mutex_errors.clear();
					step++;
				}
			}
		}
		else if (strncmp(command, "fire", 4) == 0 || strncmp(command, "f", 1) == 0)
		{
			if (sscanf(command, "fire %d", &n) == 1 || sscanf(command, "f%d", &n) == 1)
			{
				if (!uptodate)
				{
					enabled = sim.enabled();
					uptodate = true;
				}

				if (n < enabled)
				{
					if (step < (int)steps.size())
						printf("error: deviating from loaded simulation, please clear the simulation to continue\n");
					else
					{
						bool vacuous = sim.loaded[sim.ready[n].first].vacuous;
						steps.push_back(chp::term_index(sim.loaded[sim.ready[n].first].index, sim.ready[n].second));

						string flags = "";
						if (vacuous) {
							flags = " [vacuous]";
						}

						printf("%d\tT%d.%d\t%s -> %s : %s%s\n", step,
							sim.loaded[sim.ready[n].first].index,
							sim.ready[n].second,
							export_expression(sim.loaded[sim.ready[n].first].guard, g).to_string().c_str(),
							export_composition(sim.loaded[sim.ready[n].first].local_action[sim.ready[n].second], g).to_string().c_str(),
							export_expression(sim.loaded[sim.ready[n].first].guard_action, g).to_string().c_str(),
							flags.c_str());
						
						sim.fire(n);

						uptodate = false;
						sim.interference_errors.clear();
						sim.instability_errors.clear();
						sim.mutex_errors.clear();
						step++;
					}
				}
				else
					printf("error: must be in the range [0,%d)\n", enabled);
			}
			else
				printf("error: expected ID in the range [0,%d)\n", enabled);
		}
		else if (length > 0)
			printf("error: unrecognized command '%s'\n", command);
	}

}

void hsesim(hse::graph &g, vector<hse::term_index> steps = vector<hse::term_index>()) {
	hse::simulator sim;
	sim.base = &g;

	tokenizer assignment_parser(false);
	parse_expression::composition::register_syntax(assignment_parser);

	// TODO(edward.bingham) use a minheap and random event times to implement a
	// discrete event simulator here based upon the set of enabled signals.
	//vector<pair<uint64_t, > > events;

	vcd dump;
	dump.create(g.name, g);

	int seed = 0;
	srand(seed);
	int enabled = 0;
	bool uptodate = false;
	int step = 0;
	int n = 0, n1 = 0;
	char command[256];
	bool done = false;
	FILE *script = stdin;
	while (!done)
	{
		if (script == stdin)
		{
			printf("(hsesim)");
			fflush(stdout);
		}
		if (fgets(command, 255, script) == NULL && script != stdin)
		{
			fclose(script);
			script = stdin;
			printf("(hsesim)");
			fflush(stdout);
			if (fgets(command, 255, script) == NULL)
				exit(0);
		}
		int length = strlen(command);
		command[length-1] = '\0';
		length--;

		if ((strncmp(command, "help", 4) == 0 && length == 4) || (strncmp(command, "h", 1) == 0 && length == 1))
			print_hsesim_help();
		else if ((strncmp(command, "quit", 4) == 0 && length == 4) || (strncmp(command, "q", 1) == 0 && length == 1))
			done = true;
		else if (strncmp(command, "seed", 4) == 0)
		{
			if (sscanf(command, "seed %d", &n) == 1)
			{
				seed = n;
				srand(seed);
			}
			else
				printf("error: expected seed value\n");
		}
		else if ((strncmp(command, "clear", 5) == 0 && length == 5) || (strncmp(command, "c", 1) == 0 && length == 1))
			steps.resize(step);
		else if (strncmp(command, "source", 6) == 0 && length > 7)
		{
			script = fopen(&command[7], "r");
			if (script == NULL)
			{
				printf("error: file not found '%s'\n", &command[7]);
				script = stdin;
			}
		}
		else if (strncmp(command, "load", 4) == 0 && length > 5)
		{
			FILE *seq = fopen(&command[5], "r");
			if (seq != NULL)
			{
				while (fgets(command, 255, seq) != NULL)
				{
					if (sscanf(command, "%d.%d", &n, &n1) == 2)
						steps.push_back(hse::term_index(n, n1));
				}
				fclose(seq);
			}
			else
				printf("error: file not found '%s'\n", &command[5]);
		}
		else if (strncmp(command, "save", 4) == 0 && length > 5)
		{
			FILE *seq = fopen(&command[5], "w");
			if (seq != NULL)
			{
				for (int i = 0; i < (int)steps.size(); i++)
					fprintf(seq, "%d.%d\n", steps[i].index, steps[i].term);
				fclose(seq);
			}
		}
		else if (strncmp(command, "reset", 5) == 0 || strncmp(command, "r", 1) == 0)
		{
			if (sscanf(command, "reset %d", &n) == 1 || sscanf(command, "r%d", &n) == 1)
			{
				sim = hse::simulator(&g, g.reset[n]);
				uptodate = false;
				step = 0;
				srand(seed);
			}
			else
				for (int i = 0; i < (int)g.reset.size(); i++)
					printf("(%d) %s\n", i, g.reset[i].to_string(g).c_str());
		}
		else if ((strncmp(command, "tokens", 6) == 0 && length == 6) || (strncmp(command, "t", 1) == 0 && length == 1))
		{
			vector<vector<int> > tokens;
			for (int i = 0; i < (int)sim.tokens.size(); i++)
			{
				bool found = false;
				for (int j = 0; j < (int)tokens.size() && !found; j++)
					if (g.places[sim.tokens[i].index].mask == g.places[sim.tokens[tokens[j][0]].index].mask)
					{
						tokens[j].push_back(i);
						found = true;
					}

				if (!found)
					tokens.push_back(vector<int>(1, i));
			}

			for (int i = 0; i < (int)tokens.size(); i++)
			{
				printf("%s {\n", boolean::export_composition(sim.encoding.flipped_mask(g.places[sim.tokens[tokens[i][0]].index].mask), g).to_string().c_str());
				for (int j = 0; j < (int)tokens[i].size(); j++) {
					int virt = -1;
					for (int k = 0; k < (int)sim.loaded.size() and virt < 0; k++) {
						if (find(sim.loaded[k].output_marking.begin(), sim.loaded[k].output_marking.end(), tokens[i][j]) != sim.loaded[k].output_marking.end()) {
							virt = k;
						}
					}
					if (virt >= 0) {
						printf("\t\t(%d) %s->%s\tP%d\t%s\n", tokens[i][j], export_expression(sim.tokens[tokens[i][j]].guard, g).to_string().c_str(), export_composition(g.transitions[sim.loaded[virt].index].local_action, g).to_string().c_str(), sim.tokens[tokens[i][j]].index, export_node(hse::iterator(hse::place::type, sim.tokens[tokens[i][j]].index), g).c_str());
					} else {
						printf("\t(%d) P%d\t%s\n", tokens[i][j], sim.tokens[tokens[i][j]].index, export_node(hse::iterator(hse::place::type, sim.tokens[tokens[i][j]].index), g).c_str());
					}
				}
				printf("}\n");
			}
		}
		else if ((strncmp(command, "enabled", 7) == 0 && length == 7) || (strncmp(command, "e", 1) == 0 && length == 1))
		{
			if (!uptodate)
			{
				enabled = sim.enabled();
				uptodate = true;
			}

			for (int i = 0; i < enabled; i++)
			{
				printf("(%d) T%d.%d:%s->%s\n", i, sim.loaded[sim.ready[i].first].index, sim.ready[i].second, export_expression(g.transitions[sim.loaded[sim.ready[i].first].index].guard, g).to_string().c_str(), export_composition(g.transitions[sim.loaded[sim.ready[i].first].index].local_action[sim.ready[i].second], g).to_string().c_str());
				if (sim.loaded[sim.ready[i].first].vacuous) {
					printf("\tvacuous");
				}
				if (!sim.loaded[sim.ready[i].first].stable) {
					printf("\tunstable");
				}
				printf("\n");
			}
			printf("\n");
		}
		else if (strncmp(command, "set", 3) == 0)
		{
			int i = 0;
			if (sscanf(command, "set %d ", &n) != 1)
			{
				n = -1;
				i = 4;
			}
			else
			{
				i = 5;
				while (i < length && command[i-1] != ' ')
					i++;
			}

			assignment_parser.insert("", string(command).substr(i));
			parse_expression::composition expr(assignment_parser);
			boolean::cube local_action = boolean::import_cube(expr, g, 0, &assignment_parser, false);
			boolean::cube remote_action = local_action.remote(g.remote_groups());
			if (assignment_parser.is_clean())
			{
				sim.encoding = boolean::local_assign(sim.encoding, local_action, true);
				sim.global = boolean::local_assign(sim.global, remote_action, true);
				sim.encoding = boolean::remote_assign(sim.encoding, sim.global, true);
			}
			assignment_parser.reset();
			uptodate = false;

			dump.append(sim.now, sim.stripped_encoding());
		}
		else if (strncmp(command, "force", 5) == 0)
		{
			if (length <= 6)
				printf("error: expected expression\n");
			else
			{
				assignment_parser.insert("", string(command).substr(6));
				parse_expression::composition expr(assignment_parser);
				boolean::cube local_action = boolean::import_cube(expr, g, 0, &assignment_parser, false);
				boolean::cube remote_action = local_action.remote(g.remote_groups());
				if (assignment_parser.is_clean())
				{
					sim.encoding = boolean::local_assign(sim.encoding, remote_action, true);
					sim.global = boolean::local_assign(sim.global, remote_action, true);
				}
				assignment_parser.reset();
				uptodate = false;
			
				dump.append(sim.now, sim.stripped_encoding());
			}
		}
		else if (strncmp(command, "step", 4) == 0 || strncmp(command, "s", 1) == 0)
		{
			if (sscanf(command, "step %d", &n) != 1 && sscanf(command, "s%d", &n) != 1)
				n = 1;

			for (int i = 0; i < n && (enabled != 0 || !uptodate); i++)
			{
				if (!uptodate)
				{
					enabled = sim.enabled();
					uptodate = true;
				}

				if (enabled != 0)
				{
					vector<int> now;
					for (int i = 0; i < (int)sim.ready.size(); i++) {
						if (now.empty() or sim.loaded[sim.ready[i].first].fire_at < sim.loaded[sim.ready[now[0]].first].fire_at) {
							now.clear();
							now.push_back(i);
						} else if (sim.loaded[sim.ready[i].first].fire_at == sim.loaded[sim.ready[now[0]].first].fire_at) {
							now.push_back(i);
						}
					}
					int firing = now[rand()%(int)now.size()];
					bool vacuous = false;
					if (step < (int)steps.size())
					{
						for (firing = 0; firing < (int)sim.ready.size() &&
						(sim.loaded[sim.ready[firing].first].index != steps[step].index || sim.ready[firing].second != steps[step].term); firing++);

						if (firing == (int)sim.ready.size())
						{
							printf("error: loaded simulation does not match HSE, please clear the simulation to continue\n");
							break;
						}
					}
					else {
						vacuous = sim.loaded[sim.ready[firing].first].vacuous;
						steps.push_back(hse::term_index(sim.loaded[sim.ready[firing].first].index, sim.ready[firing].second));
					}

					/*for (auto h = sim.history.begin(); h != sim.history.end(); h++) {
						printf("%s->%s; ", export_expression(h->first, v).to_string().c_str(), export_composition(g.transitions[h->second.index].local_action[h->second.term], v).to_string().c_str());
					}
					printf("\n");*/

					string flags = "";
					if (vacuous) {
						flags = " [vacuous]";
					}
					boolean::cube guard = sim.loaded[sim.ready[firing].first].guard_action;
					boolean::cube action = g.transitions[sim.loaded[sim.ready[firing].first].index].local_action[sim.ready[firing].second];
					boolean::cube remote_action = g.transitions[sim.loaded[sim.ready[firing].first].index].remote_action[sim.ready[firing].second];

					printf("%" PRIu64 "\tT%d.%d\t%s -> %s%s\n", sim.now, sim.loaded[sim.ready[firing].first].index, sim.ready[firing].second, export_expression(guard, g).to_string().c_str(), export_composition(action, g).to_string().c_str(), flags.c_str());
					
					sim.fire(firing);

					dump.append(sim.now, sim.stripped_encoding());

					uptodate = false;
					sim.interference_errors.clear();
					sim.instability_errors.clear();
					sim.mutex_errors.clear();
					step++;
				}
			}
		}
		else if (strncmp(command, "fire", 4) == 0 || strncmp(command, "f", 1) == 0)
		{
			if (sscanf(command, "fire %d", &n) == 1 || sscanf(command, "f%d", &n) == 1)
			{
				if (!uptodate)
				{
					enabled = sim.enabled();
					uptodate = true;
				}

				if (n < enabled)
				{
					if (step < (int)steps.size())
						printf("error: deviating from loaded simulation, please clear the simulation to continue\n");
					else
					{
						bool vacuous = sim.loaded[sim.ready[n].first].vacuous;
						steps.push_back(hse::term_index(sim.loaded[sim.ready[n].first].index, sim.ready[n].second));

						string flags = "";
						if (vacuous) {
							flags = " [vacuous]";
						}
						boolean::cube guard = sim.loaded[sim.ready[n].first].guard_action;
						boolean::cube action = g.transitions[sim.loaded[sim.ready[n].first].index].local_action[sim.ready[n].second];
						boolean::cube remote_action = g.transitions[sim.loaded[sim.ready[n].first].index].remote_action[sim.ready[n].second];

						printf("%" PRIu64 "\tT%d.%d\t%s -> %s%s\n", sim.now, sim.loaded[sim.ready[n].first].index, sim.ready[n].second, export_expression(guard, g).to_string().c_str(), export_composition(action, g).to_string().c_str(), flags.c_str());
					
						sim.fire(n);

						dump.append(sim.now, sim.stripped_encoding());

						uptodate = false;
						sim.interference_errors.clear();
						sim.instability_errors.clear();
						sim.mutex_errors.clear();
						step++;
					}
				}
				else
					printf("error: must be in the range [0,%d)\n", enabled);
			}
			else
				printf("error: expected ID in the range [0,%d)\n", enabled);
		}
		else if (length > 0)
			printf("error: unrecognized command '%s'\n", command);
	}

	dump.close();
}

void prsim(prs::production_rule_set &pr, bool debug) {//, vector<prs::term_index> steps = vector<prs::term_index>()) {
	prs::globals g(pr);
	prs::simulator sim(&pr, debug);

	vcd dump;
	dump.create(pr.name, pr);

	tokenizer assignment_parser(false);
	parse_expression::composition::register_syntax(assignment_parser);

	int seed = 0;
	srand(seed);
	//int step = 0;
	int n = 0;
	char command[256];
	bool done = false;
	FILE *script = stdin;
	while (!done)
	{
		if (script == stdin)
		{
			printf("(prsim)");
			fflush(stdout);
		}
		if (fgets(command, 255, script) == NULL && script != stdin)
		{
			fclose(script);
			script = stdin;
			printf("(prsim)");
			fflush(stdout);
			if (fgets(command, 255, script) == NULL)
				exit(0);
		}
		int length = strlen(command);
		command[length-1] = '\0';
		length--;

		if ((strncmp(command, "help", 4) == 0 && length == 4) || (strncmp(command, "h", 1) == 0 && length == 1))
			print_prsim_help();
		else if ((strncmp(command, "quit", 4) == 0 && length == 4) || (strncmp(command, "q", 1) == 0 && length == 1))
			done = true;
		else if (strncmp(command, "seed", 4) == 0)
		{
			if (sscanf(command, "seed %d", &n) == 1)
			{
				seed = n;
				srand(seed);
			}
			else
				printf("error: expected seed value\n");
		}
		//else if ((strncmp(command, "clear", 5) == 0 && length == 5) || (strncmp(command, "c", 1) == 0 && length == 1))
		//	steps.resize(step);
		else if (strncmp(command, "source", 6) == 0 && length > 7)
		{
			script = fopen(&command[7], "r");
			if (script == NULL)
			{
				printf("error: file not found '%s'\n", &command[7]);
				script = stdin;
			}
		}
		/*else if (strncmp(command, "load", 4) == 0 && length > 5)
		{
			FILE *seq = fopen(&command[5], "r");
			if (seq != NULL)
			{
				while (fgets(command, 255, seq) != NULL)
				{
					if (sscanf(command, "%d.%d", &n, &n1) == 2)
						steps.push_back(prs::term_index(n, n1));
				}
				fclose(seq);
			}
			else
				printf("error: file not found '%s'\n", &command[5]);
		}
		else if (strncmp(command, "save", 4) == 0 && length > 5)
		{
			FILE *seq = fopen(&command[5], "w");
			if (seq != NULL)
			{
				for (int i = 0; i < (int)steps.size(); i++)
					fprintf(seq, "%d.%d\n", steps[i].index, steps[i].term);
				fclose(seq);
			}
		}*/
		else if (strncmp(command, "run", 3) == 0 || strncmp(command, "g", 1) == 0) {
			sim.run();
		} else if (strncmp(command, "reset", 5) == 0 || strncmp(command, "r", 1) == 0) {
			sim.reset();
			//step = 0;
			srand(seed);
		} else if (strncmp(command, "wait", 4) == 0 || strncmp(command, "w", 1) == 0) {
			sim.wait();
		} else if ((strncmp(command, "tokens", 6) == 0 && length == 6) || (strncmp(command, "t", 1) == 0 && length == 1)) {
			printf("%s\n", export_composition(sim.encoding, pr).to_string().c_str());
		} else if ((strncmp(command, "enabled", 7) == 0 && length == 7) || (strncmp(command, "e", 1) == 0 && length == 1)) {
			for (int i = 0; i < (int)sim.nets.size(); i++) {
				if (sim.nets[i] != nullptr) {
					printf("(%d) %s\n", i, sim.nets[i]->value.to_string(&pr).c_str());
				}
			}
		} else if (strncmp(command, "set", 3) == 0) {
			int i = 0;
			if (sscanf(command, "set %d ", &n) != 1) {
				n = -1;
				i = 4;
			} else {
				i = 5;
				while (i < length && command[i-1] != ' ') {
					i++;
				}
			}

			assignment_parser.insert("", string(command).substr(i));
			parse_expression::composition expr(assignment_parser);
			boolean::cube local_action = boolean::import_cube(expr, pr, 0, &assignment_parser, false);
			if (assignment_parser.is_clean()) {
				sim.set(local_action);
			}

			dump.append(sim.enabled.now, sim.encoding, sim.strength);

			assignment_parser.reset();
		} else if (strncmp(command, "force", 5) == 0) {
			if (length <= 6) {
				printf("error: expected expression\n");
			} else {
				assignment_parser.insert("", string(command).substr(6));
				parse_expression::composition expr(assignment_parser);
				boolean::cube local_action = boolean::import_cube(expr, pr, 0, &assignment_parser, false);
				boolean::cube remote_action = local_action.remote(pr.remote_groups());
				if (assignment_parser.is_clean()) {
					sim.set(remote_action);
				}

				dump.append(sim.enabled.now, sim.encoding, sim.strength);
				
				assignment_parser.reset();
			}
		} else if (strncmp(command, "step", 4) == 0 || strncmp(command, "s", 1) == 0) {
			if (sscanf(command, "step %d", &n) != 1 && sscanf(command, "s%d", &n) != 1) {
				n = 1;
			}

			for (int i = 0; i < n; i++) {
				if (sim.enabled.empty()) {
					sim.wait();
					if (sim.enabled.empty()) {
						break;
					}
				}

				if (debug) {
					printf("\n\n%s\n", export_composition(sim.encoding, pr).to_string().c_str());
					for (int i = 0; i < (int)sim.nets.size(); i++) {
						if (sim.nets[i] != nullptr) {
							printf("(%d) %s\n", i, sim.nets[i]->value.to_string(&pr).c_str());
						}
					}
				}

				/*if (step < (int)steps.size())
				{
					for (firing = 0; firing < (int)sim.ready.size() &&
					(sim.loaded[sim.ready[firing].first].index != steps[step].index || sim.ready[firing].second != steps[step].term); firing++);

					if (firing == (int)sim.ready.size())
					{
						printf("error: loaded simulation does not match PRS, please clear the simulation to continue\n");
						break;
					}
				}
				else
					steps.push_back(prs::term_index(sim.loaded[sim.ready[firing].first].index, sim.ready[firing].second));*/

				//boolean::cube old = sim.encoding;
				auto e = sim.fire();
				printf("%" PRIu64 "\t%s\n", e.fire_at, e.to_string(&pr).c_str());

				dump.append(e.fire_at, sim.encoding, sim.strength);

				//printf("\t%s\n", export_composition(difference(old, sim.encoding), v).to_string().c_str());

				//sim.interference_errors.clear();
				//sim.instability_errors.clear();
				//sim.mutex_errors.clear();
				//step++;
			}
		} else if (strncmp(command, "fire", 4) == 0 || strncmp(command, "f", 1) == 0) {
			if (sscanf(command, "fire %d", &n) == 1 || sscanf(command, "f%d", &n) == 1) {
				if (n >= 0 and n < (int)sim.nets.size() and sim.at(n) != nullptr) {
					/*if (step < (int)steps.size())
						printf("error: deviating from loaded simulation, please clear the simulation to continue\n");
					else
					{
						steps.push_back(prs::term_index(sim.loaded[sim.ready[n].first].index, sim.ready[n].second));*/

						//boolean::cube old = sim.encoding;
						auto e = sim.fire(n);
						printf("%" PRIu64 "\t%s\n", e.fire_at, e.to_string(&pr).c_str());
			
						dump.append(e.fire_at, sim.encoding, sim.strength);

						//printf("\t%s\n", export_composition(difference(old, sim.encoding), v).to_string().c_str());

						//sim.interference_errors.clear();
						//sim.instability_errors.clear();
						//sim.mutex_errors.clear();
						//step++;
					//}
				} else {
					printf("error: must be in the range [0,%zu)\n", sim.enabled.size());
				}
			} else {
				printf("error: expected ID in the range [0,%zu)\n", sim.enabled.size());
			}
		} else if (length > 0) {
			printf("error: unrecognized command '%s'\n", command);
		}
	}

	dump.close();
}

int sim_command(int argc, char **argv) {
	parse_ucs::function::registry.insert({"func", parse_ucs::language(&parse_cog::produce, &parse_cog::expect, &parse_cog::register_syntax)});
	parse_ucs::function::registry.insert({"proto", parse_ucs::language(&parse_cog::produce, &parse_cog::expect, &parse_cog::register_syntax)});
	parse_ucs::function::registry.insert({"circ", parse_ucs::language(&parse_prs::produce, &parse_prs::expect, &parse_prs::register_syntax)});

	weaver::Term::pushDialect("func", factoryCog);
	weaver::Term::pushDialect("proto", factoryCogw);
	weaver::Term::pushDialect("circ", factoryPrs);

	weaver::Project proj;
	if (proj.hasMod()) {
		proj.readMod();
	}

	proj.pushFiletype("", "wv", "", readWv, loadWv);
	proj.pushFiletype("func", "cog", "", readCog, loadCog);
	proj.pushFiletype("proto", "cogw", "", readCog, loadCogw);
	proj.pushFiletype("circ", "prs", "ckt", readPrs, loadPrs, writePrs);
	proj.pushFiletype("spice", "spi", "spi", readSpice, loadSpice, writeSpice);
	proj.pushFiletype("verilog", "v", "rtl", nullptr, nullptr, writeVerilog);
	proj.pushFiletype("layout", "gds", "gds", nullptr, loadGds, writeGds);
	proj.pushFiletype("func", "astg", "state", readAstg, loadAstg, writeAstg);
	proj.pushFiletype("proto", "astgw", "state", readAstg, loadAstgw, writeAstgw);

	Proto proto;

	string sfilename = "";
	bool debug = false;

	for (int i = 0; i < argc; i++) {
		string arg = argv[i];

		if (arg == "--verbose" or arg == "-v") {
			set_verbose(true);
		} else if (arg == "--debug" or arg == "-d") {
			set_debug(true);
			debug = true;
		} else if (proto.empty()) {
			proto = parseProto(proj, arg);
		} else {
			sfilename = arg;
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

	weaver::Program prgm;
	loadGlobalTypes(prgm);

	if (proto.empty()) {
		proj.incl("top.wv");
		proto.name = vector<string>({"top", "top"});
	} else {
		proj.incl(proto.path);
	}

	proj.load(prgm);

	vector<weaver::TermId> curr = findProto(prgm, proto);
	if (curr.empty() or curr[0].mod < 0) {
		error("", "module not found for term '" + proto.to_string() + "'", __FILE__, __LINE__);
		complete();
		return 1;
	}

	if (curr[0].index < 0 and prgm.mods[curr[0].mod].terms.size() == 1u) {
		curr[0].index = 0;
	}

	if (curr[0].index < 0) {
		error("", "term not found '" + proto.to_string() + "'", __FILE__, __LINE__);
		complete();
		return 1;
	}

	const weaver::Term &fn = prgm.termAt(curr[0]);

	if (fn.dialect().name == "func") {
		vector<chp::term_index> steps;
		if (sfilename != "") {
			FILE *seq = fopen(sfilename.c_str(), "r");
			char command[256];
			int n, n1;
			if (seq != nullptr) {
				while (fgets(command, 255, seq) != nullptr) {
					if (sscanf(command, "%d.%d", &n, &n1) == 2) {
						steps.push_back(chp::term_index(n, n1));
					}
				}
				fclose(seq);
			} else {
				printf("error: file not found '%s'\n", sfilename.c_str());
			}
		}

		chp::graph g = fn.as<chp::graph>();
		g.post_process(true);
		chpsim(g, steps);
	} else if (fn.dialect().name == "proto") {
		vector<hse::term_index> steps;
		if (sfilename != "") {
			FILE *seq = fopen(sfilename.c_str(), "r");
			char command[256];
			int n, n1;
			if (seq != nullptr) {
				while (fgets(command, 255, seq) != nullptr) {
					if (sscanf(command, "%d.%d", &n, &n1) == 2) {
						steps.push_back(hse::term_index(n, n1));
					}
				}
				fclose(seq);
			} else {
				printf("error: file not found '%s'\n", sfilename.c_str());
			}
		}
		
		hse::graph g = fn.as<hse::graph>();
		hsesim(g, steps);
	} else if (fn.dialect().name == "circ") {
		/*vector<prs::term_index> steps;
		if (sfilename != "") {
			FILE *seq = fopen(sfilename.c_str(), "r");
			char command[256];
			int n, n1;
			if (seq != NULL) {
				while (fgets(command, 255, seq) != NULL) {
					if (sscanf(command, "%d.%d", &n, &n1) == 2) {
						steps.push_back(prs::term_index(n, n1));
					}
				}
				fclose(seq);
			} else {
				printf("error: file not found '%s'\n", sfilename.c_str());
			}
		}*/

		prs::production_rule_set pr = fn.as<prs::production_rule_set>();

		if (debug) {
			printf("\n\n%s\n\n", export_production_rule_set(pr).to_string().c_str());
			pr.print();
			printf("\n\n");
		}

		prsim(pr, debug);//, steps);
	} else {
		error("", "unrecognized dialect '" + fn.dialect().name + "'", __FILE__, __LINE__);
	}

	complete();
	return is_clean();
}
