#include "parser.h"
#include <iostream>
#include <stdexcept>

Parser::Parser(const std::vector<Token>& toks) : tokens(toks), currentPos(0) {}

Token Parser::peek() {
    if (currentPos < tokens.size()) {
        return tokens[currentPos];
    }
    return {TokenType::Unknown, "", -1, -1}; // Конец файла/токенов
}

Token Parser::consume() {
    Token tok = peek();
    if (currentPos < tokens.size()) {
        currentPos++;
    }
    return tok;
}

bool Parser::check(TokenType type, const std::string& value) {
    Token tok = peek();
    return (tok.type == type && tok.value == value);
}

// Главный цикл разбора программы
void Parser::parse() {
    while (currentPos < tokens.size()) {
        parseStatement();
    }
}

void Parser::parserError(const Token& token, const std::string& message) {
    std::string errorMsg = "[Ошибка парсера] " + message + " (строка: " + std::to_string(token.line) + ", символ: " + std::to_string(token.column) + ", токен: '" + token.value + "')";
    throw std::runtime_error(errorMsg);
}

// ==========================================
// ГЛАВНОЕ ВЕТВЛЕНИЕ
// ==========================================
void Parser::parseStatement() {
    Token tok = peek();

    // if (tok.type == TokenType::Identifier && tok.value == "func") {
    //     parseFunc();
    // }
    // else 
    if (tok.type == TokenType::Identifier && tok.value == "if") {
        parseIf();
    }
    // else if (tok.type == TokenType::Operator && tok.value == "{") {
    //     parseBlock();
    // }
    else if (tok.type == TokenType::Identifier && tok.value == "print") {
        parsePrint();
    }
    // else if (tok.type == TokenType::Identifier && tok.value == "print") {

    // 5. По умолчанию: скип
    else {
        // parseExpression();
        consume(); //временное решение для нереализованных токенов
    }
}

void Parser::parseFunc(){
    consume();
    // Дописать функции
}

// --- Реализация конкретных разборщиков ---
// if(true){ parseStatement }
void Parser::parseIf() {
    consume(); // Съедаем токен "if"
    std::cout << "[Парсер] Обнаружено ветвление IF\n";

    // 1. Проверяем и съедаем открывающую скобку условия
    if (peek().value != "(") {
         parserError(peek(), "Ожидалась открывающая скобка '(' после 'if'");
    }
    consume(); // Съедаем '('

    parseExpression(); 

    // 3. Проверяем и съедаем закрывающую скобку
    if (peek().value != ")") {
        parserError(peek(), "Ожидалась закрывающая скобка ')' после условия if");
    }
    consume(); // Съедаем ')'

    // 4. Разбираем тело блока кода
    if (peek().value == "{"){
        parseBlock();
    } else {
        parserError(peek(), "Ожидался блок кода '{...}' после условия if");
    }

    // 5. проверка наличия ветки else
    if (check(TokenType::Identifier, "else")) {
        consume(); // Съедаем "else"
        std::cout << "[Парсер] Обнаружена ветка ELSE\n";
        parseElse(); // Разбираем тело else
    }
}

void Parser::parseElse(){
    std::cout << "[Парсер] Обнаружено ветвление ELSE\n";

    if (peek().type == TokenType::Identifier && peek().value == "if") {
        parseIf();
    } else if (peek().value == "{"){
        parseBlock();
    } else {
        parserError(peek(), "Ожидался блок кода '{...}' после условия else");
    }
}


void Parser::parseBlock() {
    consume(); // Съедаем '{'
    std::cout << "[Парсер] Начало блока кода\n";

    while (peek().value != "}" && peek().type != TokenType::Unknown) {
        parseStatement();
    }

    if (peek().value != "}") {
         parserError(peek(), "Ожидалась закрывающая фигурная скобка '}'");
    }
    consume(); // Съедаем '}'
    std::cout << "[Парсер] Конец блока кода\n";
}



void Parser::parseExpression() {
    // 1. Разбираем левую часть (операнд или фактор)
    parseFactor();

    // 2. Проверяем наличие операторов (сравнения или арифметики)
    Token tok = peek();
    while (tok.value == "==" || tok.value == "!=" || tok.value == "<" || 
           tok.value == ">" || tok.value == "&&" || tok.value == "||" || 
           tok.value == "+" || tok.value == "-") {
        consume(); // Съедаем оператор
        parseFactor(); // Разбираем правую часть выражения
        tok = peek();
    }
}

void Parser::parseFactor() {
    if (peek().value == "!" || peek().value == "not") {
        consume();
    }

    if (peek().type == TokenType::Int || peek().type == TokenType::Float || 
        peek().type == TokenType::Identifier || peek().type == TokenType::String) {
        consume(); 
    } 
    else if (peek().value == "(") {
        consume(); // Съедаем открывающую скобку
        parseExpression(); 
        
        Token closing = consume();
        if (closing.value != ")") {
             parserError(peek(), "Ожидалась закрывающая скобка");
        }
    } 
    else {
         parserError(peek(), "Синтаксическая ошибка: неожиданный токен '" + peek().value + "'");
    }
}



void Parser::parsePrint() {
    consume(); // Съедаем ключевое слово "print" (или идентификатор)
    std::cout << "[Парсер] Обнаружен оператор печати\n";

    // Проверяем опциональный модификатор .ln (например, print.ln)
    if (check(TokenType::Operator, ".")) {
        consume(); // Съедаем точку
        
        if (check(TokenType::Identifier, "ln")) {
            consume(); // Съедаем "ln"
            std::cout << "[Парсер] Модификатор: перевод строки (ln)\n";
        } else {
            parserError(peek(), "Ожидался модификатор 'ln' после точки");
        }
    }

    // Проверяем открывающую скобку для аргументов печати
    if (peek().value != "(") {
        parserError(peek(), "Ожидалась открывающая скобка '(' после print");
    }
    consume(); // Съедаем '('

    // Разбираем выражение, которое нужно вывести
    parseExpression();

    // Проверяем закрывающую скобку
    if (peek().value != ")") {
        parserError(peek(), "Ожидалась закрывающая скобка ')' после аргументов print");
    }
    consume(); // Съедаем ')'
}