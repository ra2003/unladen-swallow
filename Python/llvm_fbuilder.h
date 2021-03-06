// -*- C++ -*-
#ifndef PYTHON_LLVM_FBUILDER_H
#define PYTHON_LLVM_FBUILDER_H

#ifndef __cplusplus
#error This header expects to be included only in C++ source
#endif

#include "Util/EventTimer.h"
#include "Util/PyTypeBuilder.h"
#include "Util/RuntimeFeedback.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SparseBitVector.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Analysis/DebugInfo.h"
#include "llvm/Constants.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/Support/TargetFolder.h"
#include <string>

struct PyCodeObject;
struct PyGlobalLlvmData;

namespace py {

/// Helps the compiler build LLVM functions corresponding to Python
/// functions.  This class maintains the IRBuilder and several Value*s
/// set up in the entry block.
class LlvmFunctionBuilder {
    LlvmFunctionBuilder(const LlvmFunctionBuilder &);  // Not implemented.
    void operator=(const LlvmFunctionBuilder &);  // Not implemented.

public:
    LlvmFunctionBuilder(PyGlobalLlvmData *global_data, PyCodeObject *code);

    llvm::Function *function() { return function_; }
    typedef llvm::IRBuilder<true, llvm::TargetFolder> BuilderT;
    BuilderT& builder() { return builder_; }
    llvm::BasicBlock *unreachable_block() { return unreachable_block_; }

    /// Sets the current instruction index.  This is only put into the
    /// frame object when tracing.
    void SetLasti(int current_instruction_index);

    /// Sets the current line number being executed.  This is used to
    /// make tracebacks correct and to get tracing to fire in the
    /// right places.
    void SetLineNumber(int line);

    /// Inserts a call to llvm.dbg.stoppoint.
    void SetDebugStopPoint(int line_number);

    /// Convenience wrapper for creating named basic blocks using the current
    /// context and function.
    llvm::BasicBlock *CreateBasicBlock(const llvm::Twine &name);

    /// This function fills the block that handles a backedge.  Each
    /// backedge needs to check if it needs to handle signals or
    /// switch threads.  If the backedge doesn't land at the start of
    /// a line, it also needs to update the line number and check
    /// whether line tracing has been turned on.  This function leaves
    /// the insert point in a block with a terminator already added,
    /// so the caller should re-set the insert point.
    void FillBackedgeLanding(llvm::BasicBlock *backedge_landing,
                             llvm::BasicBlock *target,
                             bool to_start_of_line,
                             int line_number);

    /// Sets the insert point to next_block, inserting an
    /// unconditional branch to there if the current block doesn't yet
    /// have a terminator instruction.
    void FallThroughTo(llvm::BasicBlock *next_block);

    /// Register callbacks that might invalidate native code based on the
    /// optimizations performed in the generated code.
    int FinishFunction();

    /// The following methods operate like the opcodes with the same
    /// name.
    void LOAD_CONST(int index);
    void LOAD_FAST(int index);
    void STORE_FAST(int index);
    void DELETE_FAST(int index);

    void SETUP_LOOP(llvm::BasicBlock *target, int target_opindex,
                    llvm::BasicBlock *fallthrough);
    void GET_ITER();
    void FOR_ITER(llvm::BasicBlock *target, llvm::BasicBlock *fallthrough);
    void POP_BLOCK();

    void SETUP_EXCEPT(llvm::BasicBlock *target, int target_opindex,
                      llvm::BasicBlock *fallthrough);
    void SETUP_FINALLY(llvm::BasicBlock *target, int target_opindex,
                       llvm::BasicBlock *fallthrough);
    void END_FINALLY();
    void WITH_CLEANUP();

    void JUMP_FORWARD(llvm::BasicBlock *target, llvm::BasicBlock *fallthrough) {
        this->JUMP_ABSOLUTE(target, fallthrough);
    }
    void JUMP_ABSOLUTE(llvm::BasicBlock *target, llvm::BasicBlock *fallthrough);

