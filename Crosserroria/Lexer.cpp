#include <fstream>
#include <string>
#include <algorithm>
#include "Lexer.h"
#include "FuncDefLexer.h"
#include <iostream>

Lexer::Lexer() {
	ParseScript("Program.crr");
}

bool Lexer::IsNumber(char ch) { return ch >= 48 && ch <= 57; }
bool Lexer::IsRegularSymbolChar(char ch) { return isalpha(ch) || ch == '_' || IsNumber(ch); }
bool Lexer::IsSymbolSpecial(std::string& symbol) { return !IsRegularSymbolChar(symbol[0]) || symbol == "_"; }

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
	assignmentEncountered = parametersEncountered = inOptionalParameter = canBeClassDeclaration = encounteredNonTab = false;
	currentFunctionParameter.Reset();
	currentEnumDataParameter.Reset();
	latestSymbolsHistory.clear();

	normalSymbolsParsed = symbolsParsedThisLine = currentNestingLevel = 0;
}

void Lexer::ParseLine(std::string currentLine) {
	if (currentClassLevelMember.memberType != MemberType::Enum) IntitializeOnNewLine();
	currentNestingLevel = 0;
	encounteredNonTab = encounteredPossibleOperator = false;

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

	if (currentNestingLevel == 0 && encounteredPossibleOperator && CheckIfCouldBeOperatorOverload()) currentClassLevelMember.memberType = MemberType::Operator;
	TerminateCurrentClassLevelMember();
	if (currentNestingLevel == 0) return;
	EndFunctionLevelMember();
}

void Lexer::TerminateCurrentClassLevelMember() {
	if (currentClassLevelMember.assignedToValue.has_value()) currentClassLevelMember.assignedToValue.value().EndExpression();
	bool validClassLevelMember = currentNestingLevel == 0 && currentClassLevelMember.memberType != MemberType::Unknown;
	if (validClassLevelMember && latestSymbol == ":") { currentClassLevelMember.memberType = MemberType::Enum; currentEnumMember = EnumMember(); return; }
	if (currentClassLevelMember.encounteredConstructor) currentClassLevelMember.memberType = MemberType::Constructor;
	if (validClassLevelMember) listOfTokenizedInstructions.push_back(currentClassLevelMember);
}

void Lexer::HandleSymbol(std::string& symbolName) {
	if (symbolName == "") return;
	int symbolNameLen = symbolName.length();
	char lastChar = symbolName[symbolNameLen - 1]; std::string lastCharAsStr{lastChar};
	bool containsSpecialChar = symbolNameLen > 1 && !IsRegularSymbolChar(lastChar);
	if (containsSpecialChar) symbolName = symbolName.substr(0, symbolNameLen-1);
	symbolNameLen = symbolName.length();
	bool firstCharUnderscore = symbolNameLen >= 1 && symbolName[0] == '_', lastCharUnderscore = symbolNameLen >= 2 && symbolName[symbolName.size()-1] == '_';
	if (firstCharUnderscore) { ParseSymbol("_"); symbolName = symbolName.substr(1); }
	if (lastCharUnderscore) symbolName = symbolName.substr(0, symbolName.size() - 1);
	ParseSymbol(symbolName);
	if (lastCharUnderscore) ParseSymbol("_");
	if (containsSpecialChar) ParseSymbol(lastCharAsStr);
	symbolName = "";
}

