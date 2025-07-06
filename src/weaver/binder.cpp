#include "binder.h"

#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

#include <filesystem>

namespace weaver {

Binder::Binder(Program &prgm) : prgm(prgm) {
	loadGlobalTypes(this->prgm);
}

Binder::~Binder() {
}

string Binder::findWorkingDir() const {
	return std::filesystem::current_path();
}

string Binder::findProjectRoot(string workingDir) const {
	std::filesystem::path search = std::filesystem::absolute(workingDir);
	while (not search.empty()) {
		if (std::filesystem::exists(search / "lm.mod")) {
			return search.string();
		} else if (search.parent_path() == search) {
			printf("error: unable to find project root\n");
			return "";
		}
		search = search.parent_path();
	}
	printf("error: unable to find project root\n");
	return "";
}

void Binder::readPath(tokenizer &tokens, string path, string root) {
	if (not root.empty() and path[0] != '/') {
		path = root + "/" + path;
	}

	if (tokens.segment_loading(path)) {
		tokens.error("cycle found in include graph", __FILE__, __LINE__);
	} else if (!tokens.segment_loaded(path)) {
		ifstream fin;
		fin.open(path.c_str(), ios::binary | ios::in);
		/*for (int i = 0; !fin.is_open() and i < (int)includePath.size(); i++) {
			fin.open((includePath[i] + path).c_str(), ios::binary | ios::in);
		}*/

		if (!fin.is_open()) {
			tokens.error("file not found '" + path + "'", __FILE__, __LINE__);
		} else {
			fin.seekg(0, ios::end);
			int size = (int)fin.tellg();
			string buffer(size, ' ');
			fin.seekg(0, ios::beg);
			fin.read(&buffer[0], size);
			fin.clear();
			tokens.insert(path, buffer, this);
		}
	}
}

parse_ucs::source Binder::parsePath(string path, string root) {
	size_t dot = path.find_last_of(".");
	size_t slash = path.find_last_of("/");
	if (slash == string::npos) {
		slash = 0u;
	} else {
		slash += 1;
	}

	string format = "";
	if (dot != string::npos) {
		format = path.substr(dot+1);
	} else {
		dot = path.size();
	}

	if (format != "wv") {
		printf("error: unrecognized source format '%s'\n", format.c_str());
	}

	tokenizer tokens;
	tokens.register_token<parse::block_comment>(false);
	tokens.register_token<parse::line_comment>(false);
	parse_ucs::source::register_syntax(tokens);
	readPath(tokens, path, root);

	tokens.increment(true);
	tokens.expect<parse_ucs::source>();
	parse_ucs::source result;
	result.name = path.substr(slash, dot-slash);
	if (tokens.decrement(__FILE__, __LINE__)) {
		result.parse(tokens);
	}
	return result;
}

/*void Binder::addIncludePath(string path) {
	if (path.size() > 0u and path.back() != '/' and path.back() != '\\') {
		path.push_back('/');
	}

	if ((path.size() < 3u or path.substr(1, 2) != ":\\") and (path.size() < 1u or path[0] != '/')) {
		path = projectRoot + path;
	}

	includePath.push_back(path);
}*/

bool import_declaration(vector<Instance> &result, const Program &prgm, int index, const parse_ucs::declaration &syntax) {
	TypeId type = prgm.findType(index, syntax.type.names);
	if (not type.defined()) {
		printf("error: type not defined '%s'\n", syntax.type.to_string().c_str());
		return false;
	}

	for (int k = 0; k < (int)syntax.name.size(); k++) {
		result.push_back(Instance(type, syntax.name[k].name));
		// TODO(edward.bingham) reset behavior? arrays?
	}
	return true;
}

Decl import_prototype(const Program &prgm, int index, const parse_ucs::prototype &syntax, TypeId recvType) {
	TypeId retType = prgm.findType(index, syntax.ret.names);
	vector<Instance> args;
	for (auto i = syntax.args.begin(); i != syntax.args.end(); i++) {
		import_declaration(args, prgm, index, *i);
	}

	return Decl(syntax.name, args, retType, recvType);
}

bool Binder::define(vector<string> typeName, string name, vector<int> size, ucs::Netlist nets) {
	TypeId type = prgm.findType(currModule, typeName);
	Instance newInst(type, name, size);
	if (prgm.mods[currModule].terms[currTerm].symb.define(newInst)) {
		vector<int> i;
		i.resize(size.size(), 0);
		return true;
	}
	return false;
}

void Binder::pushScope() {
	prgm.mods[currModule].terms[currTerm].symb.pushScope();
}

void Binder::popScope() {
	prgm.mods[currModule].terms[currTerm].symb.popScope();
}

void Binder::loadSymbols(int index, const parse_ucs::source &syntax) {
	currModule = index;
	prgm.mods[currModule].name = syntax.name;
	for (auto i = syntax.types.begin(); i != syntax.types.end(); i++) {
		int recvType = prgm.mods[currModule].createType(Type::typeOf(i->name));
	}
}

void Binder::loadModule(int index, const parse_ucs::source &syntax) {
	currModule = index;
	//cout << syntax.to_string() << endl;
	for (auto i = syntax.types.begin(); i != syntax.types.end(); i++) {
		TypeId recvType = prgm.findType(currModule, {i->name});
		for (auto j = i->members.begin(); j != i->members.end(); j++) {
			import_declaration(prgm.typeAt(recvType).members, prgm, currModule, *j);
		}

		for (auto j = i->protocols.begin(); j != i->protocols.end(); j++) {
			prgm.typeAt(recvType).methods.push_back(import_prototype(prgm, currModule, *j, recvType));
		}
	}

	for (auto i = syntax.funcs.begin(); i != syntax.funcs.end(); i++) {
		int kind = Term::findDialect(i->lang);
		TypeId recvType;
		if (not i->recv.empty()) {
			recvType = prgm.findType(currModule, {i->recv});
			if (not recvType.defined()) {
				printf("error: type not defined '%s'\n", i->recv.c_str());
				continue;
			}
		}

		TypeId retType;
		if (i->ret.valid) {
			retType = prgm.findType(currModule, i->ret.names);
			if (not retType.defined()) {
				printf("error: type not defined '%s'\n", i->ret.to_string().c_str());
				continue;
			}
		}

		vector<Instance> args;
		for (auto j = i->args.begin(); j != i->args.end(); j++) {
			import_declaration(args, prgm, currModule, *j);
		}

		currTerm = prgm.mods[currModule].createTerm(Term::procOf(kind, i->name, args, retType, recvType));

		if (kind >= 0) {
			prgm.mods[currModule].terms[currTerm].def = Term::dialects[kind].factory(i->body, nullptr);
		}
	}
}

void Binder::load(string path) {
	// TODO(edward.bingham) Implement modules appropriately:
	// 1. load top.wv from current directory
	// 2. import paths are relative to project root, so find project root

	// TODO(edward.bingham) find project root and load all files

	// walk import tree
	string root = findProjectRoot(path);
	int offset = (int)prgm.mods.size();

	vector<parse_ucs::source> src;
	vector<string> stack;
	stack.push_back(path);
	for (int i = 0; i < (int)stack.size(); i++) {
		src.push_back(parsePath(stack[i], root));

		for (auto j = src.back().incl.begin(); j != src.back().incl.end(); j++) {
			for (auto k = j->path.begin(); k != j->path.end(); k++) {
				if (find(stack.begin(), stack.end(), k->second) == stack.end()) {
					string modPath = k->second.substr(1, k->second.size()-2)+".wv";
					stack.push_back(modPath);
				}
			}
		}
	}

	// load symbols to break dependency chains
	prgm.mods.resize(offset+src.size());
	for (int i = 0; i < (int)src.size(); i++) {
		loadSymbols(i+offset, src[i]);
	}

	// link up all of the dependencies
	for (int i = 0; i < (int)src.size(); i++) {
		loadModule(i+offset, src[i]);
	}
}

}
