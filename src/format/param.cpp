#include "param.h"

#include <parse/parse.h>
#include <parse/tokenizer.h>
#include <parse_ucs/function_decl.h>
#include <interpret_choy/import.h>
#include <interpret_choy/export.h>

choy::Decl declFromParam(const choy::Program &prgm, const std::map<std::string, std::string> &params, std::string prefix, int modIdx) {
	auto pos = params.find(prefix + "decl");
	if (pos != params.end()) {
		tokenizer tokens;
		parse_ucs::function_decl::register_syntax(tokens);
		tokens.insert("", pos->second);
		tokens.increment(true);
		tokens.expect<parse_ucs::function_decl>();
		if (tokens.decrement(__FILE__, __LINE__)) {
			parse_ucs::function_decl syntax(tokens);
			return parse_ucs::import_decl(prgm, modIdx, syntax, &tokens);
		}
	}
	return choy::Decl();
}

void declToParam(std::map<std::string, std::string> &params, const choy::Program &prgm, const choy::Decl &decl, std::string prefix) {
	params.insert({prefix+"decl", parse_ucs::export_decl(prgm, decl).to_string()});
}

std::map<std::string, std::string> declToParams(const choy::Program &prgm, const choy::Decl &decl, std::string prefix) {
	std::map<std::string, std::string> result;
	declToParam(result, prgm, decl, prefix);
	return result;
}



choy::Prototype protoFromParam(const std::map<std::string, std::string> &params, std::string prefix) {
	auto pos = params.find(prefix + "proto");
	if (pos != params.end()) {
		return choy::Prototype(pos->second);
	}
	return choy::Prototype();
}

void protoToParam(std::map<std::string, std::string> &params, const choy::Prototype &proto, std::string prefix) {
	params.insert({prefix+"proto", proto.to_string()});
}

std::map<std::string, std::string> protoToParams(const choy::Prototype &proto, std::string prefix) {
	std::map<std::string, std::string> result;
	protoToParam(result, proto, prefix);
	return result;
}
