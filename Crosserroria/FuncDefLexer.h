#pragma once
#include "Lexer.h"
#include <optional>

enum class InstructionType {
	Unknown,
	PlainExpression,
	Declaration,
	Assignment,
	IfStatement,
	ElseStatement,
	OutputStatement,
	LoopStatement,
	LoopControlFlow,
	ReturnStatement,
};

struct FunctionLevelInstruction {
	std::optional<Expression> underlyingExpression;
	InstructionType instructionType = InstructionType::Unknown;
	int nestingLevel;
	std::string variableName;
	DataType variableDeclarationType;
	bool isDeclaredVariableConstant = false;
	bool potentiallyEncounteredAssignmentSymbol = false, encounteredAssignment = false;
	operator std::string() const;
};