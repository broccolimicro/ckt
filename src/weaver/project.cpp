#include "project.h"

#include <common/text.h>
#include <filesystem>

#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse_ucs/modfile.h>

namespace weaver {

Filetype::Filetype() {
	read = nullptr;
	load = nullptr;
	write = nullptr;
}

Filetype::Filetype(string dialect, string ext, string build, Filetype::Parser read, Filetype::Loader load, Filetype::Writer write) {
	this->dialect = dialect;
	this->ext = ext;
	this->build = build;
	this->read = read;
	this->load = load;
	this->write = write;
}

Filetype::~Filetype() {
}

Project::Project() {
	workDir = fs::current_path();
	rootDir = workDir;
	while (not rootDir.empty()
		and rootDir.parent_path() != rootDir
		and not fs::exists(rootDir / "lm.mod")) {
		rootDir = rootDir.parent_path();
	}

	includePath.push_back(rootDir / SOURCE);
	includePath.push_back(rootDir / VENDOR);

	char *loom_tech = std::getenv("LOOM_TECH");
	if (loom_tech != nullptr) {
		techDir = fs::path(loom_tech);
	} else {
#if defined(_WIN32) || defined(_WIN64)
		techDir = fs::path("C:\\Program Files (x86)\\Loom\\share\\tech");
#else
		techDir = fs::path("/usr/local/share/tech");
#endif
	}
}

Project::~Project() {
}

int Project::pushFiletype(string dialect, string ext, string build, Filetype::Parser read, Filetype::Loader load, Filetype::Writer write) {
	// Register a new dialect with the given name and factory function
	filetypes.push_back(Filetype(dialect, ext, build, read, load, write));
	// Return the index of the newly registered dialect
	return (int)filetypes.size()-1;
}

const Filetype *Project::getExtension(string ext) const {
	for (int i = 0; i < (int)filetypes.size(); i++) {
		if (filetypes[i].ext == ext) {
			return &filetypes[i];
		}
	}
	return nullptr;
}

const Filetype *Project::getDialect(string dialect) const {
	for (int i = 0; i < (int)filetypes.size(); i++) {
		if (filetypes[i].dialect == dialect) {
			return &filetypes[i];
		}
	}
	return nullptr;
}

bool Project::incl(fs::path path, fs::path from) {
	if (from.empty()) {
		from = workDir;
	}

	fs::path filename;
	if (path.is_absolute()) {
		if (fs::exists(path)) {
			filename = path.string();
		}
	} else {
		if (fs::exists(from / path)) {
			filename = (from / path).string();
		}
		for (auto i = includePath.begin(); i != includePath.end() and filename.empty(); i++) {
			if (fs::exists(*i / path)) {
				filename = (*i / path).string();
			}
		}
	}
	if (filename.empty()) {
		printf("error: file not found '%s'", path.string().c_str());
		return false;
	}
	
	auto pos = find(imports.begin(), imports.end(), filename);
	if (pos == imports.end()) {
		imports.push_back(filename);
	}

	return true;	
}

bool Project::read(Program &prgm, fs::path path) {
	if (path.empty()) {
		return false;
	}

	string ext = path.extension().string().substr(1);
	auto filetype = getExtension(ext);
	if (filetype == nullptr) {
		printf("error: unrecognized filetype '%s'\n", ext.c_str());
		return false;
	}

	fs::path canon = path;
	if (not canon.is_absolute()) {
		canon = workDir / canon;
	}

	sources.push_back(Source());
	sources.back().path = fs::relative(canon, workDir);
	sources.back().modName = pathToModule(canon);
	sources.back().filetype = filetype;
	sources.back().tokens = unique_ptr<tokenizer>(new tokenizer());

	if (filetype->read != nullptr) {
		ifstream fin;
		fin.open(path.string().c_str(), ios::binary | ios::in);
		if (not fin.is_open()) {
			printf("error: file not found '%s'", path.string().c_str());
			return false;
		}

		fin.seekg(0, ios::end);
		int size = (int)fin.tellg();
		string buffer(size, ' ');
		fin.seekg(0, ios::beg);
		fin.read(&buffer[0], size);
		fin.close();

		filetype->read(*this, sources.back(), buffer);
	}
	return true;
}

bool Project::load(Program &prgm) {
	// TODO(edward.bingham) this is still wrong, we have to create a DAG and walk the DAG backwards from the leaves...
	
	for (int i = 0; i < (int)imports.size(); i++) {
		if (not read(prgm, imports[i])) {
			return false;
		}
	}

	while (not sources.empty()) {
		if (sources.back().filetype->load != nullptr) {
			sources.back().filetype->load(*this, prgm, sources.back());
		}
		sources.pop_back();
	}

	return true;
}

bool Project::save(Program &prgm, int modIdx, int termIdx) const {
	string dialect = prgm.mods[modIdx].terms[termIdx].dialect().name;
	auto filetype = getDialect(dialect);
	if (filetype == nullptr or filetype->write == nullptr) {
		return false;
	}

	fs::path emitDir = (rootDir / BUILD / filetype->build).string();
	std::filesystem::create_directories(emitDir.string());
	
	string filename = prgm.mods[modIdx].terms[termIdx].decl.name + "." + filetype->ext;
	filetype->write((emitDir / filename).string(), *this, prgm, modIdx, termIdx);
	return true;
}

void Project::save(Program &prgm) const {
	for (int i = 0; i < (int)prgm.mods.size(); i++) {
		for (int j = 0; j < (int)prgm.mods[i].terms.size(); j++) {
			if (prgm.mods[i].terms[j].kind < 0) {
				printf("internal:%s:%d: dialect not defined for term '%s'\n", __FILE__, __LINE__, prgm.mods[i].terms[j].decl.name.c_str());
				continue;
			}
			save(prgm, i, j);
		}
	}
}

void Project::setTechPath(string arg, bool setCells) {
	string path = extractPath(arg);
	string opt = (arg.size() > path.size() ? arg.substr(path.size()+1) : "");

	size_t dot = path.find_last_of(".");
	string ext = "";
	if (dot != string::npos) {
		ext = path.substr(dot+1);
	}

	if (ext == "py") {
		tech.path = arg;
		if (setCells) {
			tech.lib = "cells";
		}
	} else if (ext == "") {
		if (not techDir.empty()) {
			tech.path = escapePath((fs::path(techDir) / path / "tech.py").string()) + opt;
			if (setCells) {
				tech.lib = (fs::path(techDir) / path / "cells").string();
			}
		} else {
			tech.path = path+".py" + opt;
			if (setCells) {
				tech.lib = "cells";
			}
		}
	}

	this->techName = fs::path(tech.path).parent_path();
}

void Project::setCellsDir(string arg) {
	tech.lib = arg;
}

void Project::setTech(string techName) {
	fs::path path;
	if (not techName.empty() and techName[0] == '/') {
		path = techName;
	} else {
		path = techDir / techName;
	}

	if (not fs::exists(path)) {
		printf("tech directory '%s' not found\n", path.string().c_str());
		printf("the tech directory may be specified using the $LOOM_TECH variable\n");
	} else {
		this->techName = techName;
		tech.path = (path / "tech.py").string();
		tech.lib = (path / "cells").string();
	}
}

vector<string> Project::listTech() const {
	vector<string> result;
	if (not fs::exists(techDir)) {
		return result;
	}

	for (const auto &entry : fs::directory_iterator(techDir)) {
		if (entry.is_directory() and entry.path().stem().string()[0] != '_') {
			result.push_back(entry.path().stem());
		}
	}
	return result;
}

bool Project::hasMod() const {
	return not rootDir.empty() and rootDir.parent_path() != rootDir;
}

void Project::readMod() {
	tokenizer tokens;
	tokens.register_token<parse::block_comment>(false);
	tokens.register_token<parse::line_comment>(false);
	parse_ucs::modfile::register_syntax(tokens);

	ifstream fin;
	fin.open((rootDir / "lm.mod").string().c_str(), ios::binary | ios::in);
	if (!fin.is_open()) {
		tokens.error("file not found '" + (rootDir / "lm.mod").string() + "'", __FILE__, __LINE__);
	} else {
		fin.seekg(0, ios::end);
		int size = (int)fin.tellg();
		string buffer(size, ' ');
		fin.seekg(0, ios::beg);
		fin.read(&buffer[0], size);
		fin.clear();
		tokens.insert((rootDir / "lm.mod").string(), buffer, nullptr);
	}

	tokens.increment(true);
	tokens.expect<parse_ucs::modfile>();

	if (tokens.decrement(__FILE__, __LINE__)) {
		parse_ucs::modfile syntax(tokens);

		for (auto i = syntax.deps.begin(); i != syntax.deps.end(); i++) {
			for (auto j = i->path.begin(); j != i->path.end(); j++) {
				depends.push_back(Depend{j->first, j->second});
			}
		}

		for (auto i = syntax.attrs.begin(); i != syntax.attrs.end(); i++) {
			if (i->name == "module") {
				modName = i->value.substr(1, i->value.size()-2);
			} else if (i->name == "tech") {
				setTech(i->value.substr(1, i->value.size()-2));
			}
		}
	}
}

void Project::writeMod() {
	parse_ucs::modfile result;
	result.valid = true;
	parse_ucs::require dep;
	dep.valid = true;

	for (auto i = depends.begin(); i != depends.end(); i++) {
		dep.path.push_back({i->path, i->version});
	}
	result.deps.push_back(dep);

	parse_ucs::attribute modAttr;
	modAttr.valid = true;
	modAttr.name = "module";
	modAttr.value = "\"" + modName + "\"";
	result.attrs.push_back(modAttr);

	parse_ucs::attribute techAttr;
	techAttr.valid = true;
	techAttr.name = "tech";
	techAttr.value = "\"" + techName + "\"";
	result.attrs.push_back(techAttr);

	ofstream fout;
	fout.open((rootDir / "lm.mod").string().c_str(), ios::out);
	string buf = result.to_string("");
	fout.write(buf.c_str(), buf.size());
	fout.close();
}

void Project::vendor() const {
}

void Project::tidy() {
}

string Project::pathToModule(fs::path path) const {
	string result;

	string ext = path.extension().string().substr(1);
	auto filetype = getExtension(ext);

	fs::path dirInModule = fs::relative(path.parent_path(), rootDir);
	if (dirInModule.lexically_normal() != ".") {
		result = (fs::path(modName) / dirInModule / path.stem()).string();
	} else {
		result = (fs::path(modName) / path.stem()).string();
	}

	if (filetype != nullptr and not filetype->dialect.empty()) {
		result += ">>" + filetype->dialect;
	}
	return result;
}

fs::path Project::buildPath(string dir, string filename) const {
	return rootDir / BUILD / dir / filename;
}

}
