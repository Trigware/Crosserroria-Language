#include "Lexer.h"

void Lexer::ParseFunctionSymbol(bool specialSymbol, std::string symbolName) {
	if (!parametersEncountered) {
		switch (previousToken) {
			case TokenType::AccessModifier:
				if (symbolName == "@") currentClassLevelMember.encounteredIterator = true;
				else { currentClassLevelMember.primaryMember.memberName = symbolName; previousToken = TokenType::MemberName; }
				return;
			case TokenType::MemberName: parametersEncountered = true; previousToken = TokenType::BeforeParameterName; return;
		}
	}

	bool isDeclaration = symbolName == ":" || symbolName == "!";
	if (inOptionalParameter) {
		ParseOptionalParameterInFunctionSignature(symbolName, isDeclaration);
		return;
	}

	if (symbolName == ",") { ParseFunctionParameter(); return; }
	if (symbolName == " ") return;
	if (symbolName == "=") { inOptionalParameter = true; currentFunctionParameter.optionalValue = Expression(); return; }
	if (isDeclaration) { previousToken = TokenType::AfterParameterName; currentFunctionParameter.isConstant = symbolName == "!"; return; }

	if (inOptionalParameter) return;
	if (previousToken == TokenType::BeforeParameterName) currentFunctionParameter.parameterName = symbolName;
	if (previousToken == TokenType::AfterParameterName) {
		if (symbolName != ")") { currentFunctionParameter.parameterType = symbolName; return; }
	}
}

void Lexer::ParseOptionalParameterInFunctionSignature(const std::string& symbolName, bool& isDeclaration) {
	Expression& currentParameterOptionalValue = currentFunctionParameter.optionalValue.value();
	Operand currentExpressionOperand = currentParameterOptionalValue.currentOperand;
	bool noNesting = currentParameterOptionalValue.currentBracketNestingLevel == 0, inString = currentExpressionOperand.operandType == OperandType::StringLiteral;

	if (symbolName == ":" && !noNesting && !inString) {
		bool isLatestParenCall = currentParameterOptionalValue.leftParenCallsStack.top();
		OperatorType newOperatorType = isLatestParenCall ? OperatorType::SpecificFunctionParameter : OperatorType::MapKeyName;
		currentParameterOptionalValue.AddOperator(newOperatorType, currentParameterOptionalValue.currentOperand.operandContents);
		currentParameterOptionalValue.currentOperand.Reset();
		return;
	}

	if (noNesting && !inString && symbolName == ")") return;
	if (noNesting && !inString && symbolName == ",") { ParseFunctionParameter(); return; }
	currentParameterOptionalValue.ParseNextSymbol(symbolName);
	if (currentParameterOptionalValue.ternaryConditionalNestingLevel > 0) isDeclaration = false;
	if (inString || currentParameterOptionalValue.currentBracketNestingLevel > 0) return;
}

void Lexer::ParseFunctionParameter() {
	if (currentFunctionParameter.parameterName == "" || currentFunctionParameter.parameterType.typeName == "") return;
	previousToken = TokenType::BeforeParameterName;
	inOptionalParameter = false;
	if (currentFunctionParameter.optionalValue.has_value()) currentFunctionParameter.optionalValue.value().EndExpression();
	FunctionParameter pushedParameter(currentFunctionParameter);
	currentClassLevelMember.functionParameters.push_back(currentFunctionParameter);
	currentFunctionParameter.Reset();
}

FunctionParameter::FunctionParameter(const FunctionParameter& parameter) {
	parameterType = parameter.parameterType;
	parameterName = parameter.parameterName;
	optionalValue = std::nullopt;
	isConstant = parameter.isConstant;
	if (parameter.optionalValue.has_value()) optionalValue = parameter.optionalValue.value();
}

void Lexer::HandleStartFunctionSymbol(std::string symbolName) {
	size_t underscorePosition = symbolName.find('_');
	currentClassLevelMember.memberType = MemberType::Function;
	if (underscorePosition == std::string::npos) { currentClassLevelMember.primaryMember.dataType = symbolName; return; }

	std::string functionReturnType = symbolName.substr(0, underscorePosition);
	std::string functionName = symbolName.substr(underscorePosition + 1);
	currentClassLevelMember.primaryMember.dataType = functionReturnType;
	currentClassLevelMember.primaryMember.memberName = functionName;
	currentClassLevelMember.accessModifier = AccessModifier::Private;
	parametersEncountered = true;
	previousToken = TokenType::BeforeParameterName;
}