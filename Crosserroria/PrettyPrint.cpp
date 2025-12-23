#include "Lexer.h"
#include <iostream>

DataType::operator std::string() const {
	std::string output = typeName;
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
		case OperandType::InitializerCall: result += "init"; break;
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
		case OperatorType::AttributeAccess: result += "Attribute"; break;
		case OperatorType::SpecificFunctionParameter: result += "ParamName"; break;
		case OperatorType::CastingOverload: result += "Cast"; break;
	}
	return result + ")";
}

FunctionParameter::operator std::string() const {
	std::string result = "";
	if (isConstant) result = "const ";
	result += parameterName + ": " + (std::string)parameterType;
	if (optionalValue != std::nullopt) result += " = " + (std::string)optionalValue.value();
	return result;
}

ClassLevelMember::operator std::string() const {
	std::string output = "", typeSpecificParameters = "", accessModifierAsStr = "";
	switch (memberType) {
		case MemberType::Unknown: output = "Unknown"; break;
		case MemberType::Function: output = "Function"; break;
		case MemberType::Field: output = "Field"; break;
		case MemberType::Enum: output = "Enum"; break;
		case MemberType::Class: output = "Class"; break;
		case MemberType::Constructor: output = "Constructor"; break;
		case MemberType::Operator: output = "Operator"; break;
	}
	output += "[";

	switch (accessModifier) {
		case AccessModifier::Unknown: accessModifierAsStr = "Unknown"; break;
		case AccessModifier::Private: accessModifierAsStr = "Private"; break;
		case AccessModifier::Public: accessModifierAsStr = "Public"; break;
	}

	int numberOfEnumMembers = enumMembers.size();
	switch (memberType) {
		case MemberType::Field:
			output += "name: " + primaryMember.memberName + ", type: " + (std::string)(primaryMember.dataType) + ", access: " + accessModifierAsStr + ", ";
			if (primaryMember.fieldIsConstant) output += "constant"; else output += "mutable";
			if (assignedToValue != std::nullopt) output += ", value: " + (std::string)assignedToValue.value(); break;
		case MemberType::Function:
			output += "name: " + primaryMember.memberName + ", type: " + (std::string)(primaryMember.dataType) + ", access: " + accessModifierAsStr + ", params: {";
			output += FunctionParamsAsString();
			output += "}";
			break;
		case MemberType::Enum:
			output += "name: " + primaryMember.memberName;
			if (isAlgebraic) output += ", algebraic";
			output += ", options: {";
			for (int i = 0; i < numberOfEnumMembers; i++) {
				EnumMember currentEnumMember = enumMembers[i];
				output += currentEnumMember;
				if (i + 1 < numberOfEnumMembers) output += ", ";
			}
			output += "}";
			break;
		case MemberType::Class:
			output += "access: " + accessModifierAsStr + ", ";
			output += Lexer::GetAttributeListAsString(workingClassSuperClassList);
			if (!encounteredInheritence) break;
			output += " inherits " + Lexer::GetAttributeListAsString(inheritanceClassSuperClassList);
			break;
		case MemberType::Constructor:
			output += "access: " + accessModifierAsStr + ", params: {" + FunctionParamsAsString() + "}";
			break;
		case MemberType::Operator:
			output += "op: " + (std::string)overloadedOperator + ", lhs: " + primaryMember.memberName + " (" + (std::string)primaryMember.dataType + ")";
			if (overloadedOperator.operatorType == OperatorType::BinaryOperator) output += ", rhs: " + secondaryMember.memberName + " (" + (std::string)secondaryMember.dataType + ")";
			output += ", access: " + accessModifierAsStr;
			output += ", type: " + (std::string)overloadedOperatorReturnType;
			break;
		}
	output += "]";
	return output;
}

EnumMember::operator std::string() const {
	std::string output = memberName;
	int dataCount = associatedData.size();
	if (dataCount > 0) {
		output += "[";
		for (int i = 0; i < dataCount; i++) {
			output += associatedData[i];
			if (i + 1 != dataCount) output += ", ";
		}
		output += "]";
	}
	if (underlyingExpression.has_value()) output += " = " + (std::string)underlyingExpression.value();
	return output;
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

	std::string returnStatementTypeAsStr = "";
	switch (returnStatementType) {
		case ReturnStatementType::Regular: returnStatementTypeAsStr = "Regular"; break;
		case ReturnStatementType::ImpossibleCast: returnStatementTypeAsStr = "Impossible Cast"; break;
		case ReturnStatementType::AutoBreak: returnStatementTypeAsStr = "Auto-Break"; break;
		default: returnStatementTypeAsStr = "UNDEFINED(" + std::to_string(static_cast<int>(returnStatementType)) + ")"; break;
	}

	std::string returnValueMessage = expressionTextPure, switchCaseMessage = expressionTextPure;
	if (returnValueMessage == "") returnValueMessage = "void";
	if (isDefaultCase) switchCaseMessage = "default";

	switch (instructionType) {
		case InstructionType::PlainExpression: output = "PlainExpression[" + expressionTextPure; break;
		case InstructionType::Assignment: output = "Assignment[name: " + Lexer::GetAttributeListAsString(assignmentAttributeNameList) + expressionText; break;
		case InstructionType::Declaration: output = "Declaration[name: " + variableName + ", type: " + (std::string)variableDeclarationType + mutabilityText + expressionText + uninitializedStr; break;
		case InstructionType::Conditional: output = "Conditional[type: " + conditonalTypeAsStr + expressionText; break;
		case InstructionType::LoopStatement: output = "Loop[" + expressionTextPure + iterNameStr + iterTypeStr + indexVarNameStr; break;
		case InstructionType::LoopControlFlow: output = "LoopCF[type: " + loopControlFlowAsStr + ", loopCount: " + std::to_string(loopCount); break;
		case InstructionType::ReturnStatement: output = "Return[" + returnValueMessage + ", type: " + returnStatementTypeAsStr; break;
		case InstructionType::SwitchStatement: output = "Switch[" + expressionTextPure; break;
		case InstructionType::SwitchCase: output = "Case[" + switchCaseMessage; break;
	}
	output += ", nesting: " + std::to_string(nestingLevel) + "]";
	return output;
}

std::string Lexer::GetAttributeListAsString(const std::vector<std::string>& attributeList) {
	std::string output = "";
	for (int i = 0; i < attributeList.size(); i++) {
		output += attributeList[i];
		if (i + 1 != attributeList.size()) output += ".";
	}
	return output;
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

std::string ClassLevelMember::FunctionParamsAsString() const {
	std::string output = "";
	for (int i = 0; i < functionParameters.size(); i++) {
		FunctionParameter parameter = functionParameters[i];
		output += parameter;
		if (i + 1 < functionParameters.size()) output += ", ";
	}
	return output;
}