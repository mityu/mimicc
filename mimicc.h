#ifndef HEADER_MIMICC_H
#define HEADER_MIMICC_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define REG_ARGS_MAX_COUNT (6)
#define ONE_WORD_BYTES (8)
#define errorUnreachable() error("%s:%d: Internal error: unreachable", __FILE__, __LINE__)
#define runtimeAssert(expr)                                                              \
    do {                                                                                 \
        if (!(expr))                                                                     \
            error("%d:%d: Internal error: assert: %s", __FILE__, __LINE__, #expr);       \
    } while (0)
#define safeFree(p)                                                                      \
    do {                                                                                 \
        if (p) {                                                                         \
            free(p);                                                                     \
            p = NULL;                                                                    \
        }                                                                                \
    } while (0)

typedef struct LiteralString LiteralString;
typedef struct Obj Obj;
typedef struct Struct Struct;
typedef struct Enum Enum;
typedef struct Function Function;
typedef struct Typedef Typedef;
typedef struct Token Token;
typedef struct FilePath FilePath;

#define TypePtrdiff_t TypeInt
#define Ptrdiff_t Int
typedef enum {
    TypeNone,   // No type (block, if, for, while, ...)
    TypeVoid,   // `void`
    TypeInt,    // `int`
    TypeChar,   // `char`
    TypeNumber, // Literal number
    TypeArray,
    TypePointer,
    TypeStruct, // Struct
    TypeEnum,   // Enum
    TypeFunction,
} TypeKind;

typedef struct TypeInfo TypeInfo;
struct TypeInfo {
    TypeInfo *next;
    TypeKind type;
    TypeInfo *baseType; // Valid when type is TypePointer or TypeArray.
    int arraySize;
    Function *funcDef; // Valid when type is TypeFunction
    Struct *structDef; // Valid when type is TypeStruct
    Enum *enumDef;     // Valid when type is TypeEnum
};

extern struct Types {
    TypeInfo None;
    TypeInfo Void;
    TypeInfo Int;
    TypeInfo Char;
    TypeInfo Number;
} Types;

typedef enum {
    TokenReserved,
    TokenTypeName,
    TokenIdent,
    TokenNumber,
    TokenStatic,
    TokenExtern,
    TokenTypedef,
    TokenIf,
    TokenElseif,
    TokenElse,
    TokenSwitch,
    TokenCase,
    TokenDefault,
    TokenFor,
    TokenWhile,
    TokenDo,
    TokenBreak,
    TokenContinue,
    TokenReturn,
    TokenSizeof,
    TokenLiteralString,
    TokenStruct,
    TokenEnum,
    TokenNewLine,
    TokenSOF, // Start of file.
    TokenEOF, // End of file.
} TokenType;

struct Token {
    TokenType type;
    Token *prev;
    Token *next;
    int val;                   // Number valid when type is TokenNumber.
    TypeKind varType;          // Variable type valid when type is TokenTypeName.
    LiteralString *literalStr; // Reference to literal string when type is
                               // TokenLiteralString.
    FilePath *file;            // File path information
    char *str;                 // The token string.
    int len;                   // The token length.
    int line;                  // Line number in file.
    int column;                // Column number in line.
};

typedef struct Env Env;
struct Env {
    Env *outer;
    Struct *structs;
    Enum *enums;
    Typedef *typedefs;
    Obj *vars;   // List of variables local to this env.
    int varSize; // Total size of variables local to this env.
};

typedef enum {
    NodeNop,
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
    NodeMemberAccess,  // Struct member accessing (struct.member)
    NodeAddress,       // &{var}
    NodeDeref,         // *{ptr}
    NodeNot,           // !
    NodeLogicalOR,     // ||
    NodeLogicalAND,    // &&
    NodeBitwiseAND,    // &
    NodeBitwiseOR,     // |
    NodeBitwiseXOR,    // ^
    NodeArithShiftL,   // << (arithmetic)
    NodeArithShiftR,   // >> (arithmetic)
    NodeNum,           // Integer
    NodeLiteralString, // literal string
    NodeLVar,          // Left hand side value (local variable)
    NodeAssign,        // {lhs} = {rhs};
    NodeAssignStruct,  // Assignment, but only for structs
    NodeFCall,         // Function calls,
    NodeConditional,   // {expr} ? {expr} : {expr}
    NodeIf,
    NodeElseif,
    NodeElse,
    NodeSwitch,
    NodeSwitchCase,
    NodeFor,
    NodeDoWhile,
    NodeExprList, // Expressions concatenated by commas.
    NodeInitVar,  // Variable initialization on declaration.
    NodeInitList, // Initializer list
    NodeBlock,    // { ... }
    NodeBreak,    // break;
    NodeContinue, // continue;
    NodeReturn,   // return {expr};
    NodeFunction,
    NodeGVar,       // Global variable, work as lvar.
    NodeTypeCast,   // Type casting.
    NodeClearStack, // Clear certain range of stack with 0.
    NodeVaStart,    // Built-in va_args()
} NodeKind;

typedef struct SwitchCase SwitchCase;
typedef struct FCall FCall;
typedef struct Node Node;
struct Node {
    NodeKind kind;
    Env *env;
    Node *lhs;
    Node *rhs;
    Node *condition;   // Used by if/for/while/switch statements.
    Node *body;        // Used by if/for/while statements, block and functions.
    Node *elseblock;   // Used by if statement. Holds "else if" and "else" blocks.
    Node *initializer; // Used by for statement.
    Node *iterator;    // Used by for statement.
    Node *next;        // Next statement in the same block. NULL if next
                       // statement doesn't exist.
    TypeInfo *type;    // Type of this node's result value.
    Token *token;      // Token which gave this node.
    FCall *fcall;      // Called function information used when kind is NodeFCall.
    SwitchCase *cases; // "case" or "default" nodes within switch statement.
    Obj *obj;
    Obj *parentFunc;
    int val;     // Used when kind is NodeNum.
    int blockID; // Unique ID for jump labels. Valid only when the node
                 // is control syntax, logical AND, and logical OR.
};

struct FCall {
    char *name;         // Function name.
    int len;            // Function name length.
    int argsCount;      // The number of arguments.
    Node *args;         // Function arguments in reversed order.
    TypeInfo *declType; // Called function's type
};

struct SwitchCase {
    SwitchCase *next;
    Node *node;
};

struct Function {
    int argsCount;
    int haveVaArgs;    // TRUE if function have variadic arguments
    int haveImpl;      // TRUE when implementation is given.  FALSE if
                       // only declaration is given.
    int capStackSize;  // Size of stack which this function uses.
    Obj *args;         // Function arguments.  NOT IN REVERSED ORDER.
    TypeInfo *retType; // Type of return value.
};

typedef struct ObjAttr ObjAttr;
struct ObjAttr {
    int isStatic;
    int isExtern;
    int isTypedef;
};

// Struct for objects; variables and functions.
struct Obj {
    Obj *next;
    Token *token;
    TypeInfo *type;  // Type of object.
    int offset;      // Offset from rbp.  Variable adress is calculated
                     // as RBP - offset.
    int isExtern;    // TRUE if object is declared with "extern".
    int isStatic;    // TRUE if object is declared with "static".
    int staticVarID; // ID for static local variables.
    Function *func;  // Valid when object holds function.
};

// Struct for global variables.
typedef struct GVar GVar;
typedef struct GVarInit GVarInit;
struct GVar {
    GVar *next;
    Obj *obj;
    GVarInit *initializer;
};

typedef enum {
    GVarInitZero,
    GVarInitString,
    GVarInitNum,
    GVarInitPointer,
} GVarInitKind;

struct GVarInit {
    GVarInit *next;
    GVarInitKind kind;
    int size;
    Node *rhs;
};

struct LiteralString {
    LiteralString *next;
    char *string;
    int len; // String length in program. (Not on text editor.)
    int id;
};

struct Struct {
    Struct *next;
    Token *tagName;
    Obj *members;
    int totalSize;
    int hasImpl;
};

typedef struct EnumItem EnumItem;
struct Enum {
    Enum *next;
    Token *tagName;
    EnumItem *items;
    int hasImpl;
};

struct EnumItem {
    EnumItem *next;
    Token *token;
    int value;
};

struct Typedef {
    Typedef *next;
    Token *name;    // Typedef name.
    TypeInfo *type; // Actual type.
};

struct FilePath {
    char *basename;
    char *dirname; // Parent directory name with '/' at the end.
    char *path;
    char *display; // File path for display (error message, __FILE__ macro,
                   // etc.)
};

typedef struct Globals Globals;
struct Globals {
    Node *code; // The root node of program.
    Env globalEnv;
    Obj *functions;         // Declared function list.
    GVar *globalVars;       // Global variable list.
    GVar *staticVars;       // Static local variable list.
    LiteralString *strings; // Literal string list.
    Env *currentEnv;
    Obj *currentFunction;    // Function currently parsed.
    int blockCount;          // The number of blocks appeared in program.
    int literalStringCount;  // The number of literal strings appeared in
                             // program.
    int staticVarCount;      // The number of variables declared with "static."
    int namelessEnumCount;   // The number of nameless enums.
    int namelessStructCount; // The number of nameless structs.
    Token *token;            // Token currently watches.
    FILE *destFile;          // The output file.
    FilePath *ccFile;        // The binary file path infomation.
    char *includePath;       // The include path.
};
extern Globals globals;

// main.c
void *safeAlloc(size_t size);
_Noreturn void error(const char *fmt, ...);
_Noreturn void errorAt(Token *loc, const char *fmt, ...);
int dumpc(int c);
int dumps(const char *s);
int dumpf(const char *fmt, ...);
FilePath *analyzeFilepath(const char *path, const char *display);
char *readFile(const char *path);

// codegen.c
void genCode(const Node *n);
void genCodeGlobals(void);

// tokenizer.c
int isSpace(char c);
int checkEscapeChar(char c, char *decoded);
void popTokenRange(Token *begin, Token *end);
void removeAllNewLineToken(Token *token);
void printToken(Token *token);
void printTokenList(Token *token);
Token *tokenize(char *source, FilePath *file);

// preproc.c
void preprocess(Token *token);

// parser.c
void program(void);
Obj *findFunction(const char *name, int len);
Obj *findStructMember(const Struct *s, const char *name, int len);
GVar *findGlobalVar(char *name, int len);
Obj *findLVar(char *name, int len);
int sizeOf(const TypeInfo *ti);
int matchToken(const Token *token, const char *name, const int len);
Node *evalConstantExpr(Node *n);

// verifier.c
void verifyFlow(const Node *n);
void verifyType(const Node *n);
int checkTypeEqual(const TypeInfo *t1, const TypeInfo *t2);
int checkAssignable(const TypeInfo *lhs, const TypeInfo *rhs);
int isLvalue(const Node *n);
int isWorkLikePointer(const TypeInfo *t);
const TypeInfo *getBaseType(const TypeInfo *t);
#endif // HEADER_MIMICC_H
