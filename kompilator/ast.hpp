// ast.hpp
#ifndef AST_HPP
#define AST_HPP

#include <string>
#include <vector>
#include <cstdint>

// Typy węzłów
enum class NodeType {
    PROGRAM,
    DECLARATION,
    ASSIGNMENT,
    READ,
    WRITE,
    VARIABLE,
    NUMBER,
    BLOCK,
    BIN_OP,
    ARRAY_DECL,
    ARRAY_ACCESS,
    PROCEDURE_DECL,
    PROCEDURE_CALL,
    PARAM_DECL,
    ARGUMENT_LIST,
    PROGRAM_ALL,
    IF_ELSE,
    WHILE,
    REPEAT,
    FOR_TO,
    FOR_DOWNTO,
    CONDITION
};

// Klasa bazowa
class ASTNode {
public:
    NodeType type;
    int line;
    
    ASTNode(NodeType t, int l) : type(t), line(l) {}
    virtual ~ASTNode() = default;
    
    // Metody pomocnicze do rzutowania
    bool isDeclaration() const { return type == NodeType::DECLARATION; }
    bool isBlock() const { return type == NodeType::BLOCK; }
};

// Liczba
class NumberNode : public ASTNode {
public:
    std::string value;
    
    NumberNode(const std::string& val, int line) 
        : ASTNode(NodeType::NUMBER, line), value(val) {}
};

// Zmienna
class VariableNode : public ASTNode {
public:
    std::string name;
    
    VariableNode(const std::string& n, int line)
        : ASTNode(NodeType::VARIABLE, line), name(n) {}
};

class VariableDeclNode : public ASTNode {
public:
    std::string name;
    
    VariableDeclNode(const std::string& n, int line)
        : ASTNode(NodeType::DECLARATION, line), name(n) {}
};

// Przypisanie
class AssignmentNode : public ASTNode {
public:
    ASTNode* lhs;  
    ASTNode* rhs;
    
    AssignmentNode(ASTNode* l, ASTNode* r, int line)
        : ASTNode(NodeType::ASSIGNMENT, line), lhs(l), rhs(r) {}
    
    ~AssignmentNode() {
        delete lhs;
        delete rhs;
    }
};

// Warunek
struct ConditionNode : public ASTNode {
    std::string op;
    ASTNode *left, *right;
    ConditionNode(std::string op, ASTNode* l, ASTNode* r, int line) 
        : ASTNode(NodeType::CONDITION, line), op(op), left(l), right(r) {}
};

// If-Else
struct IfNode : public ASTNode {
    ASTNode *condition, *then_block, *else_block;
    IfNode(ASTNode* cond, ASTNode* tb, ASTNode* eb, int line) 
        : ASTNode(NodeType::IF_ELSE, line), condition(cond), then_block(tb), else_block(eb) {}
};

// While
struct WhileNode : public ASTNode {
    ASTNode *condition, *body;
    WhileNode(ASTNode* cond, ASTNode* b, int line) 
        : ASTNode(NodeType::WHILE, line), condition(cond), body(b) {}
};

// Repeat
struct RepeatNode : public ASTNode {
    ASTNode *condition, *body;
    RepeatNode(ASTNode* b, ASTNode* cond, int line) 
        : ASTNode(NodeType::REPEAT, line), condition(cond), body(b) {}
};

// For
struct ForNode : public ASTNode {
    std::string iterator;
    ASTNode *from, *to, *body;
    bool descending;
    ForNode(std::string iter, ASTNode* f, ASTNode* t, ASTNode* b, bool desc, int line) 
        : ASTNode(NodeType::FOR_TO, line), iterator(iter), from(f), to(t), body(b), descending(desc) {
        if(desc) type = NodeType::FOR_DOWNTO;
    }
};

// READ
class ReadNode : public ASTNode {
public:
    ASTNode* target;
    
    ReadNode(ASTNode* tgt, int line)
        : ASTNode(NodeType::READ, line), target(tgt) {}
    
    ~ReadNode() {
        delete target;
    }
};

// WRITE
class WriteNode : public ASTNode {
public:
    ASTNode* value;
    
    WriteNode(ASTNode* val, int line)
        : ASTNode(NodeType::WRITE, line), value(val) {}
    
    ~WriteNode() {
        delete value;
    }
};

// Operacja binarna
class BinaryOpNode : public ASTNode {
public:
    enum class OpType { ADD, SUB, MUL, DIV, MOD };
    
    OpType op;
    ASTNode* left;
    ASTNode* right;
    
    BinaryOpNode(OpType o, ASTNode* l, ASTNode* r, int line)
        : ASTNode(NodeType::BIN_OP, line), op(o), left(l), right(r) {}
    
    ~BinaryOpNode() {
        delete left;
        delete right;
    }
};

// Deklaracja tablicy
class ArrayDeclNode : public ASTNode {
public:
    std::string name;
    uint64_t start_index;
    uint64_t end_index;
    
    ArrayDeclNode(const std::string& n, uint64_t start, uint64_t end, int line)
        : ASTNode(NodeType::ARRAY_DECL, line), name(n), 
          start_index(start), end_index(end) {}
};

// Dostęp do elementu tablicy
class ArrayAccessNode : public ASTNode {
public:
    std::string array_name;
    ASTNode* index;  
    
    ArrayAccessNode(const std::string& name, ASTNode* idx, int line)
        : ASTNode(NodeType::ARRAY_ACCESS, line), array_name(name), index(idx) {}
    
    ~ArrayAccessNode() {
        delete index;
    }
};

