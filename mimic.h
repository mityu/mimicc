#ifndef HEADER_MIMIC_H
#define HEADER_MIMIC_H

#include <stddef.h>

#define REG_ARGS_MAX_COUNT  (6)
#define errorUnreachable()  \
    error("%s:%d: Internal error: unreachable", __FILE__, __LINE__);


typedef enum {
    TypeNone,   // No type (block, if, for, while, ...)
    TypeInt,    // `int`
    TypeNumber, // Literal number
    TypeArray,
    TypePointer,
} TypeKind;

typedef struct TypeInfo TypeInfo;
struct TypeInfo {
    TypeKind type;
    TypeInfo *baseType; // Valid when type is TypePointer or TypeArray.
    int arraySize;
};

#define PrimitiveType(type) (TypeInfo){type, NULL, 0}
static struct Types {
    TypeInfo None;
    TypeInfo Int;
    TypeInfo Number;
} Types = {
    .None = PrimitiveType(TypeNone),
    .Int = PrimitiveType(TypeInt),
    .Number = PrimitiveType(TypeNumber),
};
#undef PrimitiveType

typedef enum {
    TokenReserved,
    TokenTypeName,
    TokenIdent,
    TokenNumber,
    TokenIf,
    TokenElseif,
    TokenElse,
    TokenFor,
    TokenWhile,
    TokenReturn,
    TokenSizeof,
    TokenEOF,
} TokenType;

typedef struct Token Token;
struct Token {
    TokenType type;
    Token *prev;
    Token *next;
    int val;          // Number valid when type is TokenNumber.
    TypeKind varType; // Variable type valid when type is TokenTypeName.
    char *str;        // The token string.
    int len;          // The token length.
};

typedef enum {
    NodeAdd,      // +
    NodeSub,      // -
    NodeMul,      // *
    NodeDiv,      // /
    NodeDivRem,   // %
    NodeEq,       // ==
    NodeNeq,      // !=
    NodeLT,       // <
    NodeLE,       // <=
    NodePreIncl,  // ++{var}
    NodePreDecl,  // --{var}
    NodePostIncl, // {var}++
    NodePostDecl, // {var}--
    NodeAddress,  // &{var}
    NodeDeref,    // *{ptr}
    NodeNum,      // Integer
    NodeLVar,     // Left hand side value (local variable)
    NodeAssign,   // {lhs} = {rhs};
    NodeFCall,    // Function calls,
    NodeIf,
    NodeElseif,
    NodeElse,
    NodeFor,
    NodeBlock,    // { ... }
    NodeReturn,   // return {expr};
    NodeFunction,
    NodeGVar,     // Global variable, work as lvar.
} NodeKind;

typedef struct Function Function;
typedef struct LVar LVar;
typedef struct FCall FCall;
typedef struct Node Node;
struct Node {
    NodeKind kind;
    Node *lhs;
    Node *rhs;
    Node *condition;   // Used by if/for/while/switch(?) statements.
    Node *body;        // Used by if/for/while/switch statements, block and functions.
    Node *elseblock;   // Used by if statement. Holds "else if" and "else" blocks.
    Node *initializer; // Used by for statement.
    Node *iterator;    // Used by for statement.
    Node *next;        // Next statement in the same block. NULL if next
                       // statement doesn't exist.
    TypeInfo *type;    // Type of this node's result value.
    Token *token;      // Token which gave this node.
    LVar *localVars;   // List of variables local to block. (func, block, for, ...)
                       // Stored in reversed appearing order for an
                       // implementation reason.
    int localVarLen;   // The size of local variables (not includes inner blocks').
    FCall *fcall;      // Called function information used when kind is NodeFCall.
    Function *func;    // Function info.
    Node *outerBlock;  // One step outer block.
    int val;           // Used when kind is NodeNum.
    int offset;        // Used when type is TokenLVar. Offset from base pointer.
                       // Variable adress is calculated as "RBP - offset."
    int blockID;       // Unique ID for jump labels. Valid only when the node
                       // is control syntax.
};

struct FCall {
    char *name;    // Function name.
    int len;       // Function name length.
    int argsCount; // The number of arguments.
    Node *args;    // Function arguments in reversed order.
};

struct LVar {
    LVar     *next;
    char     *name;
    int      len;     // Length of name.
    TypeInfo *type;   // Type of variable.
    int      offset;  // Offset from rbp.  Variable adress is calculated as
                      // "RBP - offset."
};

struct Function {
    Function *next;
    char *name;
    int len;
    int argsCount;
    LVar *args;        // Function arguments.  NOT IN REVERSED ORDER.
    TypeInfo *retType; // Type of return value.
    int haveImpl;      // TRUE when implementation is given.  FALSE if
                       // only declaration is given.
};

typedef struct Globals Globals;
struct Globals {
    Node *code;                // The root node of program.
    Function *functions;       // Declared function list.
    LVar *vars;                // Global variables.
    Node *currentBlock;        // Current block.
    Function *currentFunction; // Function currently parsed.
    int blockCount;            // The number of blocks appeared in program.
    Token *token;              // Token currently watches
    char *source;              // The source code (input)
};
extern Globals globals;

void genCode(Node *n);
void genCodeGvarDecl();
void error(char *fmt, ...);
void errorAt(char *loc, char *fmt, ...);
Token *tokenize();
void program();
Function *findFunction(char *name, int len);
int sizeOf(TypeInfo *ti);
void verifyType(Node *n);
int isWorkLikePointer(TypeInfo *t);
#endif // HEADER_MIMIC_H
