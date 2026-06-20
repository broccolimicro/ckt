#include "compare.h"

#include <common/standard.h>
#include <common/timer.h>
#include <common/text.h>
#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

#include <parse_ucs/source.h>
#include <parse_astg/factory.h>
#include <parse_cog/factory.h>
#include <parse_chp/factory.h>
#include <parse_prs/factory.h>
#include <parse_spice/factory.h>

#include <weaver/project.h>

#include "weaver/builder.h"

#include "format/mod.h"
#include "format/dot.h"
#include "format/cog.h"
#include "format/spice.h"
#include "format/gds.h"
#include "format/verilog.h"
#include "format/prs.h"
#include "format/wv.h"
#include "format/astg.h"
#include "format/gc.h"

#include <sch/Subckt.h>
#include <sch/Tapeout.h>

#include <phy/Tech.h>
#include <phy/Script.h>
#include <phy/Layout.h>

#include <interpret_wv/import.h>

const bool debug = false;

void compare_help() {
	printf("Usage: lm compare [options] [<module|term>[=<module|term>...]...]\n");
	printf("Verify that the two circuit files are the same.\n");

	printf("Options:\n");
	printf(" -v,--verbose     display verbose messages\n");
	printf(" -d,--debug       display internal debugging messages\n");
	printf(" -h,--help        display this help text\n");
	printf(" -p,--progress    display progress information\n");
	printf("\n");

	printf("\nSupported file formats:\n");
	//printf(" *.chp                   communicating hardware processes\n");
	//printf(" *.hse                   handshaking expansions\n");
	//printf(" *.prs                   production rule set\n");
	//printf(" *.astg                  asynchronous signal transition graph\n");
	printf(" *.gds                   layout\n");
	printf(" *.spice,*.spi,*.sp,*.s  spice netlist\n");
}

struct Group {
	vector<weaver::Prototype> terms;
};

void cleanup(sch::Subckt &s) {
	s.cleanDangling(true);
	s.combineDevices();
	s.canonicalize();
}

void compare(sch::Subckt s0, sch::Subckt s1) {
	printf("\t%s = %s...[", s0.name.c_str(), s1.name.c_str());
	fflush(stdout);
	cleanup(s0);
	cleanup(s1);

	if (s0.compare(s1) == 0) {
		printf("%sMATCH%s]\n", KGRN, KNRM);
	} else {
		printf("%sMISMATCH%s]\n", KRED, KNRM);
		if (debug) {
			s0.print();
			s1.print();
		}
	}
}

void compare(weaver::Program &prgm, weaver::Variant &child, weaver::Variant &parent) {
	if (child.meta.dialect == "layout" and parent.meta.dialect == "spice") {
		phy::Layout &macro = child.as<phy::Layout>();
		sch::Subckt s0;
		extract(s0, macro);

		compare(s0, parent.as<sch::Subckt>());
	} else if (child.meta.dialect == "spice" and parent.meta.dialect == "layout") {
		phy::Layout &macro = parent.as<phy::Layout>();
		sch::Subckt s1;
		extract(s1, macro);

		compare(child.as<sch::Subckt>(), s1);
	} else if (child.meta.dialect == "spice" and parent.meta.dialect == "spice") {
		compare(child.as<sch::Subckt>(), parent.as<sch::Subckt>());
	}
}

void verifyImpl(weaver::Program &prgm, weaver::TermId idx) {
	weaver::Term &t0 = prgm.termAt(idx);
	for (int i = (int)t0.variants.size()-1; i >= 0; i--) {
		int super = t0.variants[i].super;
		if (super >= 0) {
			compare(prgm, t0.variants[i], t0.variants[super]);
			continue;
		}

		for (auto j = t0.impl.begin(); j != t0.impl.end(); j++) {
			if (not j->hasTerm()) {
				printf("error: undefined implements relationship\n");
				continue;
			}

			weaver::Term &t1 = prgm.termAt(*j);
			if (t1.variants.empty()) {
				continue;
			}

			compare(prgm, t0.variants[i], t1.variants[0]);
		}
	}
}

