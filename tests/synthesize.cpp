#include <algorithm>
#include <bit>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include <common/standard.h>
#include <parse/tokenizer.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>

#include <parse_cog/composition.h>
#include <parse_cog/branch.h>
#include <parse_cog/control.h>
#include <parse_cog/factory.h>

#include <chp/graph.h>
#include <chp/synthesize.h>
#include <interpret_chp/import_chp.h>
#include <interpret_chp/import_cog.h>
#include <interpret_chp/export_dot.h>

#include <flow/func.h>
#include <flow/module.h>
#include <flow/synthesize.h>
#include <interpret_flow/export.h>

#include "src/format/dot.h"

#define EXPECT_SUBSTRING(source, substring) \
    EXPECT_NE((source).find(substring), std::string::npos) \
		<< "ERROR: Expected Verilog to contain substring \"" << (substring) << "\""
#define EXPECT_NO_SUBSTRING(source, substring) \
    EXPECT_EQ((source).find(substring), std::string::npos) \
		<< "ERROR: Expected Verilog to NOT contain substring \"" << (substring) << "\""

using namespace std;
using std::filesystem::absolute;
using std::filesystem::current_path;

using arithmetic::Expression;
using arithmetic::Operand;

const std::filesystem::path TEST_DIR = absolute(current_path() / "tests");
const int WIDTH = 8;


string readStringFromFile(const string &absolute_filename, bool strip_newlines=false) {
	ifstream in(absolute_filename, ios::in | ios::binary);
	EXPECT_NO_THROW({
		if (!in) throw std::runtime_error("Failed to open file: " + absolute_filename);
	});

	ostringstream contents;
	contents << in.rdbuf();
	string result = contents.str();

	// Replace all newlines with spaces
	if (strip_newlines) {
		 replace(result.begin(), result.end(), '\n', ' ');
		 replace(result.begin(), result.end(), '\r', ' '); // for Windows files
	}

	return result;
}


chp::graph importCHPFromCogString(const string &cog) {
	tokenizer tokens;
	tokens.register_token<parse::block_comment>(false);
	tokens.register_token<parse::line_comment>(false);
	parse_cog::register_syntax(tokens);

	tokens.insert("string_input", cog, nullptr);
	chp::graph g;

	tokens.increment(false);
	tokens.expect<parse_cog::composition>();
	if (tokens.decrement(__FILE__, __LINE__)) {
		parse_cog::composition syntax(tokens);
		//cout << "===>" << syntax.to_string() << endl;  // for debugging parse vs interpret
		chp::import_chp(g, syntax, &tokens, true);
	}

	return g;
}


template <typename T>
auto to_unordered_multiset = [](const std::vector<T>& v) {
    return std::multiset<T>(v.begin(), v.end());
};


bool areEquivalent(flow::Func &real, flow::Func &expected) {
	//TODO: insensitive to ids and condition order

	EXPECT_EQ(real.nets.size(), expected.nets.size());
	//std::sort(real.nets.begin(), real.nets.end());
	//std::sort(expected.nets.begin(), expected.nets.end());
	auto real_nets = to_unordered_multiset<flow::Net>(real.nets);
	auto expected_nets = to_unordered_multiset<flow::Net>(expected.nets);
	EXPECT_EQ(real_nets, expected_nets); 

	if (real_nets != expected_nets) {
		std::copy(real_nets.begin(), real_nets.end(), std::ostream_iterator<flow::Net>(cout, "\n"));
		cout << endl;
		std::copy(expected_nets.begin(), expected_nets.end(), std::ostream_iterator<flow::Net>(cout, "\n"));
	}
	else { std::copy(real_nets.begin(), real_nets.end(), std::ostream_iterator<flow::Net>(cout, "\n")); }

	EXPECT_EQ(real.conds.size(), expected.conds.size());
	//std::sort(real.conds.begin(), real.conds.end());
	//std::sort(expected.conds.begin(), expected.conds.end());
	auto real_conds = to_unordered_multiset<flow::Condition>(real.conds);
	auto expected_conds = to_unordered_multiset<flow::Condition>(expected.conds);
	EXPECT_EQ(real_conds, expected_conds);

	return true;
}


