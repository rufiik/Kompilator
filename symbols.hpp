#ifndef SYMBOLS_HPP
#define SYMBOLS_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <iostream>
#include <stdexcept>
#include "ast.hpp"

// Typy symboli
enum class SymbolType {
    VARIABLE,     
    ARRAY,        
    PROCEDURE,     
    PARAMETER      
};
// Informacje o symbolu
struct SymbolInfo {
    std::string name;
    SymbolType type;
    int line;  // linia deklaracji (do komunikatów błędów)

    
    // Dla parametrów:
    std::vector<std::string> param_names;
    ParamDeclNode::ParamType param_type = ParamDeclNode::ParamType::NORMAL; 
    std::string full_name; 
    bool is_reference = false; 
    bool is_procedure = false;      
    bool is_parameter = false;   


    // Dla tablic:
    bool is_array = false;
    uint64_t array_start = 0;  
    uint64_t array_end = 0;    
    int declared_scope = 0;
    uint64_t array_size() const { return array_end - array_start + 1; }

    bool initialized = true;

    // Metody pomocnicze
    bool isConstant() const { return param_type == ParamDeclNode::ParamType::CONST; }
    bool isUninitialized() const { 
    return is_parameter && param_type == ParamDeclNode::ParamType::OUT && !initialized; 
}
    bool isArray() const { return type == SymbolType::ARRAY; }
    bool isVariable() const { return type == SymbolType::VARIABLE; }
    bool isProcedure() const { return type == SymbolType::PROCEDURE; }
    bool isParameter() const { return type == SymbolType::PARAMETER; }
    bool isReference() const { return is_reference; }
    
    
    bool isIndexInRange(uint64_t index) const {
        return is_array && index >= array_start && index <= array_end;
    }
};

// Klasa wyjątku dla błędów semantycznych
class SemanticError : public std::runtime_error {
public:
    int line;
    
    SemanticError(const std::string& msg, int line_num)
        : std::runtime_error(msg), line(line_num) {}

    const char* what() const noexcept override {
        static std::string full_msg;
        full_msg = "Błąd semantyczny w linii " + std::to_string(line) + ": " + std::runtime_error::what();
        return full_msg.c_str();
    }
};

// Główna klasa tablicy symboli
class SymbolTable {
private:
    // adresy tablic i zmiennych
    std::map<std::string, uint64_t> symbol_addresses;  
    int current_scope;
    
    
public:
    SymbolTable();
    uint64_t next_address;
    void enterScope();     
    void exitScope();     
     std::vector<std::map<std::string, SymbolInfo>> scopes;

    // Dodaj symbol
    void addSymbol(const SymbolInfo& info);
    void addVariable(const std::string& name, int line);
    void addArray(const std::string& name, uint64_t start, uint64_t end, int line);
    void addParameter(const std::string& proc_name,const std::string& param_name, int line, ParamDeclNode::ParamType ptype = ParamDeclNode::ParamType::NORMAL,bool is_array = false);
    void addProcedure(const std::string& name, int line);
    // Sprawdzenia istnienia symboli
    bool exists(const std::string& name) const;
    bool existsInCurrentScope(const std::string& name) const;
    SymbolInfo* get(const std::string& name);
    const SymbolInfo* get(const std::string& name) const;
    // Sprawdzenia
    void checkVariableDeclared(const std::string& name, int line) const;
    void checkNotDeclaredInCurrentScope(const std::string& name, int line) const;
    void checkNotConstant(const std::string& name, int line) const;
    void checkNotUninitialized(const std::string& name, int line) const;
    void checkArrayIndex(const std::string& name, uint64_t index, int line) const;
    void checkProcedureExists(const std::string& name, int line) const;
    
    // Pomocnicze
    void print() const; 
    void clear();       
    uint64_t allocateTemp();
    void addTempVariable(const std::string& name, uint64_t addr);
    void removeTempVariable(const std::string& name);
    
    // Gettery
    size_t getScopeCount() const { return scopes.size(); }
    size_t getSymbolCount() const;
    uint64_t getVariableAddress(const std::string& name) const;
    uint64_t getArrayBaseAddress(const std::string& name) const;
    uint64_t getArrayElementAddress(const std::string& name, uint64_t index) const;
    uint64_t getProcedureAddress(const std::string& name) const;
    uint64_t getParameterAddress(const std::string& proc_name,const std::string& param_name) const;
    
private:
    const SymbolInfo* findSymbol(const std::string& name) const;
    SymbolInfo* findSymbol(const std::string& name);
    std::map<std::string, uint64_t> array_base_addresses;
};

#endif // SYMBOLS_HPP