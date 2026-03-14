#ifndef SEMANTIC_HPP
#define SEMANTIC_HPP

#include "ast.hpp"
#include "symbols.hpp"
#include <vector>
#include <memory>

class SemanticAnalyzer {
private:
    SymbolTable& symbol_table;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    bool in_procedure;
    std::string current_procedure;
    std::vector<std::string> for_iterators;  
    std::map<std::string, std::map<std::string, SymbolInfo>> procedureScopes;
    std::vector<std::string> defined_procedures; 

    void addError(const std::string& msg, int line);
    void addWarning(const std::string& msg, int line);
    void checkLValue(ASTNode* node);
    void checkRValue(ASTNode* node);
    bool variableExists(const std::string& name);
    const SymbolInfo* findVariable(const std::string& name);

    void analyzeProgram(ProgramNode* node);
    void analyzeProgramAll(ProgramAllNode* node);
    void analyzeProcedure(ProcedureNode* node);
    void analyzeBlock(BlockNode* node);
    void analyzeDeclaration(DeclarationsNode* node);
    void analyzeAssignment(AssignmentNode* node);
    void analyzeRead(ReadNode* node);
    void analyzeWrite(WriteNode* node);
    void analyzeVariable(VariableNode* node, bool is_lvalue);
    void analyzeNumber(NumberNode* node);
    void analyzeBinaryOp(BinaryOpNode* node);
    void analyzeArrayDecl(ArrayDeclNode* node);
    void analyzeArrayAccess(ArrayAccessNode* node, bool is_lvalue);
    void analyzeExpression(ASTNode* node, bool is_lvalue);
    void analyzeProcedureCall(ProcedureCallNode* node);
    void analyzeFor(ForNode* node);
    void analyzeCondition(ConditionNode* node);
    void analyzeIfElse(IfNode* node);
    void analyzeWhile(WhileNode* node);
    void analyzeRepeat(RepeatNode* node);
    
public:
    SemanticAnalyzer(SymbolTable& symtab);
    
    bool analyze(ASTNode* root);
    
    const std::vector<std::string>& getErrors() const { return errors; }
    bool hasErrors() const { return !errors.empty(); }
    void printErrors() const;
    void printWarnings() const;
};

#endif // SEMANTIC_HPP