    void POP_JUMP_IF_FALSE(unsigned target_idx,
                           unsigned fallthrough_idx,
                           llvm::BasicBlock *target,
                           llvm::BasicBlock *fallthrough);
    void POP_JUMP_IF_TRUE(unsigned target_idx,
                          unsigned fallthrough_idx,
                          llvm::BasicBlock *target,
                          llvm::BasicBlock *fallthrough);
    void JUMP_IF_FALSE_OR_POP(unsigned target_idx,
                              unsigned fallthrough_idx,
                              llvm::BasicBlock *target,
                              llvm::BasicBlock *fallthrough);
    void JUMP_IF_TRUE_OR_POP(unsigned target_idx,
                             unsigned fallthrough_idx,
                             llvm::BasicBlock *target,
                             llvm::BasicBlock *fallthrough);
    void CONTINUE_LOOP(llvm::BasicBlock *target,
                       int target_opindex,
                       llvm::BasicBlock *fallthrough);

    void BREAK_LOOP();
    void RETURN_VALUE();
    void YIELD_VALUE();

    void POP_TOP();
    void DUP_TOP();
    void DUP_TOP_TWO();
    void DUP_TOP_THREE();
    void ROT_TWO();
    void ROT_THREE();
    void ROT_FOUR();

    void BINARY_ADD();
    void BINARY_SUBTRACT();
    void BINARY_MULTIPLY();
    void BINARY_TRUE_DIVIDE();
    void BINARY_DIVIDE();
    void BINARY_MODULO();
    void BINARY_POWER();
    void BINARY_LSHIFT();
    void BINARY_RSHIFT();
    void BINARY_OR();
    void BINARY_XOR();
    void BINARY_AND();
    void BINARY_FLOOR_DIVIDE();
    void BINARY_SUBSCR();

    void INPLACE_ADD();
    void INPLACE_SUBTRACT();
    void INPLACE_MULTIPLY();
    void INPLACE_TRUE_DIVIDE();
    void INPLACE_DIVIDE();
    void INPLACE_MODULO();
    void INPLACE_POWER();
    void INPLACE_LSHIFT();
    void INPLACE_RSHIFT();
    void INPLACE_OR();
    void INPLACE_XOR();
    void INPLACE_AND();
    void INPLACE_FLOOR_DIVIDE();

    void UNARY_CONVERT();
    void UNARY_INVERT();
    void UNARY_POSITIVE();
    void UNARY_NEGATIVE();
    void UNARY_NOT();

    void SLICE_NONE();
    void SLICE_LEFT();
    void SLICE_RIGHT();
    void SLICE_BOTH();
    void STORE_SLICE_NONE();
    void STORE_SLICE_LEFT();
    void STORE_SLICE_RIGHT();
    void STORE_SLICE_BOTH();
    void DELETE_SLICE_NONE();
    void DELETE_SLICE_LEFT();
    void DELETE_SLICE_RIGHT();
    void DELETE_SLICE_BOTH();
    void STORE_SUBSCR();
    void DELETE_SUBSCR();
    void STORE_MAP();
    void LIST_APPEND();
    void IMPORT_NAME();

    void COMPARE_OP(int cmp_op);
    void CALL_FUNCTION(int num_args);
    void CALL_FUNCTION_VAR(int num_args);
    void CALL_FUNCTION_KW(int num_args);
    void CALL_FUNCTION_VAR_KW(int num_args);

    void BUILD_TUPLE(int size);
    void BUILD_LIST(int size);
    void BUILD_MAP(int size);
    void BUILD_SLICE_TWO();
    void BUILD_SLICE_THREE();
    void UNPACK_SEQUENCE(int size);

