// ****************************************************************************
//  unit.cpp                                                      XL project
// ****************************************************************************
//
//   File Description:
//
//     Information about a single compilation unit, i.e. the code generated
//     for a particular tree
//
//
//
//
//
//
//
// ****************************************************************************
// This document is released under the GNU General Public License, with the
// following clarification and exception.
//
// Linking this library statically or dynamically with other modules is making
// a combined work based on this library. Thus, the terms and conditions of the
// GNU General Public License cover the whole combination.
//
// As a special exception, the copyright holders of this library give you
// permission to link this library with independent modules to produce an
// executable, regardless of the license terms of these independent modules,
// and to copy and distribute the resulting executable under terms of your
// choice, provided that you also meet, for each linked independent module,
// the terms and conditions of the license of that module. An independent
// module is a module which is not derived from or based on this library.
// If you modify this library, you may extend this exception to your version
// of the library, but you are not obliged to do so. If you do not wish to
// do so, delete this exception statement from your version.
//
// See http://www.gnu.org/copyleft/gpl.html and Matthew 25:22 for details
//  (C) 1992-2010 Christophe de Dinechin <christophe@taodyne.com>
//  (C) 2010 Taodyne SAS
// ****************************************************************************
//
// The compilation unit is where most of the "action" happens, e.g. where
// the code generation happens for a given tree. It records all information
// that is transient, i.e. only exists during a given compilation phase
//
// In the following, we will consider a rewrite such as:
//    foo X:integer, Y -> bar X + Y
//
// Such a rewrite is transformed into a function with a prototype that
// depends on the arguments, i.e. something like:
//    retType foo(int X, Tree *Y);
//
// The actual retType is determined dynamically from the return type of bar.
// An additional "closure" argument will be passed if the function captures
// variables from the surrounding context.

#include "compiler-unit.h"
#include "compiler-parms.h"
#include "compiler-args.h"
#include "compiler-expr.h"
#include "errors.h"
#include "types.h"
#include "llvm-crap.h"
#include <stdio.h>



RECORDER(unit, 64, "Evaluation in standard compilation unit");

XL_BEGIN

using namespace llvm;

CompilerUnit::CompilerUnit(Compiler &compiler, Scope *scope, Tree *source)
// ----------------------------------------------------------------------------
//   CompilerUnit constructor
// ----------------------------------------------------------------------------
    : compiler(compiler),
      jit(compiler.jit),
      context(scope),
      source(source),
      function(EvaluationFunctionPrototype()),
      types(context),
      data(jit, function, "data"),
      code(jit, function, "code"),
      exit(jit, function, "exit"),
      returned(data.AllocateReturnValue(function)),
      values(),
      mtypes(),
      captured()
{
    record(unit, "Creating unit for %t in context %t", source, context);
}


CompilerUnit::CompilerUnit(Compiler &compiler, Scope *scope, Tree *source,
                           Function_p function,
                           const Types &types,
                           const mtype_map &mtypes)
// ----------------------------------------------------------------------------
//   CompilerUnit constructor
// ----------------------------------------------------------------------------
    : compiler(compiler),
      jit(compiler.jit),
      context(scope),
      source(source),
      function(function),
      types(types),
      data(jit, function, "data.opt"),
      code(jit, function, "code.opt"),
      exit(jit, function, "exit.opt"),
      returned(data.AllocateReturnValue(function)),
      values(),
      mtypes(mtypes),
      captured()
{
    record(unit, "Creating unit %p for %t in context %t", this,source,context);
}


CompilerUnit::~CompilerUnit()
// ----------------------------------------------------------------------------
//   Delete what we must...
// ----------------------------------------------------------------------------
{
    record(unit, "Deleting unit %p for %t in context %t",
           this, source, context);
}


Value_p CompilerUnit::Compile(Tree *tree, bool forceEvaluation)
// ----------------------------------------------------------------------------
//    Compile a given tree
// ----------------------------------------------------------------------------
{
    CompileExpression cexpr(this);
    Value_p result = tree->Do(cexpr);
    if (forceEvaluation && tree->Kind() == NAME)
    {
        Value_p resultTy = jit.Type(result);
        if (IsClosureType(resTy))
            result = InvokeClosure(result);
    }
    return result;
}


bool CompilerUnit::TypeAnalysis()
// ----------------------------------------------------------------------------
//   Verify that the given program/expression is valid in current context
// ----------------------------------------------------------------------------
{
    context->ProcessDeclarations(program);
    return types.TypeCheck(source);
}


Function_p CompilerUnit::EvaluationFunctionPrototype()
// ----------------------------------------------------------------------------
//   Create a function prototoype for an eval_fn
// ----------------------------------------------------------------------------
{
    Function_p f = jit.Function(compiler.evalTy, "xl.eval");
    record(unit, "Evaluation prototype for %t is %v", source, f);
    return f;
}


Function *CompilerUnit::ClosureFunction(Tree *expr, Types *types)
// ----------------------------------------------------------------------------
//   Create a function for a closure
// ----------------------------------------------------------------------------
{
    // We have a closure type that we will build as we evaluate expression
    closureTy = llvm.OpaqueType();
    if (RECORDER_TWEAK(named_closures))
    {
        static char buffer[80]; static int count = 0;
        snprintf(buffer, 80, "xl.closure%d", count++);
        llvm.SetName(closureTy, buffer);
    }

    // Add a single parameter to the signature
    Signature signature;
    Type_p closurePtrTy = PointerType::get(closureTy, 0);
    signature.push_back(closurePtrTy);

    // Figure out the return type and function type
    Tree *rtype = this->types->Type(expr);
    Type_p retTy = compiler->MachineType(rtype);
    FunctionType *fnTy = FunctionType::get(retTy, signature, false);
    Function_p fn = InitializeFunction(fnTy,NULL,"xl.closure",true,false);

    // Return the function
    return fn;
}


