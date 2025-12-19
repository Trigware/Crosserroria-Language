#include <fstream>
#include <string>
#include <iostream>
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
		//if (i == 4) break;
	}
	file.close();
	PrintTokenizedInstructions();
}

void Lexer::PrintTokenizedInstructions() {
	for (int i = 0; i < listOfTokenizedInstructions.size(); i++) {
		std::variant<ClassLevelMember, FunctionLevelInstruction> member = listOfTokenizedInstructions[i];
		std::string currentInstructionAsStr = "";
		if (std::holds_alternative<ClassLevelMember>(member)) currentInstructionAsStr = (std::string)std::get<ClassLevelMember>(member);
		if (std::holds_alternative<FunctionLevelInstruction>(member)) currentInstructionAsStr = (std::string)std::get<FunctionLevelInstruction>(member);
		std::cout << currentInstructionAsStr << std::endl;
	}
}

void Lexer::ParseLine(std::string currentLine) {
	currentClassLevelMember = ClassLevelMember();
	currentSymbol = "";
	previousToken = TokenType::None;
	assignmentEncountered = parametersEncountered = inOptionalParameter = encounteredNonTab = false;
	currentFunctionParameter.Reset();
	normalSymbolsParsed = symbolsParsedThisLine = currentNestingLevel = 0;

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
	switch (currentClassLevelMember.memberType) {
		case MemberType::Field:
			if (currentClassLevelMember.assignedToValue.has_value()) currentClassLevelMember.assignedToValue.value().EndExpression(); break;
		case MemberType::Function: ParseFunctionParameter(); break;
	}

	if (currentNestingLevel == 0 && currentClassLevelMember.memberType != MemberType::Unknown) listOfTokenizedInstructions.push_back(currentClassLevelMember);
	if (currentNestingLevel == 0) return;
	EndFunctionLevelMember();
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

void Lexer::ParseFunctionSymbol(bool specialSymbol, std::string symbolName) {
	if (!parametersEncountered) {
		switch (previousToken) {
			case TokenType::AccessModifier: currentClassLevelMember.memberName = symbolName; previousToken = TokenType::MemberName; return;
			case TokenType::MemberName: parametersEncountered = true; previousToken = TokenType::BeforeParameterName; return;
		}
	}

	bool isDeclaration = symbolName == ":" || symbolName == "!";
	if (inOptionalParameter) {
		Expression& currentParameterOptionalValue = currentFunctionParameter.optionalValue.value();
		Operand currentExpressionOperand = currentParameterOptionalValue.currentOperand;
		bool noNesting = currentParameterOptionalValue.currentBracketNestingLevel == 0, inString = currentExpressionOperand.operandType == OperandType::StringLiteral;

		if (noNesting && !inString && symbolName == ")") return;
		if (noNesting && !inString && symbolName == ",") { ParseFunctionParameter(); return; }
		currentParameterOptionalValue.ParseNextSymbol(symbolName);
		if (currentParameterOptionalValue.ternaryConditionalNestingLevel > 0) isDeclaration = false;
		if (inString || currentParameterOptionalValue.currentBracketNestingLevel > 0) return;
	}

	if (symbolName == ",") { ParseFunctionParameter(); return; }
	if (symbolName == " ") return;
	if (symbolName == "=") { inOptionalParameter = true; currentFunctionParameter.optionalValue = Expression(); return; }
	if (isDeclaration) { previousToken = TokenType::AfterParameterName; currentFunctionParameter.isConstant = symbolName == "!"; return; }

	if (inOptionalParameter) return;
	if (previousToken == TokenType::BeforeParameterName) currentFunctionParameter.parameterName = symbolName;
	if (previousToken == TokenType::AfterParameterName && symbolName != ")") currentFunctionParameter.parameterType = symbolName;
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
	if (underscorePosition == std::string::npos) { currentClassLevelMember.dataType = symbolName; return; }
	
	std::string functionReturnType = symbolName.substr(0, underscorePosition);
	std::string functionName = symbolName.substr(underscorePosition+1);
	currentClassLevelMember.dataType = functionReturnType;
	currentClassLevelMember.memberName = functionName;
	currentClassLevelMember.accessModifier = AccessModifier::Private;
	parametersEncountered = true;
	previousToken = TokenType::BeforeParameterName;
}

#pragma region PrettyPrinting
DataType::operator std::string() const {
	std::string output = typeName;
	//if (referenceType) output += "&";
	return output;
}

Expression::operator std::string() const {
	std::string result = "Expression[";
	int numberOfContents = expressionContents.size();
	for (int i = 0; i < numberOfContents; i++) {
		std::variant<Operand, Operator> expressionObject = expressionContents[i];
		std::string objectAsStr = "";
		if (std::holds_alternative<Operand>(expressionObject)) {
			Operand currentOperand = std::get<Operand>(expressionObject);
			objectAsStr = currentOperand;
		}
		else if (std::holds_alternative<Operator>(expressionObject)) {
			Operator currentOperator = std::get<Operator>(expressionObject);
			objectAsStr = currentOperator;
		}
		result += objectAsStr;
		if (i + 1 < numberOfContents) result += "; ";
	}
	return result + "]";
}

Operand::operator std::string() const {
	std::string result = operandContents;
	if (operandType == OperandType::StringLiteral) result = "\"" + result + "\"";
	result += " (";
	switch (operandType) {
		case OperandType::Unknown: result += "unknown"; break;
		case OperandType::Variable: result += "var"; break;
		case OperandType::BooleanLiteral: result += "B"; break;
		case OperandType::IntegerLiteral: result += "Z"; break;
		case OperandType::RealLiteral: result += "R"; break;
		case OperandType::StringLiteral: result += "S"; break;
		case OperandType::TypeLiteral: result += "type"; break;
		case OperandType::FunctionCall: result += "func"; break;
	}
	return result + ")";
}

Operator::operator std::string() const {
	std::string result = operatorContents + " (";
	if (operatorContents == "") result = "(";
	switch (operatorType) {
		case OperatorType::Unknown: result += "unknown"; break;
		case OperatorType::LeftUnaryOperator: result += "Left Unary"; break;
		case OperatorType::RightUnaryOperator: result += "Right Unary"; break;
		case OperatorType::BinaryOperator: result += "Binary"; break;
		case OperatorType::LeftParenthesis: result += "Left Paren"; break;
		case OperatorType::RightParenthesis: result += "Right Paren"; break;
		case OperatorType::FunctionParameterSeperator: result += "Parameter Seperator"; break;
		case OperatorType::FunctionClosing: result += "Function Closing"; break;
		case OperatorType::TernaryOperatorValueOnSuccess: result += "Ternary Success"; break;
		case OperatorType::TernaryOperatorValueOnFail: result += "Ternary Failure"; break;
	}
	return result + ")";
}

FunctionParameter::operator std::string() const {
	std::string result = "";
	if (isConstant) result = "const ";
	result += parameterName + " (" + (std::string)parameterType + ")";
	if (optionalValue != std::nullopt) result += " = " + (std::string)optionalValue.value();
	return result;
}

ClassLevelMember::operator std::string() const {
	std::string output = "", typeSpecificParameters = "";
	switch (memberType) {
		case MemberType::Unknown: output += "Unknown"; break;
		case MemberType::Function: output += "Function"; break;
		case MemberType::Field: output += "Field"; break;
	}
	output += "[name: '" + memberName + "', type: " + (std::string)dataType + ", access: ";
	switch (accessModifier) {
		case AccessModifier::Unknown: output += "Unknown"; break;
		case AccessModifier::Private: output += "Private"; break;
		case AccessModifier::Public: output += "Public"; break;
	}
	switch (memberType) {
		case MemberType::Field:
			output += ", ";
			if (fieldIsConstant) output += "constant"; else output += "mutable";
			if (assignedToValue != std::nullopt) output += ", " + (std::string)assignedToValue.value(); break;
		case MemberType::Function:
			output += ", Params: {";
			int numberOfParams = functionParameters.size();
			for (int i = 0; i < numberOfParams; i++) {
				FunctionParameter parameter = functionParameters[i];
				output += parameter;
				if (i + 1 < numberOfParams) output += ", ";
			}
			output += "}";
			break;
	}
	output += "]";
	return output;
}
#pragma endregion