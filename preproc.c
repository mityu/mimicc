#include <string.h>
#include "mimicc.h"

typedef struct Macro Macro;
struct Macro {
    Macro *next;
    Token *token;
    Token *replace;  // Replacement-list is tokens until "TokenNewLine" appears.
};

typedef struct Preproc Preproc;
struct Preproc {
    Macro *macros;
};

static Preproc preproc;

static Macro *newMacro(Token *token, Token *replace) {
    Macro *macro = (Macro *)safeAlloc(sizeof(Macro));
    macro->token = token;
    macro->replace = replace;
    return macro;
}

static Macro *findMacro(Token *name) {
    for (Macro *macro = preproc.macros; macro; macro = macro->next) {
        if (matchToken(macro->token, name->str, name->len)) {
            return macro;
        }
    }
    return NULL;
}

static int matchTokenReserved(Token *token, const char *name) {
    return token->type == TokenReserved && matchToken(token, name, strlen(name));
}

static int matchTokenIdent(Token *token, const char *name) {
    return token->type == TokenIdent && matchToken(token, name, strlen(name));
}

static int consumeTokenReserved(Token **token, const char *name) {
    if (matchTokenReserved(*token, name)) {
        *token = (*token)->next;
        return 1;
    }
    return 0;
}

static int consumeTokenIdent(Token **token, const char *name) {
    if (matchTokenIdent(*token, name)) {
        *token = (*token)->next;
        return 1;
    }
    return 0;
}

static Token *consumeTokenAnyIdent(Token **token) {
    if ((*token)->type == TokenIdent) {
        Token *ident = *token;
        *token = (*token)->next;
        return ident;
    }
    return NULL;
}

// Clone token, but clears "next" and "prev" entry with NULL.
static Token *cloneToken(Token *token) {
    Token *clone = (Token *)safeAlloc(sizeof(Token));
    *clone = *token;
    clone->next = clone->prev = NULL;
    return clone;
}

// Clone token by range [begin, end].
static Token *cloneTokenList(Token *begin, Token *end) {
    Token head = {};
    Token *clones = &head;

    end = end->next;
    for (Token *token = begin; token != end; token = token->next) {
        Token *next = cloneToken(token);
        clones->next = next;
        next->prev = clones;
        clones = next;
    }
    if (head.next)
        head.next->prev = NULL;
    return head.next;
}

static void insertTokens(Token *pos, Token *begin, Token *end) {
    // No need to NULL check.
    Token *next = pos->next;

    pos->next = begin;
    begin->prev = pos;
    end->next = next;
    next->prev = end;
}

// Skip tokens until "TokenNewLine" appears, and returns the pointer to it.
static Token *skipUntilNewline(Token *token) {
    while (token->type != TokenNewLine)
        token = token->next;
    return token;
}


// Apply predefined macros.  Return TRUE if applied.
static int applyPredefinedMacro(Token *token) {
    if (matchToken(token , "__LINE__", 8)) {
        token->type = TokenNumber;
        token->val = token->line;
    } else {
        return 0;
    }
    return 1;
}

// Try to apply macro for "token".  If macro applied, returns the pointer to
// the last token of replacements (if macro is expanded to empty, returns the
// pointer to the prev token of "token").  Otherwise returns NULL.
// Note that this function does NOT handle any predefined macros.
static Token *applyMacro(Token *token) {
    Macro *macro = NULL;
    Token *prev = token->prev;
    Token *begin = NULL;
    Token *end = NULL;

    macro = findMacro(token);
    if (!macro)
        return NULL;

    if (macro->replace->type != TokenNewLine) {
        Token *cur = NULL;
        begin = macro->replace;
        end = skipUntilNewline(begin)->prev;
        begin = cur = cloneTokenList(begin, end);
        for (;;) {
            cur->line = token->line;
            cur->column = token->column;

            if (cur->next)
                cur = cur->next;
            else
                break;
        }
        end = cur;

        insertTokens(prev, begin, end);
    } else {
        end = prev;
    }

    popTokenRange(token, token);

    return end;
}

// Parse and apply preprocess for given tokens, and returns the head to the
// token list.
void preprocess(Token *token) {
    // TODO: Make sure the first token has type "TokenSOF"?
    while (token && token->type != TokenEOF) {
        if (consumeTokenReserved(&token, "#")) {
            if (consumeTokenIdent(&token, "define")) {
                // Macro definition
                Macro *macro = NULL;
                Token *macroName = NULL;
                Token *macroBegin = token->prev->prev;
                // Note that new-line token won't be replaced, so this is OK.
                Token *macroEnd = skipUntilNewline(token);

                macroName = consumeTokenAnyIdent(&token);

                if (!macroName) {
                    errorAt(token, "Macro name expected.");
                } else if (findMacro(macroName)) {
                    errorAt(macroName, "Redefinition of macro.");
                }

                for (Token *cur = token; cur != macroEnd; cur = cur->next) {
                    Token *applied = applyMacro(cur);
                    if (applied)
                        cur = applied;
                }

                macro = newMacro(macroName, macroName->next);
                macro->next = preproc.macros;
                preproc.macros = macro;

                token = macroEnd->next;
                popTokenRange(macroBegin, macroEnd);
            }
        } else {
            Token *prev = token->prev;
            Token *applied = applyMacro(token);
            if (applied) {
                token = applied->next;
                for (Token *cur = prev->next; cur != token; cur = cur->next)
                    applyPredefinedMacro(cur);
            } else {
                applyPredefinedMacro(token);
                token = token->next;
            }
        }
    }
}
