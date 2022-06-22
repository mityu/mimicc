#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

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

struct Globals {
    Token* token;  // The token currently watches
    char* source;  // The source code (input)
} globals;


int isSpace(char c) {
    return c == ' ' || c == '\n' || c == '\t';
}

int isDigit(char c) {
    c = c - '0';
    return 0 <= c && c <= 9;
}


// Print error message on stderr and exit.
void error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

void errorAt(char *loc, char *fmt, ...) {
    int pos = loc - globals.source;
    va_list ap;
    va_start(ap, fmt);

    fprintf(stderr, "%s\n", globals.source);
    if (pos) {
        fprintf(stderr, "%*s", pos, " ");
    }
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// Generate a new token, concatenate it to `current`, and return it.
Token *newToken(TokenType type, Token *current, char *str) {
    Token *t = (Token*)calloc(1, sizeof(Token));
    t->type = type;
    t->str = str;
    current->next = t;
    return t;
}

Token *tokenize() {
    char *p = globals.source;
    Token head;
    head.next = NULL;
    Token *current = &head;

    while (*p) {
        if (isSpace(*p)) {
            ++p;
            continue;
        }
        if (*p == '+' || *p == '-' || *p == '*' || *p == '/') {
            current = newToken(TokenReserved, current, p++);
            continue;
        }
        if (isDigit(*p)) {
            current = newToken(TokenNumber, current, p);
            current->val = strtol(p, &p, 10);
            continue;
        }

        errorAt(p, "Cannot tokenize");
    }

    newToken(TokenEOF, current, p);

    return head.next;
}

// If the current token is the expected token, consume the token and returns
// TRUE.
int consumeToken(char op) {
    if (globals.token->type == TokenReserved && *globals.token->str == op) {
        globals.token = globals.token->next;
        return 1;
    }
    return 0;
}

// If the current token is the expected sign, consume the token. Otherwise
// reports an error.
void expectSign(char op) {
    if (globals.token->type == TokenReserved && *globals.token->str == op) {
        globals.token = globals.token->next;
        return;
    }
    errorAt(globals.token->str, "'%c' is expected.", op);
}

// If the current token is the number token, consume the token and returns the
// value. Otherwise reports an error.
int expectNumber() {
    if (globals.token->type == TokenNumber) {
        int val = globals.token->val;
        globals.token = globals.token->next;
        return val;
    }
    errorAt(globals.token->str, "Non number appears.");
    return 0;
}

int atEOF() {
    return globals.token->type == TokenEOF;
}

Node *newNode(NodeKind kind, Node *lhs, Node *rhs) {
    Node *n = calloc(1, sizeof(Node));
    n->kind = kind;
    n->lhs =  lhs;
    n->rhs =  rhs;
    return n;
}

Node *newNodeNum(int val) {
    Node *n = newNode(NodeNum, NULL, NULL);
    n->val = val;
    return n;
}

Node *expr();
Node *mul();
Node *primary();

Node *expr() {
    Node *n = mul();
    for (;;) {
        if (consumeToken('+')) {
            n = newNode(NodeAdd, n, mul());
        } else if (consumeToken('-')) {
            n = newNode(NodeSub, n, mul());
        } else {
            return n;
        }
    }
}

Node *mul() {
    Node *n = primary();
    for (;;) {
        if (consumeToken('*')) {
            n = newNode(NodeMul, n, primary());
        } else if (consumeToken('/')) {
            n = newNode(NodeDiv, n, primary());
        } else {
            return n;
        }
    }
}

Node *primary() {
    if (consumeToken('(')) {
        Node *n = expr();
        expectSign(')');
        return n;
    }
    return newNodeNum(expectNumber());
}

void genCode(Node *n) {
    if (n->kind == NodeNum) {
        printf("  push %d\n", n->val);
        return;
    }

    genCode(n->lhs);
    genCode(n->rhs);

    puts("  pop rdi");
    puts("  pop rax");

    if (n->kind == NodeAdd) {
        puts("  add rax, rdi");
    } else if (n->kind == NodeSub) {
        puts("  sub rax, rdi");
    } else if (n->kind == NodeMul) {
        puts("  imul rax, rdi");
    } else if (n->kind == NodeDiv) {
        puts("  cqo");
        puts("  idiv rdi");
    }

    puts("  push rax");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Invalid arguments\n");
        return 1;
    }

    globals.source = argv[1];
    globals.token = tokenize();
    Node *n = expr();

    puts(".intel_syntax noprefix");
    puts(".globl main");
    puts("main:");

    genCode(n);

    puts("  pop rax");
    puts("  ret");

    return 0;
}
