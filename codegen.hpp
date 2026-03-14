#ifndef CODEGEN_HPP
#define CODEGEN_HPP

#include "ast.hpp"
#include "symbols.hpp"
#include <vector>
#include <string>
#include <map>
#include <fstream>

class CodeGenerator {
private:
    std::vector<std::string> code;  
    SymbolTable& symbol_table;
    std::map<std::string, uint64_t> procedure_addresses;
    std::string current_procedure;

    // Metody pomocnicze
    uint64_t getVarAddress(const std::string& name);
    void addInstruction(const std::string& instr);
    void loadConstant(uint64_t value, const std::string& reg);
    void patchJump(int jump_index, uint64_t target_address);
    // Generowanie
    void generateValue(ASTNode* node, const std::string& target_reg = "a");
    void generateAssignment(AssignmentNode* node);
    void generateRead(ReadNode* node);
    void generateWrite(WriteNode* node);
    void generateProgram(ProgramNode* node);
    void generateBlock(BlockNode* node);
    void generateBinaryOp(BinaryOpNode* node, const std::string& target_reg = "a");
    void generateArrayAccess(ArrayAccessNode* node, const std::string& target_reg, bool load_value = true);
    void generateArrayStore(ArrayAccessNode* node, const std::string& value_reg);
    void generateProgramAll(ProgramAllNode* node);
    void generateProcedure(ProcedureNode* node);
    void generateProcedureCall(ProcedureCallNode* node);
    void generateIf(IfNode* node);
    void generateWhile(WhileNode* node);
    void generateRepeat(RepeatNode* node);
    void generateFor(ForNode* node);
    void generateMultiply(ASTNode* left, ASTNode* right);
    void generateDivide(ASTNode* left, ASTNode* right, bool return_quotient);
    long long generateCondition(ConditionNode* node);



public:
    CodeGenerator(SymbolTable& symtab);
    
    // Główna metoda
    void generate(ASTNode* root);
    // Zapis do pliku
    void emit(std::ostream& out) const;
    void emitToFile(const std::string& filename) const;
    // Debug
    void printCode() const;
};

#endif // CODEGEN_HPP