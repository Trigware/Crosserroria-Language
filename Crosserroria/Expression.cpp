#include "Lexer.h"
#include "Expression.h"
#include <iostream>

Expression::Expression() {
	currentBinaryOperator = "";
	currentOperand.Reset();
	previouslyParsedOperand.Reset();
	currentBracketNestingLevel = 0;
	latestExpressionElementIsOperand = false;

	TerminateNumericLiteral();
	TerminateUnrecognizedSymbolChar();
}

void Expression::ParseNextSymbol(std::string symbolName) {
	for (int i = 0; i < symbolName.size(); i++) {
		ParseNextChar(symbolName[i]);
	}
}

void Expression::EndExpression() {
	TerminateNumericLiteral();
	TerminateUnrecognizedSymbolChar();
}

void Expression::ParseNextChar(char ch) {
	if (ch == ' ' && currentOperand.operandType != OperandType::StringLiteral) return;

	bool isNumberChar = Lexer::IsNumber(ch) || ch == '.' && currentOperand.operandType == OperandType::IntegerLiteral;
	bool isNumberLiteralTerminator = !isNumberChar && (currentOperand.operandType == OperandType::IntegerLiteral || currentOperand.operandType == OperandType::RealLiteral);
	bool isUnrecognizedSymbolChar = (Lexer::IsRegularSymbolChar(ch) && (!Lexer::IsNumber(ch) || currentOperand.operandType == OperandType::UnrecognizedSymbol)) && currentOperand.operandType != OperandType::StringLiteral;
	bool isUnrecognizedSymbolTerminator = !isUnrecognizedSymbolChar && currentOperand.operandType == OperandType::UnrecognizedSymbol;

	if (ch == '"') { ParseApostrophe(); EndBinaryOperator(); return; }
	if (currentOperand.operandType == OperandType::StringLiteral) { currentOperand.operandContents += ch; return; }

	if (isNumberChar) { ParseNumberChar(ch); EndBinaryOperator(); return; }
	if (isNumberLiteralTerminator) TerminateNumericLiteral();
	if (isUnrecognizedSymbolChar) { ParseUnrecognizedSymbolChar(ch); EndBinaryOperator(); return; }
	if (isUnrecognizedSymbolTerminator) TerminateUnrecognizedSymbolChar();

	if (ch == ',') { expressionContents.push_back(Operator(OperatorType::FunctionParameterSeperator)); return; }
	if (ch == '(') { ParseLeftParenthesis(); return; }
	if (ch == ')') { ParseRightParenthesis(); return; }

	if (previouslyParsedOperand.operandType == OperandType::Unknown) { AddOperator(OperatorType::LeftUnaryOperator, std::string(1, ch)); return; }

	currentBinaryOperator += ch;
}

void Expression::ParseLeftParenthesis() {
	currentBracketNestingLevel++;
	EndBinaryOperator();
	bool functionCall = previouslyParsedOperand.operandType == OperandType::Variable && latestExpressionElementIsOperand;
	leftParenCallsStack.push(functionCall);
	if (functionCall) { ParseFunctionCall(); return; }

	AddOperator(OperatorType::LeftParenthesis, "");
	previouslyParsedOperand.operandType = OperandType::Unknown;
}

void Expression::ParseRightParenthesis() {
	currentBracketNestingLevel--;
	bool isLatestLeftParenthesisFunctionCall = leftParenCallsStack.top();
	leftParenCallsStack.pop();
	OperatorType parenthesisType = isLatestLeftParenthesisFunctionCall ? OperatorType::FunctionClosing : OperatorType::RightParenthesis;

	AddOperator(parenthesisType, "");
	previouslyParsedOperand.operandType = OperandType::ParenthesisContents;
}

void Expression::EndBinaryOperator() {
	if (currentBinaryOperator == "") return;
	if (currentBinaryOperator == "!") { constantDeclarationEncountered = true; return; }
	std::string currentLengthOperator = "";
	int longestValidOpLength = 0;
	for (int i = 0; i < currentBinaryOperator.size(); i++) {
		char ch = currentBinaryOperator[i];
		currentLengthOperator += ch;
		if (std::find(validBinaryOperators.begin(), validBinaryOperators.end(), currentLengthOperator) != validBinaryOperators.end()) longestValidOpLength = i + 1;
	}

	std::string longestValidOperator = currentBinaryOperator.substr(0, longestValidOpLength);
	if (longestValidOpLength > 0) {
		AddOperator(OperatorType::BinaryOperator, longestValidOperator);
		for (int i = longestValidOpLength; i < currentBinaryOperator.size(); i++) {
			char ch = currentLengthOperator[i];
			AddOperator(OperatorType::LeftUnaryOperator, std::string(1, ch));
		};
	}

	currentBinaryOperator = "";
}

void Expression::ParseFunctionCall() {
	int endIndex = expressionContents.size() - 1;
	expressionContents[endIndex] = Operand(OperandType::FunctionCall, previouslyParsedOperand.operandContents);
	previouslyParsedOperand.operandType = OperandType::FunctionCall;
}

void Expression::AddOperator(OperatorType newOpType, std::string newOpContents) {
	Operator newOperator(newOpType, newOpContents);
	latestOperator = newOperator;
	expressionContents.push_back(newOperator);
	latestExpressionElementIsOperand = false;
}

void Expression::AddOperand() {
	previouslyParsedOperand = currentOperand;
	expressionContents.push_back(previouslyParsedOperand);
	currentOperand.Reset();
	latestExpressionElementIsOperand = true;
}

void Expression::ParseApostrophe() {
	if (currentOperand.operandType == OperandType::Unknown) {
		currentOperand.operandType = OperandType::StringLiteral;
		return;
	}
	AddOperand();
}

void Expression::ParseNumberChar(char ch) {
	currentOperand.operandContents += ch;
	if (currentOperand.operandType == OperandType::Unknown) currentOperand.operandType = OperandType::IntegerLiteral;
	if (ch == '.') currentOperand.operandType = OperandType::RealLiteral;
}

void Expression::TerminateNumericLiteral() {
	if (currentOperand.operandContents == "" || !(currentOperand.operandType == OperandType::IntegerLiteral || currentOperand.operandType == OperandType::RealLiteral)) return;
	AddOperand();
}

void Expression::ParseUnrecognizedSymbolChar(char ch) {
	currentOperand.operandType = OperandType::UnrecognizedSymbol;
	currentOperand.operandContents += ch;
}

void Expression::TerminateUnrecognizedSymbolChar() {
	if (currentOperand.operandContents == "") return;
	bool isTypeLiteral = latestOperator.operatorContents == "->" || latestOperator.operatorContents == "?>";
	if (isTypeLiteral) { currentOperand.operandType = OperandType::TypeLiteral; AddOperand(); return; }
	bool isBooleanLiteral = currentOperand.operandContents == "T" || currentOperand.operandContents == "F";
	if (isBooleanLiteral) { currentOperand.operandType = OperandType::BooleanLiteral; AddOperand(); return; }
	currentOperand.operandType = OperandType::Variable;
	AddOperand();
}

void Operand::Reset() {
	operandType = OperandType::Unknown;
	operandContents = "";
}

void DataType::Reset() { typeName = ""; }
void FunctionParameter::Reset() {
	parameterType.Reset();
	parameterName = "";
	optionalValue = std::nullopt;
}