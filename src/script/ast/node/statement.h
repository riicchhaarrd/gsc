#pragma once
#include "node.h"
#include <string>

namespace script
{
	namespace ast
	{
		struct Statement : Node
		{
			virtual void accept(ASTVisitor& visitor) = 0;
		};
	}; // namespace ast
};	   // namespace compiler