    /* LOAD_GLOBAL comes in two flavors: the safe version (a port of the eval
       loop that's guaranteed to work) and a fast version, which uses dict
       versioning to cache pointers as immediates in the generated IR. */
    void LOAD_GLOBAL_safe(int index);
    void LOAD_GLOBAL_fast(int index);
    void LOAD_GLOBAL(int index);
    void STORE_GLOBAL(int index);
    void DELETE_GLOBAL(int index);

    void LOAD_NAME(int index);
    void STORE_NAME(int index);
    void DELETE_NAME(int index);

    void LOAD_ATTR(int index);
    void STORE_ATTR(int index);
    void DELETE_ATTR(int index);

    void LOAD_CLOSURE(int freevar_index);
    void MAKE_CLOSURE(int num_defaults);
    void LOAD_DEREF(int index);
    void STORE_DEREF(int index);

    void RAISE_VARARGS_ZERO();
    void RAISE_VARARGS_ONE();
    void RAISE_VARARGS_TWO();
    void RAISE_VARARGS_THREE();

    bool uses_delete_fast;

private:
    /// These two functions increment or decrement the reference count
    /// of a PyObject*. The behavior is undefined if the Value's type
    /// isn't PyObject* or a subclass.
    void IncRef(llvm::Value *value);
    void DecRef(llvm::Value *value);
    void XDecRef(llvm::Value *value);

    /// These two push or pop a value onto or off of the stack. The
    /// behavior is undefined if the Value's type isn't PyObject* or a
    /// subclass.  These do no refcount operations, which means that
    /// Push() consumes a reference and gives ownership of it to the
    /// new value on the stack, and Pop() returns a pointer that owns
    /// a reference (which it got from the stack).
    void Push(llvm::Value *value);
    llvm::Value *Pop();

    /// Takes a target stack pointer and pops values off the stack
    /// until it gets there, decref'ing as it goes.
    void PopAndDecrefTo(llvm::Value *target_stack_pointer);

    /// The PyFrameObject holds several values, like the block stack
    /// and stack pointer, that we store in allocas inside this
    /// function.  When we suspend or resume a generator, or bail out
    /// to the interpreter, we need to transfer those values between
    /// the frame and the allocas.
    void CopyToFrameObject();
    void CopyFromFrameObject();

    /// We copy the function's locals into an LLVM alloca so that LLVM can
    /// better reason about them.
    void CopyLocalsFromFrameObject();

    template<typename T>
    llvm::Constant *GetSigned(int64_t val) {
        return llvm::ConstantInt::getSigned(
                PyTypeBuilder<T>::get(this->context_),
                val);
    }

    /// Returns the difference between the current stack pointer and
    /// the base of the stack.
    llvm::Value *GetStackLevel();

    /// Get the runtime feedback for the current opcode (as set by SetLasti()).
    /// Opcodes with multiple feedback units should use the arg_index version
    /// to access individual units.
    const PyRuntimeFeedback *GetFeedback() const {
        return GetFeedback(0);
    }
    const PyRuntimeFeedback *GetFeedback(unsigned arg_index) const;

    // Replaces a local variable with the PyObject* stored in
    // new_value.  Decrements the original value's refcount after
    // replacing it.
    void SetLocal(int locals_index, llvm::Value *new_value);

    // Adds handler to the switch for unwind targets and then sets up
    // a call to PyFrame_BlockSetup() with the block type, handler
    // index, and current stack level.
    void CallBlockSetup(int block_type,
                        llvm::BasicBlock *handler, int handler_opindex);

    // Look up a name in the function's names list, returning the
    // PyStringObject for the name_index.
    llvm::Value *LookupName(int name_index);

    /// Inserts a call that will print opcode_name and abort the
    /// program when it's reached.
    void DieForUndefinedOpcode(const char *opcode_name);

    /// How many parameters does the currently-compiling function have?
    int GetParamCount() const;

    /// Implements something like the C assert statement.  If
    /// should_be_true (an i1) is false, prints failure_message (with
    /// puts) and aborts.  Compiles to nothing in optimized mode.
    void Assert(llvm::Value *should_be_true,
                const std::string &failure_message);

