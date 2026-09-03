#include "parser.h"
#include <memory>
#include <vector>
#include <string>
#include <iostream>

// ============================================================
// Реализация Parser
// ============================================================

Parser::Parser(const std::vector<Token>& toks) : tokens(toks), currentPos(0) {}

Token Parser::peek() {
    if (currentPos < tokens.size()) {
        return tokens[currentPos];
    }
    return {TokenType::Unknown, "", -1, -1};
}

Token Parser::peekNext() {
    if (currentPos + 1 < tokens.size()) {
        return tokens[currentPos + 1];
    }
    return {TokenType::Unknown, "", -1, -1};
}

Token Parser::consume() {
    Token tok = peek();
    if (currentPos < tokens.size()) {
        currentPos++;
    }
    return tok;
}

bool Parser::check(TokenType type, const std::string& value) {
    return (peek().type == type && peek().value == value);
}

bool Parser::checkNext(TokenType type, const std::string& value) {
    return (peekNext().type == type && peekNext().value == value);
}

// ============================================================
// Главный цикл разбора – возвращает корневой блок
// ============================================================
std::unique_ptr<ASTNode> Parser::parse() {
    auto block = std::make_unique<BlockNode>();
    while (currentPos < tokens.size()) {
        auto stmt = parseStatement();
        if (stmt) {
            block->statements.push_back(std::move(stmt));
        } else {
            // Если оператор не распознан – ошибка, но parseStatement уже вызовет parserError
            break;
        }
    }
    return block;
}

// ============================================================
// Разбор операторов (возвращают узел AST)
// ============================================================
std::unique_ptr<ASTNode> Parser::parseStatement() {
    if (check(TokenType::Identifier, "if")) {
        return parseIf();
    } else if (check(TokenType::Identifier, "do")) {
        return parseDoWhile();
    } else if (check(TokenType::Identifier, "while")) {
        return parseWhile();
    } else if (peek().type == TokenType::Identifier && checkNext(TokenType::Operator, "=")) {
        return parseAssignment();
    } else {
        parserError(peek(), "Синтаксическая ошибка: неожиданный токен '" + peek().value + "'");
        consume(); // чтобы избежать бесконечного цикла
        return nullptr;
    }
}

std::unique_ptr<ASTNode> Parser::parseBlock() {
    debugging();
    consume(); // '{'

    auto block = std::make_unique<BlockNode>();
    while (peek().value != "}" && peek().type != TokenType::Unknown) {
        auto stmt = parseStatement();
        if (stmt) {
            block->statements.push_back(std::move(stmt));
        }
    }

    if (peek().value != "}") {
        parserError(peek(), "Ожидалась закрывающая фигурная скобка '}'");
    }
    debugging();
    consume(); // '}'
    return block;
}

// ============================================================
// Конкретные операторы
// ============================================================
std::unique_ptr<ASTNode> Parser::parseDoWhile() {
    debugging();
    Token token = consume(); // "do"

    std::unique_ptr<ASTNode> body;
    if (peek().value == "{") {
        body = parseBlock();
    } else {
        parserError(peek(), "Ожидался блок кода '{...}' после 'do'");
        return nullptr;
    }

    if (check(TokenType::Identifier, "while")) {
        token = consume(); // "while"
        if (peek().value != "(") {
            parserError(peek(), "Ожидалась открывающая скобка '(' после 'while'");
            return nullptr;
        }
        consume(); // '('
        auto condition = parseLogicalOr();
        if (peek().value != ")") {
            parserError(peek(), "Ожидалась закрывающая скобка ')' после условия");
            return nullptr;
        }
        consume(); // ')'
        return std::make_unique<DoWhileNode>(std::move(body), std::move(condition));
    } else {
        parserError(peek(), "Ожидался идентификатор 'while' после блока кода");
        return nullptr;
    }
}

std::unique_ptr<ASTNode> Parser::parseWhile() {
    debugging();
    Token token = consume(); // "while"

    if (peek().value != "(") {
        parserError(peek(), "Ожидалась открывающая скобка '(' после 'while'");
        return nullptr;
    }
    consume(); // '('
    auto condition = parseLogicalOr();
    if (peek().value != ")") {
        parserError(peek(), "Ожидалась закрывающая скобка ')' после условия");
        return nullptr;
    }
    consume(); // ')'

    std::unique_ptr<ASTNode> body;
    if (peek().value == "{") {
        body = parseBlock();
    } else {
        parserError(peek(), "Ожидался блок кода '{...}' после условия");
        return nullptr;
    }
    return std::make_unique<WhileNode>(std::move(condition), std::move(body));
}

