#include <iostream>
#include <vector>
#include "Token.h"
#include "lexer.h"
#include "parser.h"

void processToken(const Token& tok) {
    if(tok.type != TokenType::Unknown){
    std::cout << "[Парсер получил] Тип: " << tok.type
              << ", Значение: \"" << tok.value 
              << "\", Строка: " << tok.line 
              << ", Символ: " << tok.column << "\n";
}
    else {
        std::cout << ", Значение: \"" << tok.value << "\" Ошибка в строке: "  << tok.line  << ", символ: " << tok.column << "\n";
    }
}

int main(int argc, char *argv[]) {
    FILE *input = stdin; 

    if (argc > 1) {
        std::string filename = argv[1];
        
        // Проверка расширения .azy (требует C++17 или строкового метода)
        if (filename.length() < 4 || filename.compare(filename.length() - 4, 4, ".azy") != 0) {
            std::cerr << "[Ошибка] Поддерживаются только файлы с расширением .azy\n";
            return 1;
        }

        input = fopen(argv[1], "r");
        if (!input) {
            perror("Ошибка открытия файла");
            return 1;
        }
    }

    std::cout << "Главный файл запущен. Читаем токены...\n";
    std::vector<Token> tokens;

    while (true) {
        if (feof(input)) break;

        Token tok = gettok(input);
        
        if (tok.value.empty() && tok.type == TokenType::Unknown) {
            break;
        }

        processToken(tok);

        if (tok.type != TokenType::Unknown) {
            tokens.push_back(tok);
        }

    }

    if (input != stdin) {
        fclose(input);
    }

    std::cout << "Лексер закончил свою работу" << "\n";
    std::cout << "Всего сохранено токенов в массив: " << tokens.size() << "\n \n \n" ;
    std::cout << "Запущен парсер..." << "\n";

    Parser parser(tokens);
    auto root = parser.parse();        // сохраняем корень дерева

    std::cout << "Готовое дерево AST: " << "\n \n";
    parser.printAST(root.get());       // вызываем метод печати у объекта parser

    std::cout << "Парсер закончил свою работу" << "\n \n";
    

    return 0;
}