flow::Func testFuncSynthesisFromCog(flow::Func &expected, bool render=true) {
	string filenameWithoutExtension = (TEST_DIR / expected.name).string();
	string cogFilename = filenameWithoutExtension + ".cog";
	string cogRaw = readStringFromFile(cogFilename, false);
	cout << "```cog" << endl << cogRaw << endl << "```" << endl;

	chp::graph g = importCHPFromCogString(cogRaw);
	g.post_process(true, false);  //TODO: ... true, true) , should be called internally from other transformations
	g.name = expected.name;

	g.flatten(true);
	if (render) {
		string graphvizRaw = chp::export_graph(g, true).to_string();
		gvdot::render(filenameWithoutExtension + ".png", graphvizRaw);
	}
	flow::Func real = chp::synthesizeFuncFromCHP(g);

	EXPECT_EQ(real.name, expected.name);
	EXPECT_TRUE(areEquivalent(real, expected));

	// TODO: ideal API?
	//parse_cog::composition cog = importCogFromFile(cog_filename);
	//chp::graph g = synthesizeCHPFromCog(cog);
	//flow::Func func_real = chp::syntheiszeFuncFromCHP(g);
	return real;
}


parse_verilog::module_def synthesizeVerilogFromFunc(const flow::Func &func) {
	clocked::Module mod = synthesizeModuleFromFunc(func);
	parse_verilog::module_def mod_v = flow::export_module(mod);
	string verilog = mod_v.to_string();
	cout << "```verilog" << endl << verilog << endl << "```" << endl;

	auto write_verilog_file = [](const string &data, const string &filename) {
		std::ofstream export_file(filename);
		if (!export_file) {
				std::cerr << "ERROR: Failed to open file for verilog export: "
					<< filename << std::endl
					<< "ERROR: Try again from dir: <project_dir>/lib/flow" << std::endl;
		}
		export_file << data;
	};
	string export_filename = (TEST_DIR / (func.name + ".v")).string();
	EXPECT_NO_THROW(write_verilog_file(verilog, export_filename));

	// Validate branches
	size_t branch_count = func.conds.size();
	int branch_reg_width = std::bit_width(branch_count) - 1;
	if (branch_count > 2) {
		EXPECT_SUBSTRING(verilog, "reg [" + std::to_string(branch_reg_width) + ":0] branch_id;");
	} else {
		EXPECT_SUBSTRING(verilog, "reg branch_id;");
	}
	for (int i = 0; i < branch_count; i++) {
		EXPECT_SUBSTRING(verilog, "branch_id <= " + std::to_string(i) + ";");
	}

	return mod_v;
}


TEST(CogToVerilog, Counter) {
	flow::Func func_expected;
	func_expected.name = "counter";
	Operand n = func_expected.pushNet("n", flow::Type(flow::Type::FIXED, WIDTH), flow::Net::IN);
	Operand i = func_expected.pushNet("i", flow::Type(flow::Type::FIXED, WIDTH), flow::Net::REG);
	Expression expr_i(i);

	int branch0 = func_expected.pushCond(arithmetic::Expression::boolOf(true));
	func_expected.conds[branch0].mem(i, expr_i + 1);

	flow::Func func_real = testFuncSynthesisFromCog(func_expected);
	string verilog = synthesizeVerilogFromFunc(func_real).to_string();
}


