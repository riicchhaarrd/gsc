#pragma once
#include <script/ast/node/expression.h>
#include <string>
#include <memory>

namespace script
{
	namespace ast
	{
		struct AssignmentExpression : Expression
		{
			std::unique_ptr<Expression> lhs;
			std::unique_ptr<Expression> rhs;
			int op;

			AST_NODE(AssignmentExpression)

			virtual void print(Printer& out) override
			{
				out.print("assignment expression: %d", op);
				out.indent();
				out.print("lhs:");
				lhs->print(out);
				out.print("rhs:");
				rhs->print(out);
				out.unindent();
			}
		};
	}; // namespace ast
};	   // namespace compiler