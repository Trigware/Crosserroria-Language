#include <fstream>
#include <string>
#include <algorithm>
#include "Lexer.h"
#include "FuncDefLexer.h"

Lexer::Lexer() {
	ParseScript("Program.crr");
}

bool Lexer::IsNumber(char ch) { return ch >= 48 && ch <= 57; }
bool Lexer::IsRegularSymbolChar(char ch) { return isalpha(ch) || ch == '_' || IsNumber(ch); }

void Lexer::ParseScript(std::string filePath) {
	listOfTokenizedInstructions.clear();
	std::ifstream file(filePath);
	std::string currentLine = "";
	int i = 0;
	while (std::getline(file, currentLine)) {
		ParseLine(currentLine);
		i++;
	}
	if (currentClassLevelMember.memberType == MemberType::Enum) TerminateEnumParsing();
	file.close();
	PrintTokenizedInstructions();
}

void Lexer::IntitializeOnNewLine() {
	currentClassLevelMember = ClassLevelMember();
	currentSymbol = "";
	previousToken = TokenType::None;
	assignmentEncountered = parametersEncountered = inOptionalParameter = false;
	currentFunctionParameter.Reset();
	currentEnumDataParameter.Reset();

	normalSymbolsParsed = symbolsParsedThisLine = currentNestingLevel = 0;
}

void Lexer::ParseLine(std::string currentLine) {
	if (currentClassLevelMember.memberType != MemberType::Enum) IntitializeOnNewLine();
	currentNestingLevel = 0;
	encounteredNonTab = false;

	bool normalSymbol = true, currentSymbolNumber = false;
	for (int i = 0; i < currentLine.length(); i++) {
		char ch = currentLine[i];
		if (!encounteredNonTab && ch == '\t') {
			currentNestingLevel++;
			InitializeFunctionLevelMember();
			continue;
		}
		encounteredNonTab = true;
		bool isRegularCharacter = IsRegularSymbolChar(ch), currentCharNum = Lexer::IsNumber(ch);
		if (currentSymbol == "") { normalSymbol = !isRegularCharacter; currentSymbolNumber = currentCharNum; }
		if (currentSymbolNumber && currentCharNum) isRegularCharacter = true;
		currentSymbol += ch;
		if (!isRegularCharacter || ch == ' ') HandleSymbol(currentSymbol);
	}

	HandleSymbol(currentSymbol);
	bool inEnum = currentClassLevelMember.memberType == MemberType::Enum;
	if (currentNestingLevel == 1 && inEnum) ParseEnumSymbol("\n");
	if (currentNestingLevel != 1 && inEnum) TerminateEnumParsing();

	switch (currentClassLevelMember.memberType) {
		case MemberType::Field:
			if (currentClassLevelMember.assignedToValue.has_value()) currentClassLevelMember.assignedToValue.value().EndExpression(); break;
		case MemberType::Function: ParseFunctionParameter(); break;
	}

	bool validClassLevelMember = currentNestingLevel == 0 && currentClassLevelMember.memberType != MemberType::Unknown;
	if (validClassLevelMember && latestSymbol == ":") { currentClassLevelMember.memberType = MemberType::Enum; currentEnumMember = EnumMember(); return; }
	if (validClassLevelMember) listOfTokenizedInstructions.push_back(currentClassLevelMember);
	if (currentNestingLevel == 0) return;
	EndFunctionLevelMember();
}

void Lexer::HandleSymbol(std::string& symbolName) {
	if (symbolName == "") return;
	int symbolNameLen = symbolName.length();
	char lastChar = symbolName[symbolNameLen - 1]; std::string lastCharAsStr{lastChar};
	bool containsSpecialChar = symbolNameLen > 1 && !IsRegularSymbolChar(lastChar);
	if (containsSpecialChar) symbolName = symbolName.substr(0, symbolNameLen-1);
	bool firstCharUnderscore = symbolNameLen > 1 && symbolName[0] == '_';
	if (firstCharUnderscore) {
		ParseSymbol("_");
		symbolName = symbolName.substr(1);
	}
	ParseSymbol(symbolName);
	if (containsSpecialChar) ParseSymbol(lastCharAsStr);
	symbolName = "";
}

