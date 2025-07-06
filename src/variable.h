#pragma once

#include <vector>
#include <string>

namespace ucs {

struct Field {
	Field();
	Field(std::string name, std::vector<int> slice);
	~Field();

	std::string name;
	std::vector<int> slice;

	std::string to_string() const;
};

bool operator<(const Field &i0, const Field &i1);
bool operator>(const Field &i0, const Field &i1);
bool operator<=(const Field &i0, const Field &i1);
bool operator>=(const Field &i0, const Field &i1);
bool operator==(const Field &i0, const Field &i1);
bool operator!=(const Field &i0, const Field &i1);

struct Variable {
	Variable();
	Variable(std::string name, int region = 0);
	~Variable();

	std::vector<Field> name;
	int region;

	bool isNode() const;

	std::string to_string() const;
};

bool operator<(const Variable &v0, const Variable &v1);
bool operator>(const Variable &v0, const Variable &v1);
bool operator<=(const Variable &v0, const Variable &v1);
bool operator>=(const Variable &v0, const Variable &v1);
bool operator==(const Variable &v0, const Variable &v1);
bool operator!=(const Variable &v0, const Variable &v1);

}