    /// Prints failure_message (with puts) and aborts.
    void Abort(const std::string &failure_message);

    // Returns the global variable with type T, address 'var_address',
    // and name 'name'.  If the ExecutionEngine already knows of a
    // variable with the given address, we name and return it.
    // Otherwise the variable will be looked up in Python's C runtime.
    template<typename T>
    llvm::Constant *GetGlobalVariable(
        void *var_address, const std::string &name);
    // Returns the global function with type T and name 'name'. The
    // function will be looked up in Python's C runtime.
    template<typename T>
    llvm::Function *GetGlobalFunction(const std::string &name);

    // Returns a global variable that represents 'obj'.  These get
    // cached in the ExecutionEngine's global mapping table, and they
    // incref the object so its address doesn't get re-used while the
    // GlobalVariable is still alive.  See Util/ConstantMirror.h for
    // more details.  Use this in preference to GetGlobalVariable()
    // for PyObjects that may be immutable.
    llvm::Constant *GetGlobalVariableFor(PyObject *obj);

    // Copies the elements from array[0] to array[N-1] to target, bytewise.
    void MemCpy(llvm::Value *target, llvm::Value *array, llvm::Value *N);

    // Emits code to decrement _Py_Ticker and handle signals and
    // thread-switching when it expires.  Falls through to next_block (or a
    // new block if it's NULL) and leaves the insertion point there.
    void CheckPyTicker(llvm::BasicBlock *next_block = NULL);

    // Helper function for the POP_JUMP_IF_{TRUE,FALSE} and
    // JUMP_IF_{TRUE,FALSE}_OR_POP, used for omitting untake branches.
    // If sufficient data is availble, we made decide to omit one side of a
    // conditional branch, replacing that code with a jump to the interpreter.
    // If sufficient data is available:
    //      - set true_block or false_block to a bail-to-interpreter block.
    //      - set bail_idx and bail_block to handle bailing.
    // If sufficient data is available or we decide not to optimize:
    //      - leave true_block and false_block alone.
    //      - bail_idx will be 0, bail_block will be NULL.
    //
    // Out parameters: true_block, false_block, bail_idx, bail_block.
    void GetPyCondBranchBailBlock(unsigned true_idx,
                                  llvm::BasicBlock **true_block,
                                  unsigned false_idx,
                                  llvm::BasicBlock **false_block,
                                  unsigned *bail_idx,
                                  llvm::BasicBlock **bail_block);

    // Helper function for the POP_JUMP_IF_{TRUE,FALSE} and
    // JUMP_IF_{TRUE,FALSE}_OR_POP. Fill in the bail block for these opcodes
    // that was obtained from GetPyCondBranchBailBlock().
    void FillPyCondBranchBailBlock(llvm::BasicBlock *bail_to,
                                   unsigned bail_idx);

    // These are just like the CreateCall* calls on IRBuilder, except they also
    // apply callee's calling convention and attributes to the call site.
    llvm::CallInst *CreateCall(llvm::Value *callee,
                               const char *name = "");
    llvm::CallInst *CreateCall(llvm::Value *callee,
                               llvm::Value *arg1,
                               const char *name = "");
    llvm::CallInst *CreateCall(llvm::Value *callee,
                               llvm::Value *arg1,
                               llvm::Value *arg2,
                               const char *name = "");
    llvm::CallInst *CreateCall(llvm::Value *callee,
                               llvm::Value *arg1,
                               llvm::Value *arg2,
                               llvm::Value *arg3,
                               const char *name = "");
    llvm::CallInst *CreateCall(llvm::Value *callee,
                               llvm::Value *arg1,
                               llvm::Value *arg2,
                               llvm::Value *arg3,
                               llvm::Value *arg4,
                               const char *name = "");
    template<typename InputIterator>
    llvm::CallInst *CreateCall(llvm::Value *callee,
                               InputIterator begin,
                               InputIterator end,
                               const char *name = "");

