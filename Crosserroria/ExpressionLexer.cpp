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

	if (ch == '.' && currentOperand.operandType == OperandType::RealLiteral) { RangeDotOperator(); return; }
	if (ch == '"') { ParseApostrophe(); EndBinaryOperator(); return; }
	if (currentOperand.operandType == OperandType::StringLiteral) { currentOperand.operandContents += ch; return; }
	if (ch == '\t') return;

	if (isNumberChar) { ParseNumberChar(ch); EndBinaryOperator(); return; }
	if (isNumberLiteralTerminator) TerminateNumericLiteral();
	if (isUnrecognizedSymbolChar) { ParseUnrecognizedSymbolChar(ch); EndBinaryOperator(); return; }
	if (isUnrecognizedSymbolTerminator) TerminateUnrecognizedSymbolChar();

	if (ch == ',') { expressionContents.push_back(Operator(OperatorType::FunctionParameterSeperator)); return; }
	if (ch == '[' || ch == ']') { ParseOpeningSquareBrackets(ch == '['); return; }
	if (ch == '(' || ch == '{') { ParseLeftParenthesis(ch == '('); return; }
	if (ch == ')' || ch == '}') { ParseRightParenthesis(ch == ')'); return; }

	if (previouslyParsedOperand.operandType == OperandType::Unknown) { AddOperator(OperatorType::LeftUnaryOperator, std::string(1, ch)); return; }

	currentBinaryOperator += ch;
}

void Expression::ParseOpeningSquareBrackets(bool isOpening) {
	EndBinaryOperator();
	bool isArray = !latestExpressionElementIsOperand || latestOperator.operatorType == OperatorType::LeftUnaryOperator;
	if (latestExpressionElementIsOperand && !isArray)
		isArray = previouslyParsedOperand.operandType == OperandType::FunctionCall || previouslyParsedOperand.operandType == OperandType::InitializerCall;

	if (isOpening) squareBracketIsArrayStack.push(isArray);
	else {
		isArray = squareBracketIsArrayStack.top();
		squareBracketIsArrayStack.pop();
	}

	OperatorType newOperator = isArray ? OperatorType::ArrayOpen : OperatorType::IndexerOpen;
	if (!isOpening) newOperator = isArray ? OperatorType::ArrayClose : OperatorType::IndexerClose;
	currentBracketNestingLevel += isOpening ? 1 : -1;

	if (isOpening) leftParenCallsStack.push(false);
	else leftParenCallsStack.pop();

	AddOperator(newOperator, "");
}

void Expression::ParseCurlyBraces(bool isOpening) {
	OperatorType newOperator = isOpening ? OperatorType::MapOpen : OperatorType::MapClose;
	AddOperator(newOperator, "");
}

void Expression::ParseLeftParenthesis(bool regularCall) {
	currentBracketNestingLevel++;
	EndBinaryOperator();
	bool functionCall = latestExpressionElementIsOperand && previouslyParsedOperand.operandType == OperandType::Variable;
	leftParenCallsStack.push(functionCall);
	if (functionCall) { ParseFunctionCall(regularCall); return; }

	previouslyParsedOperand.operandType = OperandType::Unknown;
	if (!regularCall) { ParseCurlyBraces(true); return; }
	AddOperator(OperatorType::LeftParenthesis, "");
}

void Expression::ParseRightParenthesis(bool regularCall) {
	currentBracketNestingLevel--;
	bool isLatestLeftParenthesisFunctionCall = leftParenCallsStack.top();
	leftParenCallsStack.pop();
	OperatorType closingType = regularCall ? OperatorType::FunctionClosing : OperatorType::InitializerClosing;
	OperatorType parenthesisType = isLatestLeftParenthesisFunctionCall ? closingType : OperatorType::RightParenthesis;

	previouslyParsedOperand.operandType = OperandType::ParenthesisContents;
	if (!regularCall && !isLatestLeftParenthesisFunctionCall) { ParseCurlyBraces(false); return; }
	AddOperator(parenthesisType, "");
}

void Expression::RangeDotOperator() {
	bool isLastLiteralInteger = false;
	int currentOperandSize = currentOperand.operandContents.size();
	currentBinaryOperator = ".";
	if (currentOperand.operandContents != "") isLastLiteralInteger = currentOperand.operandContents[currentOperandSize - 1] == '.';
	if (isLastLiteralInteger) {
		currentOperand.operandContents = currentOperand.operandContents.substr(0, currentOperandSize - 1);
		currentOperand.operandType = OperandType::IntegerLiteral;
		currentBinaryOperator = "..";
	}
	TerminateNumericLiteral();
}

void Expression::EndBinaryOperator() {
	if (currentBinaryOperator == "") return;
	if (currentBinaryOperator == "!" && ternaryConditionalNestingLevel == 0) { constantDeclarationEncountered = true; currentBinaryOperator = ""; return; }
	if (currentBinaryOperator == ".") { AddOperator(OperatorType::AttributeAccess, ""); currentBinaryOperator = ""; return; }

	std::string currentLengthOperator = "";
	int longestValidOpLength = 0;
	for (int i = 0; i < currentBinaryOperator.size(); i++) {
		char ch = currentBinaryOperator[i];
		currentLengthOperator += ch;
		if (std::find(validBinaryOperators.begin(), validBinaryOperators.end(), currentLengthOperator) != validBinaryOperators.end()) longestValidOpLength = i + 1;
	}

	std::string longestValidOperator = currentBinaryOperator.substr(0, longestValidOpLength);
	if (longestValidOpLength > 0) {
		OperatorType typeOfOperator = OperatorType::BinaryOperator;
		std::string usedOperatorName = longestValidOperator;
		if (longestValidOperator == "?") typeOfOperator = OperatorType::TernaryOperatorValueOnSuccess;
		if (longestValidOperator == "!") typeOfOperator = OperatorType::TernaryOperatorValueOnFail;
		if (typeOfOperator != OperatorType::BinaryOperator) {
			usedOperatorName = "";
			ternaryConditionalNestingLevel += typeOfOperator == OperatorType::TernaryOperatorValueOnSuccess ? 1 : -1;
		}
		AddOperator(typeOfOperator, usedOperatorName);

		for (int i = longestValidOpLength; i < currentBinaryOperator.size(); i++) {
			char ch = currentLengthOperator[i];
			AddOperator(OperatorType::LeftUnaryOperator, std::string(1, ch));
		};
	}

	currentBinaryOperator = "";
}

void Expression::ParseFunctionCall(bool regularCall) {
	int endIndex = expressionContents.size() - 1;
	OperandType operand = regularCall ? OperandType::FunctionCall : OperandType::InitializerCall;
	expressionContents[endIndex] = Operand(operand, previouslyParsedOperand.operandContents);
	previouslyParsedOperand.operandType = operand;
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
	*this = FunctionParameter();
}

bool Expression::NotInString() {
	return currentOperand.operandType != OperandType::StringLiteral;
}

std::string Expression::GetLatestSymbol() {
	return previouslyParsedOperand.operandContents;
}