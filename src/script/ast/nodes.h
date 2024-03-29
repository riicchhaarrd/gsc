#pragma once
#include <memory>

#include <script/ast/node/statement.h>
#include <script/ast/node/expression/assignment_expression.h>
#include <script/ast/node/expression/binary_expression.h>
#include <script/ast/node/statement/block_statement.h>
#include <script/ast/node/statement/break_statement.h>
#include <script/ast/node/expression/call_expression.h>
#include <script/ast/node/expression/conditional_expression.h>
#include <script/ast/node/statement/do_while_statement.h>
#include <script/ast/node/statement/expression_statement.h>
#include <script/ast/node/statement/for_statement.h>
#include <script/ast/node/function_declaration.h>
#include <script/ast/node/expression/identifier.h>
#include <script/ast/node/statement/if_statement.h>
#include <script/ast/node/expression/literal.h>
#include <script/ast/node/expression/member_expression.h>
#include <script/ast/node/program.h>
#include <script/ast/node/statement/return_statement.h>
#include <script/ast/node/expression/unary_expression.h>
#include <script/ast/node/statement/while_statement.h>
#include <script/ast/node/expression.h>
#include <script/ast/node/expression/array_expression.h>
#include <script/ast/node/expression/vector_expression.h>
#include <script/ast/node/expression/function_pointer.h>
#include <script/ast/node/expression/localized_string.h>
#include <script/ast/node/statement/wait_statement.h>
#include <script/ast/node/statement/wait_till_frame_end_statement.h>
#include <script/ast/node/statement/empty_statement.h>
#include <script/ast/node/statement/continue_statement.h>
#include <script/ast/node/statement/switch_statement.h>
#include <script/ast/node/directive.h>

namespace script
{
	namespace ast
	{
		using ExpressionPtr = std::unique_ptr<Expression>;
		using StatementPtr = std::unique_ptr<Statement>;
	};
};