#include "Lexer.h"
#include "FuncDefLexer.h"
#include <iostream>
#include <algorithm>

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

void Lexer::ParseFunctionLevelSymbol(std::string symbolName) {
	if (symbolName != " " && functionLevelMember.instructionType == InstructionType::Unknown) functionLevelMember.instructionType = InstructionType::PlainExpression;
	if (IsInSwitchCase()) { HandleSwitchCases(symbolName); return; }
	if (HasExitedSwitchStatement()) { switchStatementNestingLevels.clear(); }

	bool isAssignment = DetermineIfAssigning(symbolName);
	std::string specialFunctionSymbol = symbolName;
	if (isAssignment) specialFunctionSymbol = "=";
	bool isNotInStr = functionLevelMember.underlyingExpression.NotInString();
	bool isElse = symbolName == ":" && functionLevelMember.underlyingExpression.expressionContents.size() == 1 &&
		!functionLevelMember.underlyingExpression.latestExpressionElementIsOperand &&
		functionLevelMember.underlyingExpression.latestOperator.operatorContents == "!";

	if (symbolName == "." && isNotInStr && functionLevelMember.instructionType == InstructionType::PlainExpression)
		functionLevelMember.assignmentAttributeNameList.push_back(functionLevelMember.underlyingExpression.GetLatestSymbol());

	bool colonSeperator = (symbolName == ":" && (functionLevelMember.instructionType == InstructionType::Conditional || isElse)) && isNotInStr;
	bool semicolonSeperator = symbolName == ";" && isNotInStr;
	int symbolHistorySize = functionLevelSymbolHistory.size();
	bool isLoop = symbolName == "@" && symbolHistorySize == 0, isReturnStatement = symbolName == ">" && symbolHistorySize == 0;
	bool singlelinedLoop = isNotInStr && symbolHistorySize > 0 && functionLevelSymbolHistory[symbolHistorySize - 1] == "=" && symbolName == ">";
	bool disprovingSwitch = functionLevelMember.couldBeSwitchStatement && symbolName != " " && !semicolonSeperator;
	if (singlelinedLoop) { HandleInstructionSeperationSymbol(true, true); return; }

	if (disprovingSwitch) { functionLevelMember.couldBeSwitchStatement = false; functionLevelMember.switchValue.expressionContents.clear(); }
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
	std::string latestSymbol = functionLevelMember.underlyingExpression.GetLatestSymbol();

	if (symbolName == ":") {
		functionLevelMember.variableName = latestSymbol;
		functionLevelMember.instructionType = InstructionType::Declaration;
		functionLevelMember.switchValue = functionLevelMember.underlyingExpression.expressionContents;
		functionLevelMember.couldBeSwitchStatement = true;
		functionLevelMember.underlyingExpression = Expression();
		return;
	}

	functionLevelMember.underlyingExpression = Expression();
	if (functionLevelMember.instructionType == InstructionType::Declaration) { functionLevelMember.variableDeclarationType = DataType(latestSymbol); return; }
	functionLevelMember.instructionType = InstructionType::Assignment;
	functionLevelMember.variableName = latestSymbol;

	if (functionLevelSymbolHistory.size() == 0) return;
	std::string latestAttribute = functionLevelSymbolHistory[functionLevelSymbolHistory.size() - 3]; // By last symbol it recognited assignment, second last is equals, third last is last attribute
	functionLevelMember.assignmentAttributeNameList.push_back(latestAttribute);
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

void Lexer::HandleSwitchCases(std::string symbolName) {
	functionLevelMember.instructionType = InstructionType::SwitchCase;
	bool isInStr = functionLevelMember.underlyingExpression.currentOperand.operandType != OperandType::StringLiteral;
	if (symbolName == ":" && isInStr) { HandleInstructionSeperationSymbol(true); return; }
	functionLevelMember.underlyingExpression.ParseNextSymbol(symbolName);
}

bool Lexer::HasExitedSwitchStatement() {
	for (int inspectedSwitchNestingLevel : switchStatementNestingLevels) {
		if (functionLevelMember.nestingLevel > inspectedSwitchNestingLevel) return false;
	}
	return true;
}

bool Lexer::IsInSwitchCase() {
	for (int inspectedSwitchNestingLevel : switchStatementNestingLevels) {
		if (inspectedSwitchNestingLevel + 1 == functionLevelMember.nestingLevel) return true;
	}
	return false;
}

void Lexer::HandleSwitchStatement() {
	functionLevelMember.instructionType = InstructionType::SwitchStatement;
	functionLevelMember.underlyingExpression = functionLevelMember.switchValue.expressionContents;
	functionLevelMember.switchValue.expressionContents.clear();
	switchStatementNestingLevels.push_back(currentNestingLevel);
}

void Lexer::CheckForSimpleCompoundAssignment() {
	if (functionLevelMember.instructionType != InstructionType::PlainExpression || functionLevelMember.underlyingExpression.expressionContents.size() != 1) return;
	std::variant<Operand, Operator> firstElement = functionLevelMember.underlyingExpression.expressionContents[0];
	if (!std::holds_alternative<Operand>(firstElement)) return;
	Operand firstOperand = std::get<Operand>(firstElement);
	if (firstOperand.operandType != OperandType::Variable) return;
	functionLevelMember.variableName = firstOperand.operandContents;

	std::string appropriateOperator = "";
	if (functionLevelMember.underlyingExpression.currentBinaryOperator == "++") appropriateOperator = "+";
	if (functionLevelMember.underlyingExpression.currentBinaryOperator == "--") appropriateOperator = "-";
	if (appropriateOperator == "") return;

	functionLevelMember.instructionType = InstructionType::Assignment;
	functionLevelMember.underlyingExpression.expressionContents.push_back(Operator(OperatorType::BinaryOperator, appropriateOperator));
	functionLevelMember.underlyingExpression.expressionContents.push_back(Operand(OperandType::IntegerLiteral, "1"));
}

void Lexer::HandleCompoundAssignmentAtInstructionEndTime() {
	int elementCount = functionLevelMember.underlyingExpression.expressionContents.size();
	if (elementCount > 1) {
		functionLevelMember.underlyingExpression.expressionContents.insert(functionLevelMember.underlyingExpression.expressionContents.begin(), Operator(OperatorType::LeftParenthesis));
		functionLevelMember.underlyingExpression.expressionContents.push_back(Operator(OperatorType::RightParenthesis));
	}
	functionLevelMember.underlyingExpression.expressionContents.insert(functionLevelMember.underlyingExpression.expressionContents.begin(), Operator(OperatorType::BinaryOperator, compoundOperator));
	functionLevelMember.underlyingExpression.expressionContents.insert(functionLevelMember.underlyingExpression.expressionContents.begin(), Operand(OperandType::Variable, functionLevelMember.variableName));
}

void Lexer::ParseLoopAdjecentStatement() {
	bool isLoopStatement = functionLevelMember.variableName != "" && functionLevelMember.variableDeclarationType.typeName != "" && functionLevelMember.indexVariableName != "";
	if (!isLoopStatement) {
		int expressionElements = functionLevelMember.underlyingExpression.expressionContents.size();
		functionLevelMember.loopCount = 1;
		bool encounteredLoopAbsence = false;
		std::string postLoopContents = "";
		for (int i = 0; i < expressionElements; i++) {
			std::variant<Operand, Operator> expressionElement = functionLevelMember.underlyingExpression.expressionContents[i];
			if (!std::holds_alternative<Operator>(expressionElement)) break;
			Operator currentOperator = std::get<Operator>(expressionElement);
			if (currentOperator.operatorContents == "@" && !encounteredLoopAbsence) functionLevelMember.loopCount++;
			else encounteredLoopAbsence = true;
			if (encounteredLoopAbsence) postLoopContents += currentOperator.operatorContents;
		}
		if (postLoopContents == "!") functionLevelMember.loopControlFlowType = LoopControlFlow::Break;
		if (postLoopContents == "->") functionLevelMember.loopControlFlowType = LoopControlFlow::Continue;
		functionLevelMember.instructionType = InstructionType::LoopControlFlow;
		if (functionLevelMember.loopControlFlowType != LoopControlFlow::Unknown) return;
	}
	functionLevelMember.instructionType = InstructionType::LoopStatement;
}