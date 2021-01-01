/*
 *   Copyright (c) 2019 Masatake YAMATO
 *   Copyright (c) 2019 Red Hat, Inc.
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License version 2 or (at your option) any later version.
 *
 *   This module contains functions to generate tags for Kotlin.
 */

/*
*   INCLUDE FILES
*/
#include "debug.h"
#include "options.h"
#include "parse.h"
#include "routines.h"

#include <signal.h>

/*
* FUNCTION DEFINITIONS
*/
static void pushKind (struct parserCtx *auxil, int kind)
{
	intArrayAdd (auxil->kind_stack, kind);
}

static void popKind (struct parserCtx *auxil, bool popScopeToo)
{
	intArrayRemoveLast (auxil->kind_stack);

	if (popScopeToo)
	{
		tagEntryInfo *e = getEntryInCorkQueue (auxil->scope_cork_index);
		if (e)
			auxil->scope_cork_index = e->extensionFields.scopeIndex;
	}
}

static int peekKind (struct parserCtx *auxil)
{
	return intArrayLast (auxil->kind_stack);
}

static void reportError (struct parserCtx *auxil)
{
    auxil->found_syntax_error = true;
	verbose ("%s: syntax error in \"%s\"\n",
			 getLanguageName (getInputLanguage ()),
			 getInputFileName());
	raise(SIGINT);
}

static int makeKotlinTag (struct parserCtx *auxil, const char *name, long offset)
{
	tagEntryInfo e;
	int k = peekKind (auxil);
	initTagEntry(&e, name, k);
	e.lineNumber = getInputLineNumberForFileOffset (offset);
	e.filePosition = getInputFilePositionForLine (e.lineNumber);
	e.extensionFields.scopeIndex = auxil->scope_cork_index;
	return makeTagEntry (&e);
}

static void ctxInit (struct parserCtx *auxil)
{
	auxil->kind_stack = intArrayNew ();
	pushKind (auxil, K_INTERFACE);
	auxil->scope_cork_index = CORK_NIL;
	auxil->found_syntax_error = false;
}

static void ctxFini (struct parserCtx *auxil)
{
	popKind (auxil, false);
	intArrayDelete (auxil->kind_stack);
}

static void findKotlinTags (void)
{
	struct parserCtx auxil;

	ctxInit (&auxil);
	pkotlin_context_t *pctx = pkotlin_create(&auxil);

	while (pkotlin_parse(pctx, NULL) && (!auxil.found_syntax_error) );

	pkotlin_destroy(pctx);
	ctxFini (&auxil);
}

extern parserDefinition* KotlinParser (void)
{
	static const char *const extensions [] = { "kt", "kts", NULL };
	parserDefinition* def = parserNew ("Kotlin");
	def->kindTable  = KotlinKinds;
	def->kindCount  = ARRAY_SIZE (KotlinKinds);
	def->extensions = extensions;
	def->parser     = findKotlinTags;
	def->useCork    = true;
	def->enabled    = true;
	def->defaultScopeSeparator = ".";
	return def;
}