TEST(CogToVerilog, Buffer) {
	flow::Func func_expected;
	func_expected.name = "buffer";
	Operand L = func_expected.pushNet("L", flow::Type(flow::Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand R = func_expected.pushNet("R", flow::Type(flow::Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
	Expression expr_L(L);

	int branch0 = func_expected.pushCond(Expression::boolOf(true));
	func_expected.conds[branch0].req(R, expr_L);
	func_expected.conds[branch0].ack(L);

	flow::Func func_real = testFuncSynthesisFromCog(func_expected);
	string verilog = synthesizeVerilogFromFunc(func_real).to_string();
	EXPECT_SUBSTRING(verilog, "R_state <= L_data;");
}


TEST(CogToVerilog, Receiver) {
	flow::Func func_expected;
	func_expected.name = "receiver";
	Operand in     = func_expected.pushNet("in",     flow::Type(flow::Type::FIXED, WIDTH), flow::Net::IN);
	Operand msg    = func_expected.pushNet("msg",    flow::Type(flow::Type::FIXED, WIDTH), flow::Net::REG);
	Operand logger = func_expected.pushNet("logger", flow::Type(flow::Type::FIXED, WIDTH), flow::Net::OUT);
	Expression expr_in(in);
	Expression expr_msg(msg);
	Expression expr_logger(logger);

	int branch0 = func_expected.pushCond(expr_msg != "end");
	func_expected.conds[branch0].req(logger, expr_msg);
	func_expected.conds[branch0].mem(msg, expr_in);
	func_expected.conds[branch0].ack({in});

	int branch1 = func_expected.pushCond(expr_msg == "end");
	func_expected.conds[branch1].mem(msg, expr_in);
	func_expected.conds[branch1].ack({in});

	flow::Func func_real = testFuncSynthesisFromCog(func_expected);
	string verilog = synthesizeVerilogFromFunc(func_real).to_string();
}


TEST(CogToVerilog, TrafficLight) {
	flow::Func func_expected;
	func_expected.name = "traffic_light";
	Operand s = func_expected.pushNet("state", flow::Type(flow::Type::TypeName::FIXED, WIDTH), flow::Net::REG);
	Expression expr_s(s);
	Expression expr_green(Expression::stringOf("green"));
	Expression expr_yellow(Expression::stringOf("yellow"));
	Expression expr_red(Expression::stringOf("red"));

	int branch0 = func_expected.pushCond(expr_s == expr_red);
	func_expected.conds[branch0].mem(s, expr_green);

	int branch1 = func_expected.pushCond(expr_s == expr_green);
	func_expected.conds[branch0].mem(s, expr_yellow);

	int branch2 = func_expected.pushCond(expr_s == expr_yellow);
	func_expected.conds[branch0].mem(s, expr_red);

	flow::Func func_real = testFuncSynthesisFromCog(func_expected);
	string verilog = synthesizeVerilogFromFunc(func_real).to_string();
}


TEST(CogToVerilog, IsEven) {
	flow::Func func_expected;
	func_expected.name = "is_even";
	Operand L = func_expected.pushNet("L", flow::Type(flow::Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand R = func_expected.pushNet("R", flow::Type(flow::Type::TypeName::FIXED, 1),     flow::Net::OUT);
	Expression expr_L(L);

	int branch0 = func_expected.pushCond(Expression::boolOf(true));
	func_expected.conds[branch0].req(R, expr_L % 2 == 0);
	func_expected.conds[branch0].ack({L});

	flow::Func func_real = testFuncSynthesisFromCog(func_expected);
	string verilog = synthesizeVerilogFromFunc(func_real).to_string();
}


TEST(CogToVerilog, Copy) {
	flow::Func func_expected;
	func_expected.name = "copy";
	Operand L =  func_expected.pushNet("L",  flow::Type(flow::Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand R0 = func_expected.pushNet("R0", flow::Type(flow::Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
	Operand R1 = func_expected.pushNet("R1", flow::Type(flow::Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
//TODO: Operand l REG ??
	Expression exprL(L);

	int branch0 = func_expected.pushCond(Expression::boolOf(true));
	func_expected.conds[branch0].req(R0, exprL);
	func_expected.conds[branch0].req(R1, exprL);
	func_expected.conds[branch0].ack(L);

	flow::Func func_real = testFuncSynthesisFromCog(func_expected);
	string verilog = synthesizeVerilogFromFunc(func_real).to_string();
	EXPECT_SUBSTRING(verilog, "R0_state <= L_data;");
	EXPECT_SUBSTRING(verilog, "R1_state <= L_data;");
}


TEST(CogToVerilog, Func) {
	flow::Func func_expected;
	func_expected.name = "func";
	Operand L0 = func_expected.pushNet("L0", flow::Type(flow::Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand L1 = func_expected.pushNet("L1", flow::Type(flow::Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand R  = func_expected.pushNet("R",  flow::Type(flow::Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
	Expression expr_L0(L0);
	Expression expr_L1(L1);

	int branch0 = func_expected.pushCond(Expression::boolOf(true));
	func_expected.conds[branch0].req(R, expr_L0 || expr_L1);
	//func_expected.conds[branch0].mem(m_or, expr_L0 || expr_L1);
	func_expected.conds[branch0].ack({L0, L1});

	flow::Func func_real = testFuncSynthesisFromCog(func_expected);
	string verilog = synthesizeVerilogFromFunc(func_real).to_string();
	EXPECT_SUBSTRING(verilog, "R_state <= L0_data|L1_data;");
	EXPECT_NO_SUBSTRING(verilog, "R_state <= L0_data||L1_data;");
}


TEST(CogToVerilog, Merge) {
	flow::Func func_expected;
	func_expected.name = "merge";
	Operand L0 = func_expected.pushNet("L0", flow::Type(flow::Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand L1 = func_expected.pushNet("L1", flow::Type(flow::Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand C  = func_expected.pushNet("C",  flow::Type(flow::Type::TypeName::FIXED, 1),     flow::Net::IN);
	Operand R  = func_expected.pushNet("R",  flow::Type(flow::Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
	Expression exprL0(L0);
	Expression exprL1(L1);
	Expression exprC(C);

	int branch0 = func_expected.pushCond(exprC == Expression::intOf(0));
	func_expected.conds[branch0].req(R, exprL0);
	func_expected.conds[branch0].ack({C, L0});

	int branch1 = func_expected.pushCond(exprC == Expression::intOf(1));
	func_expected.conds[branch1].req(R, exprL1);
	func_expected.conds[branch1].ack({C, L1});

	flow::Func func_real = testFuncSynthesisFromCog(func_expected);
	string verilog = synthesizeVerilogFromFunc(func_real).to_string();
	EXPECT_SUBSTRING(verilog, "R_state <= L0_data;");
	EXPECT_SUBSTRING(verilog, "R_state <= L1_data;");
}


TEST(CogToVerilog, Split) {
	flow::Func func_expected;
	func_expected.name = "split";
	Operand L  = func_expected.pushNet("L",  flow::Type(flow::Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand C  = func_expected.pushNet("C",  flow::Type(flow::Type::TypeName::FIXED, 1),     flow::Net::IN);
	Operand R0 = func_expected.pushNet("R0", flow::Type(flow::Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
	Operand R1 = func_expected.pushNet("R1", flow::Type(flow::Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
	Expression exprL(L);
	Expression exprC(C);

	int branch0 = func_expected.pushCond(exprC == Expression::intOf(0));
	func_expected.conds[branch0].req(R0, exprL);
	func_expected.conds[branch0].ack({C, L});

	int branch1 = func_expected.pushCond(exprC == Expression::intOf(1));
	func_expected.conds[branch1].req(R1, exprL);
	func_expected.conds[branch1].ack({C, L});

	flow::Func func_real = testFuncSynthesisFromCog(func_expected);
	string verilog = synthesizeVerilogFromFunc(func_real).to_string();
	EXPECT_SUBSTRING(verilog, "R0_state <= L_data;");
	EXPECT_SUBSTRING(verilog, "R1_state <= L_data;");
}


TEST(CogToVerilog, DSAdderFlat) {
	flow::Func func_expected;
	func_expected.name = "ds_adder_flat";
	Operand Ad = func_expected.pushNet("Ad", flow::Type(flow::Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand Ac = func_expected.pushNet("Ac", flow::Type(flow::Type::TypeName::FIXED, 1),		 flow::Net::IN);
	Operand Bd = func_expected.pushNet("Bd", flow::Type(flow::Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand Bc = func_expected.pushNet("Bc", flow::Type(flow::Type::TypeName::FIXED, 1),		 flow::Net::IN);
	Operand Sd = func_expected.pushNet("Sd", flow::Type(flow::Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
	Operand Sc = func_expected.pushNet("Sc", flow::Type(flow::Type::TypeName::FIXED, 1),		 flow::Net::OUT);
	Operand ci = func_expected.pushNet("ci", flow::Type(flow::Type::TypeName::FIXED, 1),		 flow::Net::REG);
	Expression expr_Ac(Ac);
	Expression expr_Ad(Ad);
	Expression expr_Bc(Bc);
	Expression expr_Bd(Bd);
	Expression expr_ci(ci);

	Expression expr_s((expr_Ad + expr_Bd + expr_ci) % pow(2, WIDTH));
	Expression expr_co((expr_Ad + expr_Bd + expr_ci) / pow(2, WIDTH));

	int branch0 = func_expected.pushCond(~expr_Ac & ~expr_Bc);
	func_expected.conds[branch0].req(Sd, expr_s);
	func_expected.conds[branch0].req(Sc, Expression::intOf(0));
	func_expected.conds[branch0].mem(ci, expr_co);
	func_expected.conds[branch0].ack({Ac, Ad, Bc, Bd});

	int branch1 = func_expected.pushCond(expr_Ac & ~expr_Bc);
	func_expected.conds[branch1].req(Sd, expr_s);
	func_expected.conds[branch1].req(Sc, Expression::intOf(0));
	func_expected.conds[branch1].mem(ci, expr_co);
	func_expected.conds[branch1].ack({Bc, Bd});

	int branch2 = func_expected.pushCond(~expr_Ac & expr_Bc);
	func_expected.conds[branch2].req(Sd, expr_s);
	func_expected.conds[branch2].req(Sc, Expression::intOf(0));
	func_expected.conds[branch2].mem(ci, expr_co);
	func_expected.conds[branch2].ack({Ac, Ad});

	int branch3 = func_expected.pushCond(expr_Ac & expr_Bc & (expr_co != expr_ci));
	func_expected.conds[branch3].req(Sd, expr_s);
	func_expected.conds[branch3].req(Sc, Expression::intOf(0));
	func_expected.conds[branch3].mem(ci, expr_co);

	int branch4 = func_expected.pushCond(expr_Ac & expr_Bc & (expr_co == expr_ci));
	func_expected.conds[branch4].req(Sd, expr_s);
	func_expected.conds[branch4].req(Sc, Expression::intOf(1));
	func_expected.conds[branch4].mem(ci, Expression::intOf(0));
	func_expected.conds[branch4].ack({Ac, Ad, Bc, Bd});

	flow::Func func_real = testFuncSynthesisFromCog(func_expected);
	string verilog = synthesizeVerilogFromFunc(func_real).to_string();

	EXPECT_SUBSTRING(verilog, "Sc_state <= 0;");
	EXPECT_SUBSTRING(verilog, "Sc_state <= 1;");
	EXPECT_SUBSTRING(verilog, "Sd_state <= (Ad_data+Bd_data+ci_data)%65536;");
	EXPECT_SUBSTRING(verilog, "ci_data <= (Ad_data+Bd_data+ci_data)/65536;");
}


TEST(CogToVerilog, DSAdder) {
	flow::Func func_expected;
	func_expected.name = "ds_adder";
	Operand Ad = func_expected.pushNet("Ad", flow::Type(flow::Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand Ac = func_expected.pushNet("Ac", flow::Type(flow::Type::TypeName::FIXED, 1),		 flow::Net::IN);
	Operand Bd = func_expected.pushNet("Bd", flow::Type(flow::Type::TypeName::FIXED, WIDTH), flow::Net::IN);
	Operand Bc = func_expected.pushNet("Bc", flow::Type(flow::Type::TypeName::FIXED, 1),		 flow::Net::IN);
	Operand Sd = func_expected.pushNet("Sd", flow::Type(flow::Type::TypeName::FIXED, WIDTH), flow::Net::OUT);
	Operand Sc = func_expected.pushNet("Sc", flow::Type(flow::Type::TypeName::FIXED, 1),		 flow::Net::OUT);
	Operand ci = func_expected.pushNet("ci", flow::Type(flow::Type::TypeName::FIXED, 1),		 flow::Net::REG);
	Expression expr_Ac(Ac);
	Expression expr_Ad(Ad);
	Expression expr_Bc(Bc);
	Expression expr_Bd(Bd);
	Expression expr_ci(ci);

	Expression expr_s((expr_Ad + expr_Bd + expr_ci) % pow(2, WIDTH));
	Expression expr_co((expr_Ad + expr_Bd + expr_ci) / pow(2, WIDTH));

	int branch0 = func_expected.pushCond(~expr_Ac & ~expr_Bc);
	func_expected.conds[branch0].req(Sd, expr_s);
	func_expected.conds[branch0].req(Sc, Expression::intOf(0));
	func_expected.conds[branch0].mem(ci, expr_co);
	func_expected.conds[branch0].ack({Ac, Ad, Bc, Bd});

	int branch1 = func_expected.pushCond(expr_Ac & ~expr_Bc);
	func_expected.conds[branch1].req(Sd, expr_s);
	func_expected.conds[branch1].req(Sc, Expression::intOf(0));
	func_expected.conds[branch1].mem(ci, expr_co);
	func_expected.conds[branch1].ack({Bc, Bd});

	int branch2 = func_expected.pushCond(~expr_Ac & expr_Bc);
	func_expected.conds[branch2].req(Sd, expr_s);
	func_expected.conds[branch2].req(Sc, Expression::intOf(0));
	func_expected.conds[branch2].mem(ci, expr_co);
	func_expected.conds[branch2].ack({Ac, Ad});

	int branch3 = func_expected.pushCond(expr_Ac & expr_Bc & (expr_co != expr_ci));
	func_expected.conds[branch3].req(Sd, expr_s);
	func_expected.conds[branch3].req(Sc, Expression::intOf(0));
	func_expected.conds[branch3].mem(ci, expr_co);

	int branch4 = func_expected.pushCond(expr_Ac & expr_Bc & (expr_co == expr_ci));
	func_expected.conds[branch4].req(Sd, expr_s);
	func_expected.conds[branch4].req(Sc, Expression::intOf(1));
	func_expected.conds[branch4].mem(ci, Expression::intOf(0));
	func_expected.conds[branch4].ack({Ac, Ad, Bc, Bd});

	flow::Func func_real = testFuncSynthesisFromCog(func_expected);
	string verilog = synthesizeVerilogFromFunc(func_real).to_string();

	EXPECT_SUBSTRING(verilog, "Sc_state <= 0;");
	EXPECT_SUBSTRING(verilog, "Sc_state <= 1;");
	EXPECT_SUBSTRING(verilog, "Sd_state <= (Ad_data+Bd_data+ci_data)%65536;");
	EXPECT_SUBSTRING(verilog, "ci_data <= (Ad_data+Bd_data+ci_data)/65536;");
}