Function *CompilerUnit::RewriteFunction(RewriteCandidate &rc)
// ----------------------------------------------------------------------------
//   Create a function for a tree rewrite
// ----------------------------------------------------------------------------
{
    Types *types = rc.types;
    Rewrite *rewrite = rc.rewrite;

    // We must have verified the types before
    assert((types && !this->types) || !"RewriteFunction: bogus type check");
    this->types = types;

    Tree *source = RewriteDefined(rewrite->left);
    Tree *def = rewrite->right;
    record(calls, "RewriteFunction %t defined as %t", source, def);

    // Extract parameters from source form
    ParameterList parameters(this);
    if (!source->Do(parameters))
    {
        record(eval, "RewriteFunction could not extract parameters");
        return NULL;
    }

    // Create the function signature, one entry per parameter
    Signature signature;
    ExtractSignature(parameters.parameters, rc, signature);

    // Compute return type:
    // - If explicitly specified, use that (TODO: Check compatibility)
    // - For definitions, infer from definition
    // - For data forms, this is the type of the data form
    Type_p retTy;
    if (Type_p specifiedRetTy = parameters.returned)
        retTy = specifiedRetTy;
    else if (def)
        retTy = ReturnType(def);
    else
        retTy = StructureType(signature, source);

    text label = "_XL_" + parameters.name;
    if (RECORDER_TWEAK(labels))
        label += "[" + text(*source) + "]";

    // Check if we are actually declaring a C function
    bool isC = false;
    bool isVararg = false;
    if (Tree *defined = parameters.defined)
    {
        if (Name *name = def->AsName())
            if (name->value == "C")
                if (ValidCName(defined, label))
                    isC = true;

        if (Prefix *prefix = def->AsPrefix())
            if (Name *name = prefix->left->AsName())
                if (name->value == "C")
                    if (ValidCName(prefix->right, label))
                        isC = true;
    }

    FunctionType *fnTy = FunctionType::get(retTy, signature, isVararg);
    Function *f = InitializeFunction(fnTy, &parameters.parameters,
                                     label.c_str(), isC, isC);
    record(calls, "RewriteFunction %t type %v is %v %s",
           source, fnTy, f, isC ? "is C" : "from XL source");
    if (isC)
    {
        void *address = sys::DynamicLibrary::SearchForAddressOfSymbol(label);
        record(xl2c, "C symbol for %t is at address %p", source, address);
        if (!address)
        {
            Ooops("No library function matching $1", rewrite->left);
            return NULL;
        }
        sys::DynamicLibrary::AddSymbol(label, address);
    }
    return f;
}


Function *CompilerUnit::InitializeFunction(FunctionType *fnTy,
                                           Parameters *parameters,
                                           kstring label,
                                           bool global, bool isC)
// ----------------------------------------------------------------------------
//   Build the LLVM function, create entry points, ...
// ----------------------------------------------------------------------------
{
    assert (!function || !"LLVM function was already built");

    // Create function and save it in the CompilerUnit
    function = llvm.CreateFunction(fnTy, label);
    record(llvm, "New function %v", function);

    if (!isC)
    {
        // Create function entry point, where we will have all allocas
        allocabb = BasicBlock::Create(llvm, "allocas", function);
        data = new IRBuilder<> (allocabb);

        // Create entry block for the function
        entrybb = BasicBlock::Create(llvm, "entry", function);
        code = new IRBuilder<> (entrybb);

        // Build storage for the return value
        Type_p retTy = function->getReturnType();
        returned = data->CreateAlloca(retTy, 0, "result");

        if (parameters)
        {
            // Associate the value for the additional arguments
            // (read-only, no alloca)
            Function::arg_iterator args = function->arg_begin();
            Parameters &plist = *parameters;
            for (Parameters::iterator p = plist.begin(); p != plist.end(); p++)
            {
                Parameter &parm = *p;
                Value_p inputArg = &*args++;
                value[parm.name] = inputArg;
            }
        }

        // Create the exit basic block and return statement
        exitbb = BasicBlock::Create(llvm, "exit", function);
        IRBuilder<> exitcode(exitbb);
        Value *retVal = exitcode.CreateLoad(returned, "retval");
        exitcode.CreateRet(retVal);
    }

    // Return the newly created function
    return function;
}


bool CompilerUnit::ExtractSignature(Parameters &parms,
                                    RewriteCandidate &rc,
                                    Signature &signature)
// ----------------------------------------------------------------------------
//   Extract the types from the parameter list
// ----------------------------------------------------------------------------
{
    bool hasClosures = false;

    RewriteBindings &bnds = rc.bindings;
    RewriteBindings::iterator b = bnds.begin();
    for (Parameters::iterator p = parms.begin(); p != parms.end(); p++, b++)
    {
        assert (b != bnds.end());
        RewriteBinding &binding = *b;
        if (Value_p closure = binding.closure)
        {
            // Deferred evaluation: pass evaluation function pointer and arg
            Type_p argTy = closure->getType();
            signature.push_back(argTy);
            hasClosures = true;
        }
        else
        {
            // Regular evaluation: just pass argument around
            signature.push_back((*p).type);
        }
    }

    return hasClosures;
}


