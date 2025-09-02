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

#include "weaver/builder.h"
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

#include <sch/Netlist.h>
#include <sch/Tapeout.h>

#include <phy/Tech.h>
#include <phy/Script.h>
#include <phy/Layout.h>
#include <phy/Library.h>

const bool debug = false;

void compare_help() {
	printf("Usage: lm compare [options] [<module|term>[=<module|term>...]...]\n");
	printf("Verify that the two circuit files are the same.\n");

	printf("\nSupported file formats:\n");
	//printf(" *.chp                   communicating hardware processes\n");
	//printf(" *.hse                   handshaking expansions\n");
	//printf(" *.prs                   production rule set\n");
	//printf(" *.astg                  asynchronous signal transition graph\n");
	printf(" *.gds                   layout\n");
	printf(" *.spice,*.spi,*.sp,*.s  spice netlist\n");
}

struct Group {
	vector<Proto> terms;
};

void compare(sch::Subckt &s0, sch::Subckt &s1) {
	printf("\t%s = %s...[", s0.name.c_str(), s1.name.c_str());
	fflush(stdout);

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

void compare(sch::Netlist &n0, sch::Netlist &n1) {
	vector<bool> c0(n0.subckts.size(), false);
	vector<bool> c1(n1.subckts.size(), false);
	for (auto i = n0.subckts.begin(); i != n0.subckts.end(); i++) {
		bool found = false;
		for (auto j = n1.subckts.begin(); j != n1.subckts.end(); j++) {
			if (i->name == j->name) {
				if (not c0[i-n0.subckts.begin()]) {
					i->cleanDangling(true);
					i->combineDevices();
					i->canonicalize();
					c0[i-n0.subckts.begin()] = true;
				}
				if (not c1[j-n1.subckts.begin()]) {
					j->cleanDangling(true);
					j->combineDevices();
					j->canonicalize();
					c1[j-n1.subckts.begin()] = true;
				}

				compare(*i, *j);
				found = true;
				break;
			}
		}
		if (not found) {
			printf("\t%s...[%sNOT FOUND%s]\n", i->name.c_str(), KYEL, KNRM);
		}
	}
}

void compare(weaver::Program &prgm, weaver::Term &child, weaver::Term &parent) {
	if (child.dialect().name == "layout" and parent.dialect().name == "spice") {
		phy::Library &lib = child.as<phy::Library>();
		sch::Netlist s0;
		extract(s0, lib);

		compare(s0, parent.as<sch::Netlist>());
	} else if (child.dialect().name == "spice" and parent.dialect().name == "spice") {
		compare(child.as<sch::Netlist>(), parent.as<sch::Netlist>());
	}
	printf("done\n\n");
}

void verifyImpl(weaver::Program &prgm, weaver::TermId idx) {
	weaver::Term &t0 = prgm.termAt(idx);
	if (t0.impl.empty()) {
		return;
	}

	for (auto j = t0.impl.begin(); j != t0.impl.end(); j++) {
		if (not j->defined()) {
			printf("error: undefined implements relationship\n");
			continue;
		}

		compare(prgm, t0, prgm.termAt(*j));
	}
}

weaver::TermId findProto(const weaver::Program &prgm, Proto proto) {
	if (proto.isModule()) {
		return weaver::TermId(prgm.findModule(proto.name[0]), -1);
	}

	weaver::TypeId recv;
	if (not proto.recv.empty()) {
		recv = prgm.findType(proto.recv);
	}
	vector<weaver::TypeId> args;
	for (auto i = proto.args.begin(); i != proto.args.end(); i++) {
		args.push_back(prgm.findType(*i));
	}
	return prgm.findTerm(recv, proto.name, args);
}

void verifyGroup(weaver::Program &prgm, Group group) {
	if (group.terms.empty()) {
		return;
	} else if (group.terms.size() == 1u) {
		printf("%s:\n", group.terms[0].to_string().c_str());
		if (group.terms[0].isModule()) {
			int modIdx = prgm.findModule(group.terms[0].name[0]);
			if (modIdx >= 0) {
				for (int termIdx = 0; termIdx < (int)prgm.mods[modIdx].terms.size(); termIdx++) {
					verifyImpl(prgm, weaver::TermId(modIdx, termIdx));
				}
			}
		} else {
			weaver::TermId idx = findProto(prgm, group.terms[0]);
			if (idx.defined()) {
				verifyImpl(prgm, idx);
			} else {
				printf("error: term not found '%s'\n", ::to_string(group.terms[0].name).c_str());
			}
		}
		return;
	}
	
	weaver::TermId prev = findProto(prgm, group.terms[0]);
	if (prev.mod < 0) {
		printf("error: term not found '%s'\n", group.terms[0].to_string().c_str());
	}
	for (int i = 1; i < (int)group.terms.size(); i++) {
		weaver::TermId curr = findProto(prgm, group.terms[i]);
		if (curr.mod < 0) {
			printf("error: term not found '%s'\n", group.terms[i].to_string().c_str());
		}
		printf("%s = %s:\n", group.terms[i-1].to_string().c_str(), group.terms[i].to_string().c_str());
		if (curr.defined() and prev.defined()) {
			weaver::Term &t0 = prgm.termAt(prev);
			weaver::Term &t1 = prgm.termAt(curr);
			compare(prgm, t0, t1);
		} else if (curr.defined() and prev.mod >= 0) {
			weaver::Term &t1 = prgm.termAt(curr);
			for (int t0i = 0; t0i < (int)prgm.mods[prev.mod].terms.size(); t0i++) {
				weaver::Term &t0 = prgm.termAt(weaver::TermId(prev.mod, t0i));
				if (t0.decl.name == t1.decl.name) {
					compare(prgm, t0, t1);
				}
			}
		} else if (curr.mod >= 0 and prev.defined()) {
			weaver::Term &t0 = prgm.termAt(prev);
			for (int t1i = 0; t1i < (int)prgm.mods[curr.mod].terms.size(); t1i++) {
				weaver::Term &t1 = prgm.termAt(weaver::TermId(curr.mod, t1i));
				if (t0.decl.name == t1.decl.name) {
					compare(prgm, t0, t1);
				}
			}
		} else if (curr.mod >= 0 and prev.mod >= 0) {
			for (int t0i = 0; t0i < (int)prgm.mods[prev.mod].terms.size(); t0i++) {
				weaver::Term &t0 = prgm.termAt(weaver::TermId(prev.mod, t0i));
				for (int t1i = 0; t1i < (int)prgm.mods[curr.mod].terms.size(); t1i++) {
					weaver::Term &t1 = prgm.termAt(weaver::TermId(curr.mod, t1i));
					if (t0.decl.name == t1.decl.name) {
						compare(prgm, t0, t1);
					}
				}
			}
		}

		prev = curr;
	}
}

int compare_command(int argc, char **argv) {
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

	vector<Group> groups;

	for (int i = 0; i < argc; i++) {
		string arg = argv[i];
		groups.push_back(Group());

		size_t eq = arg.rfind("=");
		while (eq != string::npos) {
			groups.back().terms.push_back(parseProto(proj, arg.substr(eq+1)));
			arg = arg.substr(0, eq);
			eq = arg.rfind("=");
		}
		if (not arg.empty()) {
			groups.back().terms.push_back(parseProto(proj, arg));
		}
		reverse(groups.back().terms.begin(), groups.back().terms.end());
	}

	if (groups.empty() and not proj.hasMod()) {
		printf("please initialize your module with the following.\n\nlm mod init my_module\n");
		return 1;
	}

	weaver::Program prgm;
	loadGlobalTypes(prgm);

	if (groups.empty()) {
		proj.incl("top.wv");
	} else {
		for (auto i = groups.begin(); i != groups.end(); i++) {
			for (auto j = i->terms.begin(); j != i->terms.end(); j++) {
				proj.incl(j->path);
			}
		}
	}

	proj.load(prgm);

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