void Lexer::ParseSymbol(std::string symbolName) {
	if (symbolName == "") return;
	latestSymbol = symbolName;
	if (currentClassLevelMember.memberType == MemberType::Enum) { ParseEnumSymbol(symbolName); return; }
	if (currentNestingLevel > 0) { ParseFunctionLevelSymbol(symbolName); return; }

	bool specialSymbol = IsSymbolSpecial(symbolName);
	bool isAccessModifier = symbolName == "_" || symbolName == "^";
	bool isNotInStr = !currentClassLevelMember.assignedToValue.has_value() || currentClassLevelMember.assignedToValue.value().NotInString();

	bool encounteredArrow = isNotInStr && latestSymbolsHistory.size() >= 1 && latestSymbolsHistory[latestSymbolsHistory.size() - 1] == "=" && symbolName == ">";
	if (encounteredArrow) encounteredPossibleOperator = true;
	if (symbolName != " ") latestSymbolsHistory.push_back(symbolName);
	ParsePossibleClassDeclaration();
	if (currentClassLevelMember.possiblyEndedFirstOperand && symbolName != "") {
		if (specialSymbol) currentClassLevelMember.overloadedOperator.operatorContents += symbolName;
		else currentClassLevelMember.possiblyEndedFirstOperand = false;
	}

	bool statementSeperator = symbolName == ";" && isNotInStr;
	if (statementSeperator) {
		TerminateCurrentClassLevelMember();
		IntitializeOnNewLine();
		return;
	}

	if (assignmentEncountered) { (*currentClassLevelMember.assignedToValue).ParseNextSymbol(symbolName); return; }

	if (!specialSymbol) normalSymbolsParsed++;
	symbolsParsedThisLine++;

	if (normalSymbolsParsed == 1 && !specialSymbol && currentClassLevelMember.memberType == MemberType::Unknown) {
		switch (previousToken) {
			case TokenType::AccessModifier: currentClassLevelMember.memberType = MemberType::Field; break;
			case TokenType::None: HandleStartFunctionSymbol(symbolName); return;
		}
	}

	if (symbolsParsedThisLine == 1 && symbolName == "?") {
		currentClassLevelMember.memberType = MemberType::Function;
		currentClassLevelMember.primaryMember.dataType = DataType("?");
		return;
	}

	bool inCtor = symbolName == "*" && previousToken == TokenType::AccessModifier && currentClassLevelMember.primaryMember.dataType.typeName == "";
	if (inCtor) {
		currentClassLevelMember.memberType = MemberType::Function;
		currentClassLevelMember.encounteredConstructor = true;
	}

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
			currentClassLevelMember.primaryMember.fieldIsConstant = symbolName == "!"; return;
		}
		if (symbolName == "=") {
			previousToken = TokenType::Assignment;
			assignmentEncountered = true;
			canBeClassDeclaration = true;
			currentClassLevelMember.assignedToValue = Expression();
			return;
		}
		return;
	}
	switch (previousToken) {
		case TokenType::AccessModifier: currentClassLevelMember.primaryMember.memberName = symbolName; break;
		case TokenType::Declaration:
			currentClassLevelMember.primaryMember.dataType = DataType(symbolName);
			currentClassLevelMember.possiblyEndedFirstOperand = true; break;
	}
}

void Lexer::EndFunctionLevelMember(bool arrowSyntax) {
	functionLevelMember.underlyingExpression.EndExpression();
	int contentsSize = functionLevelMember.underlyingExpression.expressionContents.size(), historySize = latestSymbolsHistory.size();
	bool latestTokenOperator = !functionLevelMember.underlyingExpression.latestExpressionElementIsOperand;
	bool isElse = contentsSize == 1 && latestTokenOperator && functionLevelMember.underlyingExpression.latestOperator.operatorContents == "!" && functionLevelMember.instructionType == InstructionType::PlainExpression;
	bool isCaseStatement = IsInSwitchCase();

	functionLevelMember.isDefaultCase = isCaseStatement && contentsSize == 1 && std::holds_alternative<Operator>(functionLevelMember.underlyingExpression.expressionContents[0])
		&& std::get<Operator>(functionLevelMember.underlyingExpression.expressionContents[0]).operatorContents == "!";
	if (isCaseStatement) isElse = false;

	if (arrowSyntax) isElse = contentsSize == 2 && latestTokenOperator && latestSymbolsHistory[historySize - 2] == "!" &&
		latestSymbolsHistory[historySize - 1] == "=";
	if (isElse) {
		functionLevelMember.instructionType = InstructionType::Conditional;
		functionLevelMember.conditionalType = ConditionalType::ElseStatement;
		functionLevelMember.underlyingExpression.expressionContents.clear();
	}
	if (functionLevelMember.isWrappedInLoop) ParseLoopAdjecentStatement();

	bool encounteredSpecialReturnType = functionLevelMember.instructionType == InstructionType::ReturnStatement && functionLevelMember.underlyingExpression.expressionContents.size() == 1;
	if (encounteredSpecialReturnType) {
		std::string latestOperator = functionLevelMember.underlyingExpression.latestOperator.operatorContents;
		if (latestOperator == "!") functionLevelMember.returnStatementType = ReturnStatementType::ImpossibleCast;
		if (latestOperator == "@") functionLevelMember.returnStatementType = ReturnStatementType::AutoBreak;
		if (functionLevelMember.returnStatementType != ReturnStatementType::Regular) functionLevelMember.underlyingExpression.expressionContents.clear();
	}

	CheckForSimpleCompoundAssignment();
	if (compoundOperator != "") HandleCompoundAssignmentAtInstructionEndTime();
	if (functionLevelMember.couldBeSwitchStatement) HandleSwitchStatement();
	if (functionLevelMember.instructionType == InstructionType::Declaration && functionLevelMember.variableDeclarationType.typeName == "") CorrectDeclarationWithoutValue();
	if (functionLevelMember.instructionType != InstructionType::Unknown) {
		FunctionLevelInstruction pushedElement(functionLevelMember);
		//std::cout << static_cast<int>(functionLevelMember.returnStatementType) << " " << static_cast<int>(pushedElement.returnStatementType) << std::endl;
		listOfTokenizedInstructions.push_back(functionLevelMember);
	}
}

std::vector<std::string>& Lexer::GetActiveSuperClassList() {
	return currentClassLevelMember.encounteredInheritence ? currentClassLevelMember.inheritanceClassSuperClassList : currentClassLevelMember.workingClassSuperClassList;
}

