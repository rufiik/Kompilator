#include <iostream>
#include <fstream>
#include <cstdlib>

#include "ast.hpp"
#include "symbols.hpp"
#include "semantic.hpp"
#include "codegen.hpp"

extern int yyparse();
extern FILE* yyin;
extern int yylineno;
extern ASTNode* program_root;

bool has_errors = false;
ASTNode* program_root = nullptr;

int main(int argc, char** argv) {
    if (!(argc == 3 || argc == 4)) {
        std::cerr << "Użycie: " << argv[0] << " <wejście> <wyjście> [-debug]" << std::endl;
        return 1;
    }
    
    // Parsowanie
    yyin = fopen(argv[1], "r");
    if (!yyin) {
        std::cerr << "Nie można otworzyć: " << argv[1] << std::endl;
        return 1;
    }
    
    yylineno = 1;
    has_errors = false;
    
    if (yyparse() != 0 || has_errors) {
        std::cerr << "Błąd parsowania" << std::endl;
        fclose(yyin);
        return 1;
    }
    bool debug = false;
    fclose(yyin);
        if (argc == 4) {
        std::string opt = argv[3];
        if (opt == "--debug") debug = true;
        else {
            std::cerr << "Nieznana opcja: " << argv[3] << std::endl;
            return 1;
        }
    }
    
    // Analiza semantyczna
    auto symtab = std::make_shared<SymbolTable>();
    SemanticAnalyzer analyzer(*symtab);
    
    if (!analyzer.analyze(program_root)) {
        analyzer.printErrors();
        analyzer.printWarnings();
        if (program_root) delete program_root;
        return 1;
    }
    analyzer.printWarnings();
 if (debug) {
        symtab->print();
    }
    // Generowanie kodu

    try {
        CodeGenerator generator(*symtab);
        generator.generate(program_root);
        generator.emitToFile(argv[2]);
    } catch (const std::exception& e) {
        std::cerr << "Błąd generowania kodu: " << e.what() << std::endl;
        if (program_root) delete program_root;
        return 1;
    }
    
    // Sprzątanie
    if (program_root) {
        delete program_root;
    }
    
    return 0;
}