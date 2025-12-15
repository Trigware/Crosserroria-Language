#include "Lexer.h"
#include "FuncDefLexer.h"
#include <iostream>
#include <algorithm>

void Lexer::ParseFunctionLevelSymbol(std::string symbolName) {
	if (functionLevelMember.instructionType == InstructionType::Unknown) {
		functionLevelMember.underlyingExpression = Expression();
		functionLevelMember.instructionType = InstructionType::PlainExpression;
		functionLevelSymbolHistory.clear();
	}

	Expression& actualExpression = functionLevelMember.underlyingExpression.value();
	bool isAssignment = false, determiningIfIsAssignment = symbolName != " " && functionLevelMember.potentiallyEncounteredAssignmentSymbol;
	if (determiningIfIsAssignment) {
		std::string symbolBeforeEqualsSymbol = "";
		int historySize = functionLevelSymbolHistory.size();
		if (historySize >= 2) symbolBeforeEqualsSymbol = functionLevelSymbolHistory[historySize - 2];
		functionLevelSymbolHistory.clear();
		isAssignment = symbolName != "=" && symbolBeforeEqualsSymbol != "!";
		functionLevelMember.potentiallyEncounteredAssignmentSymbol = false;
	}

	if (isAssignment) functionLevelMember.encounteredAssignment = true;
	if (symbolName == "=" && !functionLevelMember.potentiallyEncounteredAssignmentSymbol && !determiningIfIsAssignment) functionLevelMember.potentiallyEncounteredAssignmentSymbol = true;
	std::string specialFunctionSymbol = symbolName;
	if (isAssignment) specialFunctionSymbol = "=";

	if (symbolName != " ") functionLevelSymbolHistory.push_back(symbolName);
	if (symbolName == ":" || isAssignment) HandleExpressionOnSpecialFunctionLevelSymbol(specialFunctionSymbol);
	if (symbolName == ":") return;

	actualExpression.ParseNextSymbol(symbolName);
	if (actualExpression.constantDeclarationEncountered) HandleConstantDeclaration(actualExpression);
}

void Lexer::HandleExpressionOnSpecialFunctionLevelSymbol(std::string symbolName) {
	Expression& instructionExpression = functionLevelMember.underlyingExpression.value();
	instructionExpression.EndExpression();
	std::string latestSymbol = instructionExpression.previouslyParsedOperand.operandContents;
	instructionExpression = Expression();

	if (symbolName == ":" || symbolName == "!") {
		functionLevelMember.instructionType = InstructionType::Declaration;
		functionLevelMember.variableName = DataType(latestSymbol);
		functionLevelMember.isDeclaredVariableConstant = symbolName == "!";
		return;
	}

	if (functionLevelMember.instructionType == InstructionType::Declaration) { functionLevelMember.variableDeclarationType = DataType(latestSymbol); return; }
	functionLevelMember.instructionType = InstructionType::Assignment;
	functionLevelMember.variableName = DataType(latestSymbol);
}

void Lexer::CorrectDeclarationWithoutValue() {
	HandleExpressionOnSpecialFunctionLevelSymbol("=");
	functionLevelMember.underlyingExpression = std::nullopt;
}

void Lexer::HandleConstantDeclaration(Expression& actualExpression) {
	functionLevelMember.instructionType = InstructionType::Declaration;
	functionLevelMember.isDeclaredVariableConstant = true;
	functionLevelMember.variableName = actualExpression.previouslyParsedOperand.operandContents;
	actualExpression.constantDeclarationEncountered = false;
}

FunctionLevelInstruction::operator std::string() const {
	std::string output = "", expressionText = "", mutabilityText = isDeclaredVariableConstant ? "constant" : "mutable";
	std::string expressionTextPure = "";
	mutabilityText = ", " + mutabilityText;
	if (underlyingExpression.has_value()) expressionTextPure = "value: " + (std::string)underlyingExpression.value();
	expressionText = ", " + expressionTextPure;

	switch (instructionType) {
		case InstructionType::PlainExpression: output = "PlainExpression[" + expressionTextPure; break;
		case InstructionType::Assignment: output = "Assignment[name: " + variableName + expressionText; break;
		case InstructionType::Declaration: output = "Declaration[name: " + variableName + ", type: " + (std::string)variableDeclarationType + mutabilityText + expressionText; break;
	}
	output += ", nesting: " + std::to_string(nestingLevel) + "]";
	return output;
}