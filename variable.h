#pragma once

typedef enum
{
	VAR_UNDEFINED,
	VAR_STRING,
	VAR_INTERNED_STRING,
	VAR_INTEGER,
	VAR_BOOLEAN,
	VAR_FLOAT,
	VAR_VECTOR,
	// VAR_ANIMATION,
	VAR_FUNCTION,
	// VAR_LOCALIZED_STRING,
	VAR_OBJECT,
	VAR_REFERENCE,
	VAR_THREAD,
	// VAR_INTERNAL, // getter/setter, have to evaluate first?
	VAR_MAX
} VariableType;

static const char *variable_type_names[] = {

	"UNDEFINED", "STRING",	 "INTERNED_STRING", "INTEGER",	 "BOOLEAN", "FLOAT",
	"VECTOR",	 "FUNCTION", "OBJECT",			"REFERENCE", "THREAD",	NULL
};

#define VAR_TYPE_FLAG(X) (1 << (X))