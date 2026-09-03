#pragma once
#include <string>
#include <ostream>

// Категории токенов
enum class TokenType {
    Identifier, 
    Int,        
    Float,      
    Operator,   
    String,     
    Unknown     
};

// Структура токена
struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;
};

inline std::ostream& operator<<(std::ostream& os, TokenType type) {
    switch (type) {
        case TokenType::Identifier: os << "Identifier"; break;
        case TokenType::Int:        os << "Int"; break;
        case TokenType::Float:      os << "Float"; break;
        case TokenType::Operator:   os << "Operator"; break;
        case TokenType::String:     os << "String"; break;
        case TokenType::Unknown:    os << "Unknown"; break;
    }
    return os;
}