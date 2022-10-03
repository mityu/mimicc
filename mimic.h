#ifndef HEADER_MIMIC_H
#define HEADER_MIMIC_H
typedef enum {
    TokenReserved,
    TokenNumber,
    TokenEOF,
} TokenType;

typedef struct Token Token;
struct Token {
    TokenType type;
    Token *next;
    int val;    // The number when type == TokenNumber.
    char *str;  // The token string
};

typedef enum {
    NodeAdd, // +
    NodeSub, // -
    NodeMul, // *
    NodeDiv, // /
    NodeNum, // Integer
} NodeKind;

typedef struct Node Node;
struct Node {
    NodeKind kind;
    Node *lhs;
    Node *rhs;
    int val; // Used when kind is NodeNum.
};

typedef struct Globals Globals;
struct Globals {
    Token* token;  // The token currently watches
    char* source;  // The source code (input)
};
extern Globals globals;

void genCode(Node *n);
Token *tokenize();
Node *expr();
Node *mul();
Node *unary();
Node *primary();
#endif // HEADER_MIMIC_H
