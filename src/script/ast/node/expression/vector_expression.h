#pragma once
#include "../expression.h"
#include <string>
#include <memory>
#include <vector>

namespace script
{
	namespace ast
	{
		struct VectorExpression : Expression
		{
			AST_NODE(VectorExpression)
			std::vector<std::unique_ptr<Expression>> elements;

			virtual void print(Printer& out) override
			{
				out.print("vector expression: %d elements", elements.size());
			}
		};
	}; // namespace ast
};	   // namespace compiler