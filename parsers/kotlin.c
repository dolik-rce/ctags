#include "general.h"

#include <string.h>

#include "parse.h"
#include "read.h"
#include "entry.h"
#include "debug.h"

static kindDefinition KotlinKindTable [] = {
    { true, 'p', "package", "packages", },
    { true, 'i', "interface", "interfaces", },
    { true, 'c', "class", "classes", },
    { true, 'o', "object", "objects", },
    { true, 'm', "method", "methods", },
    { true, 'T', "typealias", "typealiases", },
    { true, 'C', "constant", "constants", },
    { true, 'v', "variable", "variables", },
};

typedef enum  {
    KIND_PACKAGE,
    KIND_INTERFACE,
    KIND_CLASS,
    KIND_OBJECT,
    KIND_METHOD,
    KIND_TYPEALIAS,
    KIND_CONSTANT,
    KIND_VARIABLE
} KotlinKind;

typedef void (*tagProcessor) (KotlinKind token);

typedef struct {
    const char* keyword;
    int length;
    tagProcessor processor;
} KotlinKindInfo;

static int readString (void);
static int readChar (void);

static void initializeKotlinParser (const langType language)
{
}

static int CurrentNamespace;

static void createKotlinTag (const vString * name, unsigned long kind)
{
    int namespace = CORK_NIL;
    tagEntryInfo e;
    Assert(vStringLength(name) > 0);
    initTagEntry (&e, vStringValue(name), kind);
    e.lineNumber = getInputLineNumber ();
    e.filePosition = getInputFilePosition ();
    namespace = makeTagEntry (&e);
    if (namespace >= 0 && kind <= KIND_METHOD)
    {
        CurrentNamespace = namespace;
        fprintf(stderr, "DBG: current namespace:%d\n", CurrentNamespace);
    }
}

static void skipWhitespace (void)
{
    int c;
	do
	{
		c = getcFromInputFile ();
	}
	while (c <= 32 && c != EOF);
    ungetcToInputFile(c);
}

static void skipComments (void) {
    int c, c2;
    do {
        c = getcFromInputFile ();
        if (c == EOF)
            return;
        if (c != '/')
        {
            ungetcToInputFile(c);
            return;
        }
        c2 = getcFromInputFile ();
        switch (c2) {
            case '*':
                skipToCharacterInInputFile2 ('*', '/');
                DebugStatement(debugPrintf(DEBUG_PARSE, "<$end-block-comment$>"););
                break;
            case '/':
                skipToCharacterInInputFile ('\n');
                DebugStatement(debugPrintf(DEBUG_PARSE, "<$end-line-comment$>"););
                break;
            default:
                ungetcToInputFile (c2);
                ungetcToInputFile (c);
                return;
        }
        skipWhitespace();
    } while (1);
}

static void skipOverPair (char openChar, char closeChar)
{
    int c;
    DebugStatement(debugPrintf(DEBUG_PARSE, "<$skip:%c%c$>", openChar, closeChar););
    do
    {
        c = getcFromInputFile ();
        if ( c == closeChar )
        {
            DebugStatement(debugPrintf(DEBUG_PARSE, "<$end-skip$>"););
            return;
        }
        else if (c == openChar)
        {
            skipOverPair(openChar, closeChar);
        }
        else
        {
            switch (c) {
                case EOF:
                    return;
                case '"':
                    ungetcToInputFile (c);
                    readString();
                    break;
                case '\'':
                    ungetcToInputFile (c);
                    readChar();
                    break;
            }
        }
    } while (1);
}

#define isIdentifierChar(c) \
	(isalnum (c) || (c) == '_' || (c) == '.')

static int readIdentifier (vString *const string)
{
	int c;
    int n = 0;
    do
	{
        if (n > 0 && c != '.')
        {
            vStringPut (string, (char) c);
        }
		c = getcFromInputFile ();
        if (c == '`')
        {
            //TODO: better handling of escaped identifiers (e.g. fun `simple test` = ...)
            c = getcFromInputFile ();
        }
		if (c == EOF)
            return 0;
        if (c == '.')
        {
            vStringClear(string);
        }
        n++;
	}
	while (isIdentifierChar (c));
	ungetcToInputFile (c);
    return vStringLength(string);
}

static int readChar (void)
{
    int c;
    c = getcFromInputFile ();
    if (c == EOF)
        return 0;
    if (c != '\'')
    {
        ungetcToInputFile(c);
        return 0;
    }
    if (c == '\\') {
        c = getcFromInputFile ();
    }
    skipToCharacterInInputFile ('\'');
    return 1;
}

static int readTemplate (int c) {
    if (c != '$')
        return 0;
    c = getcFromInputFile();
    if (c != '{')
    {
        ungetcToInputFile(c);
        return 0;
    }
    DebugStatement(debugPrintf(DEBUG_PARSE, "<$template$>"););
    skipOverPair('{', '}');
    DebugStatement(debugPrintf(DEBUG_PARSE, "<$end-template$>"););
    return 1;
}

