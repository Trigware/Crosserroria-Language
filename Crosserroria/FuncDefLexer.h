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

struct FunctionLevelInstruction {
	FunctionLevelInstruction();
	Expression underlyingExpression;
	InstructionType instructionType = InstructionType::Unknown;
	int nestingLevel = 0, loopCount = 0;
	std::string variableName = "", indexVariableName = "";
	DataType variableDeclarationType;
	bool isDeclaredVariableConstant = false;
	bool potentiallyEncounteredAssignmentSymbol = false, encounteredAssignment = false, isUninitialized = false, isWrappedInLoop = false, isInLoopIndexVariableName = false;
	ConditionalType conditionalType = ConditionalType::Unknown;
	LoopControlFlow loopControlFlowType = LoopControlFlow::Unknown;
	operator std::string() const;
};