#pragma once
#include <string>
#include <vector>
#include <variant>
#include <stack>

enum class AccessModifier {
	Unknown,
	Public,
	Private
};

struct DataType {
	std::string typeName = "";
	operator std::string() const;
	DataType() {}
	DataType(std::string name) : typeName(name) {}
	void Reset();
};

enum class OperandType {
	Unknown,
	Variable,
	BooleanLiteral,
	IntegerLiteral,
	RealLiteral,
	StringLiteral,
	TypeLiteral,
	UnrecognizedSymbol,
	ParenthesisContents,
	FunctionCall
};

enum class OperatorType {
	Unknown,
	LeftUnaryOperator,
	RightUnaryOperator,
	BinaryOperator,
	LeftParenthesis,
	RightParenthesis,
	FunctionParameterSeperator,
	FunctionClosing
};

enum class TokenType {
	None,
	AccessModifier,
	MemberName,
	Declaration,
	MemberType,
	Assignment,
	Expression,
	BeforeParameterName,
	AfterParameterName
};

struct Operand {
	OperandType operandType; std::string operandContents;
	Operand(OperandType type, std::string contents) { operandType = type; operandContents = contents; }
	Operand() {}
	operator std::string() const;
	void Reset();
};
struct Operator {
	OperatorType operatorType; std::string operatorContents;
	Operator(OperatorType type, std::string contents = "") { operatorType = type; operatorContents = contents; }
	Operator() {}
	operator std::string() const;
};

class Expression {
public:
	std::vector<std::variant<Operand, Operator>> expressionContents;
	operator std::string() const;
	Expression(std::vector<std::variant<Operand, Operator>> expr) : expressionContents(expr) {}
	Expression();
	Operand currentOperand, previouslyParsedOperand;
	Operator latestOperator;
	int currentBracketNestingLevel;
	bool constantDeclarationEncountered = false, latestExpressionElementIsOperand = false;
	std::string currentBinaryOperator = "";
	std::vector<std::string> validBinaryOperators = { "+", "-", "*", "/", "%", "^", "==", "!=", "<=", ">=", "&&", "||", "->", "?>", "|", "&", "..", "..=" };
	void ParseNextSymbol(std::string symbolName);
	void EndExpression();
private:
	void ParseApostrophe();
	void ParseNumberChar(char ch);
	void TerminateNumericLiteral();
	void ParseUnrecognizedSymbolChar(char ch);
	void TerminateUnrecognizedSymbolChar();
	void EndBinaryOperator();
	void AddOperator(OperatorType newOpType, std::string newOpContents);
	void AddOperand();
	void ParseFunctionCall();
	void ParseNextChar(char ch);
	void ParseLeftParenthesis();
	void ParseRightParenthesis();
	void RangeDotOperator();
	std::stack<bool> leftParenCallsStack;
};
