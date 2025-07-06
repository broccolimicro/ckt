#include "variable.h"

using std::vector;
using std::string;

namespace ucs {

Field::Field() {
}

Field::Field(string name, vector<int> slice) {
	this->name = name;
	this->slice = slice;
}

Field::~Field() {
}

string Field::to_string() const {
	string result = name;
	for (int i = 0; i < (int)slice.size(); i++) {
		result += "[" + std::to_string(slice[i]) + "]";
	}
	return result;
}

bool operator<(const Field &i0, const Field &i1) {
	return (i0.name < i1.name) ||
		   (i0.name == i1.name && i0.slice < i1.slice);
}

bool operator>(const Field &i0, const Field &i1) {
	return (i0.name > i1.name) ||
		   (i0.name == i1.name && i0.slice > i1.slice);
}

bool operator<=(const Field &i0, const Field &i1) {
	return (i0.name < i1.name) ||
		   (i0.name == i1.name && i0.slice <= i1.slice);
}

bool operator>=(const Field &i0, const Field &i1) {
	return (i0.name > i1.name) ||
		   (i0.name == i1.name && i0.slice >= i1.slice);
}

bool operator==(const Field &i0, const Field &i1) {
	return i0.name == i1.name && i0.slice == i1.slice;
}

bool operator!=(const Field &i0, const Field &i1) {
	return i0.name != i1.name || i0.slice != i1.slice;
}

Variable::Variable() {
	region = 0;
}

Variable::Variable(string name, int region) {
	this->name.push_back(Field(name, vector<int>()));
	this->region = region;
}

Variable::~Variable() {
}

bool Variable::isNode() const {
	if (name.size() != 1 or name[0].name.size() <= 1 or name[0].name[0] != '_') {
		return false;
	}
	for (auto c = name[0].name.begin()+1; c != name[0].name.end(); c++) {
		if (*c < '0' or *c > '9') {
			return false;
		}
	}
	return true;
}

string Variable::to_string() const {
	string result = "";
	for (int i = 0; i < (int)name.size(); i++) {
		if (i != 0) {
			result += ".";
		}

		result += name[i].to_string();
	}

	if (region != 0) {
		result += "'" + std::to_string(region);
	}

	return result;
}

bool operator<(const Variable &v0, const Variable &v1) {
	return (v0.name < v1.name) ||
		   (v0.name == v1.name && v0.region < v1.region);
}

bool operator>(const Variable &v0, const Variable &v1) {
	return (v0.name > v1.name) ||
		   (v0.name == v1.name && v0.region > v1.region);
}

bool operator<=(const Variable &v0, const Variable &v1) {
	return (v0.name < v1.name) ||
		   (v0.name == v1.name && v0.region <= v1.region);
}

bool operator>=(const Variable &v0, const Variable &v1) {
	return (v0.name > v1.name) ||
		   (v0.name == v1.name && v0.region >= v1.region);
}

bool operator==(const Variable &v0, const Variable &v1) {
	return (v0.name == v1.name && v0.region == v1.region);
}

bool operator!=(const Variable &v0, const Variable &v1) {
	return (v0.name != v1.name || v0.region != v1.region);
}

}
