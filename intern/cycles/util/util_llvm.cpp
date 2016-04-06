#include "util_llvm.h"

#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <clang/Frontend/Utils.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Lex/HeaderSearch.h>
#include <clang/Basic/FileManager.h>
#include <llvm/ADT/IntrusiveRefCntPtr.h>
#include <llvm/ADT/OwningPtr.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetSelect.h>

#include <cstdio>

llvm::Module* LLVMShaderModule::build_module(std::string source, std::vector<std::string> args)
{
	int num_arg = args.size() + 1;
	const char **arg = new const char*[num_arg];
	arg[0] = "source_string";
	for(int i = 0; i < args.size(); i++)
		arg[i+1] = args[i].c_str();

	clang::CompilerInvocation* clangInvocation = new clang::CompilerInvocation();
	clang::CompilerInvocation::CreateFromArgs(*clangInvocation, arg, arg + num_arg, *diagnostics);

	clang::CompilerInstance clangInstance;
	clangInstance.setInvocation(clangInvocation);
	clangInstance.createDiagnostics();
	if(!clangInstance.hasDiagnostics()) {
		fprintf(stderr, "No diagnostics!\n");
		return NULL;
	}

	clang::LangOptions languageOptions;
	clang::FileSystemOptions fso;
	clang::FileManager filemanager(fso);
	clang::SourceManager sourceManager(*diagnostics, filemanager);
	clang::FrontendOptions frontendOptions;

	clang::HeaderSearchOptions& headerSearchOptions = clangInstance.getHeaderSearchOpts();
	headerSearchOptions.AddPath("/usr/local/lib/clang/3.4/include", clang::frontend::Angled, false, false);
	headerSearchOptions.AddPath("/usr/include/x86_64-linux-gnu/c++/4.8", clang::frontend::Angled, false, false);
	headerSearchOptions.AddPath("/usr/include/x86_64-linux-gnu", clang::frontend::Angled, false, false);
	headerSearchOptions.AddPath("/usr/include/c++/4.8", clang::frontend::Angled, false, false);
	headerSearchOptions.AddPath("/usr/include", clang::frontend::Angled, false, false);

	llvm::MemoryBuffer *sourceBuffer = llvm::MemoryBuffer::getMemBufferCopy(source, "SIMPLE_BUFFER");
	clangInstance.getPreprocessorOpts().addRemappedFile("source_string", sourceBuffer);

	clang::CodeGenAction *codegen = new clang::EmitLLVMOnlyAction(context);
	if(!clangInstance.ExecuteAction(*codegen)) {
		fprintf(stderr, "Couldn't execute action!\n");
		return NULL;
	}

	return codegen->takeModule();
}

bool LLVMShaderModule::compile(std::vector<std::string> funcs, std::string source, std::vector<std::string> arguments)
{
	llvm::Module *l_module = build_module(source, arguments);

	if(l_module == NULL) {
		return false;
	}

	for(int i = 0; i < funcs.size(); i++) {
		functions.insert(std::pair<std::string, void*>(funcs[i], NULL));
	}

	if(linker == NULL) {
		delete module;
		module = l_module;
		linker = new llvm::Linker(module);
		return true;
	}

	std::string link_error;
	if(linker->linkInModule(l_module, &link_error)) {
		fprintf(stderr, "Linking error: %s\n", link_error.c_str());
		return false;
	}

	delete l_module;

	return true;
}

bool LLVMShaderModule::finalize()
{
	printf("Functions in the LLVMShaderModule:\n");
	for (llvm::Module::FunctionListType::iterator i = module->getFunctionList().begin(); i != module->getFunctionList().end(); ++i)
		printf("  %s\n", i->getName().str().c_str());

	std::string error;
	engine = llvm::EngineBuilder(module)
                 .setErrorStr(&error)
                 .setEngineKind(llvm::EngineKind::JIT)
                 .setUseMCJIT(true)
                 .setMCJITMemoryManager(new llvm::SectionMemoryManager())
                 .create();

	if(engine == NULL) {
		fprintf(stderr, "Engine creation error: %s\n", error.c_str());
		return false;
	}

	engine->finalizeObject();

	for(std::map<std::string, void*>::iterator it = functions.begin(); it != functions.end(); ++it) {
		llvm::Function *func = module->getFunction(it->first);
		if(func == NULL) {
			fprintf(stderr, "No function %s!\n", it->first.c_str());
			return false;
		}
		if(llvm::verifyFunction(*func)) {
			fprintf(stderr, "Verify failed for function %s!\n", it->first.c_str());
			return false;
		}
		it->second = (void*) engine->getFunctionAddress(it->first);
	}

	return true;
}

void LLVMShaderModule::reset()
{
}

void *LLVMShaderModule::get_function(std::string name)
{
	return functions[name];
}

LLVMShaderModule::LLVMShaderModule()
 : context(new llvm::LLVMContext()), linker(NULL), module(NULL), engine(NULL)
{
	LLVMInitializeNativeTarget();
	llvm::InitializeNativeTargetAsmParser();
	llvm::InitializeNativeTargetAsmPrinter();

	clang::DiagnosticIDs *dID = new clang::DiagnosticIDs();
	clang::DiagnosticOptions* dOpts = new clang::DiagnosticOptions();
	clang::TextDiagnosticPrinter *dClient = new clang::TextDiagnosticPrinter(llvm::errs(), dOpts);
	diagnostics = new clang::DiagnosticsEngine(new clang::DiagnosticIDs(), dOpts, dClient);
}

LLVMShaderModule::~LLVMShaderModule()
{
	delete linker;
	delete module;
	delete context;
	delete engine;
	delete diagnostics;
}
