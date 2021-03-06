/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BytecodeCompiler.h"

#include "jsprobes.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/FoldConstants.h"
#include "frontend/NameFunctions.h"
#include "vm/GlobalObject.h"

#include "jsinferinlines.h"

#include "frontend/ParseMaps-inl.h"
#include "frontend/Parser-inl.h"
#include "frontend/SharedContext-inl.h"

using namespace js;
using namespace js::frontend;

static bool
CheckLength(JSContext *cx, size_t length)
{
    // Note this limit is simply so we can store sourceStart and sourceEnd in
    // JSScript as 32-bits. It could be lifted fairly easily, since the compiler
    // is using size_t internally already.
    if (length > UINT32_MAX) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_SOURCE_TOO_LONG);
        return false;
    }
    return true;
}

static bool
SetSourceMap(JSContext *cx, TokenStream &tokenStream, ScriptSource *ss, UnrootedScript script)
{
    if (tokenStream.hasSourceMap()) {
        if (!ss->setSourceMap(cx, tokenStream.releaseSourceMap(), script->filename))
            return false;
    }
    return true;
}

UnrootedScript
frontend::CompileScript(JSContext *cx, HandleObject scopeChain,
                        HandleScript evalCaller,
                        const CompileOptions &options,
                        const jschar *chars, size_t length,
                        JSString *source_ /* = NULL */,
                        unsigned staticLevel /* = 0 */,
                        SourceCompressionToken *extraSct /* = NULL */)
{
    RootedString source(cx, source_);

    class ProbesManager
    {
        const char* filename;
        unsigned lineno;

      public:
        ProbesManager(const char *f, unsigned l) : filename(f), lineno(l) {
            Probes::compileScriptBegin(filename, lineno);
        }
        ~ProbesManager() { Probes::compileScriptEnd(filename, lineno); }
    };
    ProbesManager probesManager(options.filename, options.lineno);

    /*
     * The scripted callerFrame can only be given for compile-and-go scripts
     * and non-zero static level requires callerFrame.
     */
    JS_ASSERT_IF(evalCaller, options.compileAndGo);
    JS_ASSERT_IF(staticLevel != 0, evalCaller);

    if (!CheckLength(cx, length))
        return UnrootedScript(NULL);
    JS_ASSERT_IF(staticLevel != 0, options.sourcePolicy != CompileOptions::LAZY_SOURCE);
    ScriptSource *ss = cx->new_<ScriptSource>();
    if (!ss)
        return UnrootedScript(NULL);
    ScriptSourceHolder ssh(ss);
    SourceCompressionToken mysct(cx);
    SourceCompressionToken *sct = (extraSct) ? extraSct : &mysct;
    switch (options.sourcePolicy) {
      case CompileOptions::SAVE_SOURCE:
        if (!ss->setSourceCopy(cx, chars, length, false, sct))
            return UnrootedScript(NULL);
        break;
      case CompileOptions::LAZY_SOURCE:
        ss->setSourceRetrievable();
        break;
      case CompileOptions::NO_SOURCE:
        break;
    }

    Parser parser(cx, options, chars, length, /* foldConstants = */ true);
    if (!parser.init())
        return UnrootedScript(NULL);
    parser.sct = sct;

    GlobalSharedContext globalsc(cx, scopeChain, StrictModeFromContext(cx));

    ParseContext pc(&parser, &globalsc, staticLevel, /* bodyid = */ 0);
    if (!pc.init())
        return UnrootedScript(NULL);

    bool savedCallerFun =
        options.compileAndGo &&
        evalCaller &&
        (evalCaller->function() || evalCaller->savedCallerFun);
    Rooted<JSScript*> script(cx, JSScript::Create(cx, NullPtr(), savedCallerFun,
                                                  options, staticLevel, ss, 0, length));
    if (!script)
        return UnrootedScript(NULL);

    // Global/eval script bindings are always empty (all names are added to the
    // scope dynamically via JSOP_DEFFUN/VAR).
    InternalHandle<Bindings*> bindings(script, &script->bindings);
    if (!Bindings::initWithTemporaryStorage(cx, bindings, 0, 0, NULL))
        return UnrootedScript(NULL);

    // We can specialize a bit for the given scope chain if that scope chain is the global object.
    JSObject *globalScope = scopeChain && scopeChain == &scopeChain->global() ? (JSObject*) scopeChain : NULL;
    JS_ASSERT_IF(globalScope, globalScope->isNative());
    JS_ASSERT_IF(globalScope, JSCLASS_HAS_GLOBAL_FLAG_AND_SLOTS(globalScope->getClass()));

    BytecodeEmitter bce(/* parent = */ NULL, &parser, &globalsc, script, evalCaller, !!globalScope,
                        options.lineno, options.selfHostingMode);
    if (!bce.init())
        return UnrootedScript(NULL);

    /* If this is a direct call to eval, inherit the caller's strictness.  */
    if (evalCaller && evalCaller->strict)
        globalsc.strict = true;

    if (options.compileAndGo) {
        if (source) {
            /*
             * Save eval program source in script->atoms[0] for the
             * eval cache (see EvalCacheLookup in jsobj.cpp).
             */
            JSAtom *atom = AtomizeString<CanGC>(cx, source);
            jsatomid _;
            if (!atom || !bce.makeAtomIndex(atom, &_))
                return UnrootedScript(NULL);
        }

        if (evalCaller && evalCaller->functionOrCallerFunction()) {
            /*
             * An eval script in a caller frame needs to have its enclosing
             * function captured in case it refers to an upvar, and someone
             * wishes to decompile it while it's running.
             */
            JSFunction *fun = evalCaller->functionOrCallerFunction();
            ObjectBox *funbox = parser.newFunctionBox(fun, &pc, fun->strict());
            if (!funbox)
                return UnrootedScript(NULL);
            bce.objectList.add(funbox);
        }
    }

    TokenStream &tokenStream = parser.tokenStream;
    bool canHaveDirectives = true;
    for (;;) {
        TokenKind tt = tokenStream.peekToken(TSF_OPERAND);
        if (tt <= TOK_EOF) {
            if (tt == TOK_EOF)
                break;
            JS_ASSERT(tt == TOK_ERROR);
            return UnrootedScript(NULL);
        }

        ParseNode *pn = parser.statement();
        if (!pn)
            return UnrootedScript(NULL);

        if (canHaveDirectives) {
            if (!parser.maybeParseDirective(pn, &canHaveDirectives))
                return UnrootedScript(NULL);
        }

        if (!FoldConstants(cx, &pn, &parser))
            return UnrootedScript(NULL);
        if (!NameFunctions(cx, pn))
            return UnrootedScript(NULL);

        if (!EmitTree(cx, &bce, pn))
            return UnrootedScript(NULL);

        parser.freeTree(pn);
    }

    if (!SetSourceMap(cx, tokenStream, ss, script))
        return UnrootedScript(NULL);

    if (evalCaller && evalCaller->functionOrCallerFunction()) {
        JSFunction *fun = evalCaller->functionOrCallerFunction();
        HandlePropertyName arguments = cx->names().arguments;
        for (AtomDefnRange r = pc.lexdeps->all(); !r.empty(); r.popFront()) {
            if (r.front().key() == arguments) {
                if (fun->hasRest()) {
                    // It's an error to use |arguments| in a function that has
                    // a rest parameter.
                    parser.reportError(NULL, JSMSG_ARGUMENTS_AND_REST);
                    return UnrootedScript(NULL);
                }
                // Force construction of arguments objects for functions that
                // use 'arguments' within an eval.
                RootedScript script(cx, fun->nonLazyScript());
                if (script->argumentsHasVarBinding()) {
                    if (!JSScript::argumentsOptimizationFailed(cx, script))
                        return UnrootedScript(NULL);
                }
            }
        }

        // If the eval'ed script contains any debugger statement, force construction
        // of arguments objects for the caller script and any other scripts it is
        // transitively nested inside.
        if (pc.sc->hasDebuggerStatement()) {
            RootedObject scope(cx, scopeChain);
            while (scope->isScope() || scope->isDebugScope()) {
                if (scope->isCall() && !scope->asCall().isForEval()) {
                    RootedScript script(cx, scope->asCall().callee().nonLazyScript());
                    if (script->argumentsHasVarBinding()) {
                        if (!JSScript::argumentsOptimizationFailed(cx, script))
                            return UnrootedScript(NULL);
                    }
                }
                scope = scope->enclosingScope();
            }
        }
    }

    /*
     * Nowadays the threaded interpreter needs a stop instruction, so we
     * do have to emit that here.
     */
    if (Emit1(cx, &bce, JSOP_STOP) < 0)
        return UnrootedScript(NULL);

    if (!JSScript::fullyInitFromEmitter(cx, script, &bce))
        return UnrootedScript(NULL);

    bce.tellDebuggerAboutCompiledScript(cx);

    if (sct == &mysct && !sct->complete())
        return UnrootedScript(NULL);

    return script;
}