    /// Marks the end of the function and inserts a return instruction.
    llvm::ReturnInst *CreateRet(llvm::Value *retval);

    /// Get the LLVM NULL Value for the given type.
    template<typename T>
    llvm::Value *GetNull();

    // Returns an i1, true if value represents a NULL pointer.
    llvm::Value *IsNull(llvm::Value *value);
    // Returns an i1, true if value is a negative integer.
    llvm::Value *IsNegative(llvm::Value *value);
    // Returns an i1, true if value is a non-zero integer.
    llvm::Value *IsNonZero(llvm::Value *value);
    // Returns an i1, true if value is a positive (>0) integer.
    llvm::Value *IsPositive(llvm::Value *value);
    // Returns an i1, true if value is a PyObject considered true.
    // Steals the reference to value.
    llvm::Value *IsPythonTrue(llvm::Value *value);
    // Returns an i1, true if value is an instance of the class
    // represented by the flag argument.  flag should be something
    // like Py_TPFLAGS_INT_SUBCLASS.
    llvm::Value *IsInstanceOfFlagClass(llvm::Value *value, int flag);

    /// During stack unwinding it may be necessary to jump back into
    /// the function to handle a finally or except block.  Since LLVM
    /// doesn't allow us to directly store labels as data, we instead
    /// add the index->label mapping to a switch instruction and
    /// return the i32 for the index.
    llvm::ConstantInt *AddUnwindTarget(llvm::BasicBlock *target,
                                       int target_opindex);

    // Inserts a jump to the return block, returning retval.  You
    // should _never_ call CreateRet directly from one of the opcode
    // handlers, since doing so would fail to unwind the stack.
    void Return(llvm::Value *retval);

    // Propagates an exception by jumping to the unwind block with an
    // appropriate unwind reason set.
    void PropagateException();

    // Set up a block preceding the bail-to-interpreter block.
    void CreateBailPoint(unsigned bail_idx, char reason);
    void CreateBailPoint(char reason) {
        CreateBailPoint(f_lasti_, reason);
    }

    // Only for use in the constructor: Fills in the block that
    // handles bailing out of JITted code back to the interpreter
    // loop.  Code jumping to this block must first:
    //  1) Set frame->f_lasti to the current opcode index.
    //  2) Set frame->f_bailed_from_llvm to a reason.
    void FillBailToInterpreterBlock();
    // Only for use in the constructor: Fills in the block that starts
    // propagating an exception.  Jump to this block when you want to
    // add a traceback entry for the current line.  Don't jump to this
    // block (and just set retval_addr_ and unwind_reason_addr_
    // directly) when you're re-raising an exception and you want to
    // use its traceback.
    void FillPropagateExceptionBlock();
    // Only for use in the constructor: Fills in the unwind block.
    void FillUnwindBlock();
    // Only for use in the constructor: Fills in the block that
    // actually handles returning from the function.
    void FillDoReturnBlock();

    // Create an alloca in the entry block, so that LLVM can optimize
    // it more easily, and return the resulting address. The signature
    // matches IRBuilder.CreateAlloca()'s.
    llvm::Value *CreateAllocaInEntryBlock(
        const llvm::Type *alloca_type,
        llvm::Value *array_size,
        const char *name);

    // Set exception information and jump to exception handling. The
    // arguments can be Value*'s representing NULL to implement the
    // four forms of the 'raise' statement. Steals all references.
    void DoRaise(llvm::Value *exc_type, llvm::Value *exc_inst,
                 llvm::Value *exc_tb);

    // Helper methods for binary and unary operators, passing the name
    // of the Python/C API function that implements the operation.
    // GenericBinOp's apifunc is "PyObject *(*)(PyObject *, PyObject *)"
    void GenericBinOp(const char *apifunc);
    // GenericPowOp's is "PyObject *(*)(PyObject *, PyObject *, PyObject *)"
    void GenericPowOp(const char *apifunc);
    // GenericUnaryOp's is "PyObject *(*)(PyObject *)"
    void GenericUnaryOp(const char *apifunc);