void verifyGroup(weaver::Program &prgm, Group group) {
	if (group.terms.empty()) {
		return;
	} else if (group.terms.size() == 1u) {
		printf("%s:\n", group.terms[0].to_string().c_str());
		vector<weaver::TermId> idx = prgm.findTerms(group.terms[0]);
		if (group.terms[0].name.empty()) {
			if (idx[0].mod >= 0) {
				for (idx[0].index = 0; idx[0].index < (int)prgm.mods[idx[0].mod].terms.size(); idx[0].index++) {
					verifyImpl(prgm, idx[0]);
				}
			} else {
				printf("error: module not found '%s'\n", group.terms[0].to_string().c_str());
			}
		} else {
			for (auto j = idx.begin(); j != idx.end(); j++) {
				if (j->hasTerm()) {
					verifyImpl(prgm, *j);
				} else {
					printf("error: term not found '%s'\n", group.terms[0].to_string().c_str());
				}
			}
		}
		return;
	}
	
	vector<weaver::TermId> prev = prgm.findTerms(group.terms[0]);
	int prevVariant = group.terms[0].variant;
	if (prevVariant < 0) {
		prevVariant = 0;
	}

	if (prev.empty() or prev[0].mod < 0) {
		printf("error: term not found '%s'\n", group.terms[0].to_string().c_str());
	}
	for (int i = 1; i < (int)group.terms.size(); i++) {
		vector<weaver::TermId> curr = prgm.findTerms(group.terms[i]);
		int currVariant = group.terms[i].variant;
		if (currVariant < 0) {
			currVariant = 0;
		}
		if (curr.empty() or curr[0].mod < 0) {
			printf("error: term not found '%s'\n", group.terms[i].to_string().c_str());
		}
		printf("%s = %s:\n", group.terms[i-1].to_string().c_str(), group.terms[i].to_string().c_str());
		for (auto j = prev.begin(); j != prev.end(); j++) {
			for (auto k = curr.begin(); k != curr.end(); k++) {
				if (k->hasTerm() and j->hasTerm()) {
					weaver::Term &t0 = prgm.termAt(*j);
					weaver::Term &t1 = prgm.termAt(*k);
					if (prevVariant >= (int)t0.variants.size()) {
						printf("error: variant not found '%s'\n", group.terms[i-1].to_string().c_str());
						break;
					}
					if (currVariant >= (int)t1.variants.size()) {
						printf("error: variant not found '%s'\n", group.terms[i].to_string().c_str());
						continue;
					}	
					compare(prgm, t0.variants[prevVariant], t1.variants[currVariant]);
				} else if (k->hasTerm() and j->mod >= 0) {
					weaver::Term &t1 = prgm.termAt(*k);
					if (currVariant >= (int)t1.variants.size()) {
						printf("error: variant not found '%s'\n", group.terms[i].to_string().c_str());
						continue;
					}

					for (int t0i = 0; t0i < (int)prgm.mods[j->mod].terms.size(); t0i++) {
						weaver::Term &t0 = prgm.termAt(weaver::TermId(j->mod, t0i));
						if (prevVariant >= (int)t0.variants.size()) {
							printf("error: variant not found '%s'\n", group.terms[i-1].to_string().c_str());
							continue;
						}
						if (t0.decl.name == t1.decl.name) {
							compare(prgm, t0.variants[prevVariant], t1.variants[currVariant]);
						}
					}
				} else if (k->mod >= 0 and j->hasTerm()) {
					weaver::Term &t0 = prgm.termAt(*j);
					if (prevVariant >= (int)t0.variants.size()) {
						printf("error: variant not found '%s'\n", group.terms[i-1].to_string().c_str());
						continue;
					}
					for (int t1i = 0; t1i < (int)prgm.mods[k->mod].terms.size(); t1i++) {
						weaver::Term &t1 = prgm.termAt(weaver::TermId(k->mod, t1i));
						if (currVariant >= (int)t1.variants.size()) {
							printf("error: variant not found '%s'\n", group.terms[i].to_string().c_str());
							continue;
						}
						if (t0.decl.name == t1.decl.name) {
							compare(prgm, t0.variants[prevVariant], t1.variants[currVariant]);
						}
					}
				} else if (k->mod >= 0 and j->mod >= 0) {
					for (int t0i = 0; t0i < (int)prgm.mods[j->mod].terms.size(); t0i++) {
						weaver::Term &t0 = prgm.termAt(weaver::TermId(j->mod, t0i));
						if (prevVariant >= (int)t0.variants.size()) {
							printf("error: variant not found '%s'\n", group.terms[i-1].to_string().c_str());
							continue;
						}
						for (int t1i = 0; t1i < (int)prgm.mods[k->mod].terms.size(); t1i++) {
							weaver::Term &t1 = prgm.termAt(weaver::TermId(k->mod, t1i));
							if (currVariant >= (int)t1.variants.size()) {
								printf("error: variant not found '%s'\n", group.terms[i].to_string().c_str());
								continue;
							}
							if (t0.decl.name == t1.decl.name) {
								compare(prgm, t0.variants[prevVariant], t1.variants[currVariant]);
							}
						}
					}
				}
			}
		}

		prev = curr;
		prevVariant = currVariant;
	}
}

