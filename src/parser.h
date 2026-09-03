#pragma once
#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <memory>          // для std::unique_ptr
#include <iostream>
#include "Token.h"

// ============================================================
// Базовые узлы абстрактного синтаксического дерева (AST)
// ============================================================
struct ASTNode {
    virtual ~ASTNode() = default;
};

struct ExpressionNode : ASTNode {};

// Узлы для операторов
struct BlockNode : ASTNode {
    std::vector<std::unique_ptr<ASTNode>> statements;
};

struct AssignmentNode : ASTNode {
    std::string varName;
    std::unique_ptr<ExpressionNode> expr;
    AssignmentNode(std::string name, std::unique_ptr<ExpressionNode> e)
        : varName(name), expr(std::move(e)) {}
};

struct IfNode : ASTNode {
    std::unique_ptr<ExpressionNode> condition;
    std::unique_ptr<ASTNode> thenBody;
    std::unique_ptr<ASTNode> elseBody; // может быть nullptr
    IfNode(std::unique_ptr<ExpressionNode> cond,
           std::unique_ptr<ASTNode> thenB,
           std::unique_ptr<ASTNode> elseB = nullptr)
        : condition(std::move(cond)), thenBody(std::move(thenB)), elseBody(std::move(elseB)) {}
};

struct WhileNode : ASTNode {
    std::unique_ptr<ExpressionNode> condition;
    std::unique_ptr<ASTNode> body;
    WhileNode(std::unique_ptr<ExpressionNode> cond, std::unique_ptr<ASTNode> b)
        : condition(std::move(cond)), body(std::move(b)) {}
};

struct DoWhileNode : ASTNode {
    std::unique_ptr<ASTNode> body;
    std::unique_ptr<ExpressionNode> condition;
    DoWhileNode(std::unique_ptr<ASTNode> b, std::unique_ptr<ExpressionNode> cond)
        : body(std::move(b)), condition(std::move(cond)) {}
};

// Узлы для выражений
struct LiteralNode : ExpressionNode {
    enum Type { Int, Float, String };
    Type type;
    std::string value;
    LiteralNode(Type t, const std::string& val) : type(t), value(val) {}
};

struct VariableNode : ExpressionNode {
    std::string name;
    VariableNode(const std::string& n) : name(n) {}
};

struct UnaryOpNode : ExpressionNode {
    std::string op;
    std::unique_ptr<ExpressionNode> operand;
    UnaryOpNode(const std::string& o, std::unique_ptr<ExpressionNode> opnd)
        : op(o), operand(std::move(opnd)) {}
};

struct BinaryOpNode : ExpressionNode {
    std::string op;
    std::unique_ptr<ExpressionNode> left;
    std::unique_ptr<ExpressionNode> right;
    BinaryOpNode(const std::string& o,
                 std::unique_ptr<ExpressionNode> l,
                 std::unique_ptr<ExpressionNode> r)
        : op(o), left(std::move(l)), right(std::move(r)) {}
};

// ============================================================
// Класс парсера
// ============================================================
class Parser {
private:
    std::vector<Token> tokens;
    size_t currentPos = 0;

    // Вспомогательные методы для навигации по токенам
    Token peek();
    Token peekNext();
    Token consume();
    bool check(TokenType type, const std::string& value);
    bool checkNext(TokenType type, const std::string& value);

    void debugging() {
        Token token = peek();
        std::cout << "[Парсер] " << token.value << "\n";
    }

public:
    Parser(const std::vector<Token>& toks);

