#include <iostream>
#include <string>
#include <cstdio>
#include <cctype>

// Категории токенов
enum class TokenType {
    Identifier, // переменные
    Int,        // целые числа
    Float,      // числа с плавающей точкой
    Operator,   // знаки операций
    String,     // строки
    Unknown     // Неизвестные
};

// Структура для хранения одного токена с позицией
struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;
};

// Вспомогательная функция для чтения с подсчетом строк и колонок
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

int peek_prev(FILE *stream) {
    long current_pos = ftell(stream);
    if (current_pos <= 0) return EOF;

    fseek(stream, -1, SEEK_CUR);
    int prev_char = fgetc(stream);
    
    fseek(stream, current_pos, SEEK_SET); 

    return prev_char;
}

// Функция для получения следующего токена из потока
Token gettok(FILE *stream) {
    static int LastChar = ' ';
    static int current_line = 1;
    static int current_col = 1;

    // Пропуск всех пробельных символов с обновлением координат
    while (isspace(static_cast<unsigned char>(LastChar))) {
        LastChar = next_char(stream, current_line, current_col);
    }

    if (LastChar == EOF) {
        return {TokenType::Unknown, "", current_line, current_col};
    }

    // Запоминаем позицию начала токена
    int token_line = current_line;
    int token_col = current_col;

    // 1. Распознавание идентификаторов (начинаются с буквы)
    if (isalpha(static_cast<unsigned char>(LastChar))) {
        std::string identifierStr;
        identifierStr += static_cast<char>(LastChar);
        while (isalnum(static_cast<unsigned char>(LastChar = next_char(stream, current_line, current_col)))) {
            identifierStr += static_cast<char>(LastChar);
        }
        return {TokenType::Identifier, identifierStr, token_line, token_col};
    }

    // 2. Распознавания событий с точкой
    if (LastChar == '.') {
        
    }

    // 3. Распознавание чисел (Int)
    if (isdigit(static_cast<unsigned char>(LastChar))) {
        std::string numStr;
        bool isFloat = false;

        while (isdigit(static_cast<unsigned char>(LastChar))) {
            numStr += static_cast<char>(LastChar);
            LastChar = next_char(stream, current_line, current_col);
        }
    }

    // 3. Обработка комментариев (если начинается с '#')
    if (LastChar == '#') {
        do {
            LastChar = next_char(stream, current_line, current_col);
        } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');
        
        if (LastChar != EOF) {
            return gettok(stream); // Рекурсивно пропускаем комментарий и берем следующий токен
        }
        return {TokenType::Unknown, "", token_line, token_col};
    }

    // 4. Строки в кавычках
    if (LastChar == '"') {
        std::string strVal;
        LastChar = next_char(stream, current_line, current_col); // пропускаем открывающую кавычку
        while (LastChar != EOF && LastChar != '"') {
            strVal += static_cast<char>(LastChar);
            LastChar = next_char(stream, current_line, current_col);
        }
        if (LastChar == '"') {
            LastChar = next_char(stream, current_line, current_col); // пропускаем закрывающую кавычку
        }
        return {TokenType::String, strVal, token_line, token_col};
    }

    // 5. Операторы и прочие символы
    char c = static_cast<char>(LastChar);
    LastChar = next_char(stream, current_line, current_col); // Считываем следующий символ для проверки составных операторов

    std::string opStr(1, c);

    // Проверяем составные операторы (==, !=, <=, >=, &&, ||)
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

int main(int argc, char *argv[]) {
    FILE *input = stdin; // По умолчанию читаем из консоли

    if (argc > 1) {
        input = fopen(argv[1], "r"); // Если передан аргумент — читаем из файла
        if (!input) {
            perror("Ошибка открытия файла");
            return 1;
        }
    }

    std::cout << "Лексический анализ запущен. Введите данные (для завершения ввода в консоли используйте Ctrl+D / Ctrl+Z):\n";

    // Цикл разбора потока на токены
    while (true) {
        if (feof(input)) break;

        Token tok = gettok(input);
        if (tok.value.empty()) {
            break;
        }

        // Преобразуем enum в строковое представление для вывода
        std::string typeName;
        switch (tok.type) {
            case TokenType::Identifier: typeName = "Identifier"; break;
            case TokenType::Int:        typeName = "Int";        break;
            case TokenType::Float:      typeName = "Float";      break;
            case TokenType::Operator:   typeName = "Operator";   break;
            case TokenType::String:     typeName = "String";     break;
            case TokenType::Unknown:    typeName = "Unknown";    break;
        }

        std::cout << "Токен: [" << typeName << "] -> \"" << tok.value 
                  << "\" (строка " << tok.line << ", колонка " << tok.column << ")\n";
    }

    if (input != stdin) {
        fclose(input);
    }
    return 0;
}