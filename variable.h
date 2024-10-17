#pragma once

typedef enum
{
	VAR_UNDEFINED,
	VAR_STRING,
	VAR_INTEGER,
	VAR_BOOLEAN,
	VAR_FLOAT,
	VAR_VECTOR,
	// VAR_ANIMATION,
	VAR_FUNCTION,
	VAR_LOCALIZED_STRING,
	VAR_OBJECT,
	VAR_REFERENCE,
	VAR_MAX
} VariableType;

static const char *variable_type_names[] = { "UNDEFINED", "STRING",			  "INTEGER", "BOOLEAN",	  "FLOAT", "VECTOR",
											 "FUNCTION",  "LOCALIZED_STRING", "OBJECT",	 "REFERENCE", NULL };

#define VAR_TYPE_FLAG(X) (1 << (X))

static const char *variable_globals[] = { "level", "game", "self", "anim", NULL };
typedef enum
{
	VAR_GLOB_LEVEL,
	VAR_GLOB_GAME,
	VAR_GLOB_SELF,
	VAR_GLOB_ANIM,
	VAR_GLOB_MAX
} VariableGlob;