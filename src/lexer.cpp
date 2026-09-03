#include "lexer.h"
#include <cctype>

int next_char(FILE *stream, int &line, int &column) {
    int c = fgetc(stream);
    if (c == '\n') {
        line++;
        column = 1;
    } else {
        column++;
    }
    return c;
}

int peek_next(FILE *stream) {
    int next = fgetc(stream);
    if (next != EOF) {
        ungetc(next, stream);
    }
    return next;
}

Token gettok(FILE *stream) {
    static int LastChar = ' ';
    static int current_line = 1;
    static int current_col = 1;

    // Пропуск пробельных символов
    while (isspace(static_cast<unsigned char>(LastChar))) {
        LastChar = next_char(stream, current_line, current_col);
    }

    if (LastChar == EOF) {
        return {TokenType::Unknown, "", current_line, current_col};
    }

    int token_line = current_line;
    int token_col = current_col;

    // 1. Распознавание идентификаторов
    if (isalpha(static_cast<unsigned char>(LastChar))) {
        std::string identifierStr;
        identifierStr += static_cast<char>(LastChar);
        while (isalnum(static_cast<unsigned char>(LastChar = next_char(stream, current_line, current_col)))) {
            identifierStr += static_cast<char>(LastChar);
        }
        return {TokenType::Identifier, identifierStr, token_line, token_col};
    }

    // 2. Распознавание чисел (Int и Float)
    if (isdigit(static_cast<unsigned char>(LastChar)) || LastChar == '.') {
        std::string numStr;
        bool isFloat = false;

        // Если число начинается с точки (например, .5)
        if (LastChar == '.') {
            isFloat = true;
            numStr += '.';
            LastChar = next_char(stream, current_line, current_col);
            
            // Если после точки не цифра, это просто оператор точки (доступ к полям)
            if (!isdigit(static_cast<unsigned char>(LastChar))) {
                return {TokenType::Operator, ".", token_line, token_col};
            }
        }

        // Читаем целую часть (или цифры после начальной точки)[cite: 1]
        while (isdigit(static_cast<unsigned char>(LastChar))) {
            numStr += static_cast<char>(LastChar);
            LastChar = next_char(stream, current_line, current_col);
        }

        // Если встретили точку после целой части (например, 3.14)[cite: 1]
        if (!isFloat && LastChar == '.') {
            isFloat = true;
            numStr += '.';
            LastChar = next_char(stream, current_line, current_col);

            // Читаем дробную часть[cite: 1]
            while (isdigit(static_cast<unsigned char>(LastChar))) {
                numStr += static_cast<char>(LastChar);
                LastChar = next_char(stream, current_line, current_col);
            }
        }

        TokenType type = isFloat ? TokenType::Float : TokenType::Int;
        return {type, numStr, token_line, token_col};
    }

    // 3. Обработка комментариев
    if (LastChar == '#') {
        do {
            LastChar = next_char(stream, current_line, current_col);
        } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');
        
        if (LastChar != EOF) {
            return gettok(stream); 
        }
        return {TokenType::Unknown, "", token_line, token_col};
    }

    // 4. Строки в кавычках
    if (LastChar == '"') {
        std::string strVal;
        LastChar = next_char(stream, current_line, current_col); 
        while (LastChar != EOF && LastChar != '"') {
            strVal += static_cast<char>(LastChar);
            LastChar = next_char(stream, current_line, current_col);
        }
        if (LastChar == '"') {
            LastChar = next_char(stream, current_line, current_col); 
        }
        return {TokenType::String, strVal, token_line, token_col};
    }

    // 5. Операторы и прочие символы
    char c = static_cast<char>(LastChar);
    LastChar = next_char(stream, current_line, current_col); 

    std::string opStr(1, c);

    if (c == '=' && LastChar == '=') {
        opStr += '=';
        LastChar = next_char(stream, current_line, current_col);
    } else if (c == '!' && LastChar == '=') {
        opStr += '=';
        LastChar = next_char(stream, current_line, current_col);
    } else if (c == '<' && LastChar == '=') {
        opStr += '=';
        LastChar = next_char(stream, current_line, current_col);
    } else if (c == '>' && LastChar == '=') {
        opStr += '=';
        LastChar = next_char(stream, current_line, current_col);
    } else if (c == '&' && LastChar == '&') {
        opStr += '&';
        LastChar = next_char(stream, current_line, current_col);
    } else if (c == '|' && LastChar == '|') {
        opStr += '|';
        LastChar = next_char(stream, current_line, current_col);
    }

    TokenType type = TokenType::Operator;
    if (!ispunct(static_cast<unsigned char>(c))) {
        type = TokenType::Unknown;
    }

    return {type, opStr, token_line, token_col};
}