// Compile a JS function body, which might appear as the value of an event
// handler attribute in an HTML <INPUT> tag, or in a Function() constructor.
bool
frontend::CompileFunctionBody(JSContext *cx, HandleFunction fun, CompileOptions options,
                              const AutoNameVector &formals, const jschar *chars, size_t length)
{
    if (!CheckLength(cx, length))
        return false;
    ScriptSource *ss = cx->new_<ScriptSource>();
    if (!ss)
        return false;
    ScriptSourceHolder ssh(ss);
    SourceCompressionToken sct(cx);
    JS_ASSERT(options.sourcePolicy != CompileOptions::LAZY_SOURCE);
    if (options.sourcePolicy == CompileOptions::SAVE_SOURCE) {
        if (!ss->setSourceCopy(cx, chars, length, true, &sct))
            return false;
    }

    options.setCompileAndGo(false);
    Parser parser(cx, options, chars, length, /* foldConstants = */ true);
    if (!parser.init())
        return false;
    parser.sct = &sct;

    JS_ASSERT(fun);

    fun->setArgCount(formals.length());

    /* FIXME: make Function format the source for a function definition. */
    ParseNode *fn = CodeNode::create(PNK_FUNCTION, &parser);
    if (!fn)
        return false;

    fn->pn_body = NULL;
    fn->pn_funbox = NULL;
    fn->pn_cookie.makeFree();

    ParseNode *argsbody = ListNode::create(PNK_ARGSBODY, &parser);
    if (!argsbody)
        return false;
    argsbody->setOp(JSOP_NOP);
    argsbody->makeEmpty();
    fn->pn_body = argsbody;

    Rooted<JSScript*> script(cx, JSScript::Create(cx, NullPtr(), false, options,
                                                  /* staticLevel = */ 0, ss,
                                                  /* sourceStart = */ 0, length));
    if (!script)
        return false;

    // If the context is strict, immediately parse the body in strict
    // mode. Otherwise, we parse it normally. If we see a "use strict"
    // directive, we backup and reparse it as strict.
    TokenStream::Position start;
    parser.tokenStream.tell(&start);
    bool initiallyStrict = StrictModeFromContext(cx);
    bool becameStrict;
    FunctionBox *funbox;
    ParseNode *pn = parser.standaloneFunctionBody(fun, formals, script, fn, &funbox,
                                                  initiallyStrict, &becameStrict);
    if (!pn) {
        if (initiallyStrict || !becameStrict || parser.tokenStream.hadError())
            return false;

        // Reparse in strict mode.
        parser.tokenStream.seek(start);
        pn = parser.standaloneFunctionBody(fun, formals, script, fn, &funbox,
                                           /* strict = */ true);
        if (!pn)
            return false;
    }

    BytecodeEmitter funbce(/* parent = */ NULL, &parser, funbox, script,
                           /* evalCaller = */ NullPtr(),
                           /* hasGlobalScope = */ false, options.lineno);
    if (!funbce.init())
        return false;

    if (!NameFunctions(cx, pn))
        return false;

    if (fn->pn_body) {
        JS_ASSERT(fn->pn_body->isKind(PNK_ARGSBODY));
        fn->pn_body->append(pn);
        fn->pn_body->pn_pos = pn->pn_pos;
        pn = fn->pn_body;
    }

    if (!SetSourceMap(cx, parser.tokenStream, ss, script))
        return false;

    if (!EmitFunctionScript(cx, &funbce, pn))
        return false;

    if (!sct.complete())
        return false;

    return true;
}
