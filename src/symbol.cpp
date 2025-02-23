#include "symbol.h"

map<string, Function>::iterator SymbolTable::createFunc(string name) {
	return funcs.insert(pair<string, Function>(name, Function())).first;
}

map<string, Structure>::iterator SymbolTable::createStruct(string name) {
	return structs.insert(pair<string, Structure>(name, Structure())).first;
}

bool SymbolTable::load(configuration &config, string path, const Tech *tech) {
	size_t dot = path.find_last_of(".");
	string format = "";
	if (dot != string::npos) {
		format = path.substr(dot+1);
		if (format == "spice"
			or format == "sp"
			or format == "s") {
			format = "spi";
		}
	}

	string prefix = dot != string::npos ? path.substr(0, dot) : path;

	if (format == "ucs") {
		cout << "parsing ucs" << endl;
		tokenizer tokens;
		tokens.register_token<parse::block_comment>(false);
		tokens.register_token<parse::line_comment>(false);
		parse_ucs::source::register_syntax(tokens);
		config.load(tokens, path, "");

		tokens.increment(true);
		tokens.expect<parse_ucs::source>();
		if (tokens.decrement(__FILE__, __LINE__)) {
			parse_ucs::source syntax(tokens);
			cout << syntax.to_string() << endl;

			for (auto i = syntax.funcs.begin(); i != syntax.funcs.end(); i++) {
				if (i->lang == "func") {
					auto proc = createFunc(i->name);
					proc->second.cg = shared_ptr<chp::graph>(new chp::graph());
					proc->second.cg->name = i->name;
					arithmetic::expression covered;
					bool hasRepeat = false;
					proc->second.cg->merge(chp::parallel, chp::import_chp(*(parse_cog::composition*)i->body, proc->second.v, covered, hasRepeat, 0, &tokens, true));

					/*auto proc = createFunc(i->name);
					proc->second.hg = shared_ptr<hse::graph>(new hse::graph());
					proc->second.hg->name = i->name;
					boolean::cover covered;
					bool hasRepeat = false;
					proc->second.hg->merge(hse::parallel, hse::import_hse(*(parse_cog::composition*)i->body, proc->second.v, covered, hasRepeat, 0, &tokens, true));*/
				} else if (i->lang == "chp") {
					auto proc = createFunc(i->name);
					proc->second.cg = shared_ptr<chp::graph>(new chp::graph());
					proc->second.cg->name = i->name;
					proc->second.cg->merge(chp::parallel, chp::import_chp(*(parse_chp::composition*)i->body, proc->second.v, 0, &tokens, true));
				} else if (i->lang == "hse") {
					auto proc = createFunc(i->name);
					proc->second.hg = shared_ptr<hse::graph>(new hse::graph());
					proc->second.hg->name = i->name;
					proc->second.hg->merge(hse::parallel, hse::import_hse(*(parse_chp::composition*)i->body, proc->second.v, 0, &tokens, true));
				} else if (i->lang == "prs") {
					/*auto proc = createStruct(prefix);
					proc->second.pr = shared_ptr<prs::production_rule_set>(new prs::production_rule_set());
					proc->second.pr->name = i->name;
					map<int, int> nodemap;
					prs::import_production_rule_set(*(parse_prs::production_rule_set*)i->body, *proc->second.pr, -1, -1, prs::attributes(), proc->second.v, nodemap, 0, &tokens, true);*/
				} else if (i->lang == "spice") {
				}
			}	
		}

		cout << "done" << endl;
	} else if (format == "chp") {
		tokenizer tokens;
		tokens.register_token<parse::block_comment>(false);
		tokens.register_token<parse::line_comment>(false);
		parse_chp::composition::register_syntax(tokens);
		config.load(tokens, path, "");

		auto proc = createFunc(prefix);
		proc->second.cg = shared_ptr<chp::graph>(new chp::graph());
		proc->second.cg->name = prefix;

		tokens.increment(false);
		tokens.expect<parse_chp::composition>();
		while (tokens.decrement(__FILE__, __LINE__))
		{
			parse_chp::composition syntax(tokens);
			proc->second.cg->merge(chp::parallel, chp::import_chp(syntax, proc->second.v, 0, &tokens, true));

			tokens.increment(false);
			tokens.expect<parse_chp::composition>();
		}
	} else if (format == "hse") {
		tokenizer tokens;
		tokens.register_token<parse::block_comment>(false);
		tokens.register_token<parse::line_comment>(false);
		parse_chp::composition::register_syntax(tokens);
		config.load(tokens, path, "");
		
		auto proc = createFunc(prefix);
		proc->second.hg = shared_ptr<hse::graph>(new hse::graph());
		proc->second.hg->name = prefix;

		tokens.increment(false);
		tokens.expect<parse_chp::composition>();
		while (tokens.decrement(__FILE__, __LINE__))
		{
			parse_chp::composition syntax(tokens);
			proc->second.hg->merge(hse::parallel, hse::import_hse(syntax, proc->second.v, 0, &tokens, true));

			tokens.increment(false);
			tokens.expect<parse_chp::composition>();
		}
	} else if (format == "astg") {
		tokenizer tokens;
		tokens.register_token<parse::block_comment>(false);
		tokens.register_token<parse::line_comment>(false);
		parse_astg::graph::register_syntax(tokens);
		config.load(tokens, path, "");
		
		auto proc = createFunc(prefix);
		proc->second.hg = shared_ptr<hse::graph>(new hse::graph());
		proc->second.hg->name = prefix;
		
		tokens.increment(false);
		tokens.expect<parse_astg::graph>();
		while (tokens.decrement(__FILE__, __LINE__))
		{
			parse_astg::graph syntax(tokens);
			proc->second.hg->merge(hse::parallel, hse::import_hse(syntax, proc->second.v, &tokens));

			tokens.increment(false);
			tokens.expect<parse_astg::graph>();
		}
	} else if (format == "cog") {
		tokenizer tokens;
		tokens.register_token<parse::block_comment>(false);
		tokens.register_token<parse::line_comment>(false);
		parse_cog::composition::register_syntax(tokens);
		config.load(tokens, path, "");
		
		auto proc = createFunc(prefix);
		proc->second.hg = shared_ptr<hse::graph>(new hse::graph());
		proc->second.hg->name = prefix;

		tokens.increment(false);
		tokens.expect<parse_cog::composition>();
		while (tokens.decrement(__FILE__, __LINE__))
		{
			parse_cog::composition syntax(tokens);
			boolean::cover covered;
			bool hasRepeat = false;
			proc->second.hg->merge(hse::parallel, hse::import_hse(syntax, proc->second.v, covered, hasRepeat, 0, &tokens, true));

			tokens.increment(false);
			tokens.expect<parse_cog::composition>();
		}
	} else if (format == "prs") {
		tokenizer tokens;
		tokens.register_token<parse::block_comment>(false);
		tokens.register_token<parse::line_comment>(false);
		parse_prs::production_rule_set::register_syntax(tokens);
		config.load(tokens, path, "");
		
		auto proc = createStruct(prefix);
		proc->second.pr = shared_ptr<prs::production_rule_set>(new prs::production_rule_set());
		proc->second.pr->name = prefix;

		tokens.increment(false);
		tokens.expect<parse_prs::production_rule_set>();
		if (tokens.decrement(__FILE__, __LINE__))
		{
			parse_prs::production_rule_set syntax(tokens);
			map<int, int> nodemap;
			prs::import_production_rule_set(syntax, *proc->second.pr, -1, -1, prs::attributes(), proc->second.v, nodemap, 0, &tokens, true);
		}
	} else if (format == "spi") {
		if (tech == nullptr) {
			printf("unable to open spice file without technology\n");
		}

		tokenizer tokens;
		parse_spice::netlist::register_syntax(tokens);
		config.load(tokens, path, "");

		tokens.increment(false);
		tokens.expect<parse_spice::netlist>();
		if (tokens.decrement(__FILE__, __LINE__))
		{
			parse_spice::netlist syntax(tokens);
			
			sch::Netlist lst;
			sch::import_netlist(*tech, lst, syntax, &tokens);

			for (auto i = lst.subckts.begin(); i != lst.subckts.end(); i++) {
				auto proc = createStruct(prefix);
				proc->second.sp = shared_ptr<sch::Subckt>(new sch::Subckt(*i));
			}
		}
	} else {
		printf("unrecognized file format '%s'\n", format.c_str());
		return false;
	}
	return true;
}
