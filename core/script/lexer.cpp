// avs_lib - Portable Advanced Visualization Studio library
// Based on Advanced Visualization Studio by Nullsoft, Inc.
// AVS Copyright (C) 2005 Nullsoft, Inc.
// C++20 port Copyright (C) 2025 Tim Redfern
// Licensed under MIT License

#include "lexer.h"
#include <cctype>

namespace avs {

Lexer::Lexer(const std::string& input) : input(input), current_token(TokenType::END_OF_INPUT) {
}

Token Lexer::next_token() {
    if (has_current) {
        has_current = false;
        return current_token;
    }
    
    skip_whitespace();
    
    if (position >= input.length()) {
        return Token(TokenType::END_OF_INPUT);
    }
    
    char ch = input[position];
    
    if (std::isdigit(ch) || ch == '.') {
        return read_number();
    }
    
    if (std::isalpha(ch) || ch == '_' || ch == '$') {
        return read_identifier();
    }
    
    // Multi-character and single-character tokens
    position++;
    switch (ch) {
        case '+': return Token(TokenType::PLUS);
        case '-': return Token(TokenType::MINUS);
        case '*': return Token(TokenType::MULTIPLY);
        case '/':
            // Check for comment: //
            if (position < input.length() && input[position] == '/') {
                // Skip to end of line
                while (position < input.length() && input[position] != '\n') {
                    position++;
                }
                // Recursively get next real token
                return next_token();
            }
            return Token(TokenType::DIVIDE);
        case '%': return Token(TokenType::MODULO);
        case '&': return Token(TokenType::BITWISE_AND);
        case '|': return Token(TokenType::BITWISE_OR);
        case '<':
            // Check for <=
            if (position < input.length() && input[position] == '=') {
                position++;
                return Token(TokenType::LESS_EQUAL);
            }
            return Token(TokenType::LESS);
        case '>':
            // Check for >=
            if (position < input.length() && input[position] == '=') {
                position++;
                return Token(TokenType::GREATER_EQUAL);
            }
            return Token(TokenType::GREATER);
        case '=':
            // Check for ==
            if (position < input.length() && input[position] == '=') {
                position++;
                return Token(TokenType::EQUAL_EQUAL);
            }
            return Token(TokenType::ASSIGN);
        case '!':
            // Check for !=
            if (position < input.length() && input[position] == '=') {
                position++;
                return Token(TokenType::NOT_EQUAL);
            }
            // Standalone ! is not supported, skip it
            return next_token();
        case '(': return Token(TokenType::LPAREN);
        case ')': return Token(TokenType::RPAREN);
        case '[': return Token(TokenType::LBRACKET);
        case ']': return Token(TokenType::RBRACKET);
        case ',': return Token(TokenType::COMMA);
        case ';': return Token(TokenType::SEMICOLON);
        default:
            // Unknown character, skip it
            return next_token();
    }
}

Token Lexer::peek_token() {
    if (!has_current) {
        current_token = next_token();
        has_current = true;
    }
    return current_token;
}

void Lexer::skip_whitespace() {
    while (position < input.length() && std::isspace(input[position])) {
        position++;
    }
}

Token Lexer::read_number() {
    size_t start = position;
    
    while (position < input.length() && 
           (std::isdigit(input[position]) || input[position] == '.')) {
        position++;
    }
    
    std::string value = input.substr(start, position - start);
    return Token(TokenType::NUMBER, value);
}

Token Lexer::read_identifier() {
    size_t start = position;
    
    while (position < input.length() && 
           (std::isalnum(input[position]) || input[position] == '_' || input[position] == '$')) {
        position++;
    }
    
    std::string value = input.substr(start, position - start);
    return Token(TokenType::IDENTIFIER, value);
}

} // namespace avs