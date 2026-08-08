#include "param.h"

#include <parse/parse.h>
#include <parse/tokenizer.h>
#include <parse_ucs/function_decl.h>
#include <interpret_wv/import.h>
#include <interpret_wv/export.h>

weaver::Decl declFromParam(const weaver::Program &prgm, const std::map<std::string, std::string> &params, std::string prefix, int modIdx) {
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
	return weaver::Decl();
}

void declToParam(std::map<std::string, std::string> &params, const weaver::Program &prgm, const weaver::Decl &decl, std::string prefix) {
	params.insert({prefix+"decl", parse_ucs::export_decl(prgm, decl).to_string()});
}

std::map<std::string, std::string> declToParams(const weaver::Program &prgm, const weaver::Decl &decl, std::string prefix) {
	std::map<std::string, std::string> result;
	declToParam(result, prgm, decl, prefix);
	return result;
}



weaver::Prototype protoFromParam(const std::map<std::string, std::string> &params, std::string prefix) {
	auto pos = params.find(prefix + "proto");
	if (pos != params.end()) {
		return weaver::Prototype(pos->second);
	}
	return weaver::Prototype();
}

void protoToParam(std::map<std::string, std::string> &params, const weaver::Prototype &proto, std::string prefix) {
	params.insert({prefix+"proto", proto.to_string()});
}

std::map<std::string, std::string> protoToParams(const weaver::Prototype &proto, std::string prefix) {
	std::map<std::string, std::string> result;
	protoToParam(result, proto, prefix);
	return result;
}
