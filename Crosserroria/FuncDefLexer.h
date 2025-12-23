#pragma once
#include "Lexer.h"
#include <optional>
#include <string>

enum class InstructionType {
	Unknown,
	PlainExpression,
	Declaration,
	Assignment,
	Conditional,
	LoopStatement,
	LoopControlFlow,
	ReturnStatement,
	SwitchStatement,
	SwitchCase
};

enum class ConditionalType {
	Unknown,
	IfStatement,
	ElifStatement,
	ElseStatement
};

enum class LoopControlFlow {
	Unknown,
	Continue,
	Break
};

enum class ReturnStatementType {
	Regular,
	ImpossibleCast,
	AutoBreak
};

struct FunctionLevelInstruction {
	FunctionLevelInstruction() = default;
	Expression underlyingExpression, switchValue;
	InstructionType instructionType = InstructionType::Unknown;
	int nestingLevel = 0, loopCount = 0;
	std::string variableName = "", indexVariableName = "";
	std::vector<std::string> assignmentAttributeNameList{};
	DataType variableDeclarationType;
	bool isDeclaredVariableConstant = false, couldBeSwitchStatement = false, isDefaultCase = false;
	bool potentiallyEncounteredAssignmentSymbol = false, encounteredAssignment = false, isUninitialized = false,
		isWrappedInLoop = false, isInLoopIndexVariableName = false, declarationConstruction = false;
	ConditionalType conditionalType = ConditionalType::Unknown;
	LoopControlFlow loopControlFlowType = LoopControlFlow::Unknown;
	ReturnStatementType returnStatementType = ReturnStatementType::Regular;
	operator std::string() const;
};