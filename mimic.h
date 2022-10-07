#ifndef HEADER_MIMIC_H
#define HEADER_MIMIC_H
typedef enum {
    TokenReserved,
    TokenIdent,
    TokenNumber,
    TokenIf,
    TokenElseif,
    TokenElse,
    TokenFor,
    TokenWhile,
    TokenReturn,
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
    NodeAdd,    // +
    NodeSub,    // -
    NodeMul,    // *
    NodeDiv,    // /
    NodeEq,     // ==
    NodeNeq,    // !=
    NodeLT,     // <
    NodeLE,     // <=
    NodeNum,    // Integer
    NodeLVar,   // Left hand side value (variable)
    NodeAssign, // {lhs} = {rhs};
    NodeFCall,   // Function calls,
    NodeIf,
    NodeElseif,
    NodeElse,
    NodeFor,
    NodeWhile,
    NodeBlock,  // { ... }
    NodeReturn, // return {expr};
} NodeKind;

typedef struct FCall FCall;
typedef struct Node Node;
struct Node {
    NodeKind kind;
    Node *lhs;
    Node *rhs;
    Node *condition;   // Used by if/for/while/switch(?) statements.
    Node *body;        // Used by if/for/while/switch statements.
    Node *elseblock;   // Used by if statement. Holds "else if" and "else" blocks.
    Node *initializer; // Used by for statement.
    Node *iterator;    // Used by for statement.
    Node *next;        // Next statement in the same block. NULL if next
                       // statement doesn't exist.
    FCall *fcall;      // Called function information used when kind is NodeFCall.
    int val;           // Used when kind is NodeNum.
    int offset;        // Used when type is TokenLVar. Offset from base pointer.
    int blockID;       // Unique ID for jump labels. Valid only when the node
                       // is control syntax.
};

struct FCall {
    char *name; // Function name.
    int len;  // Function name length.
    int argsCount; // The number of arguments.
    Node *args; // Function arguments in reversed order.
};

typedef struct LVar LVar;
struct LVar {
    LVar *next;
    char *name;
    int len;     // Length of name.
    int offset;  // Offset from rbp.
};

typedef struct Globals Globals;
struct Globals {
    Node *code; // List of expressions.
    LVar *locals;    // List of local variables.
    int blockCount;  // The number of blocks appeared in program.
    Token* token;    // The token currently watches
    char* source;    // The source code (input)
};
extern Globals globals;

void genCode(Node *n);
void error(char *fmt, ...);
Token *tokenize();
void program();
#endif // HEADER_MIMIC_H
