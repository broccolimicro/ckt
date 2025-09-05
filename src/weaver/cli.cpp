#include "cli.h"

bool Proto::isModule() const {
	return name.size() == 1u;
}

bool Proto::isTerm() const {
	return name.size() == 2u;
}

string Proto::to_string() const {
	string result = name[0];

	if (isTerm()) {
		result += ":";
		if (not unqualified and not recv.empty()) {
			result += recv[0];
			for (int i = 1; i < (int)recv.size(); i++) {
				result += "." + recv[i];
			}
			result += "::";
		}

		result += name[1];
		if (not unqualified) {
			result += "(";
			for (int i = 0; i < (int)args.size(); i++) {
				if (i != 0) {
					result += ",";
				}
				if (not args[i].empty()) {
					result += args[i][0];
					for (int j = 1; j < (int)args[i].size(); j++) {
						result += "." + args[i][j];
					}
				}
			}
			result += ")";
		}
	}
	return result;
}

vector<string> parseIdent(string id) {
	vector<string> result;
	string term = "";
	size_t col = id.rfind(":");
	if (col != string::npos) {
		term = id.substr(col+1);
		id = id.substr(0, col);
	}

	result.push_back(id);
	if (not term.empty()) {
		result.push_back(term);
	}
	return result;
}

Proto parseProto(const weaver::Project &proj, string proto) {
	Proto result;
	result.unqualified = true;
	size_t par = proto.rfind("(");
	if (par != string::npos) {
		result.unqualified = false;
		string args = proto.substr(par+1, proto.size()-par-2);
		proto = proto.substr(0, par);
		size_t com = args.rfind(",");
		while (com != string::npos) {
			string arg = args.substr(com+1);
			result.args.push_back(parseIdent(arg));
			args = args.substr(0, com);
			com = args.rfind(",");
		}
		if (not args.empty()) {
			result.args.push_back(parseIdent(args));
		}
	}

	size_t rec = proto.rfind("::");
	if (rec != string::npos) {
		result.recv = parseIdent(proto.substr(0, rec));
		proto = proto.substr(rec+2);
	}

	result.name = parseIdent(proto);

	fs::path canon = result.name[0];
	if (canon.extension().empty()) {
		canon = result.name[0] + ".wv";
	}
	if (not canon.is_absolute()) {
		canon = proj.workDir / canon;
	}

	result.path = fs::relative(canon, proj.workDir);

	result.name[0] = proj.pathToModule(result.path);

	return result;
}

vector<weaver::TermId> findProto(const weaver::Program &prgm, Proto proto) {
	if (proto.isModule()) {
		return vector<weaver::TermId>(1, weaver::TermId(prgm.findModule(proto.name[0]), -1));
	} else if (proto.unqualified) {
		return prgm.findTerms(proto.name);
	}

	weaver::TypeId recv;
	if (not proto.recv.empty()) {
		recv = prgm.findType(proto.recv);
	}
	vector<weaver::TypeId> args;
	for (auto i = proto.args.begin(); i != proto.args.end(); i++) {
		args.push_back(prgm.findType(*i));
	}
	return vector<weaver::TermId>(1, prgm.findTerm(recv, proto.name, args));
}
