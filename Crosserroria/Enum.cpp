#include "Lexer.h"
#include <iostream>

void Lexer::ParseEnumSymbol(const std::string& symbolName) {
	bool isIgnoring = symbolName == "\t" || symbolName == " ";
	bool parameterHasValue = currentEnumDataParameter.optionalValue.has_value(), underlyingHasValue = currentEnumMember.underlyingExpression.has_value();

	bool notInString = (!parameterHasValue || currentEnumDataParameter.optionalValue.value().NotInString()) &&
		(!underlyingHasValue || currentEnumMember.underlyingExpression.value().NotInString());

	bool notInParen = (!parameterHasValue || currentEnumDataParameter.optionalValue.value().currentBracketNestingLevel == 0) &&
		(!underlyingHasValue || currentEnumMember.underlyingExpression.value().currentBracketNestingLevel == 0);

	bool terminateEnumMember = (symbolName == "," && !currentEnumMember.inData || symbolName == "\n") && currentEnumMember.memberName != "" && notInString && notInParen;
	if (isIgnoring) return;

	if (terminateEnumMember) {
		AddNewEnumMemberDataParameter();
		currentClassLevelMember.enumMembers.push_back(currentEnumMember);
		currentEnumMember = EnumMember();
		return;
	}

	bool isDataSeperator = symbolName == "," && currentEnumMember.inData && notInString && notInParen;
	if (isDataSeperator) {
		AddNewEnumMemberDataParameter();
		return;
	}

	bool endingExpression = symbolName == ")" && (!currentEnumDataParameter.optionalValue.has_value() ||
		currentEnumDataParameter.optionalValue.value().currentBracketNestingLevel == 0) && notInString && notInParen;

	if (symbolName == "(" && !currentEnumMember.inData) { currentEnumMember.inData = true; currentClassLevelMember.isAlgebraic = true; return; }
	if (endingExpression) {
		currentEnumMember.inData = false;
		if (currentEnumDataParameter.optionalValue.has_value()) currentEnumDataParameter.optionalValue.value().EndExpression();
		return;
	}

	if (currentEnumMember.inUnderlyingExpression) { currentEnumMember.underlyingExpression.value().ParseNextSymbol(symbolName); return; }

	if (symbolName == "=" && !currentEnumMember.inUnderlyingExpression && !currentEnumMember.inData) {
		currentEnumMember.inUnderlyingExpression = true;
		currentEnumMember.underlyingExpression = Expression();
		return;
	}
	if (!currentEnumMember.inData) { currentEnumMember.memberName = symbolName; return; }

	ParseEnumMemberData(symbolName, notInString);
}

void Lexer::TerminateEnumParsing() {
	listOfTokenizedInstructions.push_back(currentClassLevelMember);
	currentClassLevelMember.memberType = MemberType::Unknown;
}

void Lexer::AddNewEnumMemberDataParameter() {
	if (currentEnumDataParameter.optionalValue.has_value()) currentEnumDataParameter.optionalValue.value().EndExpression();
	if (currentEnumMember.underlyingExpression.has_value()) currentEnumMember.underlyingExpression.value().EndExpression();
	if (currentEnumDataParameter.parameterName != "") currentEnumMember.associatedData.push_back(currentEnumDataParameter);
	currentEnumDataParameter.Reset();
	currentEnumMember.inExpression = false;
}

void Lexer::ParseEnumMemberData(const std::string& symbolName, bool notInString) {
	if (symbolName == "=" && currentEnumMember.inData && !currentEnumMember.inExpression && notInString) {
		currentEnumMember.inExpression = true;
		currentEnumDataParameter.optionalValue = Expression();
		return;
	}

	if (currentEnumMember.inExpression) { currentEnumDataParameter.optionalValue.value().ParseNextSymbol(symbolName); return; }
	bool isDeclaration = symbolName == ":" || symbolName == "!" && notInString;
	if (isDeclaration) { currentEnumDataParameter.isConstant = symbolName == "!"; currentEnumDataParameter.afterDeclaration = true; return; }
	if (!currentEnumDataParameter.afterDeclaration) { currentEnumDataParameter.parameterName = symbolName; return; }
	currentEnumDataParameter.parameterType = symbolName;
}