// Deklaracja parametru procedury
class ParamDeclNode : public ASTNode {
public:
    enum class ParamType { 
        NORMAL,   // domyślnie - IN-OUT
        CONST,    // I - stała (tylko odczyt)
        OUT,      // 0 - wyjściowy (tylko zapis)
        ARRAY     // T - tablica przez referencję
    };
    
    ParamType param_type;
    std::string param_name;
    bool is_array;  // true jeśli to T_name
    
    ParamDeclNode(ParamType type, const std::string& n, bool is_arr, int line)
        : ASTNode(NodeType::PARAM_DECL, line), 
          param_type(type), param_name(n), is_array(is_arr) {}
};

// Lista parametrów formalnych
class ParamsListNode : public ASTNode {
public:
    std::vector<ParamDeclNode*> params;
    
    ParamsListNode(int line) : ASTNode(NodeType::DECLARATION, line) {}
    
    ~ParamsListNode() {
        for (auto param : params) {
            delete param;
        }
    }
    
    void addParam(ParamDeclNode* param) {
        params.push_back(param);
    }
};

// Nagłówek procedury (nazwa + parametry)
class ProcedureHeaderNode : public ASTNode {
public:
    std::string name;
    ParamsListNode* params;
    
    ProcedureHeaderNode(const std::string& n, ParamsListNode* p, int line)
        : ASTNode(NodeType::DECLARATION, line), name(n), params(p) {}
    
    ~ProcedureHeaderNode() {
        delete params;
    }
};



// Lista argumentów przy wywołaniu procedury
class ArgsListNode : public ASTNode {
public:
    std::vector<ASTNode*> args;  // VariableNode, ArrayAccessNode, lub NumberNode
    
    ArgsListNode(int line) : ASTNode(NodeType::ARGUMENT_LIST, line) {}
    
    ~ArgsListNode() {
        for (auto arg : args) {
            delete arg;
        }
    }
    
    void addArg(ASTNode* arg) {
        args.push_back(arg);
    }
};

// Wywołanie procedury
class ProcedureCallNode : public ASTNode {
public:
    std::string proc_name;
    ArgsListNode* args;
    
    ProcedureCallNode(const std::string& name, ArgsListNode* a, int line)
        : ASTNode(NodeType::PROCEDURE_CALL, line), proc_name(name), args(a) {}
    
    ~ProcedureCallNode() {
        delete args;
    }
};



// Blok
class BlockNode : public ASTNode {
public:
    std::vector<ASTNode*> statements;
    
    BlockNode(int line) : ASTNode(NodeType::BLOCK, line) {}
    
    ~BlockNode() {
        for (auto stmt : statements) {
            delete stmt;
        }
    }
    
    void addStatement(ASTNode* stmt) {
        statements.push_back(stmt);
    }
};

// Deklaracja
class DeclarationsNode : public ASTNode {
public:
    std::vector<ASTNode*> declarations;  // VariableDeclNode lub ArrayDeclNode
    
    DeclarationsNode(int line) : ASTNode(NodeType::DECLARATION, line) {}
    
    ~DeclarationsNode() {
        for (auto decl : declarations) {
            delete decl;
        }
    }
    
    void addDeclaration(ASTNode* decl) {
        declarations.push_back(decl);
    }
};

// Program
class ProgramNode : public ASTNode {
public:
    ASTNode* declarations;  
    BlockNode* commands;
    
    ProgramNode(ASTNode* decls, BlockNode* cmds, int line)
        : ASTNode(NodeType::PROGRAM, line), declarations(decls), commands(cmds) {}
    
    ~ProgramNode() {
        delete declarations;
        delete commands;
    }
};

// Definicja całej procedury
class ProcedureNode : public ASTNode {
public:
    ProcedureHeaderNode* header;
    DeclarationsNode* local_decls;  // deklaracje lokalne (może być nullptr)
    BlockNode* body;                // ciało procedury
    int scope_id = -1;
    ProcedureNode(ProcedureHeaderNode* hdr, DeclarationsNode* decls, 
                  BlockNode* b, int line)
        : ASTNode(NodeType::PROCEDURE_DECL, line), 
          header(hdr), local_decls(decls), body(b) {}
    
    ~ProcedureNode() {
        delete header;
        delete local_decls;
        delete body;
    }
};

// Cały program z procedurami i głównym programem
class ProgramAllNode : public ASTNode {
public:
    std::vector<ProcedureNode*> procedures;
    ProgramNode* main_program;
    
    ProgramAllNode(int line) : ASTNode(NodeType::PROGRAM_ALL, line), 
                               main_program(nullptr) {}
    
    ~ProgramAllNode() {
        for (auto proc : procedures) {
            delete proc;
        }
        delete main_program;
    }
    
    void addProcedure(ProcedureNode* proc) {
        procedures.push_back(proc);
    }
    
    void setMainProgram(ProgramNode* main) {
        main_program = main;
    }
};

// Funkcja do konwersji string na typ parametru
inline ParamDeclNode::ParamType stringToParamType(const std::string& str) {
    if (str == "I") return ParamDeclNode::ParamType::CONST;
    if (str == "0") return ParamDeclNode::ParamType::OUT;
    if (str == "T") return ParamDeclNode::ParamType::ARRAY;
    return ParamDeclNode::ParamType::NORMAL;  // domyślnie
}

// Funkcja do konwersji typu parametru na string (debug)
inline std::string paramTypeToString(ParamDeclNode::ParamType type) {
    switch (type) {
        case ParamDeclNode::ParamType::CONST: return "I";
        case ParamDeclNode::ParamType::OUT: return "0";
        case ParamDeclNode::ParamType::ARRAY: return "T";
        default: return "";
    }
}

#endif // AST_HPP