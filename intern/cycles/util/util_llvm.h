#include <llvm/Linker.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <clang/Basic/Diagnostic.h>
#include <string>
#include <vector>

#ifndef __UTIL_LLVM_H__
#define __UTIL_LLVM_H__

class LLVMShaderModule {
public:
	LLVMShaderModule();
	~LLVMShaderModule();
	bool compile(std::vector<std::string> funcs, std::string source, std::vector<std::string> arguments);
	bool finalize();
	void reset();
	void *get_function(std::string name);

private:
	llvm::LLVMContext *context;
	llvm::Linker *linker;
	llvm::Module *module;
	llvm::ExecutionEngine *engine;
	clang::DiagnosticsEngine *diagnostics;

	llvm::Module* build_module(std::string source, std::vector<std::string> args);

	std::map<std::string, void*> functions;
};

#endif /* __UTIL_LLVM_H__ */