void Lexer::ParseSymbol(std::string symbolName) {
	latestSymbol = symbolName;
	if (currentClassLevelMember.memberType == MemberType::Enum) { ParseEnumSymbol(symbolName); return; }
	if (currentNestingLevel > 0) { ParseFunctionLevelSymbol(symbolName); return; }
	if (assignmentEncountered) { (*currentClassLevelMember.assignedToValue).ParseNextSymbol(symbolName); return; }
	bool specialSymbol = !IsRegularSymbolChar(symbolName[0]) || symbolName == "_";
	bool isAccessModifier = symbolName == "_" || symbolName == "^";
	if (!specialSymbol) normalSymbolsParsed++;
	symbolsParsedThisLine++;

	if (normalSymbolsParsed == 1 && !specialSymbol && currentClassLevelMember.memberType == MemberType::Unknown) {
		switch (previousToken) {
			case TokenType::AccessModifier: currentClassLevelMember.memberType = MemberType::Field; break;
			case TokenType::None: HandleStartFunctionSymbol(symbolName); return;
		}
	}
	if (symbolsParsedThisLine == 1 && symbolName == "?") { currentClassLevelMember.memberType = MemberType::Function; currentClassLevelMember.dataType = DataType("?"); return; }

	if (isAccessModifier) {
		previousToken = TokenType::AccessModifier;
		currentClassLevelMember.accessModifier = symbolName == "_" ? AccessModifier::Private : AccessModifier::Public; return;
	}

	switch (currentClassLevelMember.memberType) {
		case MemberType::Field: ParseFieldSymbol(specialSymbol, symbolName); break;
		case MemberType::Function: ParseFunctionSymbol(specialSymbol, symbolName); break;
	}
}

void Lexer::ParseFieldSymbol(bool specialSymbol, std::string symbolName) {
	if (specialSymbol) {
		if (symbolName == ":" || symbolName == "!") {
			previousToken = TokenType::Declaration;
			currentClassLevelMember.fieldIsConstant = symbolName == "!"; return;
		}
		if (symbolName == "=") { previousToken = TokenType::Assignment; assignmentEncountered = true; currentClassLevelMember.assignedToValue = Expression(); return; }
		return;
	}
	switch (previousToken) {
		case TokenType::AccessModifier: currentClassLevelMember.memberName = symbolName; break;
		case TokenType::Declaration: currentClassLevelMember.dataType = DataType(symbolName); break;
	}
}

void Lexer::EndFunctionLevelMember(bool arrowSyntax) {
	functionLevelMember.underlyingExpression.EndExpression();
	int contentsSize = functionLevelMember.underlyingExpression.expressionContents.size(), historySize = functionLevelSymbolHistory.size();
	bool latestTokenOperator = !functionLevelMember.underlyingExpression.latestExpressionElementIsOperand;
	bool isElse = contentsSize == 1 && latestTokenOperator && functionLevelMember.underlyingExpression.latestOperator.operatorContents == "!";
	bool isCaseStatement = IsInSwitchCase();
	functionLevelMember.isDefaultCase = isCaseStatement && contentsSize == 1 && std::holds_alternative<Operator>(functionLevelMember.underlyingExpression.expressionContents[0])
		&& std::get<Operator>(functionLevelMember.underlyingExpression.expressionContents[0]).operatorContents == "!";
	if (isCaseStatement) isElse = false;

	if (arrowSyntax) isElse = contentsSize == 2 && latestTokenOperator && functionLevelSymbolHistory[historySize - 2] == "!" && functionLevelSymbolHistory[historySize - 1] == "=";
	if (isElse) {
		functionLevelMember.instructionType = InstructionType::Conditional;
		functionLevelMember.conditionalType = ConditionalType::ElseStatement;
		functionLevelMember.underlyingExpression.expressionContents.clear();
	}
	if (functionLevelMember.isWrappedInLoop) ParseLoopAdjecentStatement();

	CheckForSimpleCompoundAssignment();
	if (compoundOperator != "") HandleCompoundAssignmentAtInstructionEndTime();
	if (functionLevelMember.couldBeSwitchStatement) HandleSwitchStatement();
	if (functionLevelMember.instructionType == InstructionType::Declaration && functionLevelMember.variableDeclarationType.typeName == "") CorrectDeclarationWithoutValue();
	if (functionLevelMember.instructionType != InstructionType::Unknown) listOfTokenizedInstructions.push_back(functionLevelMember);
}