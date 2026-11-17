#include "flow.h"
#include "param.h"

#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

#include <flow/func.h>

#include <choy/params.h>

choy::Prototype protoFromInstance(const choy::Program &prgm, const flow::Instance &inst, bool qualify) {
	std::map<std::string, std::string> params = choy::readParams({inst.comment});
	choy::Prototype proto = protoFromParam(params);
	if (not proto.empty()) {
		return proto;
	}

	proto = choy::Prototype::fromMangled(inst.type);

	if (qualify) {
		proto.qualified = true;
		for (auto j : inst.ports) {
			proto.args.push_back(choy::Typename("chan"));
		}
		proto.argsHash = choy::getHash(proto.args);
	}
	return proto;
}

