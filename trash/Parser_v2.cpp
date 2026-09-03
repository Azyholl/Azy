#include "parser.h"


Parser::Parser(const std::vector<Token>& toks) : tokens(toks), currentPos(0) {}

Token Parser::peek() {
    if (currentPos < tokens.size()) {
        return tokens[currentPos];
    }
    return {TokenType::Unknown, "", -1, -1}; // Конец файла/токенов
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



// Главный цикл разбора программы
void Parser::parse() {
    while (currentPos < tokens.size()) {
        parseStatement();
    }
}

// ==========================================
// ГЛАВНОЕ ВЕТВЛЕНИЕ
// ==========================================
void Parser::parseStatement() {
    if (check(TokenType::Identifier, "if")){
        parseIF();
    } else if (check(TokenType::Identifier, "do") ){
        parseDo_While();
    }else if (check(TokenType::Identifier, "while")){
        parseWhile();
    } else if (peek().type == TokenType::Identifier && checkNext(TokenType::Operator, "=")) {
        parseAssignment();
    } else {
        // parseExpression();
        consume(); //временное решение для нереализованных токенов
    }
}

void Parser::parseBlock() {
    debugging();
    consume(); // Съедаем '{'

    while (peek().value != "}" && peek().type != TokenType::Unknown) {
        parseStatement();
    }

    if (peek().value != "}") {
         parserError(peek(), "Ожидалась закрывающая фигурная скобка '}'");
    }
    debugging();
    consume(); // Съедаем '}'
}


//Реализация конкретных функций

void Parser::parseDo_While(){
    debugging();
    Token token = consume();
    if (peek().value == "{"){
        parseBlock();
    } else {
        parserError(peek(), "Ожидался блок кода '{...}' после идентификатора", token.value);
    }

    if (check(TokenType::Identifier, "while")){
        token = peek();
        consume();
         if (peek().value != "(") {
            parserError(peek(), "Ожидалась открывающая скобка '(' после ", token.value);
        }
        consume();
        parseLogicalOr(); 

        if (peek().value != ")") {
            parserError(peek(), "Ожидалась закрывающая скобка ')' после условия ", token.value);
        }
        consume();
    } else {
        parserError(peek(), "Ожидался идентификатор while после блока кода ", token.value);
    }

}

void Parser::parseWhile(){
    Token token = peek();
    consume();
    if (peek().value != "(") {
         parserError(peek(), "Ожидалась открывающая скобка '(' после ", token.value);
    }
    consume();
    parseLogicalOr(); 

    if (peek().value != ")") {
        parserError(peek(), "Ожидалась закрывающая скобка ')' после условия ", token.value);
    }
    consume();
    if (peek().value == "{"){
        parseBlock();
    } else {
        parserError(peek(), "Ожидался блок кода '{...}' после условия ", token.value);
    }
}

void Parser::parseIF() {
    Token token = peek();
    debugging();
    consume();

    if (peek().value != "(") {
         parserError(peek(), "Ожидалась открывающая скобка '(' после ", token.value);
    }
    consume();
    parseLogicalOr(); 

    if (peek().value != ")") {
        parserError(peek(), "Ожидалась закрывающая скобка ')' после условия ", token.value);
    }
    consume();

    if (peek().value == "{"){
        parseBlock();
    } else {
        parserError(peek(), "Ожидался блок кода '{...}' после условия ", token.value);
    }

    if (check(TokenType::Identifier, "else")) {
        parseElse();
    }
}

void Parser::parseElse(){
    debugging();
    consume();
    if (check(TokenType::Identifier, "if")) {
        parseIF();
    } else if (peek().value == "{"){
        parseBlock();
    } else {
        parserError(peek(), "Ожидался блок кода '{...}' после условия else");
    }
}

void Parser::parseAssignment() {
    Token varName = consume(); // Забираем имя переменной (идентификатор)
    Token op = consume();      // Забираем знак '='
    parseLogicalOr(); 
    std::cout << "[Парсер] Присваивание переменной: " << varName.value << "\n";
}

//Вспомогательный блок условий кода
//Проверяет на выполнение условий
void Parser::parseLogicalOr(){
   parseLogicalAnd();

   while (true) {
        if (peek().value == "||") {
            consume(); 
            parseLogicalAnd(); 
        } else {
            break;
        }
    }
} 

void Parser::parseLogicalAnd(){
   parseEquality();

   while (true) {
        if (peek().value == "&&") {
            consume(); 
            parseEquality(); 
        } else {
            break;
        }
    }
}

void Parser::parseEquality(){
   parseRelational();

   while (true) {
        if (peek().value == "==" || peek().value == "!=") {
            consume(); 
            parseRelational(); 
        } else {
            break;
        }
    }
}

void Parser::parseRelational(){
   parseAdditive();

   while (true) {
        if (peek().value == "<" || peek().value == ">" || peek().value == "<=" || peek().value == ">=") {
            consume(); 
            parseAdditive(); 
        } else {
            break;
        }
    }
}

void Parser::parseAdditive(){
   parseMultiplicative();

   while (true) {
        if (peek().value == "+" || peek().value == "-") {
            consume(); 
            parseMultiplicative(); 
        } else {
            break;
        }
    }
}

void Parser::parseMultiplicative(){
   parseUnary();

   while (true) {
        if (peek().value == "*" || peek().value == "/") {
            consume(); 
            parseUnary(); 
        } else {
            break;
        }
    }
}

void Parser::parseUnary(){
    while (true) {
        if (peek().value == "!" || peek().value == "not" || peek().value == "-") {
            consume();         
        } else {
            break;
        }
    }
    parsePrimary(); 
}

void Parser::parsePrimary(){
   if (peek().type == TokenType::Int || peek().type == TokenType::Float || 
        peek().type == TokenType::Identifier || peek().type == TokenType::String) {
        consume(); 
    } 
    else if (peek().value == "(") {
        consume(); // Съедаем открывающую скобку
        parseLogicalOr(); 
        
        Token closing = consume();
        if (closing.value != ")") {
             parserError(closing, "Ожидалась закрывающая скобка");
        }
    } 
    else {
         parserError(peek(), "Синтаксическая ошибка: неожиданный токен '" + peek().value + "'");
    }
}
// Конец блока