#include "link.h"

bool link(Build &builder, choy::Program &prgm, choy::TermId id) {
	if (not id.hasVar()) {
		return false;
	}
	prgm.varAt(id).meta.set("choy.link");
	builder.push(prgm, id);

	std::string dialect = prgm.varAt(id).meta.dialect;
	choy::Dialect *ref = builder.proj.getDialect(dialect);
	if (ref != nullptr and ref->link != nullptr) {
		for (const auto &proto : ref->link(builder.proj, prgm, id)) {
			std::vector<choy::TermId> ids = prgm.findTerms(proto, id.mod);
			if (ids.empty()) {
				//prgm.print();
				error("", "term not defined '" + proto.to_string() + "'", __FILE__, __LINE__);
				continue;
			}
			for (choy::TermId j : ids) {
				builder.push(prgm, j);
			}
		}
	}
	
	return true;
}
