#pragma once
#include "Expression.h"
#include "FuncDefLexer.h"
#include <optional>

enum class MemberType {
	Unknown,
	Function,
	Field
};

struct FunctionParameter {
	DataType parameterType;
	std::string parameterName;
	std::optional<Expression> optionalValue;
	bool isConstant = false;
	void Reset();
	operator std::string() const;
	FunctionParameter() = default;
	FunctionParameter(const FunctionParameter& parameter);
};

struct ClassLevelMember {
	MemberType memberType = MemberType::Unknown;
	std::string memberName = "";
	DataType dataType;
	AccessModifier accessModifier = AccessModifier::Unknown;

	bool fieldIsConstant = false;
	std::optional<Expression> assignedToValue = std::nullopt;

	std::vector<FunctionParameter> functionParameters = {};
	ClassLevelMember() {}
	operator std::string() const;
};

class Lexer {
public:
	Lexer();
	void ParseScript(std::string filePath);
	static bool IsNumber(char ch);
	static bool IsRegularSymbolChar(char ch);
private:
	std::vector<std::variant<ClassLevelMember, FunctionLevelInstruction>> listOfTokenizedInstructions;
	std::vector<std::string> functionLevelSymbolHistory;
	std::string currentSymbol = "";
	ClassLevelMember currentClassLevelMember;
	FunctionLevelInstruction functionLevelMember;
	void ParseLine(std::string currentLine);
	void HandleSymbol(std::string& symbolName);
	void ParseSymbol(std::string symbolName);
	void HandleStartFunctionSymbol(std::string symbolName);
	TokenType previousToken = TokenType::None;
	FunctionParameter currentFunctionParameter;
	bool assignmentEncountered = false, parametersEncountered = false, inOptionalParameter = false, encounteredNonTab = false;
	int normalSymbolsParsed = 0, symbolsParsedThisLine = 0, currentNestingLevel = 0;
	void ParseFieldSymbol(bool specialSymbol, std::string symbolName);
	void ParseFunctionSymbol(bool specialSymbol, std::string symbolName);
	void ParseFunctionParameter();
	void PrintTokenizedInstructions();
	void ParseFunctionLevelSymbol(std::string symbolName);
	void HandleExpressionOnSpecialFunctionLevelSymbol(std::string symbolName);
	void CorrectDeclarationWithoutValue();
	void HandleConstantDeclaration();
	bool DetermineIfAssigning(std::string symbolName);
	void InitializeFunctionLevelMember();
	void ParseConditionalSymbol();
	void SetupConditional(ConditionalType conditonalType);
	void HandleInstructionSeperationSymbol(bool createsScope, bool arrowSyntax = false);
	void EndFunctionLevelMember(bool arrowSyntax = false);
	void ParseLoopAdjecentStatement();
};