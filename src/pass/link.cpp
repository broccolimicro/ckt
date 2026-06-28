#include "link.h"

bool link(Build &builder, weaver::Program &prgm, weaver::TermId id) {
	if (not id.hasVar()) {
		return false;
	}
	prgm.varAt(id).meta.set("wv.link");
	builder.push(prgm, id);

	std::string dialect = prgm.varAt(id).meta.dialect;
	weaver::Dialect *ref = builder.proj.getDialect(dialect);
	if (ref != nullptr and ref->link != nullptr) {
		for (const auto &proto : ref->link(builder.proj, prgm, id)) {
			std::vector<weaver::TermId> ids = prgm.findTerms(proto, id.mod);
			if (ids.empty()) {
				error("", "term not defined '" + proto.to_string() + "'", __FILE__, __LINE__);
				continue;
			}
			for (weaver::TermId j : ids) {
				builder.push(prgm, j);
			}
		}
	}
	
	return true;
}