    // Call PyObject_RichCompare(lhs, rhs, cmp_op), pushing the result
    // onto the stack. cmp_op is one of Py_EQ, Py_NE, Py_LT, Py_LE, Py_GT
    // or Py_GE as defined in Python/object.h. Steals both references.
    void RichCompare(llvm::Value *lhs, llvm::Value *rhs, int cmp_op);
    // Call PySequence_Contains(seq, item), returning the result as an i1.
    // Steals both references.
    llvm::Value *ContainerContains(llvm::Value *seq, llvm::Value *item);
    // Check whether exc (a thrown exception) matches exc_type
    // (a class or tuple of classes) for the purpose of catching
    // exc in an except clause. Returns an i1. Steals both references.
    llvm::Value *ExceptionMatches(llvm::Value *exc, llvm::Value *exc_type);

    // If 'value' represents NULL, propagates the exception.
    // Otherwise, falls through.
    void PropagateExceptionOnNull(llvm::Value *value);
    // If 'value' represents a negative integer, propagates the exception.
    // Otherwise, falls through.
    void PropagateExceptionOnNegative(llvm::Value *value);
    // If 'value' represents a non-zero integer, propagates the exception.
    // Otherwise, falls through.
    void PropagateExceptionOnNonZero(llvm::Value *value);

    // Get the address of the idx'th item in a list or tuple object.
    llvm::Value *GetListItemSlot(llvm::Value *lst, int idx);
    llvm::Value *GetTupleItemSlot(llvm::Value *tup, int idx);
    // Helper method for building a new sequence from items on the stack.
    // 'size' is the number of items to build, 'createname' the Python/C API
    // function to call to create the sequence, and 'getitemslot' is called
    // to get each item's address (GetListItemSlot or GetTupleItemSlot.)
    void BuildSequenceLiteral(
        int size, const char *createname,
        llvm::Value *(LlvmFunctionBuilder::*getitemslot)(llvm::Value *, int));

    // Apply a classic slice to a sequence, pushing the result onto the
    // stack.  'start' and 'stop' can be Value*'s representing NULL to
    // indicate missing arguments, and all references are stolen.
    void ApplySlice(llvm::Value *seq, llvm::Value *start, llvm::Value *stop);
    // Assign to or delete a slice of a sequence. 'start' and 'stop' can be
    // Value*'s representing NULL to indicate missing arguments, and
    // 'source' can be a Value* representing NULL to indicate slice
    // deletion. All references are stolen.
    void AssignSlice(llvm::Value *seq, llvm::Value *start, llvm::Value *stop,
                     llvm::Value *source);

#ifdef WITH_TSC
    // Emit code to record a given event with the TSC EventTimer.h system.
    void LogTscEvent(_PyTscEventId event_id);
#endif

    // Helper method for CALL_FUNCTION_(VAR|KW|VAR_KW); calls
    // _PyEval_CallFunctionVarKw() with the given flags and the current
    // stack pointer.
    void CallVarKwFunction(int num_args, int call_flag);

    // CALL_FUNCTION comes in two flavors: CALL_FUNCTION_safe is guaranteed to
    // work, while CALL_FUNCTION_fast takes advantage of data gathered while
    // running through the eval loop to omit as much flexibility as possible.
    void CALL_FUNCTION_safe(int num_args);
    void CALL_FUNCTION_fast(int num_args, const PyRuntimeFeedback *);

    // LOAD/STORE_ATTR_safe always works, while LOAD/STORE_ATTR_fast is
    // optimized to skip the descriptor/method lookup on the type if the object
    // type matches.  It will return false if it fails.
    void LOAD_ATTR_safe(int names_index);
    bool LOAD_ATTR_fast(int names_index);
    void STORE_ATTR_safe(int names_index);
    bool STORE_ATTR_fast(int names_index);