static void readRawString (void)
{
    int c;
    do
    {
        c = getcFromInputFile ();
        if (readTemplate(c) || c != '"')
            continue;
        c = getcFromInputFile ();
        if (readTemplate(c) || c != '"')
            continue;
        c = getcFromInputFile ();
        if (readTemplate(c) || c != '"')
            continue;
        break;
    } while (c != EOF);
    DebugStatement(debugPrintf(DEBUG_PARSE, "<$end-raw-string$>"););
}

static int readString (void)
{
    int c, c2;
    c = getcFromInputFile ();
    if (c != '"')
    {
        ungetcToInputFile(c);
        return 0;
    }
    DebugStatement(debugPrintf(DEBUG_PARSE, "<$string$>"););
    c = getcFromInputFile ();
    c2 = getcFromInputFile ();
    if (c == EOF || c2 == EOF)
        return 0;
    if (c == '"' && c2 == '"')
    {
        readRawString ();
        return 1;
    }
    else if ( c == '"')
    {
        ungetcToInputFile (c2);
        DebugStatement(debugPrintf(DEBUG_PARSE, "<$end-empty-string$>"););
        return 1;
    }
    else
    {
        ungetcToInputFile (c2);
        ungetcToInputFile (c);
    }
    do
    {
        c = getcFromInputFile ();
        if (c == '\\')
        {
            getcFromInputFile ();
        }
        readTemplate(c);
    } while (c != '"' && c != '\n' && c != EOF);
    DebugStatement(debugPrintf(DEBUG_PARSE, "<$end-string$>"););
    return 1;
}

static int readSingleChar (vString* token)
{
    int c;
    c = getcFromInputFile ();
    vStringPut(token, c);
    if (c == EOF)
        return 0;
    return 1;
}

static int readToken (vString* token)
{
    int res;
    do
    {
        vStringClear(token);
        skipWhitespace();
        skipComments();
        res = readIdentifier(token) || readString() || readChar() || readSingleChar(token);
        if (res == 0) {
            return 0;
        }
        if (vStringLength(token) > 0) {
            return 1;
        }
    }
    while(1);
}

#define vStringCompare(vs, str, n) (vStringLength(vs) == n && strncmp(vStringValue(vs), str, n) == 0)

static void simpleProcessor (KotlinKind kind) {
    vString * tmp = vStringNew();
    readToken(tmp);
    createKotlinTag(tmp, kind);
}

static void functionProcessor (KotlinKind kind) {
    vString * tmp = vStringNew();
    readToken(tmp);
    if (vStringCompare(tmp, "<", 1))
    {
        skipOverPair ('<','>');
        readToken(tmp);
    }
    createKotlinTag(tmp, kind);
}

static void propertyProcessor(KotlinKind kind) {
    vString * tmp = vStringNew();
    readToken(tmp);
    if (vStringCompare(tmp, "(", 1)) {
        readToken(tmp);
        createKotlinTag(tmp, kind);
        while (readToken(tmp))
        {
            if (vStringCompare(tmp, "<", 1))
            {
                skipOverPair('<', '>');
            }
            else if (vStringCompare(tmp, ",", 1))
            {
                readToken(tmp);
                createKotlinTag(tmp, kind);
            }
            else if (vStringCompare(tmp, ")", 1))
            {
                break;
            }
        }
    } else {
        createKotlinTag(tmp, kind);
    }
}

static void packageProcessor (KotlinKind kind) {
    int c;
    vString * tmp = vStringNew();
    skipWhitespace();
    skipComments();
    do
    {
        c = getcFromInputFile ();
        if (c == EOF)
            return;
        if (isIdentifierChar(c))
        {
            vStringPut(tmp, c);
        }
        else
        {
            ungetcToInputFile (c);
            break;
        }
    }
    while (1);
    if (vStringLength(tmp) > 0)
    {
        createKotlinTag(tmp, kind);
    }
}

static KotlinKindInfo kotlinKindInfoTable [] = {
    { "package", 7, packageProcessor },
    { "interface", 9, simpleProcessor },
    { "class", 5, simpleProcessor },
    { "object", 6, simpleProcessor },
    { "fun", 3, functionProcessor },
    { "typealias", 9, simpleProcessor },
    { "val", 3, propertyProcessor },
    { "var", 3, propertyProcessor },
};

static void findKotlinTags (void)
{
    vString * token = vStringNew();
    while(readToken(token)) {
        DebugStatement(debugPrintf(DEBUG_PARSE, "<$token:%s$>", vStringValue(token)););
        for (int i = 0; i < ARRAY_SIZE(kotlinKindInfoTable); i++)
        {
            KotlinKindInfo* info = kotlinKindInfoTable + i;
            if (vStringCompare(token, info->keyword, info->length))
            {
                info->processor((KotlinKind)i);
                break;
            }
        }
    }
}

extern parserDefinition* KotlinParser (void)
{
	static const char *const extensions [] = { "kt", "kts", NULL };


	parserDefinition* const def = parserNew ("Kotlin");

	def->enabled       = true;
	def->extensions    = extensions;
	def->kindTable     = KotlinKindTable;
	def->kindCount     = ARRAY_SIZE(KotlinKindTable);
	def->initialize    = initializeKotlinParser;
    def->parser        = findKotlinTags;
    def->useCork       = CORK_QUEUE;
    def->requestAutomaticFQTag = 1;
	return def;
}
