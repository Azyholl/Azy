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

void Parser::Close_block(){
    if (check(TokenType::Operator, ";")){
        consume();
    } else {
        parserError(peek(), "Синтаксическая ошибка: ожидался терминатор: '" + peek().value + "'");
    }
}

// ============================================================
// Главный цикл разбора – возвращает корневой блок
// ============================================================
std::unique_ptr<ASTNode> Parser::parse() {
    auto block = std::make_unique<BlockNode>();
    while (currentPos < tokens.size()) {
        size_t before = currentPos;
        auto stmt = parseStatement();
        if (stmt) {
            block->statements.push_back(std::move(stmt));
        } else if (currentPos == before) {
            consume();   // защита от зацикливания
        }
        // else: stmt == nullptr, но позиция сдвинулась — продолжаем
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
    } else if (check(TokenType::Identifier, "for")) {
        return parseFor();
    } else if (check(TokenType::Identifier, "break")) {
        return parseBreak();
    } else if (check(TokenType::Identifier, "continue")) {      
        return parseContinue();
    } else if (check(TokenType::Identifier, "print")) {
        return parsePrint();

    } else if (peek().type == TokenType::Identifier &&
            (checkNext(TokenType::Operator, "=")  ||
             checkNext(TokenType::Operator, "+=") ||
             checkNext(TokenType::Operator, "-=") ||
             checkNext(TokenType::Operator, "*=") ||
             checkNext(TokenType::Operator, "/="))
        ) {
            return parseAssignment();
    } else if (peek().type == TokenType::Identifier &&
            (checkNext(TokenType::Operator, "++") ||
                checkNext(TokenType::Operator, "--"))) {
        return parsePostfixStatement();
    } 
    else {
        std::cerr << "[Парсер] Пропущен неожиданный токен: '"
                << peek().value << "' (строка " << peek().line << ")\n";
        consume();
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
        Close_block();
        return std::make_unique<DoWhileNode>(std::move(body), std::move(condition));
    } else {
        parserError(peek(), "Ожидался идентификатор 'while' после блока кода");
        return nullptr;
    }
}

// Узел while (BinaryOpNode) {}
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

// ============================================================
// Цикл for: три формы
//   for (i == x)               — сокращённая
//   for (i = 0; i == x; i++)   — полная
//   for (i in (int/string/array)) — foreach
// ============================================================
std::unique_ptr<ASTNode> Parser::parseFor() {
    debugging();
    consume(); // "for"

    if (peek().value != "(") {
        parserError(peek(), "Ожидалась '(' после 'for'");
        return nullptr;
    }
    consume(); // '('

    if (peek().type != TokenType::Identifier) {
        parserError(peek(), "Ожидался идентификатор переменной цикла");
        return nullptr;
    }

    std::string varName = peek().value;
    Token next = peekNext();

    // ================== ФОРМА 3: for (i in ...) ==================
    if (next.type == TokenType::Identifier && next.value == "in") {
        consume(); // i
        consume(); // in

        auto iterable = parseLogicalOr();
        if (!iterable) return nullptr;

        if (peek().value != ")") {
            parserError(peek(), "Ожидалась ')' после for-in");
            return nullptr;
        }
        consume(); // ')'

        auto body = parseForBody();
        if (!body) return nullptr;

        auto node = std::make_unique<ForNode>();
        node->varName   = varName;
        node->iterable  = std::move(iterable);
        node->isForeach = true;
        node->body      = std::move(body);
        return node;
    }

    // ================== ФОРМА 1: for (i == x) ==================
    if (next.type == TokenType::Operator && next.value == "==") {
        // ВАЖНО: НЕ consume() — пусть parseLogicalOr() сам разберёт "i == 5"
        auto cond = parseLogicalOr();
        if (!cond) return nullptr;

        if (peek().value != ")") {
            parserError(peek(), "Ожидалась ')' после условия for");
            return nullptr;
        }
        consume(); // ')'

        auto body = parseForBody();
        if (!body) return nullptr;

        auto node = std::make_unique<ForNode>();
        node->varName   = varName;
        node->init      = nullptr;   // по умолчанию i = 0
        node->condition = std::move(cond);
        node->step      = nullptr;   // по умолчанию i++
        node->body      = std::move(body);
        return node;
    }

    // ================== ФОРМА 2: for (i = 0; i == x; i++) ==================
    if (next.type == TokenType::Operator && next.value == "=") {
        auto init = parseForInit();
        if (!init) return nullptr;

        if (peek().value != ";") {
            parserError(peek(), "Ожидался ';' после инициализации for");
            return nullptr;
        }
        consume(); // ';'

        auto cond = parseLogicalOr();
        if (!cond) return nullptr;

        if (peek().value != ";") {
            parserError(peek(), "Ожидался ';' после условия for");
            return nullptr;
        }
        consume(); // ';'

        auto step = parseForStep();
        if (!step) return nullptr;

        if (peek().value != ")") {
            parserError(peek(), "Ожидалась ')' после шага for");
            return nullptr;
        }
        consume(); // ')'

        auto body = parseForBody();
        if (!body) return nullptr;

        auto node = std::make_unique<ForNode>();
        node->varName   = varName;
        node->init      = std::move(init);
        node->condition = std::move(cond);
        node->step      = std::move(step);
        node->body      = std::move(body);
        return node;
    }

    parserError(peek(), "Неизвестная форма for: ожидалось '=', '==' или 'in' после переменной");
    return nullptr;
}

