#include "flow.h"
#include "param.h"

#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

#include <flow/func.h>

#include <weaver/params.h>

weaver::Prototype protoFromInstance(const weaver::Program &prgm, const flow::Instance &inst, bool qualify) {
	std::map<std::string, std::string> params = weaver::readParams({inst.comment});
	weaver::Prototype proto = protoFromParam(params);
	if (not proto.empty()) {
		return proto;
	}

	proto = weaver::Prototype::fromMangled(inst.type);

	if (qualify) {
		proto.qualified = true;
		for (auto j : inst.ports) {
			proto.args.push_back(weaver::Typename("chan"));
		}
		proto.argsHash = weaver::getHash(proto.args);
	}
	return proto;
}