int compare_command(int argc, char **argv) {
	parse_ucs::function::registry.insert({"func", parse_ucs::language(&parse_cog::produce, &parse_cog::expect, &parse_cog::register_syntax)});
	parse_ucs::function::registry.insert({"proto", parse_ucs::language(&parse_cog::produce, &parse_cog::expect, &parse_cog::register_syntax)});
	parse_ucs::function::registry.insert({"circ", parse_ucs::language(&parse_prs::produce, &parse_prs::expect, &parse_prs::register_syntax)});

	weaver::Language lang;
	lang.dialects.insert({"func", factoryCog});
	lang.dialects.insert({"struct", factoryGc});
	lang.dialects.insert({"proto", factoryCogw});
	lang.dialects.insert({"circ", factoryPrs});

	weaver::Project proj;
	if (proj.hasMod()) {
		readMod(proj);
	}

	proj.pushFiletype("", "wv", "", readWv, loadWv, nullptr, lang);
	proj.pushFiletype("func", "cog", "", readCog, loadCog);
	proj.pushFiletype("proto", "cogw", "", readCog, loadCogw);
	proj.pushFiletype("circ", "prs", "ckt", readPrs, loadPrs, writePrs);
	proj.pushFiletype("spice", "spi", "spi", readSpice, loadSpice, writeSpice);
	proj.pushFiletype("verilog", "v", "rtl", nullptr, nullptr, writeVerilog);
	proj.pushFiletype("layout", "gds", "gds", nullptr, loadGds, writeGds);
	proj.pushFiletype("func", "astg", "state", readAstg, loadAstg, writeAstg);
	proj.pushFiletype("proto", "astgw", "state", readAstg, loadAstgw, writeAstgw);

	vector<Group> groups;

	bool debug = false;
	bool progress = false;

	for (int i = 0; i < argc; i++) {
		string arg = argv[i];

		if (arg == "--verbose" or arg == "-v") {
			set_verbose(true);
		} else if (arg == "--debug" or arg == "-d") {
			set_debug(true);
			debug = true;
		} else if (arg == "-h" or arg == "--help") {
			compare_help();
			return 0;
		} else if (arg == "--progress" or arg == "-p") {
			progress = true;
		} else {
			groups.push_back(Group());

			size_t eq = arg.rfind("=");
			while (eq != string::npos) {
				groups.back().terms.push_back(weaver::Prototype(arg.substr(eq+1)));
				arg = arg.substr(0, eq);
				eq = arg.rfind("=");
			}
			if (not arg.empty()) {
				groups.back().terms.push_back(weaver::Prototype(arg));
			}
			reverse(groups.back().terms.begin(), groups.back().terms.end());
		}
	}

	if (groups.empty() and not proj.hasMod()) {
		printf("please initialize your module with the following.\n\nlm mod init my_module\n");
		return 1;
	}

	weaver::Program prgm;
	loadGlobalTypes(prgm);

	if (groups.empty()) {
		proj.incl(proj.modName);
	} else {
		for (auto i = groups.begin(); i != groups.end(); i++) {
			for (auto j = i->terms.begin(); j != i->terms.end(); j++) {
				proj.incl(j->mod);
			}
		}
	}

	proj.load(prgm);

	if (debug) {
		prgm.print();
	}

	if (groups.empty()) {
		for (auto i = prgm.begin(); i != prgm.end(); i = prgm.next(i)) {
			verifyImpl(prgm, i);
		}
	} else {
		for (auto i = groups.begin(); i != groups.end(); i++) {
			verifyGroup(prgm, *i);
		}
	}
	
	complete();
	return is_clean();
}