    // Specifies which kind of attribute access we are performing, either load
    // or store.  Eventually we may support delete, but they are rare enough
    // that it is unlikely to be worth it.
    enum AttrAccessKind {
        ATTR_ACCESS_LOAD,
        ATTR_ACCESS_STORE
    };

    // This class encapsulates the common data and code for doing optimized
    // attribute access.  This object helps perform checks, generate guard
    // code, and register invalidation listeners when generating an optimized
    // LOAD_ATTR or STORE_ATTR opcode.
    class AttributeAccessor {
    public:
        // Construct an attribute accessor object.  "name" is a reference to
        // the attribute to access borrowed from co_names, and access_kind
        // determines whether we are generating a LOAD_ATTR or STORE_ATTR
        // opcode.
        AttributeAccessor(LlvmFunctionBuilder *fbuilder, PyObject *name,
                          AttrAccessKind kind)
            : fbuilder_(fbuilder),
              access_kind_(kind),
              guard_type_(0),
              name_(name),
              dictoffset_(0),
              descr_(0),
              guard_descr_type_(0),
              is_data_descr_(false),
              descr_get_(0),
              descr_set_(0),
              guard_type_v_(0),
              name_v_(0),
              dictoffset_v_(0),
              descr_v_(0),
              is_data_descr_v_(0) { }

        // This helper method returns false if a LOAD_ATTR or STORE_ATTR opcode
        // cannot be optimized.  If the opcode can be optimized, it fills in
        // all of the fields of this object by reading the feedback from the
        // code object.
        bool CanOptimizeAttrAccess();

        // This helper method emits the common type guards for an optimized
        // LOAD_ATTR or STORE_ATTR.
        void GuardAttributeAccess(llvm::Value *obj_v,
                                  llvm::BasicBlock *do_access);

        LlvmFunctionBuilder *fbuilder_;
        AttrAccessKind access_kind_;

        // This is the type we have chosen to guard on from the feedback.  All
        // of the other attributes hold references borrowed from this type
        // reference.  The validity of this type reference is ensured by
        // listening for type modification and destruction.
        PyTypeObject *guard_type_;

        PyObject *name_;
        long dictoffset_;

        // This is the descriptor cached from the type object.  It may be NULL
        // if the type has no attribute with the name we're looking up.
        PyObject *descr_;
        // This is the type of the descriptor, if it exists, at compile time.
        // We guard that the type of the descriptor is the same at run time as
        // it is at compile time.
        PyTypeObject *guard_descr_type_;
        // These fields mirror the descriptor accessor fields if they are
        // available.
        bool is_data_descr_;
        descrgetfunc descr_get_;
        descrsetfunc descr_set_;

        // llvm::Value versions of the above data created with EmbedPointer or
        // ConstantInt::get.
        llvm::Value *guard_type_v_;
        llvm::Value *name_v_;
        llvm::Value *dictoffset_v_;
        llvm::Value *descr_v_;
        llvm::Value *is_data_descr_v_;

    private:
        // Cache all of the data required to do attribute access.  This fills
        // in all of the non-llvm::Value fields of this object.
        void CacheTypeLookup();

        // Make LLVM Value versions of all the data we're caching in the IR.
        // Note that we borrow references to the descriptor from the type
        // object.  If the type object is modified to drop its references, this
        // code will be invalidated.  Furthermore, if the type object itself is
        // freed, this code will be invalidated, which will safely drop our
        // references.
        void MakeLlvmValues();
    };

    // A safe version that always works, and a fast version that omits NULL
    // checks where we know the local cannot be NULL.
    void LOAD_FAST_safe(int index);
    void LOAD_FAST_fast(int index);

    /// Emits code to conditionally bail out to the interpreter loop
    /// if a line tracing function is installed.  If the line tracing
    /// function is not installed, execution will continue at
    /// fallthrough_block.  direction should be _PYFRAME_LINE_TRACE or
    /// _PYFRAME_BACKEDGE_TRACE.
    void MaybeCallLineTrace(llvm::BasicBlock *fallthrough_block,
                            char direction);

