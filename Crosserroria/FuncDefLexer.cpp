#include "Lexer.h"
#include "FuncDefLexer.h"
#include <iostream>
#include <algorithm>

void Lexer::ParseFunctionLevelSymbol(std::string symbolName) {
	if (symbolName != " " && functionLevelMember.instructionType == InstructionType::Unknown) functionLevelMember.instructionType = InstructionType::PlainExpression;

	bool isAssignment = DetermineIfAssigning(symbolName);
	std::string specialFunctionSymbol = symbolName;
	if (isAssignment) specialFunctionSymbol = "=";
	bool isNotInStr = functionLevelMember.underlyingExpression.currentOperand.operandType != OperandType::StringLiteral;
	bool isElse = symbolName == ":" && functionLevelMember.underlyingExpression.expressionContents.size() == 1 &&
		!functionLevelMember.underlyingExpression.latestExpressionElementIsOperand &&
		functionLevelMember.underlyingExpression.latestOperator.operatorContents == "!";

	bool colonSeperator = (symbolName == ":" && (functionLevelMember.instructionType == InstructionType::Conditional || isElse)) && isNotInStr;
	bool semicolonSeperator = symbolName == ";" && isNotInStr;
	int symbolHistorySize = functionLevelSymbolHistory.size();
	bool isLoop = symbolName == "@" && symbolHistorySize == 0, isReturnStatement = symbolName == ">" && symbolHistorySize == 0;
	bool singlelinedLoop = isNotInStr && symbolHistorySize > 0 && functionLevelSymbolHistory[symbolHistorySize - 1] == "=" && symbolName == ">";
	if (singlelinedLoop) { HandleInstructionSeperationSymbol(true, true); return; }

	if (symbolName != " ") functionLevelSymbolHistory.push_back(symbolName);
	if (isReturnStatement) { functionLevelMember.instructionType = InstructionType::ReturnStatement; return; }
	if (symbolName == "," && functionLevelMember.underlyingExpression.currentBracketNestingLevel == 0 && functionLevelMember.isWrappedInLoop && isNotInStr) { functionLevelMember.isInLoopIndexVariableName = true; return; }
	if (isLoop) { functionLevelMember.isWrappedInLoop = true; return; }
	if (colonSeperator) { HandleInstructionSeperationSymbol(true); return; }
	if (semicolonSeperator) { HandleInstructionSeperationSymbol(false); return; }
	if (symbolName != " " && functionLevelMember.isInLoopIndexVariableName) { functionLevelMember.indexVariableName = symbolName; functionLevelMember.isInLoopIndexVariableName = false; return; }

	if (symbolName == "?" && isNotInStr && ParseConditionalSymbol()) return;
	if ((symbolName == ":" || isAssignment) && isNotInStr) HandleExpressionOnSpecialFunctionLevelSymbol(specialFunctionSymbol);
	if (symbolName == ":" && isNotInStr) return;

	functionLevelMember.underlyingExpression.ParseNextSymbol(symbolName);
	if (functionLevelMember.underlyingExpression.constantDeclarationEncountered) HandleConstantDeclaration();
}

void Lexer::HandleExpressionOnSpecialFunctionLevelSymbol(std::string symbolName) {
	functionLevelMember.underlyingExpression.EndExpression();
	std::string latestSymbol = functionLevelMember.underlyingExpression.previouslyParsedOperand.operandContents;
	functionLevelMember.underlyingExpression = Expression();

	if (symbolName == ":") {
		functionLevelMember.variableName = DataType(latestSymbol);
		functionLevelMember.instructionType = InstructionType::Declaration;
		return;
	}

	if (functionLevelMember.instructionType == InstructionType::Declaration) { functionLevelMember.variableDeclarationType = DataType(latestSymbol); return; }
	functionLevelMember.instructionType = InstructionType::Assignment;
	functionLevelMember.variableName = DataType(latestSymbol);
}

void Lexer::CorrectDeclarationWithoutValue() {
	HandleExpressionOnSpecialFunctionLevelSymbol("=");
	functionLevelMember.isUninitialized = true;
}

void Lexer::HandleConstantDeclaration() {
	functionLevelMember.instructionType = InstructionType::Declaration;
	functionLevelMember.isDeclaredVariableConstant = true;
	functionLevelMember.variableName = functionLevelMember.underlyingExpression.previouslyParsedOperand.operandContents;
	functionLevelMember.underlyingExpression.constantDeclarationEncountered = false;
}

bool Lexer::DetermineIfAssigning(std::string symbolName) {
	bool isAssignment = false, determiningIfIsAssignment = symbolName != " " && functionLevelMember.potentiallyEncounteredAssignmentSymbol;
	if (determiningIfIsAssignment) {
		std::string symbolBeforeEqualsSymbol = "";
		int historySize = functionLevelSymbolHistory.size();
		if (historySize >= 2) symbolBeforeEqualsSymbol = functionLevelSymbolHistory[historySize - 2];
		isAssignment = symbolName != "=" && symbolBeforeEqualsSymbol != "!";

		bool isValidBinOp = false;
		isValidBinOp = std::find(functionLevelMember.underlyingExpression.validBinaryOperators.begin(),
			functionLevelMember.underlyingExpression.validBinaryOperators.end(),
			functionLevelMember.underlyingExpression.currentBinaryOperator) !=
			functionLevelMember.underlyingExpression.validBinaryOperators.end();

		if (isValidBinOp) isAssignment = false;
		functionLevelMember.potentiallyEncounteredAssignmentSymbol = false;
	}

	if (isAssignment) functionLevelMember.encounteredAssignment = true;
	if (isAssignment && functionLevelMember.underlyingExpression.currentBinaryOperator != "=") HandleCompoundAssignmentAtFindTime();
	if (symbolName == "=" && !functionLevelMember.potentiallyEncounteredAssignmentSymbol && !determiningIfIsAssignment) functionLevelMember.potentiallyEncounteredAssignmentSymbol = true;
	return isAssignment;
}

