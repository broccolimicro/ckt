#include "sim.h"

#include <common/standard.h>
#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>

#include <parse_chp/composition.h>
#include <chp/graph.h>
//#include <chp/simulator.h>
#include <interpret_chp/import.h>
#include <interpret_chp/export.h>
#include <interpret_arithmetic/export.h>
#include <interpret_arithmetic/import.h>

#include <hse/graph.h>
#include <hse/simulator.h>
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

void sim_help() {
	printf("Usage: ckt sim [options] <ckt-file> [sim-file]\n");
	printf("A simulation environment for various behavioral descriptions.\n");

	printf("\nSupported file formats:\n");
	printf(" *.chp           communicating hardware processes\n");
	printf(" *.hse           handshaking expansions\n");
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

void chpsim(chp::graph &g, ucs::variable_set &v, vector<chp::term_index> steps = vector<chp::term_index>())
{
	/*chp::simulator sim;
	sim.base = &g;
	sim.variables = &v;

	tokenizer assignment_parser(false);
	parse_expression::composition::register_syntax(assignment_parser);

	int seed = 0;
	srand(seed);
	int enabled = 0;
	int step = 0;
	int n = 0, n1 = 0;
	char command[256];
	bool done = false;
	FILE *script = stdin;
	while (!done)
	{
		printf("(chpsim)");
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
				printf("error: file not found '%s'", &command[7]);
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
				sim = chp::simulator(&g, &v, g.reset[n], 0, false);
				enabled = sim.enabled();
				step = 0;
				srand(seed);
			}
			else
				for (int i = 0; i < (int)g.reset.size(); i++)
					printf("(%d) %s\n", i, g.reset[i].to_string(v).c_str());
		}
		else if ((strncmp(command, "tokens", 6) == 0 && length == 6) || (strncmp(command, "t", 1) == 0 && length == 1))
		{
			vector<vector<int> > tokens;
			for (int i = 0; i < (int)sim.local.tokens.size(); i++)
			{
				bool found = false;
				for (int j = 0; j < (int)tokens.size() && !found; j++)
					if (g.places[sim.local.tokens[i].index].mask == g.places[sim.local.tokens[tokens[j][0]].index].mask)
					{
						tokens[j].push_back(i);
						found = true;
					}

				if (!found)
					tokens.push_back(vector<int>(1, i));
			}

			for (int i = 0; i < (int)tokens.size(); i++)
			{
				printf("%s {\n", export_expression(sim.encoding.flipped_mask(g.places[sim.local.tokens[tokens[i][0]].index].mask), v).to_string().c_str());
				for (int j = 0; j < (int)tokens[i].size(); j++)
					printf("\t(%d) P%d\t%s\n", tokens[i][j], sim.local.tokens[tokens[i][j]].index, export_node(chp::iterator(chp::place::type, sim.local.tokens[tokens[i][j]].index), g, v).c_str());
				printf("}\n");
			}
		}
		else if ((strncmp(command, "enabled", 7) == 0 && length == 7) || (strncmp(command, "e", 1) == 0 && length == 1))
		{
			for (int i = 0; i < enabled; i++)
			{
				if (g.transitions[sim.local.ready[i].index].behavior == chp::transition::active)
					printf("(%d) T%d.%d:%s     ", i, sim.local.ready[i].index, sim.local.ready[i].term, export_composition(g.transitions[sim.local.ready[i].index].local_action[sim.local.ready[i].term], v).to_string().c_str());
				else
					printf("(%d) T%d.%d:[%s]     ", i, sim.local.ready[i].index, sim.local.ready[i].term, export_expression(g.transitions[sim.local.ready[i].index].local_action[sim.local.ready[i].term], v).to_string().c_str());
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
			arithmetic::cube action = import_cube(expr, v, 0, &assignment_parser, false);
			arithmetic::cube remote_action = action.remote(v.get_groups());
			if (assignment_parser.is_clean())
			{
				sim.encoding = arithmetic::local_assign(sim.encoding, action, true);
				sim.global = arithmetic::local_assign(sim.global, remote_action, true);
				sim.encoding = arithmetic::remote_assign(sim.encoding, sim.global, true);
			}
			assignment_parser.reset();
			enabled = sim.enabled();
			sim.interference_errors.clear();
			sim.instability_errors.clear();
			sim.mutex_errors.clear();
		}
		else if (strncmp(command, "force", 5) == 0)
		{
			if (length <= 6)
				printf("error: expected expression\n");
			else
			{
				assignment_parser.insert("", string(command).substr(6));
				parse_expression::composition expr(assignment_parser);
				arithmetic::cube action = import_cube(expr, v, 0, &assignment_parser, false);
				arithmetic::cube remote_action = action.remote(v.get_groups());
				if (assignment_parser.is_clean())
				{
					sim.encoding = arithmetic::local_assign(sim.encoding, remote_action, true);
					sim.global = arithmetic::local_assign(sim.global, remote_action, true);
				}
				assignment_parser.reset();
				enabled = sim.enabled();
				sim.interference_errors.clear();
				sim.instability_errors.clear();
				sim.mutex_errors.clear();
			}
		}
		else if (strncmp(command, "step", 4) == 0 || strncmp(command, "s", 1) == 0)
		{
			if (sscanf(command, "step %d", &n) != 1 && sscanf(command, "s%d", &n) != 1)
				n = 1;

			for (int i = 0; i < n && enabled != 0; i++)
			{
				int firing = rand()%enabled;
				if (step < (int)steps.size())
				{
					for (firing = 0; firing < (int)sim.local.ready.size() &&
					(sim.local.ready[firing].index != steps[step].index || sim.local.ready[firing].term != steps[step].term); firing++);

					if (firing == (int)sim.local.ready.size())
					{
						printf("error: loaded simulation does not match HSE, please clear the simulation to continue\n");
						break;
					}
				}
				else
					steps.push_back(chp::term_index(sim.local.ready[firing].index, sim.local.ready[firing].term));

				if (g.transitions[sim.local.ready[firing].index].behavior == chp::transition::active)
					printf("%d\tT%d.%d\t%s\n", step, sim.local.ready[firing].index, sim.local.ready[firing].term, export_composition(g.transitions[sim.local.ready[firing].index].local_action[sim.local.ready[firing].term], v).to_string().c_str());
				else if (g.transitions[sim.local.ready[firing].index].behavior == chp::transition::passive)
					printf("%d\tT%d\t[%s]\n", step, sim.local.ready[firing].index, export_expression(sim.local.ready[firing].guard_action, v).to_string().c_str());

				sim.fire(firing);

				enabled = sim.enabled();
				sim.interference_errors.clear();
				sim.instability_errors.clear();
				sim.mutex_errors.clear();
				step++;
			}
		}
		else if (strncmp(command, "fire", 4) == 0 || strncmp(command, "f", 1) == 0)
		{
			if (sscanf(command, "fire %d", &n) == 1 || sscanf(command, "f%d", &n) == 1)
			{
				if (n < enabled)
				{
					if (step < (int)steps.size())
						printf("error: deviating from loaded simulation, please clear the simulation to continue\n");
					else
					{
						steps.push_back(chp::term_index(sim.local.ready[n].index, sim.local.ready[n].term));

						if (g.transitions[sim.local.ready[n].index].behavior == chp::transition::active)
							printf("%d\tT%d.%d\t%s\n", step, sim.local.ready[n].index, sim.local.ready[n].term, export_composition(g.transitions[sim.local.ready[n].index].local_action[sim.local.ready[n].term], v).to_string().c_str());
						else if (g.transitions[sim.local.ready[n].index].behavior == chp::transition::passive)
							printf("%d\tT%d\t[%s]\n", step, sim.local.ready[n].index, export_expression(sim.local.ready[n].guard_action, v).to_string().c_str());

						sim.fire(n);

						enabled = sim.enabled();
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
	}*/
}

void hsesim(hse::graph &g, ucs::variable_set &v, vector<hse::term_index> steps = vector<hse::term_index>()) {
	hse::simulator sim;
	sim.base = &g;
	sim.variables = &v;

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
				printf("error: file not found '%s'", &command[7]);
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
				sim = hse::simulator(&g, &v, g.reset[n]);
				uptodate = false;
				step = 0;
				srand(seed);
			}
			else
				for (int i = 0; i < (int)g.reset.size(); i++)
					printf("(%d) %s\n", i, g.reset[i].to_string(v).c_str());
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
				printf("%s {\n", export_composition(sim.encoding.flipped_mask(g.places[sim.tokens[tokens[i][0]].index].mask), v).to_string().c_str());
				for (int j = 0; j < (int)tokens[i].size(); j++) {
					int virt = -1;
					for (int k = 0; k < (int)sim.loaded.size() and virt < 0; k++) {
						if (find(sim.loaded[k].output_marking.begin(), sim.loaded[k].output_marking.end(), tokens[i][j]) != sim.loaded[k].output_marking.end()) {
							virt = k;
						}
					}
					if (virt >= 0) {
						printf("\t\t(%d) %s->%s\tP%d\t%s\n", tokens[i][j], export_expression(sim.tokens[tokens[i][j]].guard, v).to_string().c_str(), export_composition(g.transitions[sim.loaded[virt].index].local_action, v).to_string().c_str(), sim.tokens[tokens[i][j]].index, export_node(hse::iterator(hse::place::type, sim.tokens[tokens[i][j]].index), g, v).c_str());
					} else {
						printf("\t(%d) P%d\t%s\n", tokens[i][j], sim.tokens[tokens[i][j]].index, export_node(hse::iterator(hse::place::type, sim.tokens[tokens[i][j]].index), g, v).c_str());
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
				printf("(%d) T%d.%d:%s->%s\n", i, sim.loaded[sim.ready[i].first].index, sim.ready[i].second, export_expression(g.transitions[sim.loaded[sim.ready[i].first].index].guard, v).to_string().c_str(), export_composition(g.transitions[sim.loaded[sim.ready[i].first].index].local_action[sim.ready[i].second], v).to_string().c_str());
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
			boolean::cube local_action = import_cube(expr, v, 0, &assignment_parser, false);
			boolean::cube remote_action = local_action.remote(v.get_groups());
			if (assignment_parser.is_clean())
			{
				sim.encoding = boolean::local_assign(sim.encoding, local_action, true);
				sim.global = boolean::local_assign(sim.global, remote_action, true);
				sim.encoding = boolean::remote_assign(sim.encoding, sim.global, true);
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
				boolean::cube local_action = import_cube(expr, v, 0, &assignment_parser, false);
				boolean::cube remote_action = local_action.remote(v.get_groups());
				if (assignment_parser.is_clean())
				{
					sim.encoding = boolean::local_assign(sim.encoding, remote_action, true);
					sim.global = boolean::local_assign(sim.global, remote_action, true);
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
					printf("%d\tT%d.%d\t%s -> %s%s\n", step, sim.loaded[sim.ready[firing].first].index, sim.ready[firing].second, export_expression(sim.loaded[sim.ready[firing].first].guard_action, v).to_string().c_str(), export_composition(g.transitions[sim.loaded[sim.ready[firing].first].index].local_action[sim.ready[firing].second], v).to_string().c_str(), flags.c_str());

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
						steps.push_back(hse::term_index(sim.loaded[sim.ready[n].first].index, sim.ready[n].second));

						string flags = "";
						if (vacuous) {
							flags = " [vacuous]";
						}

						printf("%d\tT%d.%d\t%s -> %s%s\n", step, sim.loaded[sim.ready[n].first].index, sim.ready[n].second, export_expression(sim.loaded[sim.ready[n].first].guard_action, v).to_string().c_str(), export_composition(g.transitions[sim.loaded[sim.ready[n].first].index].local_action[sim.ready[n].second], v).to_string().c_str(), flags.c_str());
						
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

void prsim(prs::production_rule_set &pr, ucs::variable_set &v, vector<prs::term_index> steps = vector<prs::term_index>()) {
	prs::simulator sim(&pr, &v);

	tokenizer assignment_parser(false);
	parse_expression::composition::register_syntax(assignment_parser);

	int seed = 0;
	srand(seed);
	int enabled = 0;
	bool uptodate = false;
	int step = 0;
	int n = 0, n1 = 0, n2 = 0;
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
		else if ((strncmp(command, "clear", 5) == 0 && length == 5) || (strncmp(command, "c", 1) == 0 && length == 1))
			steps.resize(step);
		else if (strncmp(command, "source", 6) == 0 && length > 7)
		{
			script = fopen(&command[7], "r");
			if (script == NULL)
			{
				printf("error: file not found '%s'", &command[7]);
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
		}
		else if (strncmp(command, "run", 3) == 0 || strncmp(command, "g", 1) == 0) {
			sim.run();
			uptodate = false;
		}
		else if (strncmp(command, "reset", 5) == 0 || strncmp(command, "r", 1) == 0) {
			sim.reset();
			uptodate = false;
			step = 0;
			srand(seed);
		}
		else if (strncmp(command, "wait", 4) == 0 || strncmp(command, "w", 1) == 0)
			sim.encoding = sim.global;
		else if ((strncmp(command, "tokens", 6) == 0 && length == 6) || (strncmp(command, "t", 1) == 0 && length == 1))
			printf("%s\n", export_composition(sim.encoding, v).to_string().c_str());
		else if ((strncmp(command, "enabled", 7) == 0 && length == 7) || (strncmp(command, "e", 1) == 0 && length == 1))
		{
			if (!uptodate)
			{
				enabled = sim.enabled();
				uptodate = true;
			}

			for (int i = 0; i < enabled; i++)
				printf("(%d) T%d.%d:%s     ", i, sim.loaded[sim.ready[i].first].index, sim.ready[i].second, export_composition(pr.rules[sim.loaded[sim.ready[i].first].index].local_action[sim.ready[i].second], v).to_string().c_str());
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
			boolean::cube local_action = import_cube(expr, v, 0, &assignment_parser, false);
			boolean::cube remote_action = local_action.remote(v.get_groups());
			if (assignment_parser.is_clean())
			{
				sim.encoding = boolean::local_assign(sim.encoding, local_action, true);
				sim.global = boolean::local_assign(sim.global, remote_action, true);
				sim.encoding = boolean::remote_assign(sim.encoding, sim.global, true);
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
				boolean::cube local_action = import_cube(expr, v, 0, &assignment_parser, false);
				boolean::cube remote_action = local_action.remote(v.get_groups());
				if (assignment_parser.is_clean())
				{
					sim.encoding = boolean::local_assign(sim.encoding, remote_action, true);
					sim.global = boolean::local_assign(sim.global, remote_action, true);
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

				if (enabled == 0) {
					sim.wait();
					enabled = sim.enabled();
				}

				if (enabled != 0)
				{
					int firing = rand()%enabled;
					if (step < (int)steps.size())
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
						steps.push_back(prs::term_index(sim.loaded[sim.ready[firing].first].index, sim.ready[firing].second));

					printf("%d\tT%d.%d\t%s -> %s\n", step, sim.loaded[sim.ready[firing].first].index, sim.ready[firing].second, export_expression(sim.loaded[sim.ready[firing].first].guard_action, v).to_string().c_str(), export_composition(pr.rules[sim.loaded[sim.ready[firing].first].index].local_action[sim.ready[firing].second], v).to_string().c_str());

					//boolean::cube old = sim.encoding;
					sim.fire(firing);

					//printf("\t%s\n", export_composition(difference(old, sim.encoding), v).to_string().c_str());

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
						steps.push_back(prs::term_index(sim.loaded[sim.ready[n].first].index, sim.ready[n].second));

						printf("%d\tT%d.%d\t%s -> %s\n", step, sim.loaded[sim.ready[n].first].index, sim.ready[n].second, export_expression(sim.loaded[sim.ready[n].first].guard_action, v).to_string().c_str(), export_composition(pr.rules[sim.loaded[sim.ready[n].first].index].local_action[sim.ready[n].second], v).to_string().c_str());

						//boolean::cube old = sim.encoding;
						sim.fire(n);

						//printf("\t%s\n", export_composition(difference(old, sim.encoding), v).to_string().c_str());

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

int sim_command(configuration &config, int argc, char **argv) {
	tokenizer tokens;
	tokens.register_token<parse::block_comment>(false);
	tokens.register_token<parse::line_comment>(false);
	
	string filename = "";
	string format = "";

	string sfilename = "";

	for (int i = 0; i < argc; i++) {
		if (filename == "") {
			filename = argv[i];
			size_t dot = filename.find_last_of(".");
			if (dot == string::npos) {
				printf("unrecognized file format\n");
				return 0;
			}
			format = filename.substr(dot+1);
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

		// TODO(edward.bingham) create CHP simulator
		return 0;
	} else if (format == "hse" or format == "astg") {
		vector<hse::term_index> steps;
		if (sfilename != "") {
			FILE *seq = fopen(sfilename.c_str(), "r");
			char command[256];
			int n, n1;
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
				printf("error: file not found '%s'\n", sfilename.c_str());
		}

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

		hsesim(hg, v, steps);
	} else if (format == "prs") {
		vector<prs::term_index> steps;
		if (sfilename != "") {
			FILE *seq = fopen(sfilename.c_str(), "r");
			char command[256];
			int n, n1, n2;
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
				printf("error: file not found '%s'\n", sfilename.c_str());
		}

		prs::production_rule_set pr;

		parse_prs::production_rule_set::register_syntax(tokens);
		config.load(tokens, filename, "");

		tokens.increment(false);
		tokens.expect<parse_prs::production_rule_set>();
		if (tokens.decrement(__FILE__, __LINE__))
		{
			parse_prs::production_rule_set syntax(tokens);
			pr = prs::import_production_rule_set(syntax, v, 0, &tokens, true);
		}
		pr.post_process(v);

		prsim(pr, v, steps);
	}

	complete();
	return is_clean();
}
