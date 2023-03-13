#include <string.h>
#include "mimicc.h"

typedef struct Macro Macro;
struct Macro {
    Macro *next;
    Token *token;
    Token *replace;  // Replacement-list is tokens until "TokenNewLine" appears.
    int isUsed;
};

typedef struct Preproc Preproc;
struct Preproc {
    Macro *macros;          // All macro list.
};

static Preproc preproc;

static Token *applyMacro(Token *token);
static Token *applyMacroInRange(Token *begin, Token *end);

static Macro *newMacro(Token *token, Token *replace) {
    Macro *macro = (Macro *)safeAlloc(sizeof(Macro));
    macro->token = token;
    macro->replace = replace;
    return macro;
}

// Search for macro named "name" in all macro list, and returns the matched
// macro object if found.  If macro not found, returns NULL instead.
static Macro *findMacro(Token *name) {
    for (Macro *macro = preproc.macros; macro; macro = macro->next) {
        if (matchToken(macro->token, name->str, name->len)) {
            return macro;
        }
    }
    return NULL;
}

// Return TRUE if macro is used in one macro expansion sequence.
static int isMacroUsed(Macro *macro) {
    return macro->isUsed;
}

static void markMacroAsUsed(Macro *macro) {
    macro->isUsed = 1;
}

static void clearUsedMacroMarks(void) {
    for (Macro *macro = preproc.macros; macro; macro = macro->next)
        macro->isUsed = 0;
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

// Parse "#define" directive and returns one token after the token at the end
// of this "#define" directive.  Note that "token" parameter must points the
// "#" token of "#define".
static Token *parseDefineDirective(Token *token) {
    Macro *macro = NULL;
    Token *macroName = NULL;
    Token *tokenHash = token;
    Token *nextLine = NULL;

    if (!(consumeTokenReserved(&token, "#") && consumeTokenIdent(&token, "define")))
        errorUnreachable();

    macroName = consumeTokenAnyIdent(&token);

    if (!macroName) {
        errorAt(token, "Macro name expected.");
    } else if (findMacro(macroName)) {
        errorAt(macroName, "Redefinition of macro.");
    }

    macro = newMacro(macroName, macroName->next);
    macro->next = preproc.macros;
    preproc.macros = macro;

    nextLine = skipUntilNewline(token)->next;
    popTokenRange(tokenHash, nextLine->prev);
    return nextLine;
}

// Parse "#undef" directive and returns one token after the token at the
// end of this "#undef" directive.  Note that "token" parameter must points the
// "#" token of "#define".
static Token *parseUndefDirective(Token *token) {
    Token *head = token;
    Token *name = NULL;
    Token *nextLine = NULL;
    Macro *macro = NULL;

    if (!(consumeTokenReserved(&token, "#") && consumeTokenIdent(&token, "undef")))
        errorUnreachable();

    name = consumeTokenAnyIdent(&token);
    if (!name)
        errorAt(token, "Macro name expected.");

    macro = findMacro(name);
    if (!macro)
        errorAt(name, "Undefined macro.");

    if (macro->next) {
        *macro = *macro->next;
        safeFree(macro->next);
    } else {
        if (preproc.macros != macro) {
            Macro *prev = preproc.macros;
            while (prev->next != macro)
                prev = prev->next;
            prev->next = NULL;
        } else {
            preproc.macros = NULL;
        }
        safeFree(macro);
    }

    nextLine = skipUntilNewline(head)->next;
    popTokenRange(head, nextLine->prev);
    return nextLine;
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
    if (!macro || isMacroUsed(macro))
        return NULL;

    if (macro->replace->type != TokenNewLine) {
        Token *cur = NULL;

        markMacroAsUsed(macro);
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

        popTokenRange(token, token);
        insertTokens(prev, begin, end);
        applyMacroInRange(begin, end);
    } else {
        end = prev;
        popTokenRange(token, token);
    }

    return end;
}

// Try to apply macro for each tokens in range [begin, end].  Return the
// pointer to the end of range [begin', end'].
static Token *applyMacroInRange(Token *begin, Token *end) {
    Token *stop = end->next;
    Token *cur = NULL;
    for (cur = begin; cur != stop; cur = cur->next) {
        Token *applied = applyMacro(cur);
        if (applied)
            cur = applied;
    }
    return cur;
}

// Parse and apply preprocess for given tokens, and returns the head to the
// token list.
void preprocess(Token *token) {
    // TODO: Make sure the first token has type "TokenSOF"?
    while (token && token->type != TokenEOF) {
        if (consumeTokenReserved(&token, "#")) {
            Token *tokenHash = token->prev;

            if (consumeTokenIdent(&token, "define")) {
                token = parseDefineDirective(tokenHash);
            } else if (consumeTokenIdent(&token, "undef")) {
                token = parseUndefDirective(tokenHash);
            }
        } else {
            Token *prev = token->prev;
            Token *applied = NULL;

            clearUsedMacroMarks();
            applied = applyMacro(token);
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