Value_p CompilerUnit::Compile(RewriteCandidate &rc, Value_ps &args)
// ----------------------------------------------------------------------------
//    Compile a given rewrite for a tree
// ----------------------------------------------------------------------------
{
    // Check if we already have built this function, e.g. recursive calls
    text fkey = compiler->FunctionKey(rc.rewrite, args);
    llvm::Function *&function = compiler->FunctionFor(fkey);

    // If we have not, then we need to build it
    if (function == NULL)
    {
        Types *types = rc.types;
        Rewrite *rewrite = rc.rewrite;
        Context_p rewriteContext = types->context;
        CompilerUnit rewriteUnit(compiler, rewriteContext);

        // Copy initial machine types in the rewrite unit
        rewriteUnit.InheritMachineTypes(*this);

        function = rewriteUnit.RewriteFunction(rc);
        if (function && rewriteUnit.code)
        {
            rewriteUnit.ImportClosureInfo(this);
            Tree *value = rewrite->right;
            if (value && value != xl_self)
            {
                // Regular function
                Value_p returned = rewriteUnit.CompileTopLevel(value);
                if (!returned)
                    return NULL;
                if (!rewriteUnit.Return(returned))
                    return NULL;
            }
            else
            {
                // Constructor for a 'data' form
                uint index = 0;
                Tree *form = RewriteDefined(rewrite->left);
                Value_p returned = rewriteUnit.Data(form, index);
                if (!returned)
                    return NULL;
            }
            rewriteUnit.Finalize(false);
        }

        // Inherit boxed types generated by this rewrite
        InheritMachineTypes(rewriteUnit);
    }
    return function;
}


Value_p CompilerUnit::Data(Tree *form, uint &index)
// ----------------------------------------------------------------------------
//    Generate a constructor for a data form
// ----------------------------------------------------------------------------
//    Generate a function that constructs the given data form
{
    Value_p left, right, child;

    switch(form->Kind())
    {
    case INTEGER:
    case REAL:
    case TEXT:
    {
        // For all these cases, simply compute the corresponding value
        CompileExpression expr(this);
        Value_p result = form->Do(expr);
        return result;
    }

    case NAME:
    {
        Scope_p   scope;
        Rewrite_p rw;
        Tree      *existing;

        // Bound names are returned as is, parameters are evaluated
        existing = context->Bound(form, true, &rw, &scope);
        assert (existing || !"Type check didn't realize a name was missing");

        // Arguments bound here are returned directly as a tree
        if (scope == context->CurrentScope())
        {
            Tree *defined = RewriteDefined(rw->left);
            if (Value_p result = Known(defined))
            {
                // Store that in the result tree
                Value_p ptr = llvm.CreateStructGEP(code,
                                                      returned, index++,
                                                      "resultp");
                result = code->CreateStore(result, ptr);
                return result;
            }
        }

        // Arguments not bound here are returned as a constant
        Tree *form = RewriteDefined(rw->left);
        return compiler->TreeConstant(form);
    }

    case INFIX:
    {
        Infix *infix = (Infix *) form;
        left = Data(infix->left, index);
        right = Data(infix->right, index);
        return right;
    }

    case PREFIX:
    {
        Prefix *prefix = (Prefix *) form;
        left = Data(prefix->left, index);
        right = Data(prefix->right, index);
        return right;
    }

    case POSTFIX:
    {
        Postfix *postfix = (Postfix *) form;
        left = Data(postfix->left, index);
        right = Data(postfix->right, index);
        return right;
    }

    case BLOCK:
    {
        Block *block = (Block *) form;
        child = Data(block->child, index);
        return child;
    }
    }

    (void)left;
    assert (!"Unknown kind of tree in Data()");
    return NULL;
}


Value_p CompilerUnit::Unbox(Value_p boxed, Tree *form, uint &index)
// ----------------------------------------------------------------------------
//   Generate code to unbox a value
// ----------------------------------------------------------------------------
{
    Type_p ttp = compiler->treePtrTy;
    Value_p ref, left, right, child;

    switch(form->Kind())
    {
    case INTEGER:
    case REAL:
    case TEXT:
    {
        // For all these cases, simply compute the corresponding value
        CompileExpression expr(this);
        Value_p result = form->Do(expr);
        return result;
    }

    case NAME:
    {
        Scope_p   scope;
        Rewrite_p rw;
        Tree      *existing;

        // Bound names are returned as is, parameters are evaluated
        existing = context->Bound(form, true, &rw, &scope);
        assert(existing || !"Type checking didn't realize a name is missing");

        // Arguments bound here are returned directly as a tree
        if (scope == context->CurrentScope())
        {
            // Get element from input argument
            Value_p ptr = llvm.CreateStructGEP(code, boxed, index++, "inp");
            return code->CreateLoad(ptr);
        }

        // Arguments not bound here are returned as a constant
        Tree *defined = RewriteDefined(rw->left);
        return compiler->TreeConstant(defined);
    }

    case INFIX:
    {
        Infix *infix = (Infix *) form;
        ref = compiler->TreeConstant(form);
        left = Unbox(boxed, infix->left, index);
        right = Unbox(boxed, infix->right, index);
        left = Autobox(left, ttp);
        right = Autobox(right, ttp);
        return llvm.CreateCall(code, compiler->xl_new_infix,
                               ref, left, right);
    }

    case PREFIX:
    {
        Prefix *prefix = (Prefix *) form;
        ref = compiler->TreeConstant(form);
        if (prefix->left->Kind() == NAME)
            left = compiler->TreeConstant(prefix->left);
        else
            left = Unbox(boxed, prefix->left, index);
        right = Unbox(boxed, prefix->right, index);
        left = Autobox(left, ttp);
        right = Autobox(right, ttp);
        return llvm.CreateCall(code, compiler->xl_new_prefix,
                               ref, left, right);
    }

    case POSTFIX:
    {
        Postfix *postfix = (Postfix *) form;
        ref = compiler->TreeConstant(form);
        left = Unbox(boxed, postfix->left, index);
        if (postfix->right->Kind() == NAME)
            right = compiler->TreeConstant(postfix->right);
        else
            right = Unbox(boxed, postfix->right, index);
        left = Autobox(left, ttp);
        right = Autobox(right, ttp);
        return llvm.CreateCall(code, compiler->xl_new_postfix,
                               ref, left, right);
    }

    case BLOCK:
    {
        Block *block = (Block *) form;
        ref = compiler->TreeConstant(form);
        child = Unbox(boxed, block->child, index);
        child = Autobox(child, ttp);
        return llvm.CreateCall(code, compiler->xl_new_block, ref, child);
    }
    }

    assert(!"Invalid tree kind in CompilerUnit::Unbox");
    return NULL;
}


