#include "mimicc.h"

typedef struct Macro Macro;
struct Macro {
    Macro *next;
    Token *token;
};

// Parse and apply preprocess for given tokens, and returns the head to the
// token list.
Token *preprocess(Token *token) {
    Token *head = token;
    while (token && token->type != TokenEOF) {
        if (token->type == TokenReserved && matchToken(token, "#", 1)) {
            Token *begin = token;
            Token *end = NULL;

            while (token && !(token->type == TokenEOF || token->type == TokenNewLine))
                token = token->next;

            end = token;
            if (end->type == TokenEOF)
                end = end->prev;
            else
                token = token->next;

            head = popTokenRange(head, begin, end);
        } else {
            token = token->next;
        }
    }
    return head;
}
