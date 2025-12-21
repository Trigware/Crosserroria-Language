#include "Lexer.h"
#include <iostream>

void Lexer::ParseEnumSymbol(const std::string& symbolName) {
	bool isIgnoring = symbolName == "\t" || symbolName == " ";
	bool terminateEnumMember = (symbolName == "," && !currentEnumMember.isData || symbolName == "\n") && currentEnumMember.memberName != "";
	if (isIgnoring) return;

	if (terminateEnumMember) {
		AddNewEnumMemberDataParameter();
		currentClassLevelMember.enumMembers.push_back(currentEnumMember);
		currentEnumMember = EnumMember();
		return;
	}

	bool isDataSeperator = symbolName == "," && currentEnumMember.isData;
	if (isDataSeperator) {
		AddNewEnumMemberDataParameter();
		return;
	}
	if (symbolName == "(" && !currentEnumMember.isData) { currentEnumMember.isData = true; return; }
	if (symbolName == ")" && currentEnumMember.isData) { currentEnumMember.isData = false; return; }
	if (!currentEnumMember.isData) { currentEnumMember.memberName = symbolName; return; }

	bool isDeclaration = symbolName == ":" || symbolName == "!";
	if (isDeclaration) { currentEnumDataParameter.isConstant = symbolName == "!"; currentEnumDataParameter.afterDeclaration = true; return; }
	if (!currentEnumDataParameter.afterDeclaration) { currentEnumDataParameter.parameterName = symbolName; return; }
	currentEnumDataParameter.parameterType = symbolName;
}

void Lexer::TerminateEnumParsing() {
	listOfTokenizedInstructions.push_back(currentClassLevelMember);
	currentClassLevelMember.memberType = MemberType::Unknown;
}

void Lexer::AddNewEnumMemberDataParameter() {
	if (currentEnumDataParameter.parameterName != "") currentEnumMember.associatedData.push_back(currentEnumDataParameter);
	currentEnumDataParameter.Reset();
}