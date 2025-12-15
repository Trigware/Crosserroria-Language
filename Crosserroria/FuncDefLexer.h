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

struct LoopStatement {

};

struct FunctionLevelInstruction {
	std::optional<Expression> underlyingExpression;
	InstructionType instructionType = InstructionType::Unknown;
	int nestingLevel;
	std::string variableName;
	DataType variableDeclarationType;
	bool isDeclaredVariableConstant = false;
	bool potentiallyEncounteredAssignmentSymbol = false, encounteredAssignment = false;
	ConditionalType conditionalType;
	operator std::string() const;
};