// Отдельный оператор: i++;  или  i--;
std::unique_ptr<ASTNode> Parser::parsePostfixStatement() {
    Token name = consume();
    std::string op = consume().value;
    auto var = std::make_unique<VariableNode>(name.value);
    Close_block();
    auto unary = std::make_unique<UnaryOpNode>(op + "_post", std::move(var));
    return std::make_unique<ExpressionStatementNode>(std::move(unary));
}

// init в полной форме: i = <expr>
std::unique_ptr<ASTNode> Parser::parseForInit() {
    if (peek().type != TokenType::Identifier) {
        parserError(peek(), "Ожидался идентификатор в инициализации for");
        return nullptr;
    }
    std::string name = consume().value; // i
    if (!check(TokenType::Operator, "=")) {
        parserError(peek(), "Ожидался '=' в инициализации for");
        return nullptr;
    }
    consume(); // '='
    auto expr = parseLogicalOr();
    return std::make_unique<AssignmentNode>(name, "=", std::move(expr));
}

// step в полной форме: i++ / i-- / i = expr
std::unique_ptr<ASTNode> Parser::parseForStep() {
    if (peek().type != TokenType::Identifier) {
        parserError(peek(), "Ожидался идентификатор в шаге for");
        return nullptr;
    }
    std::string name = consume().value; // i

    // i++ / i--
    if (peek().value == "++" || peek().value == "--") {
        std::string op = consume().value;
        auto var = std::make_unique<VariableNode>(name);
        return std::make_unique<UnaryOpNode>(op + "_post", std::move(var));
    }

    // i = expr
    if (check(TokenType::Operator, "=")) {
        consume();
        auto expr = parseLogicalOr();
        return std::make_unique<AssignmentNode>(name, "=", std::move(expr));
    }

    parserError(peek(), "Ожидался '++', '--' или '=' в шаге for");
    return nullptr;
}

// тело цикла — блок { ... }
std::unique_ptr<ASTNode> Parser::parseForBody() {
    if (peek().value != "{") {
        parserError(peek(), "Ожидался блок кода '{...}' после for");
        return nullptr;
    }
    return parseBlock();
}

// break;
std::unique_ptr<ASTNode> Parser::parseBreak() {
    consume();          // "break"
    Close_block();      // ';'
    return std::make_unique<BreakNode>();
}

// continue;
std::unique_ptr<ASTNode> Parser::parseContinue() {
    consume();          // "continue"
    Close_block();      // ';'
    return std::make_unique<ContinueNode>();
}

// Узел If (BinaryOpNode) {Block_code}
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

// Узел else {Block_code}
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

// Узел вывода текста
std::unique_ptr<ASTNode> Parser::parsePrint() {
    debugging();
    consume();

    bool newline = false;

    // Проверяем опциональный модификатор .ln
    if (check(TokenType::Operator, ".")) {
        consume(); // съедаем точку
        if (check(TokenType::Identifier, "ln")) {
            consume(); // съедаем "ln"
            newline = true;
        } else {
            parserError(peek(), "Ожидался модификатор 'ln' после точки в print");
        }
    }
    if (peek().value != "(") {
        parserError(peek(), "Ожидалась открывающая скобка '(' после print");
    }
    consume(); // '('

    auto expr = parseLogicalOr();
    if (peek().value != ")") {
        parserError(peek(), "Ожидалась закрывающая скобка ')' после аргументов print");
    }
    consume(); // ')'
    Close_block();
    return std::make_unique<PrintNode>(std::move(expr), newline);
}

// Узел присваивания 
// Identificator = parseLogicalOr
std::unique_ptr<ASTNode> Parser::parseAssignment() {
    Token varName = consume(); // идентификатор
    std::string op = consume().value;
    auto expr = parseLogicalOr();
    Close_block();
    return std::make_unique<AssignmentNode>(varName.value, op, std::move(expr));
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
        if (tok.value == "true" || tok.value == "false") {
            return std::make_unique<LiteralNode>(LiteralNode::Bool, tok.value);
        }

        auto var = std::make_unique<VariableNode>(tok.value);
        if (peek().value == "++" || peek().value == "--") {
            std::string op = consume().value;
            return std::make_unique<UnaryOpNode>(op + "_post", std::move(var));
        }
        return var;
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

// ============================================================
// 
// ============================================================