std::unique_ptr<ASTNode> Parser::parseIf() {
    debugging();
    Token token = consume(); // "if"

    if (peek().value != "(") {
        parserError(peek(), "Ожидалась открывающая скобка '(' после 'if'");
        return nullptr;
    }
    consume(); // '('
    auto condition = parseLogicalOr();
    if (peek().value != ")") {
        parserError(peek(), "Ожидалась закрывающая скобка ')' после условия");
        return nullptr;
    }
    consume(); // ')'

    std::unique_ptr<ASTNode> thenBody;
    if (peek().value == "{") {
        thenBody = parseBlock();
    } else {
        parserError(peek(), "Ожидался блок кода '{...}' после условия");
        return nullptr;
    }

    std::unique_ptr<ASTNode> elseBody = nullptr;
    if (check(TokenType::Identifier, "else")) {
        elseBody = parseElse();
    }
    return std::make_unique<IfNode>(std::move(condition), std::move(thenBody), std::move(elseBody));
}

std::unique_ptr<ASTNode> Parser::parseElse() {
    debugging();
    consume(); // "else"

    if (check(TokenType::Identifier, "if")) {
        return parseIf(); // else if
    } else if (peek().value == "{") {
        return parseBlock();
    } else {
        parserError(peek(), "Ожидался блок кода '{...}' после 'else'");
        return nullptr;
    }
}

std::unique_ptr<ASTNode> Parser::parseAssignment() {
    Token varName = consume(); // идентификатор
    consume(); // '='
    auto expr = parseLogicalOr();
    return std::make_unique<AssignmentNode>(varName.value, std::move(expr));
}

// ============================================================
// Выражения (возвращают ExpressionNode)
// ============================================================
std::unique_ptr<ExpressionNode> Parser::parseLogicalOr() {
    auto left = parseLogicalAnd();
    while (true) {
        if (peek().value == "||") {
            consume();
            auto right = parseLogicalAnd();
            left = std::make_unique<BinaryOpNode>("||", std::move(left), std::move(right));
        } else {
            break;
        }
    }
    return left;
}

std::unique_ptr<ExpressionNode> Parser::parseLogicalAnd() {
    auto left = parseEquality();
    while (true) {
        if (peek().value == "&&") {
            consume();
            auto right = parseEquality();
            left = std::make_unique<BinaryOpNode>("&&", std::move(left), std::move(right));
        } else {
            break;
        }
    }
    return left;
}

std::unique_ptr<ExpressionNode> Parser::parseEquality() {
    auto left = parseRelational();
    while (true) {
        if (peek().value == "==" || peek().value == "!=") {
            std::string op = peek().value;
            consume();
            auto right = parseRelational();
            left = std::make_unique<BinaryOpNode>(op, std::move(left), std::move(right));
        } else {
            break;
        }
    }
    return left;
}

std::unique_ptr<ExpressionNode> Parser::parseRelational() {
    auto left = parseAdditive();
    while (true) {
        if (peek().value == "<" || peek().value == ">" ||
            peek().value == "<=" || peek().value == ">=") {
            std::string op = peek().value;
            consume();
            auto right = parseAdditive();
            left = std::make_unique<BinaryOpNode>(op, std::move(left), std::move(right));
        } else {
            break;
        }
    }
    return left;
}

std::unique_ptr<ExpressionNode> Parser::parseAdditive() {
    auto left = parseMultiplicative();
    while (true) {
        if (peek().value == "+" || peek().value == "-") {
            std::string op = peek().value;
            consume();
            auto right = parseMultiplicative();
            left = std::make_unique<BinaryOpNode>(op, std::move(left), std::move(right));
        } else {
            break;
        }
    }
    return left;
}

std::unique_ptr<ExpressionNode> Parser::parseMultiplicative() {
    auto left = parseUnary();
    while (true) {
        if (peek().value == "*" || peek().value == "/") {
            std::string op = peek().value;
            consume();
            auto right = parseUnary();
            left = std::make_unique<BinaryOpNode>(op, std::move(left), std::move(right));
        } else {
            break;
        }
    }
    return left;
}

std::unique_ptr<ExpressionNode> Parser::parseUnary() {
    if (peek().value == "!" || peek().value == "not" || peek().value == "-") {
        std::string op = peek().value;
        consume();
        auto operand = parseUnary(); // унарные операции могут быть вложенными
        return std::make_unique<UnaryOpNode>(op, std::move(operand));
    } else {
        return parsePrimary();
    }
}

std::unique_ptr<ExpressionNode> Parser::parsePrimary() {
    Token tok = peek();
    if (tok.type == TokenType::Int) {
        consume();
        return std::make_unique<LiteralNode>(LiteralNode::Int, tok.value);
    } else if (tok.type == TokenType::Float) {
        consume();
        return std::make_unique<LiteralNode>(LiteralNode::Float, tok.value);
    } else if (tok.type == TokenType::String) {
        consume();
        return std::make_unique<LiteralNode>(LiteralNode::String, tok.value);
    } else if (tok.type == TokenType::Identifier) {
        consume();
        return std::make_unique<VariableNode>(tok.value);
    } else if (tok.value == "(") {
        consume(); // '('
        auto expr = parseLogicalOr();
        if (peek().value != ")") {
            parserError(peek(), "Ожидалась закрывающая скобка ')'");
        }
        consume(); // ')'
        return expr;
    } else {
        parserError(tok, "Синтаксическая ошибка: неожиданный токен '" + tok.value + "'");
        return nullptr;
    }
}