    /// Emits code to conditionally bail out to the interpreter loop if a
    /// profiling function is installed. If a profiling function is not
    /// installed, execution will continue at fallthrough_block.
    void BailIfProfiling(llvm::BasicBlock *fallthrough_block);

    /// Embed a pointer of some type directly into the LLVM IR.
    template <typename T>
    llvm::Value *EmbedPointer(void *ptr);

    /// Return the BasicBlock we should jump to in order to bail to the
    /// interpreter.
    llvm::BasicBlock *GetBailBlock() const;

    /// Return the BasicBlock we should jump to in order to handle a Python
    /// exception.
    llvm::BasicBlock *GetExceptionBlock() const;

    PyGlobalLlvmData *const llvm_data_;
    // The code object is used for looking up peripheral information
    // about the function.  It's not used to examine the bytecode
    // string.
    PyCodeObject *const code_object_;
    llvm::LLVMContext &context_;
    llvm::Module *const module_;
    llvm::Function *const function_;
    BuilderT builder_;
    const bool is_generator_;
    llvm::DIFactory *const debug_info_;
    const llvm::DICompileUnit debug_compile_unit_;
    const llvm::DISubprogram debug_subprogram_;

    // The most recent index we've started emitting an instruction for.
    int f_lasti_;

    // Flag to indicate whether this code object uses the LOAD_GLOBALS
    // optimization.
    bool uses_load_global_opt_;

    // The following pointers hold values created in the function's
    // entry block. They're constant after construction.
    llvm::Value *frame_;

    // Address of code_object_->co_use_llvm, used for guards.
    llvm::Value *use_llvm_addr_;

    llvm::Value *tstate_;
    llvm::Value *stack_bottom_;
    llvm::Value *stack_pointer_addr_;
    // The tmp_stack_pointer is used when we need to have another
    // function update the stack pointer.  Passing the stack pointer
    // directly would prevent mem2reg from working on it, so we copy
    // it to and from the tmp_stack_pointer around the call.
    llvm::Value *tmp_stack_pointer_addr_;
    llvm::Value *varnames_;
    llvm::Value *names_;
    llvm::Value *globals_;
    llvm::Value *builtins_;
    llvm::Value *fastlocals_;
    llvm::Value *freevars_;
    llvm::Value *f_lineno_addr_;
    llvm::Value *f_lasti_addr_;
    // These two fields correspond to the f_blockstack and f_iblock
    // fields in the frame object.  They get explicitly copied back
    // and forth when the frame escapes.
    llvm::Value *blockstack_addr_;
    llvm::Value *num_blocks_addr_;

    // Expose the frame's locals to LLVM. We copy them in on function-entry,
    // copy them out on write. We use a separate alloca for each local
    // because LLVM's scalar replacement of aggregates pass doesn't handle
    // array allocas.
    std::vector<llvm::Value*> locals_;

    llvm::BasicBlock *unreachable_block_;

    // In generators, we use this switch to jump back to the most
    // recently executed yield instruction.
    llvm::SwitchInst *yield_resume_switch_;

    llvm::BasicBlock *bail_to_interpreter_block_;

    llvm::BasicBlock *propagate_exception_block_;
    llvm::BasicBlock *unwind_block_;
    llvm::Value *unwind_target_index_addr_;
    llvm::SparseBitVector<> existing_unwind_targets_;
    llvm::SwitchInst *unwind_target_switch_;
    // Stores one of the UNWIND_XXX constants defined at the top of
    // llvm_fbuilder.cc
    llvm::Value *unwind_reason_addr_;
    llvm::BasicBlock *do_return_block_;
    llvm::Value *retval_addr_;

    llvm::SmallPtrSet<PyTypeObject*, 5> types_used_;
};

}  // namespace py

#endif  // PYTHON_LLVM_FBUILDER_H
