#include <string.h>
#include "mimicc.h"

typedef struct Macro Macro;
struct Macro {
    Macro *next;
    Token *token;    // Macro name.
    Token *replace;  // Replacement-list is tokens, until "TokenNewLine" appears.
    int isFunc;      // TRUE if macro is function-like macro.
    Token *args;     // Argument list of function-like macro.
};

// Structure to use in function-like macro expansion.  Holds which tokens are
// replacement tokens of a macro arguments.
typedef struct MacroArg MacroArg;
struct MacroArg {
    MacroArg *next;
    Token *name;        // Argument name
    Token *begin;       // Start of replacement
    Token *end;         // End of replacement
};

typedef struct Preproc Preproc;
struct Preproc {
    Macro *macros;            // All macro list.
};

static Preproc preproc;

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

static Token *newTokenSOF(void) {
    Token *token = (Token *)safeAlloc(sizeof(Token));
    token->type = TokenSOF;
    return token;
}

static Token *newTokenEOF(void) {
    Token *token = (Token *)safeAlloc(sizeof(Token));
    token->type = TokenEOF;
    return token;
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

static void concatToken(Token *first, Token *second) {
    first->next = second;
    second->prev = first;
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

    // Check for function-like macro.  Function-like macro doesn't allow any
    // white-spaces between identifier and lbrace, e.g.:
    //   #define FUNC(args)       //  Function-like macro
    //   #define NOT_FUNC (args)  //  Not a function-like macro
    // So, we need to check not only there's a "(" after identifier but also
    // there's no white-spaces between identifier and "(".
    if (matchTokenReserved(token, "(") &&
            (token->prev->str + token->prev->len) == token->str) {
        macro->isFunc = 1;
        consumeTokenReserved(&token, "(");
        if (!consumeTokenReserved(&token, ")")) {
            Token head = {};
            Token *cur = &head;
            for (;;) {
                Token *arg = consumeTokenAnyIdent(&token);
                if (!arg)
                    errorAt(token, "An identifier is expected.");
                arg = cloneToken(arg);
                cur->next = arg;
                arg->prev = cur;
                cur = cur->next;

                if (consumeTokenReserved(&token, ")"))
                    break;
                else if (!consumeTokenReserved(&token, ","))
                    errorAt(token, "',' or ')' is expected.");
            }
            macro->args = head.next;
            macro->replace = token;
        }
    }

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
        Macro *tofree = macro->next;
        *macro = *macro->next;
        safeFree(tofree);
    } else {
        // Macro is at the end of entry.
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

// Replace arguments of function-like macro.  Target tokens are what in range
// [begin, end].
static void replaceMacroArgs(MacroArg *args, Token *begin, Token *end) {
    Token *termination = end->next;
    Token *token = begin;

    while (token != termination) {
        MacroArg *replacement = NULL;
        Token *repBegin = NULL;
        Token *repEnd = NULL;

        for (MacroArg *arg = args; arg; arg = arg->next) {
            if (matchToken(arg->name, token->str, token->len)) {
                replacement = arg;
                break;
            }
        }
        if (!replacement) {
            token = token->next;
            continue;
        }

        repBegin = repEnd = cloneTokenList(replacement->begin, replacement->end);
        while (repEnd->next)
            repEnd = repEnd->next;

        insertTokens(token, repBegin, repEnd);
        popTokenRange(token, token);

        token = repEnd->next;
    }
}

// Parse arguments of function-like macro and returns result.  If parse
// succeeded, the "endOfArg" parameter is set to the token at the end of
// arguments.
// NOTE: Make sure "begin" points to the macro name token.
static MacroArg *parseMacroArguments(Macro *macro, Token *begin, Token **endOfArg) {
    Token *token = begin;
    MacroArg head = {};
    MacroArg *curArg = &head;

    if (!matchToken(macro->token, token->str, token->len))
        errorUnreachable();

    token = token->next;  // Skip macro name token.

    if (!consumeTokenReserved(&token, "("))
        errorAt(token, "'(' is expected.");

    if (!macro->args && !consumeTokenReserved(&token, ")"))
        errorAt(token, "')' is expected.");

    for (Token *argName = macro->args; argName; argName = argName->next) {
        MacroArg *arg = (MacroArg *)safeAlloc(sizeof(MacroArg));
        int parenthesis = 0;
        char *terminator = ",";

        if (!argName->next)
            terminator = ")";

        arg->name = argName;
        arg->begin = token;

        for (;;) {
            if (parenthesis == 0 && consumeTokenReserved(&token, terminator)) {
                arg->end = token->prev->prev;
                *endOfArg = token->prev;
                break;
            } else if (consumeTokenReserved(&token, "(")) {
                parenthesis++;
            } else if (consumeTokenReserved(&token, ")")) {
                parenthesis--;
                if (parenthesis < 0)
                    errorAt(token->prev, "Unmatched ')'.");
            } else if (token->type == TokenEOF) {
                errorAt(token, "Unexpected EOF.");
            } else {
                token = token->next;
            }
        }

        curArg->next = arg;
        curArg = curArg->next;
    }

    return head.next;
}

// Apply macro for "begin".  If macro applied, returns the pointer to the first
// token of replacements (if macro is expanded to empty, returns the pointer to
// the next token of "begin").  Otherwise returns NULL.
static Token *applyMacro(Token *begin) {
    typedef struct {
        Token *begin;
        Token *end;
    } Range;

    Macro *macro = NULL;
    MacroArg *macroArgs = NULL;
    Token *cur = begin;
    Token *retpos = NULL;
    Range src = {}, dest = {};

    macro = findMacro(cur);
    if (!macro)
        return NULL;

    src.begin = src.end = cur;
    dest.begin = dest.end = macro->replace;
    if (dest.end->type != TokenNewLine)
        while (dest.end->next->type != TokenNewLine)
            dest.end = dest.end->next;

    if (!macro->isFunc) {
        if (src.begin->type != TokenNewLine) {
            dest.begin = dest.end = cloneTokenList(dest.begin, dest.end);
            for (Token *token = dest.begin; token; token = token->next) {
                token->line = src.begin->line;
                token->column = src.begin->column;
                if (!token->next)
                    dest.end = token;
            }
            insertTokens(src.end, dest.begin, dest.end);
        }
        retpos = src.end->next;
        popTokenRange(src.begin, src.end);
        return retpos;
    }

    macroArgs = parseMacroArguments(macro, cur, &src.end);
    cur = src.end->next;

    if (dest.begin->type != TokenNewLine) {
        Range wrapper = {};
        wrapper.begin = newTokenSOF();
        wrapper.end = newTokenEOF();

        dest.begin = cloneTokenList(dest.begin, dest.end);
        for (Token *token = dest.begin; token; token = token->next) {
            token->line = src.begin->line;
            token->column = src.begin->column;
            if (!token->next)
                dest.end = token;
        }

        concatToken(wrapper.begin, dest.begin);
        concatToken(dest.end, wrapper.end);

        replaceMacroArgs(macroArgs, dest.begin, dest.end);
        insertTokens(src.end, dest.begin, dest.end);
        popTokenRange(src.begin, src.end);

        retpos = wrapper.begin->next;

        safeFree(wrapper.begin);
        safeFree(wrapper.end);
    } else {
        retpos = src.begin->next;
        popTokenRange(src.begin, src.end);
    }

    return retpos;
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
            Token *applied = NULL;
            applied = applyMacro(token);

            if (applied) {
                token = applied;
            } else {
                applyPredefinedMacro(token);
                token = token->next;
            }
        }
    }
}
