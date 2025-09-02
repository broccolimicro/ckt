#pragma once

#include <vector>
#include <string>
#include <filesystem>
#include "project.h"

using namespace std;
namespace fs = std::filesystem;

struct Proto {
	fs::path path;
	vector<string> recv;
	vector<string> name;
	vector<vector<string> > args;

	bool isModule() const;
	bool isTerm() const;

	string to_string() const;
};

vector<string> parseIdent(string id);
Proto parseProto(const weaver::Project &proj, string proto);
