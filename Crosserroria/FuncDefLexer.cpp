#include "Lexer.h"
#include "FuncDefLexer.h"
#include <iostream>
#include <algorithm>

void Lexer::ParseFunctionLevelSymbol(std::string symbolName) {
	Expression& actualExpression = functionLevelMember.underlyingExpression.value();
	bool isAssignment = DetermineIfAssigning(symbolName);
	std::string specialFunctionSymbol = symbolName;
	if (isAssignment) specialFunctionSymbol = "=";
	bool isNotInStr = actualExpression.currentOperand.operandType != OperandType::StringLiteral;
	bool isElse = symbolName == ":" && actualExpression.expressionContents.size() == 1 && !actualExpression.latestExpressionElementIsOperand && actualExpression.latestOperator.operatorContents == "!";
	bool colonSeperator = (symbolName == ":" && functionLevelMember.instructionType == InstructionType::Conditional || isElse) && isNotInStr, semicolonSeperator = symbolName == ";" && isNotInStr;

	if (colonSeperator) { HandleInstructionSeperationSymbol(true); return; }
	if (semicolonSeperator) { HandleInstructionSeperationSymbol(false); return; }

	if (symbolName != " ") functionLevelSymbolHistory.push_back(symbolName);
	if (symbolName == "?" && isNotInStr) { ParseConditionalSymbol(); return; }
	if ((symbolName == ":" || isAssignment) && isNotInStr) HandleExpressionOnSpecialFunctionLevelSymbol(specialFunctionSymbol);
	if (symbolName == ":" && isNotInStr) return;

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

bool Lexer::DetermineIfAssigning(std::string symbolName) {
	bool isAssignment = false, determiningIfIsAssignment = symbolName != " " && functionLevelMember.potentiallyEncounteredAssignmentSymbol;
	if (determiningIfIsAssignment) {
		std::string symbolBeforeEqualsSymbol = "";
		int historySize = functionLevelSymbolHistory.size();
		if (historySize >= 2) symbolBeforeEqualsSymbol = functionLevelSymbolHistory[historySize - 2];
		isAssignment = symbolName != "=" && symbolBeforeEqualsSymbol != "!";
		functionLevelMember.potentiallyEncounteredAssignmentSymbol = false;
	}

	if (isAssignment) functionLevelMember.encounteredAssignment = true;
	if (symbolName == "=" && !functionLevelMember.potentiallyEncounteredAssignmentSymbol && !determiningIfIsAssignment) functionLevelMember.potentiallyEncounteredAssignmentSymbol = true;
	return isAssignment;
}

FunctionLevelInstruction::operator std::string() const {
	std::string output = "", expressionText = "", mutabilityText = isDeclaredVariableConstant ? "constant" : "mutable";
	std::string expressionTextPure = "";
	mutabilityText = ", " + mutabilityText;
	if (underlyingExpression.has_value()) expressionTextPure = "value: " + (std::string)underlyingExpression.value();
	if (expressionTextPure != "") expressionText = ", " + expressionTextPure;

	std::string conditonalTypeAsStr = "";
	switch (conditionalType) {
		case ConditionalType::IfStatement: conditonalTypeAsStr = "IF"; break;
		case ConditionalType::ElifStatement: conditonalTypeAsStr = "ELIF"; break;
		case ConditionalType::ElseStatement: conditonalTypeAsStr = "ELSE"; break;
	}

	switch (instructionType) {
		case InstructionType::PlainExpression: output = "PlainExpression[" + expressionTextPure; break;
		case InstructionType::Assignment: output = "Assignment[name: " + variableName + expressionText; break;
		case InstructionType::Declaration: output = "Declaration[name: " + variableName + ", type: " + (std::string)variableDeclarationType + mutabilityText + expressionText; break;
		case InstructionType::Conditional: output = "Conditional[type: " + conditonalTypeAsStr + expressionText; break;
	}
	output += ", nesting: " + std::to_string(nestingLevel) + "]";
	return output;
}

void Lexer::InitializeFunctionLevelMember() {
	functionLevelMember.underlyingExpression = Expression();
	functionLevelMember.instructionType = InstructionType::PlainExpression;
	functionLevelMember.isDeclaredVariableConstant = false;
	functionLevelMember.variableName = "";
	functionLevelMember.variableDeclarationType.typeName = "";
	functionLevelSymbolHistory.clear();
	functionLevelMember.conditionalType = ConditionalType::Unknown;
	functionLevelMember.nestingLevel = currentNestingLevel;
}

void Lexer::ParseConditionalSymbol() {
	int historySize = functionLevelSymbolHistory.size();
	if (historySize == 1) { SetupConditional(ConditionalType::IfStatement); return; }
	std::string firstSymbol = functionLevelSymbolHistory[0];
	if (firstSymbol == "!" && historySize == 2) { SetupConditional(ConditionalType::ElifStatement); functionLevelMember.underlyingExpression.value() = Expression(); }
}

void Lexer::SetupConditional(ConditionalType conditionalType) {
	functionLevelMember.instructionType = InstructionType::Conditional;
	functionLevelMember.conditionalType = conditionalType;
}

void Lexer::HandleInstructionSeperationSymbol(bool createsScope) {
	int previousNestingLevel = functionLevelMember.nestingLevel;
	EndFunctionLevelMember();
	InitializeFunctionLevelMember();
	functionLevelMember.nestingLevel = previousNestingLevel;
	if (createsScope) functionLevelMember.nestingLevel++;
}