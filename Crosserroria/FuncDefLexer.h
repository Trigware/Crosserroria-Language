#pragma once
#include "Lexer.h"
#include <optional>

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

struct FunctionLevelInstruction {
	FunctionLevelInstruction();
	Expression underlyingExpression;
	InstructionType instructionType = InstructionType::Unknown;
	int nestingLevel = 0;
	std::string variableName;
	DataType variableDeclarationType;
	bool isDeclaredVariableConstant = false;
	bool potentiallyEncounteredAssignmentSymbol = false, encounteredAssignment = false, isUninitialized = false;
	ConditionalType conditionalType;
	operator std::string() const;
};