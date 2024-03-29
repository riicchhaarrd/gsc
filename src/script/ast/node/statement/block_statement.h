#pragma once
#include <script/ast/node/statement.h>
#include <string>
#include <vector>
#include <memory>

namespace script
{
	namespace ast
	{
		struct BlockStatement : Statement
		{
			std::vector<std::unique_ptr<Statement>> body;

			AST_NODE(BlockStatement)

			virtual void print(Printer& out) override
			{
				out.print("block statement:");
				out.indent();
				for (auto& stmt : body)
					stmt->print(out);
				out.unindent();
			}
		};
	}; // namespace ast
};	   // namespace compiler