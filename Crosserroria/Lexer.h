#pragma once
#include "Expression.h"
#include "FuncDefLexer.h"
#include <optional>

enum class MemberType {
	Unknown,
	Function,
	Field,
	Enum,
	Class
};

struct FunctionParameter {
	DataType parameterType;
	std::string parameterName;
	std::optional<Expression> optionalValue;
	bool isConstant = false, afterDeclaration = false;
	void Reset();
	operator std::string() const;
	FunctionParameter() : optionalValue(std::nullopt) {}
	FunctionParameter(const FunctionParameter& parameter);
};

struct EnumMember {
	std::string memberName = "";
	std::vector<FunctionParameter> associatedData;
	std::optional<Expression> underlyingExpression = std::nullopt;
	EnumMember() = default;
	bool inData = false, inExpression = false, inUnderlyingExpression = false;
	operator std::string() const;
};

struct ClassLevelMember {
	MemberType memberType = MemberType::Unknown;
	std::string memberName = "";
	DataType dataType;
	AccessModifier accessModifier = AccessModifier::Unknown;

	bool fieldIsConstant = false, isAlgebraic = false, encounteredInheritence = false, autoconstructedField = false;
	std::optional<Expression> assignedToValue = std::nullopt;
	std::vector<std::string> workingClassSuperClassList, inheritanceClassSuperClassList{};

	std::vector<FunctionParameter> functionParameters = {};
	std::vector<EnumMember> enumMembers = {};
	ClassLevelMember() {}
	operator std::string() const;
};

class Lexer {
public:
	Lexer();
	void ParseScript(std::string filePath);
	static bool IsNumber(char ch);
	static bool IsRegularSymbolChar(char ch);
	static std::string GetAttributeListAsString(const std::vector<std::string>& attributeList);
private:
	std::vector<std::variant<ClassLevelMember, FunctionLevelInstruction>> listOfTokenizedInstructions;
	std::vector<std::string> functionLevelSymbolHistory;
	std::string currentSymbol = "", compoundOperator = "", latestSymbol = "";
	ClassLevelMember currentClassLevelMember;
	FunctionLevelInstruction functionLevelMember;
	EnumMember currentEnumMember;
	void ParseLine(std::string currentLine);
	void HandleSymbol(std::string& symbolName);
	void ParseSymbol(std::string symbolName);
	void HandleStartFunctionSymbol(std::string symbolName);
	TokenType previousToken = TokenType::None;
	FunctionParameter currentFunctionParameter, currentEnumDataParameter;
	bool assignmentEncountered = false, parametersEncountered = false, inOptionalParameter = false,
		encounteredNonTab = false, canBeClassDeclaration = false;
	int normalSymbolsParsed = 0, symbolsParsedThisLine = 0, currentNestingLevel = 0;
	std::vector<int> switchStatementNestingLevels{};
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
	bool ParseConditionalSymbol();
	void SetupConditional(ConditionalType conditonalType);
	void HandleInstructionSeperationSymbol(bool createsScope, bool arrowSyntax = false);
	void EndFunctionLevelMember(bool arrowSyntax = false);
	void ParseLoopAdjecentStatement();
	void HandleCompoundAssignmentAtFindTime();
	void HandleCompoundAssignmentAtInstructionEndTime();
	void CheckForSimpleCompoundAssignment();
	void HandleSwitchStatement();
	void HandleSwitchCases(std::string symbolName);
	bool HasExitedSwitchStatement();
	bool IsInSwitchCase();
	void IntitializeOnNewLine();
	void ParseEnumSymbol(const std::string& symbolName);
	void TerminateEnumParsing();
	void AddNewEnumMemberDataParameter();
	void ParseEnumMemberData(const std::string& symbolName, bool notInString);
	void TerminateCurrentClassLevelMember();
	std::vector<std::string>& GetActiveSuperClassList();
	bool IsCurrentStatementClassDeclaration(const std::string& symbolName);
	static const int AmountOfEqualsInClassDeclarations = 2;
	static const int ActualClassNameHistoryOffset = AmountOfEqualsInClassDeclarations + 2, BeforeClassDotSeperatorHistoryOffset = 2;
};