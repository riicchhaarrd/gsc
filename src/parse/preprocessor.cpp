#include "preprocessor.h"

bool parse::preprocessor::resolve_identifier(token_parser& parser, std::string ident, token_list& preprocessed_tokens,
											 definition_map& definitions)
{

	parse::token t;
	auto def = definitions.find(ident);
	if (def != definitions.end())
	{
		//check if it's a function macro
		if (def->second.is_function)
		{
			if(!parser.accept_token(t, '('))
				throw preprocessor_error("function macro, expecting (", t.to_string(), t.line_number());

			//build token list arguments of parameters
			std::vector<token_list> arguments;

			token_list tl;
			int numparens = 0;
			while (1)
			{
				t = parser.read_token();
				if (t.type == parse::token_type::eof)
					throw preprocessor_error("unexpected eof", t.to_string(), t.line_number());

				if (t.type_as_int() == '(')
				{
					++numparens;
				}
				else if (t.type_as_int() == ',')
				{
					if (numparens == 0)
					{
						arguments.push_back(tl);
						tl.clear();
					}
				} else if (t.type_as_int() == ')')
				{
					if (numparens <= 0)
						break;
					--numparens;
				}
				if (t.type_as_int() != ',')
					tl.push_back(t);
			}
			arguments.push_back(tl);

			parse_opts popts;
			popts.newlines = true;
			token_parser def_parser(def->second.body, popts);
			while (1)
			{
				t = def_parser.read_token();
				if (t.type == parse::token_type::eof)
					break;
				if (t.type_as_int() == '#')
				{
					if (!def_parser.accept_token(t, parse::token_type::identifier))
						throw preprocessor_error(common::format("expected identifier got {}", t.type_as_string()), t.to_string(), t.line_number());

					// if so, check if it matches any of the parameters
					auto parm = def->second.parameters.find(t.to_string());
					if (parm == def->second.parameters.end())
					{
						throw preprocessor_error("no such parameter", t.to_string(), t.line_number());
					}
					// we found the parameter, so stringify it
					if (parm->second >= arguments.size())
						throw preprocessor_error("argument out of bounds for macro function", t.to_string(), t.line_number());
						
					std::string concatenation;
					for (auto& it : arguments[parm->second])
					{
						concatenation += it.to_string();
					}
					preprocessed_tokens.push_back(parse::token(concatenation, parse::token_type::string));
				}
				else if (t.type == parse::token_type::string) // is the current token in our block/body a ident?
				{
					std::string concatenation = t.to_string();
					while (def_parser.accept_token(t, parse::token_type::pound_pound)) // concatenate
					{
						if (!def_parser.accept_token(t, parse::token_type::identifier))
							throw preprocessor_error("expected identifier", t.to_string(), t.line_number());
						auto parm = def->second.parameters.find(t.to_string());
						if (parm == def->second.parameters.end())
						{
							throw preprocessor_error("no such parameter", t.to_string(), t.line_number());
						}
						if (parm->second >= arguments.size())
							throw preprocessor_error("argument out of bounds for concatenation", t.to_string(),
													 t.line_number());

						for (auto& it : arguments[parm->second])
						{
							concatenation += it.to_string();
						}
					}
					preprocessed_tokens.push_back(parse::token(concatenation, parse::token_type::string));
				}
				else if (t.type_as_int() == (int)parse::token_type::identifier)
				{
					// if so, check if it matches any of the parameters
					auto parm = def->second.parameters.find(t.to_string());
					if (parm != def->second.parameters.end())
					{
						// we found the parameter, so replace it..
						if (parm->second >= arguments.size())
							throw preprocessor_error("argument out of bounds for macro function", t.to_string(), t.line_number());
						auto& tl = arguments[parm->second];
						for (auto& tl_it : tl)
						{
							if (tl_it.type_as_int() == (int)parse::token_type::identifier)
							{
								if (!resolve_identifier(def_parser, tl_it.to_string(), preprocessed_tokens,
														definitions))
									preprocessed_tokens.push_back(tl_it);
							}
							else
							{
								preprocessed_tokens.push_back(tl_it);
							}
						}
					}
					else
					{
						if (!resolve_identifier(def_parser, t.to_string(), preprocessed_tokens, definitions))
							preprocessed_tokens.push_back(t);
					}
				}
				else
				{
					preprocessed_tokens.push_back(t);
				}
			}
		}
		else
		{
			// printf("found %s\n", t.to_string().c_str());
			parse_opts popts;
			popts.newlines = true;
			token_parser def_parser(def->second.body, popts);
			while (1)
			{
				t = def_parser.read_token();
				if (t.type == parse::token_type::eof)
					break;
				if (t.type == parse::token_type::identifier)
				{
					if (!resolve_identifier(def_parser, t.to_string(), preprocessed_tokens, definitions))
						preprocessed_tokens.push_back(t);
				}
				else
				{
					preprocessed_tokens.push_back(t);
				}
			}
		}
		return true;
	}
	return false;
}
