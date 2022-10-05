#ifndef HEADER_MIMIC_H
#define HEADER_MIMIC_H
typedef enum {
    TokenReserved,
    TokenIdent,
    TokenNumber,
    TokenEOF,
} TokenType;

typedef struct Token Token;
struct Token {
    TokenType type;
    Token *next;
    int val;    // The number when type == TokenNumber.
    char *str;  // The token string.
    int len;    // The token length.
};

typedef enum {
    NodeAdd, // +
    NodeSub, // -
    NodeMul, // *
    NodeDiv, // /
    NodeEq, // ==
    NodeNeq, // !=
    NodeLT, // <
    NodeLE, // <=
    NodeNum, // Integer
    NodeLVar, // Left hand side value (variable)
    NodeAssign,
} NodeKind;

typedef struct Node Node;
struct Node {
    NodeKind kind;
    Node *lhs;
    Node *rhs;
    int val; // Used when kind is NodeNum.
    int offset; // Used when type is TokenLVar. Offset from base pointer.
};

typedef struct LVar LVar;
struct LVar {
    LVar *next;
    char *name;
    int len; // Length of name.
    int offset;  // Offset from rbp.
};

typedef struct Globals Globals;
struct Globals {
    Node *code[100]; // List of expressions.
    LVar *locals;  // List of local variables.
    Token* token;  // The token currently watches
    char* source;  // The source code (input)
};
extern Globals globals;

void genCode(Node *n);
void error(char *fmt, ...);
Token *tokenize();
void program();
#endif // HEADER_MIMIC_H