Value_p CompilerUnit::Closure(Name *name, Tree *expr)
// ----------------------------------------------------------------------------
//    Compile code to pass a given tree as a closure
// ----------------------------------------------------------------------------
//    Closures are represented as functions taking a pointer to a structure
//    that will contain the values being used by the closure code
{
    // Record the function that we build
    text fkey = compiler->ClosureKey(expr, context);
    Function_p &fn = compiler->FunctionFor(fkey);
    assert (fn == NULL);

    // Create the evaluation function
    CompilerUnit cunit(compiler, context);
    fn = cunit.ClosureFunction(expr, types);
    if (!fn || !cunit.code || !cunit.closureTy)
        return NULL;
    cunit.ImportClosureInfo(this);
    Value_p returned = cunit.CompileTopLevel(expr);
    if (!returned)
        return NULL;
    if (!cunit.Return(returned))
        return NULL;
    cunit.Finalize(false);

    // Values imported from closure are now in cunit.closure[]
    // Allocate a local data block to pass as the closure
    Value_p stackPtr = data->CreateAlloca(cunit.closureTy);
    compiler->MarkAsClosureType(stackPtr->getType());

    // First, store the function pointer
    uint field = 0;
    Value_p fptr = llvm.CreateStructGEP(code, stackPtr, field++, "fnPtr");
    code->CreateStore(fn, fptr);

    // Then loop over all values that were detected while evaluating expr
    value_map &cls = cunit.closure;
    for (value_map::iterator v = cls.begin(); v != cls.end(); v++)
    {
        Tree *subexpr = (*v).first;
        Value_p subval = Compile(subexpr);
        fptr = llvm.CreateStructGEP(code, stackPtr, field++, "itemPtr");
        code->CreateStore(subval, fptr);
    }

    // Remember the machine type associated with this closure
    Type_p mtype = stackPtr->getType();
    ExpressionMachineType(name, mtype);

    // Return the stack pointer that we'll use later to evaluate the closure
    return stackPtr;
}


Value_p CompilerUnit::InvokeClosure(Value_p result, Value_p fnPtr)
// ----------------------------------------------------------------------------
//   Invoke a closure with a known closure function
// ----------------------------------------------------------------------------
{
    result = llvm.CreateCall(code, fnPtr, result);
    return result;
}


Value_p CompilerUnit::InvokeClosure(Value_p result)
// ----------------------------------------------------------------------------
//   Invoke a closure loading the function pointer dynamically
// ----------------------------------------------------------------------------
{
    // Get function pointer and argument
    Value_p fnPtrPtr = llvm.CreateStructGEP(data, result, 0, "fnPtrPtr");
    Value_p fnPtr = data->CreateLoad(fnPtrPtr);

    // Call the closure callback
    result = InvokeClosure(result, fnPtr);

    // Overwrite the function pointer to its original value
    // (actually improves optimizations by showing it doesn't change)
    code->CreateStore(fnPtr, fnPtrPtr);

    return result;
}


Value_p CompilerUnit::Return(Value_p value)
// ----------------------------------------------------------------------------
//   Return the given value, after appropriate boxing
// ----------------------------------------------------------------------------
{
    Type_p retTy = jit.ReturnType(function);
    value = Autobox(value, retTy);
    code->CreateStore(value, returned);
    return value;
}


eval_fn CompilerUnit::Finalize(bool createCode)
// ----------------------------------------------------------------------------
//   Finalize the build of the current function
// ----------------------------------------------------------------------------
{
    record(llvm, "Finalize function %v", function);

    // If we had closure information, finish building the closure type
    if (closureTy)
    {
        Signature sig;

        // First argument is always the pointer to the evaluation function
        Type_p fnTy = function->getType();
        sig.push_back(fnTy);

        // Loop over other elements that need a closure
        for (value_map::iterator v = closure.begin(); v != closure.end(); v++)
        {
            Value_p value = (*v).second;
            Type_p allocaTy = value->getType();
            const llvm::PointerType *ptrTy = cast<PointerType>(allocaTy);
            Type_p type = ptrTy->getElementType();
            sig.push_back(type);
        }

        // Build the structure type and unify it with opaque type used in decl
        closureTy = llvm.Struct(closureTy, sig);

        // Load the elements from the closure
        Function::arg_iterator args = function->arg_begin();
        Value_p closureArg = &*args++;
        uint field = 1;
        for (value_map::iterator v = closure.begin(); v != closure.end(); v++)
        {
            Tree *value = (*v).first;
            Value_p storage = NeedStorage(value);
            Value_p ptr = llvm.CreateStructGEP(data, closureArg, field++,
                                                  "closure_input_ptr");
            Value_p input = data->CreateLoad(ptr);
            data->CreateStore(input, storage);
        }

    }

    // Branch to the exit block from the last test we did
    code->CreateBr(exitbb);

    // Connect the "allocas" to the actual entry point
    data->CreateBr(entrybb);

    // Verify the function we built
    if (RECORDER_TRACE(llvm_code) & 2)
    {
        errs() << "LLVM IR before verification and optimizations:\n";
        function->print(errs());
    }
    verifyFunction(*function);
    llvm.FinalizeFunction(function);
    if (RECORDER_TRACE(llvm_code) & 4)
    {
        errs() << "LLVM IR after optimizations:\n";
        function->print(errs());
    }

    void *result = NULL;
    if (createCode)
    {
        result = llvm.FunctionPointer(function);
        if (RECORDER_CODE(llvm_code) & 8)
        {
            errs() << "After pointer generation:\n";
            function->print(errs());
        }
        record(llvm_functions, "Function code %p for %v", result, function);
    }

    exitbb = NULL;              // Tell destructor we were successful
    return (eval_fn) result;
}


