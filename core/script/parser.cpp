// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include "parser.h"

namespace avs {

Parser::Parser(Lexer& lexer) : lexer(lexer) {
}

std::unique_ptr<ASTNode> Parser::parse() {
    return parse_statement_sequence();
}

// Parse statement sequence: statement (';' statement)*
std::unique_ptr<ASTNode> Parser::parse_statement_sequence() {
    auto sequence = std::make_unique<StatementSequenceNode>();
    
    // Parse first statement
    sequence->statements.push_back(parse_statement());
    
    // Parse additional statements separated by semicolons
    while (lexer.peek_token().type == TokenType::SEMICOLON) {
        lexer.next_token(); // consume ';'

        // Skip empty statements (consecutive semicolons)
        while (lexer.peek_token().type == TokenType::SEMICOLON) {
            lexer.next_token();
        }

        // Allow optional trailing semicolon
        if (lexer.peek_token().type == TokenType::END_OF_INPUT) {
            break;
        }

        sequence->statements.push_back(parse_statement());
    }
    
    // If only one statement and it's not an assignment, return it directly
    if (sequence->statements.size() == 1) {
        return std::move(sequence->statements[0]);
    }
    
    return std::move(sequence);
}

// Parse statement: assignment or expression
std::unique_ptr<ASTNode> Parser::parse_statement() {
    return parse_assignment_or_expression();
}

// Parse assignment or expression: IDENTIFIER '=' expression | IDENTIFIER '[' expr ']' '=' expression | expression
std::unique_ptr<ASTNode> Parser::parse_assignment_or_expression() {
    // Check for assignment: IDENTIFIER '=' or array assignment: IDENTIFIER '[' expr ']' '='
    // But need to distinguish from function calls: IDENTIFIER '('
    if (lexer.peek_token().type == TokenType::IDENTIFIER) {
        // Look ahead to see if this is assignment (IDENTIFIER '=') or array assignment or something else
        Lexer temp_lexer = lexer;  // Make a copy to look ahead
        temp_lexer.next_token(); // consume identifier
        TokenType next_type = temp_lexer.peek_token().type;

        if (next_type == TokenType::ASSIGN) {
            // This is a simple assignment: x = expr
            Token id_token = lexer.next_token();
            lexer.next_token(); // consume '='
            auto value = parse_expression();
            return std::make_unique<AssignmentNode>(id_token.value, std::move(value));
        } else if (next_type == TokenType::LBRACKET) {
            // This might be an array assignment: arr[i] = expr
            // or just an array access in an expression: arr[i] + 1
            // We need to look further ahead to check for '=' after ']'
            temp_lexer.next_token(); // consume '['
            // Skip tokens until we find ']' (but not past end of input)
            int bracket_depth = 1;
            while (bracket_depth > 0 && temp_lexer.peek_token().type != TokenType::END_OF_INPUT) {
                TokenType t = temp_lexer.next_token().type;
                if (t == TokenType::LBRACKET) bracket_depth++;
                else if (t == TokenType::RBRACKET) bracket_depth--;
            }
            // Now check if next token is '='
            if (temp_lexer.peek_token().type == TokenType::ASSIGN) {
                // This is an array assignment
                Token id_token = lexer.next_token();  // consume identifier
                lexer.next_token();  // consume '['
                auto index_expr = parse_expression();
                if (lexer.peek_token().type != TokenType::RBRACKET) {
                    throw std::runtime_error("Expected ']' in array assignment");
                }
                lexer.next_token();  // consume ']'
                lexer.next_token();  // consume '='
                auto value_expr = parse_expression();
                return std::make_unique<ArrayAssignmentNode>(id_token.value, std::move(index_expr), std::move(value_expr));
            }
            // Not an array assignment, fall through to expression parsing
        }
    }

    // Not an assignment, parse as regular expression
    return parse_expression();
}

// Check if token is a comparison operator
bool Parser::is_comparison_op(TokenType type) const {
    return type == TokenType::LESS || type == TokenType::GREATER ||
           type == TokenType::LESS_EQUAL || type == TokenType::GREATER_EQUAL ||
           type == TokenType::EQUAL_EQUAL || type == TokenType::NOT_EQUAL;
}

// Parse expression: bitwise_expr (('&' | '|') bitwise_expr)*
std::unique_ptr<ASTNode> Parser::parse_expression() {
    auto left = parse_comparison();
    return parse_expression_with_first_term(std::move(left));
}

// Continue parsing expression with first term already parsed
std::unique_ptr<ASTNode> Parser::parse_expression_with_first_term(std::unique_ptr<ASTNode> first_term) {
    auto left = std::move(first_term);

    // Handle bitwise operators (lowest precedence)
    while (true) {
        Token token = lexer.peek_token();
        if (token.type != TokenType::BITWISE_AND && token.type != TokenType::BITWISE_OR) {
            break;
        }

        lexer.next_token(); // consume the operator
        auto right = parse_comparison();
        left = std::make_unique<BinaryOpNode>(std::move(left), token.type, std::move(right));
    }

    return left;
}

// Parse comparison: additive (('<' | '>' | '<=' | '>=' | '==' | '!=') additive)*
std::unique_ptr<ASTNode> Parser::parse_comparison() {
    auto left = parse_additive();

    while (is_comparison_op(lexer.peek_token().type)) {
        Token token = lexer.next_token(); // consume the operator
        auto right = parse_additive();
        left = std::make_unique<BinaryOpNode>(std::move(left), token.type, std::move(right));
    }

    return left;
}

// Parse additive: term (('+' | '-') term)*
std::unique_ptr<ASTNode> Parser::parse_additive() {
    auto left = parse_term();

    while (true) {
        Token token = lexer.peek_token();
        if (token.type != TokenType::PLUS && token.type != TokenType::MINUS) {
            break;
        }

        lexer.next_token(); // consume the operator
        auto right = parse_term();
        left = std::make_unique<BinaryOpNode>(std::move(left), token.type, std::move(right));
    }

    return left;
}

// Parse term: factor (('*' | '/' | '%') factor)*
std::unique_ptr<ASTNode> Parser::parse_term() {
    auto left = parse_factor();

    while (true) {
        Token token = lexer.peek_token();
        if (token.type != TokenType::MULTIPLY && token.type != TokenType::DIVIDE && token.type != TokenType::MODULO) {
            break;
        }

        lexer.next_token(); // consume the operator
        auto right = parse_factor();
        left = std::make_unique<BinaryOpNode>(std::move(left), token.type, std::move(right));
    }

    return left;
}

// Parse factor: NUMBER | IDENTIFIER | IDENTIFIER '(' arguments ')' | '(' expression ')' | ('-'|'+') factor
std::unique_ptr<ASTNode> Parser::parse_factor() {
    Token token = lexer.next_token();
    
    switch (token.type) {
        case TokenType::NUMBER:
            return std::make_unique<NumberNode>(token.number_value);
            
        case TokenType::IDENTIFIER: {
            // Check if this is a function call
            if (lexer.peek_token().type == TokenType::LPAREN) {
                lexer.next_token(); // consume '('

                // Special handling for while() and loop() - they need AST nodes, not evaluated args
                // Bodies can contain semicolons (statement sequences), so parse specially
                if (token.value == "while") {
                    // while(condition, body)
                    auto condition = parse_expression();
                    std::unique_ptr<ASTNode> body;
                    if (lexer.peek_token().type == TokenType::COMMA) {
                        lexer.next_token(); // consume ','
                        body = parse_loop_body();  // Parse body allowing semicolons
                    } else {
                        // Single argument form: while(expr) - expr is both condition and body
                        body = std::make_unique<NumberNode>(0.0);  // No-op body
                    }
                    lexer.next_token(); // consume ')'
                    return std::make_unique<WhileNode>(std::move(condition), std::move(body));
                } else if (token.value == "loop") {
                    // loop(count, body)
                    auto count = parse_expression();
                    std::unique_ptr<ASTNode> body;
                    if (lexer.peek_token().type == TokenType::COMMA) {
                        lexer.next_token(); // consume ','
                        body = parse_loop_body();  // Parse body allowing semicolons
                    } else {
                        body = std::make_unique<NumberNode>(0.0);  // No-op body
                    }
                    lexer.next_token(); // consume ')'
                    return std::make_unique<LoopNode>(std::move(count), std::move(body));
                }

                // Regular function call
                auto func_node = std::make_unique<FunctionCallNode>(token.value);

                // Parse arguments
                if (lexer.peek_token().type != TokenType::RPAREN) {
                    func_node->arguments.push_back(parse_expression());

                    while (lexer.peek_token().type == TokenType::COMMA) {
                        lexer.next_token(); // consume ','
                        func_node->arguments.push_back(parse_expression());
                    }
                }

                lexer.next_token(); // consume ')'
                return std::move(func_node);
            } else if (lexer.peek_token().type == TokenType::LBRACKET) {
                // Array access: midi_cc[expr], midi_note[expr], midi_note_index[expr]
                lexer.next_token(); // consume '['
                auto index_expr = parse_expression();
                if (lexer.peek_token().type != TokenType::RBRACKET) {
                    throw std::runtime_error("Expected ']' after array index");
                }
                lexer.next_token(); // consume ']'
                return std::make_unique<ArrayAccessNode>(token.value, std::move(index_expr));
            } else {
                // Regular variable
                return std::make_unique<VariableNode>(token.value);
            }
        }
            
        case TokenType::LPAREN: {
            auto node = parse_expression();
            lexer.next_token(); // consume ')'
            return node;
        }
        
        case TokenType::MINUS:
        case TokenType::PLUS: {
            // Unary operator
            auto operand = parse_factor();
            return std::make_unique<UnaryOpNode>(token.type, std::move(operand));
        }
        
        default:
            // Error: unexpected token - throw for syntax errors
            throw std::runtime_error("Unexpected token in expression");
    }
}

// Parse loop body - like statement sequence but stops at ')' instead of end of input
std::unique_ptr<ASTNode> Parser::parse_loop_body() {
    auto sequence = std::make_unique<StatementSequenceNode>();

    // Parse first statement
    sequence->statements.push_back(parse_statement());

    // Parse additional statements separated by semicolons until we hit ')'
    while (lexer.peek_token().type == TokenType::SEMICOLON) {
        lexer.next_token(); // consume ';'

        // Skip empty statements (consecutive semicolons)
        while (lexer.peek_token().type == TokenType::SEMICOLON) {
            lexer.next_token();
        }

        // Stop if we hit the closing paren
        if (lexer.peek_token().type == TokenType::RPAREN) {
            break;
        }

        sequence->statements.push_back(parse_statement());
    }

    // If only one statement, return it directly
    if (sequence->statements.size() == 1) {
        return std::move(sequence->statements[0]);
    }

    return std::move(sequence);
}

} // namespace avs