#ifndef HEADER_MIMIC_H
#define HEADER_MIMIC_H

#include <stddef.h>

#define REG_ARGS_MAX_COUNT  (6)
#define ONE_WORD_BYTES  (8)
#define errorUnreachable()  \
    error("%s:%d: Internal error: unreachable", __FILE__, __LINE__)
#define runtimeAssert(expr)  \
    do {\
        if (!(expr)) \
            error("%d:%d: Internal error: assert: %s", \
                    __FILE__, __LINE__, #expr); \
    } while (0)

typedef struct LiteralString LiteralString;

typedef enum {
    TypeNone,   // No type (block, if, for, while, ...)
    TypeVoid,   // `void`
    TypeInt,    // `int`
    TypeChar,   // `char`
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
    TypeInfo Void;
    TypeInfo Int;
    TypeInfo Char;
    TypeInfo Number;
} Types = {
    .None   = PrimitiveType(TypeNone),
    .Void   = PrimitiveType(TypeVoid),
    .Int    = PrimitiveType(TypeInt),
    .Char   = PrimitiveType(TypeChar),
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
    TokenBreak,
    TokenContinue,
    TokenReturn,
    TokenSizeof,
    TokenLiteralString,
    TokenEOF,
} TokenType;

typedef struct Token Token;
struct Token {
    TokenType type;
    Token *prev;
    Token *next;
    int val;                   // Number valid when type is TokenNumber.
    TypeKind varType;          // Variable type valid when type is TokenTypeName.
    LiteralString *literalStr; // Reference to literal string when type is
                               // TokenLiteralString.
    char *str;                 // The token string.
    int len;                   // The token length.
};

typedef enum {
    NodeAdd,           // +
    NodeSub,           // -
    NodeMul,           // *
    NodeDiv,           // /
    NodeDivRem,        // %
    NodeEq,            // ==
    NodeNeq,           // !=
    NodeLT,            // <
    NodeLE,            // <=
    NodePreIncl,       // ++{var}
    NodePreDecl,       // --{var}
    NodePostIncl,      // {var}++
    NodePostDecl,      // {var}--
    NodeAddress,       // &{var}
    NodeDeref,         // *{ptr}
    NodeNum,           // Integer
    NodeLiteralString, // literal string
    NodeLVar,          // Left hand side value (local variable)
    NodeAssign,        // {lhs} = {rhs};
    NodeFCall,         // Function calls,
    NodeIf,
    NodeElseif,
    NodeElse,
    NodeFor,
    NodeExprList,      // Expressions concatenated by commas.
    NodeBlock,         // { ... }
    NodeBreak,         // break;
    NodeContinue,      // continue;
    NodeReturn,        // return {expr};
    NodeFunction,
    NodeGVar,          // Global variable, work as lvar.
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
    int localVarSize;   // The size of local variables (not includes inner blocks').
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
    int haveVaArgs;    // TRUE if function have variadic arguments
    LVar *args;        // Function arguments.  NOT IN REVERSED ORDER.
    TypeInfo *retType; // Type of return value.
    int haveImpl;      // TRUE when implementation is given.  FALSE if
                       // only declaration is given.
};

struct LiteralString {
    LiteralString *next;
    char *string;
    int len;            // String length in program. (Not on text editor.)
    int id;
};

typedef struct Globals Globals;
struct Globals {
    Node *code;                // The root node of program.
    Function *functions;       // Declared function list.
    LVar *vars;                // Global variables.
    LiteralString *strings;    // Literal string list.
    Node *currentBlock;        // Current block.
    Function *currentFunction; // Function currently parsed.
    int blockCount;            // The number of blocks appeared in program.
    int literalStringCount;    // The number of literal strings appeared in program.
    Token *token;              // Token currently watches.
    char *source;              // The source code (input).
    char *sourceFile;          // The source file path.
};
extern Globals globals;

// main.c
void *safeAlloc(size_t size);
_Noreturn void error(const char *fmt, ...);
_Noreturn void errorAt(char *loc, const char *fmt, ...);

// codegen.c
void genCode(const Node *n);
void genCodeGlobals(void);

// tokenizer.c
Token *tokenize(void);
int checkEscapeChar(char c, char *decoded);

// parser.c
void program();
Function *findFunction(const char *name, int len);
int sizeOf(const TypeInfo *ti);

// verifier.c
void verifyFlow(const Node *n);
void verifyType(const Node *n);
int isWorkLikePointer(const TypeInfo *t);
#endif // HEADER_MIMIC_H
