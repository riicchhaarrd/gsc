#pragma once

typedef enum
{
	VAR_UNDEFINED,
	VAR_STRING,
	VAR_INTEGER,
	VAR_BOOLEAN,
	VAR_FLOAT,
	VAR_VECTOR,
	VAR_ANIMATION,
	VAR_FUNCTION,
	VAR_LOCALIZED_STRING,
	VAR_OBJECT,
	VAR_MAX
} VariableType;

static const char *variable_type_names[] = { "UNDEFINED", "STRING",	  "INTEGER",		  "BOOLEAN", "FLOAT", "VECTOR",
											 "ANIMATION", "FUNCTION", "LOCALIZED_STRING", "OBJECT",	 NULL };

#define VAR_TYPE_FLAG(X) (1 << (X))