    void printAST(const ASTNode* node, int indent = 0) {
        std::string pad(indent * 2, ' ');

        if (auto block = dynamic_cast<const BlockNode*>(node)) {
            std::cout << pad << "Block {\n";
            for (const auto& stmt : block->statements) {
                printAST(stmt.get(), indent + 1);
            }
            std::cout << pad << "}\n";
        }
        else if (auto assign = dynamic_cast<const AssignmentNode*>(node)) {
            std::cout << pad << "Assignment: " << assign->varName << " = ";
            printAST(assign->expr.get(), 0); // выражение печатаем без отступа, или с тем же
        }
        else if (auto ifNode = dynamic_cast<const IfNode*>(node)) {
            std::cout << pad << "If (";
            printAST(ifNode->condition.get(), 0);
            std::cout << ") {\n";
            printAST(ifNode->thenBody.get(), indent + 1);
            if (ifNode->elseBody) {
                std::cout << pad << "} else {\n";
                printAST(ifNode->elseBody.get(), indent + 1);
            }
            std::cout << pad << "}\n";
        }
        else if (auto whileNode = dynamic_cast<const WhileNode*>(node)) {
            std::cout << pad << "While (";
            printAST(whileNode->condition.get(), 0);
            std::cout << ") {\n";
            printAST(whileNode->body.get(), indent + 1);
            std::cout << pad << "}\n";
        }
        else if (auto doWhile = dynamic_cast<const DoWhileNode*>(node)) {
            std::cout << pad << "Do {\n";
            printAST(doWhile->body.get(), indent + 1);
            std::cout << pad << "} While (";
            printAST(doWhile->condition.get(), 0);
            std::cout << ")\n";
        }
        else if (auto literal = dynamic_cast<const LiteralNode*>(node)) {
            std::cout << pad << "Literal(";
            switch (literal->type) {
                case LiteralNode::Int:   std::cout << "int"; break;
                case LiteralNode::Float: std::cout << "float"; break;
                case LiteralNode::String:std::cout << "string"; break;
            }
            std::cout << ", \"" << literal->value << "\")";
        }
        else if (auto var = dynamic_cast<const VariableNode*>(node)) {
            std::cout << pad << "Variable(\"" << var->name << "\")";
        }
        else if (auto unary = dynamic_cast<const UnaryOpNode*>(node)) {
            std::cout << pad << "UnaryOp(\"" << unary->op << "\", ";
            printAST(unary->operand.get(), 0);
            std::cout << ")";
        }
        else if (auto binary = dynamic_cast<const BinaryOpNode*>(node)) {
            std::cout << pad << "BinaryOp(\"" << binary->op << "\", ";
            printAST(binary->left.get(), 0);
            std::cout << ", ";
            printAST(binary->right.get(), 0);
            std::cout << ")";
        }
        else {
            std::cout << pad << "Unknown node\n";
        }
    }

    // Главный метод – возвращает корневой узел (блок)
    std::unique_ptr<ASTNode> parse();

    // Шаблон для генерации ошибок (без изменений)
    template <typename... Args>
    void parserError(const Token& token, Args&&... args) {
        std::ostringstream msgStream;
        (msgStream << ... << std::forward<Args>(args));

        std::string errorMsg = "[Ошибка парсера] " + msgStream.str() +
            " (строка: " + std::to_string(token.line) +
            ", символ: " + std::to_string(token.column) +
            ", токен: '" + token.value + "')";

        throw std::runtime_error(errorMsg);
    }

    // Методы разбора операторов (возвращают узлы)
    std::unique_ptr<ASTNode> parseStatement();
    std::unique_ptr<ASTNode> parseBlock();

    std::unique_ptr<ASTNode> parseIf();
    std::unique_ptr<ASTNode> parseElse();
    std::unique_ptr<ASTNode> parseWhile();
    std::unique_ptr<ASTNode> parseDoWhile();
    std::unique_ptr<ASTNode> parseAssignment();

    // Методы разбора выражений (возвращают ExpressionNode)
    std::unique_ptr<ExpressionNode> parseLogicalOr();
    std::unique_ptr<ExpressionNode> parseLogicalAnd();
    std::unique_ptr<ExpressionNode> parseEquality();
    std::unique_ptr<ExpressionNode> parseRelational();
    std::unique_ptr<ExpressionNode> parseAdditive();
    std::unique_ptr<ExpressionNode> parseMultiplicative();
    std::unique_ptr<ExpressionNode> parseUnary();
    std::unique_ptr<ExpressionNode> parsePrimary();
};