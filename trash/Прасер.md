Ваш текущий парсер выполняет **синтаксический анализ** (проверяет структуру программы, соответствие грамматике) и **не строит абстрактное синтаксическое дерево (AST)**. Все методы `parseXXX()` имеют тип `void` – они просто «проглатывают» токены, выводят отладочные сообщения и вызывают ошибки при несоответствии. **Результат анализа нигде не сохраняется**, поэтому после завершения `parse()` у вас остаётся только факт «корректно / некорректно», но нет структурированного представления программы.

## Зачем нужно AST?

AST – это промежуточное представление исходного кода, которое удобно использовать для:
- **Семантического анализа** (проверка типов, область видимости, разрешение имён);
- **Оптимизации** (упрощение выражений, удаление мёртвого кода);
- **Генерации кода** (машинный код, байт-код, интерпретация);
- **Рефакторинга** и других трансформаций.

Без AST вы не можете выполнить эти этапы, потому что у вас нет структуры программы – только линейный поток токенов.

## Как обычно строят AST при рекурсивном спуске?

Каждая грамматическая конструкция возвращает **узел** (объект, представляющий эту конструкцию). Для этого методы парсера должны возвращать не `void`, а указатель или умный указатель на базовый класс узла.

### Пример структуры узлов (базовые классы)

```cpp
// Базовый класс для всех узлов
struct ASTNode {
    virtual ~ASTNode() = default;
};

// Узел для бинарной операции
struct BinaryOpNode : ASTNode {
    std::string op;
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;
    BinaryOpNode(std::string op, std::unique_ptr<ASTNode> left, std::unique_ptr<ASTNode> right)
        : op(op), left(std::move(left)), right(std::move(right)) {}
};

// Узел для присваивания
struct AssignmentNode : ASTNode {
    std::string varName;
    std::unique_ptr<ASTNode> expr;
    AssignmentNode(std::string name, std::unique_ptr<ASTNode> expr)
        : varName(name), expr(std::move(expr)) {}
};

// Узел для блока (список операторов)
struct BlockNode : ASTNode {
    std::vector<std::unique_ptr<ASTNode>> statements;
};

// Узел для if
struct IfNode : ASTNode {
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode> thenBlock;
    std::unique_ptr<ASTNode> elseBlock; // может быть nullptr
};

// Аналогично для while, do-while, литералов, переменных и т.д.
```

### Как переписать методы парсера

Вместо `void Parser::parseStatement()` – возвращаем `std::unique_ptr<ASTNode>`.

```cpp
std::unique_ptr<ASTNode> Parser::parseStatement() {
    if (check(TokenType::Identifier, "if")) {
        return parseIF();
    } else if (check(TokenType::Identifier, "do")) {
        return parseDo_While();
    } else if (check(TokenType::Identifier, "while")) {
        return parseWhile();
    } else if (peek().type == TokenType::Identifier && checkNext(TokenType::Operator, "=")) {
        return parseAssignment();
    } else {
        // parseExpression() – тоже должен возвращать узел
        // пока заглушка
        consume();
        return nullptr;
    }
}
```

Теперь каждая конструкция создаёт и возвращает узел:

**Пример parseAssignment**:

```cpp
std::unique_ptr<ASTNode> Parser::parseAssignment() {
    Token varName = consume();                       // имя переменной
    Token op = consume();                            // '='
    auto expr = parseLogicalOr();                    // выражение справа
    return std::make_unique<AssignmentNode>(varName.value, std::move(expr));
}
```

**Пример parseBlock**:

```cpp
std::unique_ptr<ASTNode> Parser::parseBlock() {
    consume(); // съедаем '{'
    auto block = std::make_unique<BlockNode>();
    while (peek().value != "}" && peek().type != TokenType::Unknown) {
        auto stmt = parseStatement();
        if (stmt) block->statements.push_back(std::move(stmt));
    }
    if (peek().value != "}") {
        parserError(peek(), "Ожидалась '}'");
    }
    consume(); // съедаем '}'
    return block;
}
```

**Для выражений** – каждый уровень (parseLogicalOr, parseLogicalAnd, parseEquality и т.д.) возвращает узел, и они комбинируются:

```cpp
std::unique_ptr<ASTNode> Parser::parseLogicalOr() {
    auto node = parseLogicalAnd();
    while (peek().value == "||") {
        consume();
        auto right = parseLogicalAnd();
        node = std::make_unique<BinaryOpNode>("||", std::move(node), std::move(right));
    }
    return node;
}
```

## Где хранить и как передавать итоговое дерево?

Есть два основных подхода:

1. **Корневой узел хранится внутри парсера** – добавить поле `std::unique_ptr<ASTNode> root;` и в методе `parse()` заполнять его списком операторов (например, блоком верхнего уровня).

   ```cpp
   class Parser {
       // ...
       std::unique_ptr<ASTNode> root;
   public:
       void parse() {
           auto program = std::make_unique<BlockNode>();
           while (currentPos < tokens.size()) {
               auto stmt = parseStatement();
               if (stmt) program->statements.push_back(std::move(stmt));
           }
           root = std::move(program);
       }
       ASTNode* getRoot() const { return root.get(); }
   };
   ```

2. **Метод parse() возвращает корневой узел** – это удобнее, если парсер не хранит состояние после разбора.

   ```cpp
   std::unique_ptr<ASTNode> Parser::parse() {
       auto program = std::make_unique<BlockNode>();
       while (currentPos < tokens.size()) {
           auto stmt = parseStatement();
           if (stmt) program->statements.push_back(std::move(stmt));
       }
       return program;
   }
   ```

   Тогда в вызывающем коде:
   ```cpp
   Parser parser(tokens);
   auto ast = parser.parse();
   // передаём ast в семантический анализатор, генератор кода и т.д.
   ```

## Что делать с текущим кодом?

Ваш текущий код – это хорошая основа, но его нужно расширить:
- Определить иерархию классов узлов AST;
- Переписать все `parseXXX` на возврат `std::unique_ptr<ASTNode>`;
- В методах, где сейчас просто `consume()`, создавать соответствующие узлы (литералы, переменные, вызовы функций и т.д.);
- Учесть, что `parsePrimary()` должна возвращать узел для чисел, строк, идентификаторов и выражений в скобках.

После этого парсер будет не просто проверять синтаксис, а строить полноценное дерево, которое можно сохранить, передать и использовать для дальнейших этапов.

## Заключение

- **Сейчас** – только валидация, результат не сохраняется.
- **Нужно** – каждая конструкция возвращает узел, корень собирается в программу.
- **Передача** – через возвращаемое значение или поле класса, затем передаётся в следующие модули (семантический анализ, кодогенерация).
- **Сохранение** – можно сериализовать AST в JSON/XML, но обычно его держат в памяти для последующих проходов.

Реализовав AST, вы получите полноценный фронтенд компилятора/интерпретатора.