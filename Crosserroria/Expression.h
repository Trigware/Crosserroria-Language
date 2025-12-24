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
	FunctionCall,
	InitializerCall
};

enum class OperatorType {
	Unknown,
	LeftUnaryOperator,
	BinaryOperator,
	LeftParenthesis,
	RightParenthesis,
	FunctionParameterSeperator,
	FunctionClosing,
	InitializerClosing,
	TernaryOperatorValueOnSuccess,
	TernaryOperatorValueOnFail,
	AttributeAccess,
	SpecificFunctionParameter,
	CastingOverload,
	ArrayOpen,
	ArrayClose,
	IndexerOpen,
	IndexerClose,
	MapOpen,
	MapClose,
	MapKeyName
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
	AfterParameterName,
	ArrowSyntax
};

struct Operand {
	OperandType operandType = OperandType::Unknown; std::string operandContents;
	Operand(OperandType type, std::string contents) { operandType = type; operandContents = contents; }
	Operand() {}
	operator std::string() const;
	void Reset();
};
struct Operator {
	OperatorType operatorType = OperatorType::Unknown; std::string operatorContents;
	Operator(OperatorType type, std::string contents = "") { operatorType = type; operatorContents = contents; }
	Operator() {}
	operator std::string() const;
};

class Expression {
public:
	std::vector<std::variant<Operand, Operator>> expressionContents;
	operator std::string() const;
	inline Expression(const std::vector<std::variant<Operand, Operator>>& expr) : expressionContents(expr) {}
	Expression();
	Operand currentOperand, previouslyParsedOperand;
	Operator latestOperator;
	int currentBracketNestingLevel = 0, ternaryConditionalNestingLevel = 0;
	bool constantDeclarationEncountered = false, latestExpressionElementIsOperand = false;
	std::string currentBinaryOperator = "";
	std::vector<std::string> validBinaryOperators = { "+", "-", "*", "/", "%", "^", "==", "!=", ">", "<", "<=", ">=", "&&", "||", "->", "?>", "|", "&", "..", "..=", "?", "!" };
	void ParseNextSymbol(std::string symbolName);
	void EndExpression();
	bool NotInString();
	std::string GetLatestSymbol();
	std::stack<bool> leftParenCallsStack = std::stack<bool>(), squareBracketIsArrayStack = std::stack<bool>();
	void AddOperator(OperatorType newOpType, std::string newOpContents);
private:
	void ParseApostrophe();
	void ParseNumberChar(char ch);
	void TerminateNumericLiteral();
	void ParseUnrecognizedSymbolChar(char ch);
	void TerminateUnrecognizedSymbolChar();
	void EndBinaryOperator();
	void AddOperand();
	void ParseFunctionCall(bool regularCall);
	void ParseNextChar(char ch);
	void ParseLeftParenthesis(bool regularCall);
	void ParseRightParenthesis(bool regularCall);
	void RangeDotOperator();
	void ParseOpeningSquareBrackets(bool isOpenning);
	void ParseCurlyBraces(bool isOpenning);
};