Value *CompilerUnit::NeedStorage(Tree *tree)
// ----------------------------------------------------------------------------
//    Allocate storage for a given tree
// ----------------------------------------------------------------------------
{
    assert(types || !"Storage() called without type check");

    Value *result = storage[tree];
    if (!result)
    {
        // Get the associated machine type
        Type_p mtype = ExpressionMachineType(tree);

        // Create alloca to store the new form
        text label = "loc";
        if (RECORDER_TWEAK(labels))
            label += "[" + text(*tree) + "]";
        const char *clabel = label.c_str();
        result = data->CreateAlloca(mtype, 0, clabel);
        storage[tree] = result;

        // If this started with a value or global, initialize on function entry
        Value_p initializer = NULL;
        if (value.count(tree))
        {
            initializer = value[tree];
        }
        else if (Value *global = compiler->TreeConstant(tree))
        {
            initializer = global;
        }
        if (initializer && initializer->getType() == mtype)
            data->CreateStore(initializer, result);
    }

    return result;
}


Value_p CompilerUnit::NeedClosure(Tree *tree)
// ----------------------------------------------------------------------------
//   Allocate a closure variable
// ----------------------------------------------------------------------------
{
    Value_p storage = closure[tree];
    if (!storage)
    {
        storage = NeedStorage(tree);
        closure[tree] = storage;
    }
    Value_p result = code->CreateLoad(storage);
    return result;
}


bool CompilerUnit::IsKnown(Tree *tree, uint which)
// ----------------------------------------------------------------------------
//   Check if the tree has a known local or global value
// ----------------------------------------------------------------------------
{
    if ((which & knowLocals) && storage.count(tree) > 0)
        return true;
    else if ((which & knowValues) && value.count(tree) > 0)
        return true;
    else if (which & knowGlobals)
        return true;
    return false;
}


Value *CompilerUnit::Known(Tree *tree, uint which)
// ----------------------------------------------------------------------------
//   Return the known local or global value if any
// ----------------------------------------------------------------------------
{
    Value *result = NULL;
    if ((which & knowLocals) && storage.count(tree) > 0)
    {
        // Value is stored in a local variable
        result = code->CreateLoad(storage[tree], "loc");
    }
    else if ((which & knowValues) && value.count(tree) > 0)
    {
        // Immediate value of some sort, use that
        result = value[tree];
    }
    else if (which & knowGlobals)
    {
        // Check if this is a global
        result = compiler->TreeConstant(tree);
    }
    return result;
}


void CompilerUnit::ImportClosureInfo(const CompilerUnit *parent)
// ----------------------------------------------------------------------------
//   Copy closure data from parent to child
// ----------------------------------------------------------------------------
{
    machineType = parent->machineType;
}


Value *CompilerUnit::ConstantInteger(Integer *what)
// ----------------------------------------------------------------------------
//    Generate an Integer tree
// ----------------------------------------------------------------------------
{
    Value *result = Known(what, knowGlobals);
    if (!result)
    {
        result = compiler->TreeConstant(what);
        if (storage.count(what))
            code->CreateStore(result, storage[what]);
    }
    return result;
}


Value *CompilerUnit::ConstantReal(Real *what)
// ----------------------------------------------------------------------------
//    Generate a Real tree
// ----------------------------------------------------------------------------
{
    Value *result = Known(what, knowGlobals);
    if (!result)
    {
        result = compiler->TreeConstant(what);
        if (storage.count(what))
            code->CreateStore(result, storage[what]);
    }
    return result;
}


Value *CompilerUnit::ConstantText(Text *what)
// ----------------------------------------------------------------------------
//    Generate a Text tree
// ----------------------------------------------------------------------------
{
    Value *result = Known(what, knowGlobals);
    if (!result)
    {
        result = compiler->TreeConstant(what);
        if (storage.count(what))
            code->CreateStore(result, storage[what]);
    }
    return result;
}


Value *CompilerUnit::ConstantTree(Tree *what)
// ----------------------------------------------------------------------------
//    Generate a constant tree
// ----------------------------------------------------------------------------
{
    Value *result = Known(what, knowGlobals);
    if (!result)
        result = jit.PointerConstant(treePtrTy, what);
    return result;
}


Value *CompilerUnit::CallFormError(Tree *what)
// ----------------------------------------------------------------------------
//   Report a type error trying to evaluate some argument
// ----------------------------------------------------------------------------
{
    Value *ptr = ConstantTree(what); assert(what);
    Value *nullContext = ConstantPointerNull::get(compiler->contextPtrTy);
    Value *callVal = llvm.CreateCall(code, compiler->xl_form_error,
                                     nullContext, ptr);
    return callVal;
}


Type_p CompilerUnit::ReturnType(Tree *form)
// ----------------------------------------------------------------------------
//   Compute the return type associated with the given form
// ----------------------------------------------------------------------------
{
    // Type inference gives us the return type for this form
    Tree *type = types->Type(form);
    Type_p mtype = compiler->MachineType(type);
    return mtype;
}