bool Lexer::IsCurrentStatementClassDeclaration(const std::string& symbolName) {
	bool couldBeValid = symbolName == ">" && canBeClassDeclaration;
	if (!couldBeValid) return false;
	for (int i = 0; i < Lexer::AmountOfEqualsInClassDeclarations; i++) {
		int historyIndex = latestSymbolsHistory.size() - 2 - i;
		if (historyIndex < 0 || latestSymbolsHistory[historyIndex] != "=") return false;
	}
	return true;
}

void Lexer::ParsePossibleClassDeclaration() {
	if (assignmentEncountered && IsCurrentStatementClassDeclaration(latestSymbol)) {
		currentClassLevelMember.memberType = MemberType::Class;
		std::string actualClassName = latestSymbolsHistory[latestSymbolsHistory.size() - Lexer::ActualClassNameHistoryOffset];
		GetActiveSuperClassList().push_back(actualClassName);
	}

	bool possibleClass = currentClassLevelMember.memberType == MemberType::Field && currentClassLevelMember.primaryMember.dataType.typeName == ""
		&& latestSymbolsHistory.size() > 0;
	bool addingSuperClass = latestSymbol == "." && possibleClass, encounteredInheritsSymbol = latestSymbol == ">" && possibleClass;
	if (addingSuperClass)
		GetActiveSuperClassList().push_back(latestSymbolsHistory[latestSymbolsHistory.size() - BeforeClassDotSeperatorHistoryOffset]);
	if (encounteredInheritsSymbol) {
		std::string actualBaseClassName = latestSymbolsHistory[latestSymbolsHistory.size() - BeforeClassDotSeperatorHistoryOffset];
		currentClassLevelMember.workingClassSuperClassList.push_back(actualBaseClassName);
		currentClassLevelMember.encounteredInheritence = true;
	}
}

BasicMemberInfo& Lexer::GetCurrentWorkingMember() {
	return currentClassLevelMember.isInFirstOperand ? currentClassLevelMember.primaryMember : currentClassLevelMember.secondaryMember;
}

bool Lexer::CheckIfCouldBeOperatorOverload() {
	if (currentClassLevelMember.accessModifier == AccessModifier::Unknown) return false;
	previousToken = TokenType::BeforeParameterName;
	currentClassLevelMember.overloadedOperator.operatorContents = "";

	for (int i = 1; i < latestSymbolsHistory.size(); i++) {
		std::string currentSymbol = latestSymbolsHistory[i];
		bool isSymbolSpecial = IsSymbolSpecial(currentSymbol);

		if (isSymbolSpecial && previousToken == TokenType::BeforeParameterName) {
			currentClassLevelMember.overloadedOperator.operatorContents += currentSymbol;
			currentClassLevelMember.everEncounteredUnary = true;
			continue;
		}

		bool isInArrowSyntax = latestSymbolsHistory[i - 1] == "=" && currentSymbol == ">";
		if (previousToken == TokenType::AfterParameterName && currentSymbol == ":") { previousToken = TokenType::Declaration; continue; }

		if (isInArrowSyntax) {
			previousToken = TokenType::ArrowSyntax;
			std::string opName = currentClassLevelMember.overloadedOperator.operatorContents;
			currentClassLevelMember.overloadedOperator.operatorContents = opName.substr(0, opName.size()-1);
			continue;
		}

		if (isSymbolSpecial && previousToken == TokenType::AfterParameterName) {
			currentClassLevelMember.overloadedOperator.operatorContents += currentSymbol;
			currentClassLevelMember.isInFirstOperand = false;
			continue;
		}

		switch (previousToken) {
			case TokenType::ArrowSyntax:
				currentClassLevelMember.overloadedOperatorReturnType = DataType(currentSymbol);
				break;
			case TokenType::BeforeParameterName:
			case TokenType::AfterParameterName:
				GetCurrentWorkingMember().memberName = currentSymbol; previousToken = TokenType::AfterParameterName;
				continue;
			case TokenType::Declaration:
				GetCurrentWorkingMember().dataType = DataType(currentSymbol);
				previousToken = TokenType::AfterParameterName;
				continue;
		}
	}

	bool hasOperator = currentClassLevelMember.overloadedOperator.operatorContents != "";
	if (currentClassLevelMember.everEncounteredUnary) currentClassLevelMember.overloadedOperator.operatorType = OperatorType::LeftUnaryOperator;
	else currentClassLevelMember.overloadedOperator.operatorType = hasOperator ? OperatorType::BinaryOperator : OperatorType::CastingOverload;

	bool wasPreviousArrow = previousToken == TokenType::ArrowSyntax;
	bool hasReturnType = currentClassLevelMember.overloadedOperatorReturnType.typeName != "";
	return wasPreviousArrow && hasReturnType;
}