void Lexer::HandleCompoundAssignmentAtFindTime() {
	std::string assignmentOp = functionLevelMember.underlyingExpression.currentBinaryOperator;
	compoundOperator = assignmentOp.substr(0, assignmentOp.size()-1);
}

FunctionLevelInstruction::operator std::string() const {
	std::string output = "", expressionText = "", mutabilityText = isDeclaredVariableConstant ? "constant" : "mutable", uninitializedStr = "", iterNameStr = "", iterTypeStr = "", indexVarNameStr = "";
	std::string expressionTextPure = "";
	mutabilityText = ", " + mutabilityText;
	if (underlyingExpression.expressionContents.size() > 0) expressionTextPure = "value: " + (std::string)underlyingExpression;
	if (expressionTextPure != "") expressionText = ", " + expressionTextPure;
	if (isUninitialized) uninitializedStr = ", uninitialized";
	if (variableName != "") iterNameStr = ", iteratorName: " + variableName;
	if (variableDeclarationType.typeName != "") iterTypeStr = ", iteratorType: " + (std::string)variableDeclarationType;
	if (indexVariableName != "") indexVarNameStr = ", indexVariableName: " + indexVariableName;

	std::string conditonalTypeAsStr = "";
	switch (conditionalType) {
		case ConditionalType::IfStatement: conditonalTypeAsStr = "IF"; break;
		case ConditionalType::ElifStatement: conditonalTypeAsStr = "ELIF"; break;
		case ConditionalType::ElseStatement: conditonalTypeAsStr = "ELSE"; break;
	}

	std::string loopControlFlowAsStr = "";
	switch (loopControlFlowType) {
		case LoopControlFlow::Continue: loopControlFlowAsStr = "CONTINUE"; break;
		case LoopControlFlow::Break: loopControlFlowAsStr = "BREAK"; break;
	}

	std::string returnValueMessage = expressionTextPure;
	if (returnValueMessage == "") returnValueMessage = "void";

	switch (instructionType) {
		case InstructionType::PlainExpression: output = "PlainExpression[" + expressionTextPure; break;
		case InstructionType::Assignment: output = "Assignment[name: " + variableName + expressionText; break;
		case InstructionType::Declaration: output = "Declaration[name: " + variableName + ", type: " + (std::string)variableDeclarationType + mutabilityText + expressionText + uninitializedStr; break;
		case InstructionType::Conditional: output = "Conditional[type: " + conditonalTypeAsStr + expressionText; break;
		case InstructionType::LoopStatement: output = "Loop[" + expressionTextPure + iterNameStr + iterTypeStr + indexVarNameStr; break;
		case InstructionType::LoopControlFlow: output = "LoopCF[type: " + loopControlFlowAsStr + ", loopCount: " + std::to_string(loopCount); break;
		case InstructionType::ReturnStatement: output = "Return[" + returnValueMessage; break;
	}
	output += ", nesting: " + std::to_string(nestingLevel) + "]";
	return output;
}

void Lexer::InitializeFunctionLevelMember() {
	functionLevelMember = FunctionLevelInstruction();
	functionLevelSymbolHistory.clear();
	functionLevelMember.nestingLevel = currentNestingLevel;
	compoundOperator = "";
}

FunctionLevelInstruction::FunctionLevelInstruction() {
	underlyingExpression = Expression();
	instructionType = InstructionType::Unknown;
	isDeclaredVariableConstant = false;
	variableName = "";
	variableDeclarationType = DataType();
	conditionalType = ConditionalType::Unknown;
}

bool Lexer::ParseConditionalSymbol() {
	int historySize = functionLevelSymbolHistory.size();
	if (historySize == 1) { SetupConditional(ConditionalType::IfStatement); return true; }
	std::string firstSymbol = functionLevelSymbolHistory[0];
	if (firstSymbol == "!" && historySize == 2) { SetupConditional(ConditionalType::ElifStatement); functionLevelMember.underlyingExpression = Expression(); return true; }
	return false;
}

void Lexer::SetupConditional(ConditionalType conditionalType) {
	functionLevelMember.instructionType = InstructionType::Conditional;
	functionLevelMember.conditionalType = conditionalType;
}

void Lexer::HandleInstructionSeperationSymbol(bool createsScope, bool arrowSyntax) {
	int previousNestingLevel = functionLevelMember.nestingLevel;
	EndFunctionLevelMember(arrowSyntax);
	InitializeFunctionLevelMember();
	functionLevelMember.nestingLevel = previousNestingLevel;
	if (createsScope) functionLevelMember.nestingLevel++;
}