Type_p CompilerUnit::StructureType(Signature &signature, Tree *source)
// ----------------------------------------------------------------------------
//   Compute the return type associated with the given data form
// ----------------------------------------------------------------------------
{
    // Check if we already had this signature
    Type_p found = machineType[source];
    if (found)
        return found;

    // Build the corresponding structure type
    StructType *stype = StructType::get(llvm, signature);
    text tname = "boxed";
    if (RECORDER_TWEAK(labels))
        tname += "[" + text(*source) + "]";
    llvm.SetName(stype, tname);

    // Record boxing and unboxing for that particular tree
    machineType[source] = stype;
    unboxed[stype] = source;

    // Record boxing for the given type
    Tree *baseType = types->Type(source);
    boxed[baseType] = stype;

    return stype;
}


Type_p CompilerUnit::ExpressionMachineType(Tree *expr, Type_p type)
// ----------------------------------------------------------------------------
//   Define the machine type associated with an expression
// ----------------------------------------------------------------------------
{
    assert (type && "ExpressionMachineType called with null type");
    assert ((machineType[expr] == NULL || machineType[expr] == type) &&
            "ExpressionMachineType overrides type");
    machineType[expr] = type;
    return type;
}


Type_p CompilerUnit::ExpressionMachineType(Tree *expr)
// ----------------------------------------------------------------------------
//   Return the machine type associated with a given expression
// ----------------------------------------------------------------------------
{
    Type_p type = machineType[expr];
    if (!type)
    {
        assert(types || !"ExpressionMachineType without type check");
        Tree *typeTree = types->Type(expr);
        type = MachineType(typeTree);
        machineType[expr] = type;
    }
    return type;
}


Type_p CompilerUnit::MachineType(Tree *type)
// ----------------------------------------------------------------------------
//   Return the machine type associated with a given type
// ----------------------------------------------------------------------------
{
    assert(types || !"ExpressionMachineType without type check");

    type = types->Base(type);

    // First check if we have something matching in our boxed types
    for (type_map::iterator t = boxed.begin(); t != boxed.end(); t++)
        if (types->Base((*t).first) == type)
            return (*t).second;

    // Otherwise, return the default representation for the type
    return compiler->MachineType(type);
}


void CompilerUnit::InheritMachineTypes(CompilerUnit &unit)
// ----------------------------------------------------------------------------
//   Get all the machine types we defined for the other unit
// ----------------------------------------------------------------------------
{
    type_map &uboxed = unit.boxed;
    for (type_map::iterator i = uboxed.begin(); i != uboxed.end(); i++)
        boxed[(*i).first] = (*i).second;
}


