#pragma once

#include <phy/Tech.h>
#include <parse/parse.h>
#include <weaver/program.h>

#include <filesystem>

namespace fs = std::filesystem;

namespace weaver {

struct Project;
struct Filetype;

struct Depend {
	string path;
	string version;
};

struct Source {
	fs::path path;
	fs::path modName;
	unique_ptr<parse::syntax> syntax;
	unique_ptr<tokenizer> tokens;
	const Filetype *filetype;
};

struct Filetype {
	// Project &proj, string path, string buffer
	typedef void (*Parser)(Project &, Source &, string);
	// Project &proj, Program &prgm, string path, parse::syntax *syntax
	typedef void (*Loader)(Project &, Program &, const Source &source);
	// Program &prgm, int modIdx, int termIdx
	typedef void (*Writer)(fs::path, const Project &, const Program &, int, int);

	Filetype();
	Filetype(string dialect, string ext, string build, Parser read, Loader load, Writer write);
	~Filetype();

	string dialect;
	string ext;
	string build;

	Parser read;
	Loader load;
	Writer write;
};

struct Project {
	Project();
	~Project();

	static constexpr string BUILD = "build";
	static constexpr string VENDOR = "vendor";
	static constexpr string SOURCE = "src";
	
	vector<fs::path> includePath;

	phy::Tech tech;
	fs::path workDir;
	fs::path rootDir;
	fs::path techDir;

	string modName;
	string techName;
	vector<Depend> depends;

	vector<fs::path> imports;
	vector<Source> sources;

	vector<Filetype> filetypes;

	int pushFiletype(string dialect, string ext, string build, Filetype::Parser read, Filetype::Loader load, Filetype::Writer write=nullptr);	
	const Filetype *getExtension(string ext) const;
	const Filetype *getDialect(string dialect) const;

	bool incl(fs::path path, fs::path from="");	
	bool read(Program &prgm, fs::path path);
	bool load(Program &prgm);

	bool save(Program &prgm, int modIdx, int termIdx) const;
	void save(Program &prgm) const;

	void setTechPath(string arg, bool setCells=true);
	void setCellsDir(string arg);
	void setTech(string techName);
	vector<string> listTech() const;

	bool hasMod() const;
	void readMod();
	void writeMod();

	fs::path buildPath(string dialect, string filename) const;
};

}