Value_p CompilerUnit::Autobox(Value_p value, Type_p req)
// ----------------------------------------------------------------------------
//   Automatically box/unbox primitive types
// ----------------------------------------------------------------------------
//   Primitive values like integers can exist in two forms during execution:
//   - In boxed form, e.g. as a pointer to an instance of Integer
//   - In native form, e.g. as an integer
//   This function automatically converts from one to the other as necessary
{
    Type_p  type   = value->getType();
    Value_p result = value;
    Function * boxFn  = NULL;

    // Short circuit if we are already there
    if (req == type)
        return result;

    if (req == compiler->booleanTy)
    {
        assert (type == compiler->treePtrTy || type == compiler->nameTreePtrTy);
        Value *falsePtr = compiler->TreeConstant(xl_false);
        result = code->CreateICmpNE(value, falsePtr, "notFalse");
    }
    else if (req->isIntegerTy())
    {
        if (req == compiler->characterTy && type == compiler->textTreePtrTy)
        {
            // Convert text constant to character
            result = llvm.CreateStructGEP(code, result, TEXT_VALUE_INDEX,
                                          "unbox_char_tree_ptr");
            result = llvm.CreateStructGEP(code, result, 0,
                                          "unbox_char_ptr_ptr");
            result = llvm.CreateStructGEP(code, result, 0,
                                          "unbox_char_ptr");
            result = code->CreateLoad(result, "unbox_char");
        }
        else
        {
            // Convert integer constants
            assert (type == compiler->integerTreePtrTy);
            result = llvm.CreateStructGEP(code, value, INTEGER_VALUE_INDEX,
                                          "unbox_integer");
            if (req != compiler->integerTy)
                result = code->CreateTrunc(result, req);
        }
    }
    else if (req->isFloatingPointTy())
    {
        assert(type == compiler->realTreePtrTy);
        result = llvm.CreateStructGEP(code, value, REAL_VALUE_INDEX,
                                      "unbox_real");
        if (req != compiler->realTy)
            result = code->CreateFPTrunc(result, req);
    }
    else if (req == compiler->charPtrTy)
    {
        assert(type == compiler->textTreePtrTy);
        result = llvm.CreateStructGEP(code, result, TEXT_VALUE_INDEX,
                                      "unbox_text_ptr");
        result = llvm.CreateStructGEP(code, result, 0,
                                      "unbox_char_ptr_ptr");
        result = code->CreateLoad(result, "unbox_char_ptr");
    }
    else if (req == compiler->textTy)
    {
        assert (type == compiler->textTreePtrTy);
        result = llvm.CreateStructGEP(code, result, TEXT_VALUE_INDEX,
                                      "unbox_text_ptr");
    }
    else if (type == compiler->booleanTy)
    {
        assert(req == compiler->treePtrTy || req == compiler->nameTreePtrTy);

        // Insert code corresponding to value ? xl_true : xl_false
        BasicBlock *isTrue = BasicBlock::Create(llvm, "isTrue", function);
        BasicBlock *isFalse = BasicBlock::Create(llvm, "isFalse", function);
        BasicBlock *exit = BasicBlock::Create(llvm, "booleanBoxed", function);
        Value *ptr = data->CreateAlloca(compiler->treePtrTy);
        code->CreateCondBr(value, isTrue, isFalse);

        // True block
        code->SetInsertPoint(isTrue);
        Value *truePtr = compiler->TreeConstant(xl_true);
        result = code->CreateStore(truePtr, ptr);
        code->CreateBr(exit);

        // False block
        code->SetInsertPoint(isFalse);
        Value *falsePtr = compiler->TreeConstant(xl_false);
        result = code->CreateStore(falsePtr, ptr);
        code->CreateBr(exit);

        // Now on shared exit block
        code->SetInsertPoint(exit);
        result = code->CreateLoad(ptr);
        type = result->getType();
    }
    else if (type == compiler->characterTy &&
             (req == compiler->treePtrTy || req == compiler->textTreePtrTy))
    {
        boxFn = compiler->xl_new_character;
    }
    else if (type->isIntegerTy())
    {
        assert(req == compiler->treePtrTy || req == compiler->integerTreePtrTy);
        boxFn = compiler->xl_new_integer;
        if (type != compiler->integerTy)
            result = code->CreateSExt(result, type); // REVISIT: Signed?
    }
    else if (type->isFloatingPointTy())
    {
        assert(req == compiler->treePtrTy || req == compiler->realTreePtrTy);
        boxFn = compiler->xl_new_real;
        if (type != compiler->realTy)
            result = code->CreateFPExt(result, type);
    }
    else if (type == compiler->textTy)
    {
        assert(req == compiler->treePtrTy || req == compiler->textTreePtrTy);
        boxFn = compiler->xl_new_text;
    }
    else if (type == compiler->charPtrTy)
    {
        assert(req == compiler->treePtrTy || req == compiler->textTreePtrTy);
        boxFn = compiler->xl_new_ctext;
    }
    else if (unboxed.count(type) &&
             (req == compiler->blockTreePtrTy ||
              req == compiler->infixTreePtrTy ||
              req == compiler->prefixTreePtrTy ||
              req == compiler->postfixTreePtrTy ||
              req == compiler->treePtrTy))
    {
        Tree *form = unboxed[type];
        boxFn = compiler->UnboxFunction(context, type, form);
    }

    // If we need to invoke a boxing function, do it now
    if (boxFn)
    {
        result = llvm.CreateCall(code, boxFn, value);
        type = result->getType();
    }


    if (req == compiler->treePtrTy && type != req)
    {
        assert(type == compiler->integerTreePtrTy ||
               type == compiler->realTreePtrTy ||
               type == compiler->textTreePtrTy ||
               type == compiler->nameTreePtrTy ||
               type == compiler->blockTreePtrTy ||
               type == compiler->prefixTreePtrTy ||
               type == compiler->postfixTreePtrTy ||
               type == compiler->infixTreePtrTy);
        result = code->CreateBitCast(result, req);
    }

    // Return what we built if anything
    return result;
}


Value *CompilerUnit::Global(Tree *tree)
// ----------------------------------------------------------------------------
//   Return a global value if there is any
// ----------------------------------------------------------------------------
{
    // Check if this is a global
    Value *result = NULL;
    CompilerInfo *info = compiler->Info(tree);
    if (info)
        result = compiler->TreeConstant(tree);
    return result;
}


bool CompilerUnit::ValidCName(Tree *tree, text &label)
// ----------------------------------------------------------------------------
//   Check if the name is valid for C
// ----------------------------------------------------------------------------
{
    uint len = 0;

    if (Name *name = tree->AsName())
    {
        label = name->value;
        len = label.length();
    }
    else if (Text *text = tree->AsText())
    {
        label = text->value;
        len = label.length();
    }

    if (len == 0)
    {
        Ooops("No valid C name in $1", tree);
        return false;
    }

    // We will NOT call functions beginning with _ (internal functions)
    for (uint i = 0; i < len; i++)
    {
        char c = label[i];
        if (!isalpha(c) && c != '_' && !(i && isdigit(c)))
        {
            Ooops("C name $1 contains invalid characters", tree);
            return false;
        }
    }
    return true;
}


Function_p CompilerUnit::UnboxFunction(Context_p ctx, Type_p type, Tree *form)
// ----------------------------------------------------------------------------
//    Create a function transforming a boxed (structure) value into tree form
// ----------------------------------------------------------------------------
{
    // Check if we have a matching boxing function
    std::ostringstream out;
    out << "Unbox" << (void *) type << ";" << (void *) ctx;

    llvm::Function * &fn = FunctionFor(out.str());
    if (fn)
        return fn;

    // Get original form representing that data type
    Type_p mtype = TreeMachineType(form);

    // Create a function taking a boxed type as an argument, returning a tree
    Signature signature;
    signature.push_back(type);
    FunctionType *ftype = FunctionType::get(mtype, signature, false);
    CompilerUnit unit(this, ctx);
    fn = unit.InitializeFunction(ftype, NULL, "xl.unbox", false, false);

    // Take the first input argument, which is the boxed value.
    llvm::Function::arg_iterator args = fn->arg_begin();
    Value_p arg = &*args++;

    // Generate code to create the unboxed tree
    uint index = 0;
    Value_p tree = unit.Unbox(arg, form, index);
    tree = unit.Autobox(tree, treePtrTy);
    unit.Return(tree);

    return fn;
}

Value_p Compiler::Primitive(CompilerUnit &unit,
                               text name, uint arity, Value_p args)
// ----------------------------------------------------------------------------
//   Invoke an LLVM primitive, assuming it's found in the table
// ----------------------------------------------------------------------------
{
    // Find the entry in the primitives table
    auto found = primitives.find(name);
    if (found == llvm_primitives.end())
        return NULL;

    // If the entry doesn't have the expected arity, give up
    CompilerPrimitive *primitive = (*found).second;
    if (primitive->arity != arity)
        return NULL;

    // Invoke the entry
    Value_p result = primitive->function(unit, args);
    return result;
}


RECORDER(array_to_args, 64, "Array to args adapters");
adapter_fn Compiler::ArrayToArgsAdapter(uint numargs)
// ----------------------------------------------------------------------------
//   Generate code to call a function with N arguments
// ----------------------------------------------------------------------------
//   The generated code serves as an adapter between code that has
//   tree arguments in a C array and code that expects them as an arg-list.
//   For example, it allows you to call foo(Tree *src, Tree *a1, Tree *a2)
//   by calling generated_adapter(foo, Tree *src, Tree *args[2])
{
    record(array_to_args, "Enter adapter for %u args", numargs);

    // Check if we already computed it
    adapter_fn result = array_to_args_adapters[numargs];
    if (result)
    {
        record(array_to_args,
               "Adapter existed at %p for %u args", (void *) result, numargs);
        return result;
    }

    // We need a new independent module for this adapter with the MCJIT
    LLVMCrap::JITModule module(llvm, "xl.array2arg.adapter");

    // Generate the function type:
    // Tree *generated(Context *, native_fn, Tree *, Tree **)
    Signature parms;
    parms.push_back(nativeFnTy);
    parms.push_back(contextPtrTy);
    parms.push_back(treePtrTy);
    parms.push_back(treePtrPtrTy);
    FunctionType *fnType = FunctionType::get(treePtrTy, parms, false);
    llvm::Function *adapter = llvm.CreateFunction(fnType, "xl.adapter");

    // Generate the function type for the called function
    Signature called;
    called.push_back(contextPtrTy);
    called.push_back(treePtrTy);
    for (uint a = 0; a < numargs; a++)
        called.push_back(treePtrTy);
    FunctionType *calledType = FunctionType::get(treePtrTy, called, false);
    PointerType *calledPtrType = PointerType::get(calledType, 0);

    // Create the entry for the function we generate
    BasicBlock *entry = BasicBlock::Create(llvm, "adapt", adapter);
    IRBuilder<> code(entry);

    // Read the arguments from the function we are generating
    llvm::Function::arg_iterator inArgs = adapter->arg_begin();
    Value *fnToCall = &*inArgs++;
    Value *contextPtr = &*inArgs++;
    Value *sourceTree = &*inArgs++;
    Value *treeArray = &*inArgs++;

    // Cast the input function pointer to right type
    Value *fnTyped = code.CreateBitCast(fnToCall, calledPtrType, "xl.fnCast");

    // Add source as first argument to output arguments
    std::vector<Value *> outArgs;
    outArgs.push_back (contextPtr);
    outArgs.push_back (sourceTree);

    // Read other arguments from the input array
    for (uint a = 0; a < numargs; a++)
    {
        Value *elementPtr = code.CreateConstGEP1_32(treeArray, a);
        Value *fromArray = code.CreateLoad(elementPtr, "arg");
        outArgs.push_back(fromArray);
    }

    // Call the function
    Value *retVal = llvm.CreateCall(&code, fnTyped, outArgs);

    // Return the result
    code.CreateRet(retVal);

    // Verify the function and optimize it.
    verifyFunction (*adapter);

    // Enter the result in the map
    llvm.FinalizeFunction(adapter);
    result = (adapter_fn) llvm.FunctionPointer(adapter);
    array_to_args_adapters[numargs] = result;

    record(array_to_args, "Created adapter %p for %d args",
           (void *) result, numargs);

    // And return it to the caller
    return result;
}


llvm::Function *Compiler::ExternFunction(kstring name, void *address,
                                         Type_p retType, int parmCount, ...)
// ----------------------------------------------------------------------------
//   Return a Function for some given external symbol
// ----------------------------------------------------------------------------
{
    record(builtins, "Extern function %s, %d parameters, address %p",
             name, parmCount, address);

    va_list va;
    Signature parms;
    bool isVarArg = parmCount < 0;
    if (isVarArg)
        parmCount = -parmCount;

    va_start(va, parmCount);
    for (int i = 0; i < parmCount; i++)
    {
        Type *ty = va_arg(va, Type *);
        parms.push_back(ty);
    }
    va_end(va);
    FunctionType *fnType = FunctionType::get(retType, parms, isVarArg);
    llvm::Function *result = llvm.CreateExternFunction(fnType, name);
    sys::DynamicLibrary::AddSymbol(name, address);

    record(builtins, "Result function %v", result);

    return result;
}


text Compiler::FunctionKey(Rewrite *rw, Value_ps &args)
// ----------------------------------------------------------------------------
//    Return a unique function key corresponding to a given overload
// ----------------------------------------------------------------------------
{
    std::ostringstream out;
    out << (void *) rw;

    for (Value_ps::iterator a = args.begin(); a != args.end(); a++)
    {
        Value_p value = *a;
        Type_p type = value->getType();
        out << ';' << (void *) type;
    }

    return out.str();
}


text Compiler::ClosureKey(Tree *tree, Context *context)
// ----------------------------------------------------------------------------
//    Return a unique function key corresponding to a given closure
// ----------------------------------------------------------------------------
{
    std::ostringstream out;
    out << (void *) tree << "@" << (void *) context;
    return out.str();
}



XL_END
