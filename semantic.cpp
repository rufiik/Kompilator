#include "semantic.hpp"
#include <iostream>
#include <sstream>

SemanticAnalyzer::SemanticAnalyzer(SymbolTable& symtab) 
    : symbol_table(symtab), in_procedure(false), current_procedure("") {}

void SemanticAnalyzer::addError(const std::string& msg, int line) {
    std::ostringstream oss;
    oss << "Linia " << line << ": " << msg;
    errors.push_back(oss.str());
}
void SemanticAnalyzer::addWarning(const std::string& msg, int line) {
    std::ostringstream oss;
    oss << "Linia " << line << ": " << msg;
    warnings.push_back(oss.str());
}


const SymbolInfo* SemanticAnalyzer::findVariable(const std::string& name) {
    // Aktualna procedura 
    if (!current_procedure.empty()) {
        std::string full_name = current_procedure + "." + name;
        const SymbolInfo* info = symbol_table.get(full_name);
        if (info) {
            return info;
        }
        info = symbol_table.get(name);
        if (info && !info->is_parameter) {
            return info;
        }
        return nullptr;
    }
    // Globalny zakres
    const SymbolInfo* info = symbol_table.get(name);
    if (info && info->is_parameter) {
        return nullptr;
    }
    return info;
}

bool SemanticAnalyzer::variableExists(const std::string& name) {
    return findVariable(name) != nullptr;
}

void SemanticAnalyzer::analyzeFor(ForNode* node) {
    if (!node) return;
    analyzeExpression(node->from, false);
    analyzeExpression(node->to, false);
    // Iterator jest lokalny dla pętli
    symbol_table.enterScope();
    symbol_table.addVariable(node->iterator, node->line);
    // Dodaj do listy aktywnych iteratorów (zablokuj modyfikację)
    for_iterators.push_back(node->iterator);
    if (node->body) analyzeBlock(static_cast<BlockNode*>(node->body));
    for_iterators.pop_back();
    symbol_table.exitScope();
}

// checkLValue
void SemanticAnalyzer::checkLValue(ASTNode* node) {
    if (!node) return;
    
    if (node->type == NodeType::VARIABLE) {
        VariableNode* var = static_cast<VariableNode*>(node);
        
        if (!variableExists(var->name)) {
            addError("Użycie niezadeklarowanej zmiennej: '" + var->name + "'", var->line);
            return;
        }
    
        const SymbolInfo* info = findVariable(var->name);
        if (!info) return;
        
        if (info->isConstant()) {
            addError("Nie można modyfikować stałej (parametr I): '" + var->name + "'", var->line);
        }
        
        for (const auto& iter : for_iterators) {
            if (iter == var->name) {
                addError("Nie można modyfikować iteratora pętli FOR: '" + var->name + "'", var->line);
                break;
            }
        }
    }
}

// checkRValue
void SemanticAnalyzer::checkRValue(ASTNode* node) {
    if (!node) return;
    
    if (node->type == NodeType::VARIABLE) {
        VariableNode* var = static_cast<VariableNode*>(node);
        
        if (!variableExists(var->name)) {
            addError("Użycie niezadeklarowanej zmiennej: '" + var->name + "'", var->line);
            return;
        }
        
        const SymbolInfo* info = findVariable(var->name);
        if (!info) {
            return;
        }

        if (info->isUninitialized()) {
            addError("Czytanie niezainicjalizowanej zmiennej (parametr O): '" + var->name + "'", var->line);
        }
    }
}

// Główna funkcja analizy
bool SemanticAnalyzer::analyze(ASTNode* root) {
    if (!root) {
        addError("Puste drzewo AST", 0);
        return false;
    }
    errors.clear();
    try {
        switch (root->type) {
            case NodeType::PROGRAM_ALL: 
                analyzeProgramAll(static_cast<ProgramAllNode*>(root));
                break;
            default:
                addError("Nieznany typ węzła AST", root->line);
                return false;
        }
    } catch (const SemanticError& e) {
        addError(e.what(), e.line);
    } catch (const std::exception& e) {
        addError(std::string("Błąd analizy: ") + e.what(), 0);
    }
    
    return !hasErrors();
}
// Analiza całego programu (procedury + main)
void SemanticAnalyzer::analyzeProgramAll(ProgramAllNode* node) {
    if (!node) return;
    
    for (auto proc : node->procedures) {
        analyzeProcedure(proc);
    }
    
    if (node->main_program) {
        analyzeProgram(node->main_program);
  }
}  
// Analiza programu
void SemanticAnalyzer::analyzeProgram(ProgramNode* node) {
    if (!node) return;
    symbol_table.enterScope();
    // 1. Dodaj deklaracje do tablicy symboli
    if (node->declarations) {
        analyzeDeclaration(static_cast<DeclarationsNode*>(node->declarations));
    }
    // 2. Przeanalizuj blok komend
    if (node->commands) {
        analyzeBlock(node->commands);
    }
}

// // Analiza pojedynczej procedury
void SemanticAnalyzer::analyzeProcedure(ProcedureNode* node) {
    if (!node) return;
    std::string old_procedure = current_procedure;
    current_procedure = node->header->name;
    in_procedure = true;
    node->scope_id = symbol_table.scopes.size();
    
    symbol_table.enterScope();
    
    try {
        symbol_table.checkNotDeclaredInCurrentScope(node->header->name, node->header->line);
        symbol_table.addProcedure(node->header->name, node->header->line);
        SymbolInfo* proc_info = symbol_table.get(node->header->name);
        if (proc_info && node->header->params) {
            for (auto param : node->header->params->params) {
                proc_info->param_names.push_back(param->param_name);
            }
        }
    } catch (const SemanticError& e) {
        addError(e.what(), e.line);
    }

    if (node->header->params) {
        for (auto param : node->header->params->params) {
            try {
                symbol_table.checkNotDeclaredInCurrentScope(param->param_name, param->line);
                symbol_table.addParameter(
                    node->header->name,
                    param->param_name,
                    param->line,
                    param->param_type,
                    param->is_array
                );
            } catch (const SemanticError& e) {
                addError(e.what(), e.line);
            }
        }
    }

    if (node->local_decls) {
        analyzeDeclaration(node->local_decls);
    }

    if (node->body) {
        analyzeBlock(node->body);
    }

    defined_procedures.push_back(node->header->name);

    current_procedure = old_procedure;
    in_procedure = !old_procedure.empty();
}

void SemanticAnalyzer::analyzeProcedureCall(ProcedureCallNode* node) {
    if (!node) return;
    
    if (node->proc_name == current_procedure) {
        addError("Rekurencja jest zabroniona: procedura '" + node->proc_name + 
                 "' nie może wywoływać sama siebie", node->line);
        return;
    }
    
    bool found = false;
    for (const auto& proc : defined_procedures) {
        if (proc == node->proc_name) {
            found = true;
            break;
        }
    }
    if (!found) {
        addError("Procedura '" + node->proc_name + 
                 "' musi być zdefiniowana przed wywołaniem", node->line);
        return;
    }
    
    const SymbolInfo* proc_info = symbol_table.get(node->proc_name);
    if (!proc_info || !proc_info->isProcedure()) {
        addError("Wywołanie nieistniejącej procedury: '" + node->proc_name + "'", node->line);
        return;
    }
    
    if (node->args->args.size() != proc_info->param_names.size()) {
        addError("Nieprawidłowa liczba argumentów dla procedury '" + node->proc_name + "'", node->line);
        return;
    }
    
    for (size_t i = 0; i < node->args->args.size(); ++i) {
        ASTNode* arg = node->args->args[i];
        std::string param_name = proc_info->param_names[i];
        
        std::string full_param_name = node->proc_name + "." + param_name;
        const SymbolInfo* param_info = symbol_table.get(full_param_name);
        
        if (!param_info) continue;
        if (param_info && !param_info->isConstant()) { // Parametr O lub IO
            if (arg->type == NodeType::VARIABLE) {
                VariableNode* var = static_cast<VariableNode*>(arg);
                for (const auto& iter : for_iterators) {
                    if (iter == var->name) {
                        addError("Nie można przekazać iteratora pętli FOR jako argumentu do modyfikowalnego parametru: '" + var->name + "'", node->line);
                        break;
                    }
                }
            }
        }
        if (param_info->is_array) {
            if (arg->type == NodeType::VARIABLE) {
                VariableNode* var = static_cast<VariableNode*>(arg);
                const SymbolInfo* arg_info = findVariable(var->name);
                if (!arg_info || !arg_info->is_array) {
                    addError("Parametr T wymaga tablicy, podano: '" + var->name + "'", node->line);
                }
            } else {
                addError("Parametr T wymaga tablicy", node->line);
            }
        } else {
            if (arg->type == NodeType::VARIABLE) {
                VariableNode* var = static_cast<VariableNode*>(arg);
                const SymbolInfo* arg_info = findVariable(var->name);
                if (arg_info && arg_info->is_array) {
                    addError("Parametr wymaga zmiennej, podano tablicę: '" + var->name + "'", node->line);
                }
            }
        }       
        if (arg->type == NodeType::VARIABLE) {
            VariableNode* var = static_cast<VariableNode*>(arg);
            const SymbolInfo* arg_info = findVariable(var->name);
            if (arg_info && arg_info->isConstant() && !param_info->isConstant()) {
                addError("Parametr I '" + var->name + 
                         "' może być przekazany tylko do parametru I", node->line);
            }
        }
        
        if (param_info->isConstant()) {
            if (arg->type == NodeType::VARIABLE) {
                VariableNode* var = static_cast<VariableNode*>(arg);
                const SymbolInfo* arg_info = findVariable(var->name);
                if (arg_info && arg_info->isUninitialized()) {
                    addError("Parametr O '" + var->name + 
                             "' nie może być przekazany do parametru I", node->line);
                }
            }
        }
        if (param_info && !param_info->isConstant()) { // O lub IO
            if (arg->type == NodeType::VARIABLE) {
                VariableNode* var = static_cast<VariableNode*>(arg);
                SymbolInfo* arg_info = nullptr;
                if (!current_procedure.empty()) {
                    std::string full_name = current_procedure + "." + var->name;
                    arg_info = symbol_table.get(full_name);
                }
                if (!arg_info) {
                    arg_info = symbol_table.get(var->name);
                }
                if (arg_info) {
                    arg_info->initialized = true;
                }
            }
        }
    }
}

// Analiza deklaracji
void SemanticAnalyzer::analyzeDeclaration(DeclarationsNode* node) {
    if (!node) return;

    for (auto decl : node->declarations) {
        if (!decl) continue;
        if (decl->type == NodeType::DECLARATION) {
            VariableDeclNode* var_decl = static_cast<VariableDeclNode*>(decl);
            try {
                std::string full_name = var_decl->name;
                if (!current_procedure.empty()) {
                    full_name = current_procedure + "." + var_decl->name;
                }
                symbol_table.checkNotDeclaredInCurrentScope(full_name, var_decl->line);
                symbol_table.addVariable(full_name, var_decl->line);
            } catch (const SemanticError& e) {
                addError(e.what(), e.line);
            }
        } else if (decl->type == NodeType::ARRAY_DECL) {
            ArrayDeclNode* arr_decl = static_cast<ArrayDeclNode*>(decl);
            analyzeArrayDecl(arr_decl);
        }
    }
}

// Analiza bloku instrukcji
void SemanticAnalyzer::analyzeBlock(BlockNode* node) {
    if (!node) return;
    
    for (auto stmt : node->statements) {
        if (!stmt) continue;
        
        switch (stmt->type) {
            case NodeType::ASSIGNMENT:
                analyzeAssignment(static_cast<AssignmentNode*>(stmt));
                break;
            case NodeType::READ:
                analyzeRead(static_cast<ReadNode*>(stmt));
                break;
            case NodeType::WRITE:
                analyzeWrite(static_cast<WriteNode*>(stmt));
                break;
            case NodeType::PROCEDURE_CALL:
                analyzeProcedureCall(static_cast<ProcedureCallNode*>(stmt));
                break;
            case NodeType::BLOCK:
                analyzeBlock(static_cast<BlockNode*>(stmt));
                break;
            case NodeType::IF_ELSE:
                analyzeIfElse(static_cast<IfNode*>(stmt));
                break;
            case NodeType::WHILE:
                analyzeWhile(static_cast<WhileNode*>(stmt));
                break;
            case NodeType::REPEAT:
                analyzeRepeat(static_cast<RepeatNode*>(stmt));
                break;
            case NodeType::FOR_TO:
            case NodeType::FOR_DOWNTO:
                analyzeFor(static_cast<ForNode*>(stmt));
                break;
            default:
                addError("Nieznany typ instrukcji", stmt->line);
                break;
        }
    }
}

// Analiza IF-ELSE
void SemanticAnalyzer::analyzeIfElse(IfNode* node) {
    if (!node) return;
    
    // Sprawdź warunek
    if (node->condition) {
        analyzeCondition(static_cast<ConditionNode*>(node->condition));
    } else {
        addError("Brak warunku w instrukcji IF", node->line);
    }
    
    // Sprawdź blok THEN
    if (node->then_block) {
        analyzeBlock(static_cast<BlockNode*>(node->then_block));
    }
    
    // Sprawdź blok ELSE (opcjonalny)
    if (node->else_block) {
        analyzeBlock(static_cast<BlockNode*>(node->else_block));
    }
}

// Analiza WHILE
void SemanticAnalyzer::analyzeWhile(WhileNode* node) {
    if (!node) return;
    
    // Sprawdź warunek
    if (node->condition) {
        analyzeCondition(static_cast<ConditionNode*>(node->condition));
    } else {
        addError("Brak warunku w pętli WHILE", node->line);
    }
    
    // Sprawdź ciało pętli
    if (node->body) {
        analyzeBlock(static_cast<BlockNode*>(node->body));
    }
}

// Analiza REPEAT-UNTIL
void SemanticAnalyzer::analyzeRepeat(RepeatNode* node) {
    if (!node) return;
    
    if (node->body) {
        analyzeBlock(static_cast<BlockNode*>(node->body));
    }
    
    if (node->condition) {
        analyzeCondition(static_cast<ConditionNode*>(node->condition));
    } else {
        addError("Brak warunku w pętli REPEAT-UNTIL", node->line);
    }
}

// Analiza wyrażenia (value)
void SemanticAnalyzer::analyzeExpression(ASTNode* node, bool is_lvalue) {
    if (!node) return;
    
    switch (node->type) {
        case NodeType::VARIABLE:
            analyzeVariable(static_cast<VariableNode*>(node), is_lvalue);
            break;
        case NodeType::NUMBER:
            analyzeNumber(static_cast<NumberNode*>(node));
            break;
        case NodeType::ARRAY_ACCESS:
            analyzeArrayAccess(static_cast<ArrayAccessNode*>(node), is_lvalue);
            break;
        case NodeType::BIN_OP:
            analyzeBinaryOp(static_cast<BinaryOpNode*>(node));
            break;
        default:
            addError("Nieznany typ wyrażenia", node->line);
            break;
    }
}

// Analiza przypisania 
void SemanticAnalyzer::analyzeAssignment(AssignmentNode* node) {
    if (!node) return;
    
    if (node->lhs->type == NodeType::VARIABLE) {
        checkLValue(node->lhs);
        VariableNode* var = static_cast<VariableNode*>(node->lhs);
        SymbolInfo* info = nullptr;
        if (!current_procedure.empty()) {
            std::string full_name = current_procedure + "." + var->name;
            info = symbol_table.get(full_name);
        }
        if (!info) {
            info = symbol_table.get(var->name);
        }
        if (info) {
            info->initialized = true;
        }
    }
    else if (node->lhs->type == NodeType::ARRAY_ACCESS) {
        ArrayAccessNode* array_access = static_cast<ArrayAccessNode*>(node->lhs);
        analyzeArrayAccess(array_access, true);  
    }
    else {
        addError("Nieprawidłowa lewa strona przypisania", node->line);
    }
    
    if (node->rhs) {
        analyzeExpression(node->rhs, false);  
    }
}

// Analiza READ
void SemanticAnalyzer::analyzeRead(ReadNode* node) {
    if (!node || !node->target) return;

    if (node->target->type == NodeType::VARIABLE) {
        VariableNode* var = static_cast<VariableNode*>(node->target);
        const SymbolInfo* info = findVariable(var->name);

        if (info && info->is_array) {
            addError("Nie można wczytać całej tablicy bez indeksu: '" + var->name + "'", var->line);
        } else {
            checkLValue(node->target);
        }
    } else if (node->target->type == NodeType::ARRAY_ACCESS) {
        analyzeArrayAccess(static_cast<ArrayAccessNode*>(node->target), true);
    }
}

// Analiza WRITE
void SemanticAnalyzer::analyzeWrite(WriteNode* node) {
    if (!node || !node->value) return;

    if (node->value->type == NodeType::VARIABLE) {
        VariableNode* var = static_cast<VariableNode*>(node->value);
        const SymbolInfo* info = findVariable(var->name);

        if (info && info->is_array) {
            addError("Nie można wypisać całej tablicy bez indeksu: '" + var->name + "'", var->line);
        } else {
            analyzeExpression(node->value, false);
        }
    } else if (node->value->type == NodeType::ARRAY_ACCESS) {
        analyzeArrayAccess(static_cast<ArrayAccessNode*>(node->value), false);
    } else {
        analyzeExpression(node->value, false);
    }
}

// Analiza zmiennej
void SemanticAnalyzer::analyzeVariable(VariableNode* node, bool is_lvalue) {
    if (!node) return;
    
    if (is_lvalue) {
        checkLValue(node);
    } else {
        checkRValue(node);
    }
}

// Analiza liczby (zawsze poprawna)
void SemanticAnalyzer::analyzeNumber(NumberNode* node) {
    if (!node) {
        addError("Pusty węzeł liczby", 0);
    }
}
void SemanticAnalyzer::analyzeBinaryOp(BinaryOpNode* node) {
    if (!node || !node->left || !node->right) return;

    checkRValue(node->left);

    checkRValue(node->right);
}

void SemanticAnalyzer::analyzeArrayDecl(ArrayDeclNode* node) {
    if (!node) return;
    
    if (node->start_index > node->end_index) {
        addError("Nieprawidłowy zakres tablicy " + node->name + 
                 ": " + std::to_string(node->start_index) + 
                 ":" + std::to_string(node->end_index), node->line);
        return;
    }
    
    try {
        symbol_table.addArray(node->name, node->start_index, 
                             node->end_index, node->line);
    } catch (const SemanticError& e) {
        addError(e.what(), e.line);
    }
}

void SemanticAnalyzer::analyzeArrayAccess(ArrayAccessNode* node, bool is_lvalue) {
    if (!node) return;
    
    const SymbolInfo* info = nullptr;
    
    if (!current_procedure.empty()) {
        std::string full_name = current_procedure + "." + node->array_name;
        info = symbol_table.get(full_name);
    }
    
    if (!info) {
        info = symbol_table.get(node->array_name);
    }
    
    if (!info) {
        addError("Użycie niezadeklarowanej tablicy: '" + node->array_name + "'", node->line);
        return;
    }
    if (info->is_array && !info->is_parameter) {
        if (node->index->type == NodeType::NUMBER) {
            NumberNode* num = static_cast<NumberNode*>(node->index);
            try {
                long long idx = std::stoll(num->value);
                symbol_table.checkArrayIndex(node->array_name, idx, node->line);
            } catch (...) {
                addError("Nieprawidłowy indeks tablicy", num->line);
            }
        }
        else {
            addWarning("Dynamiczny indeks tablicy", node->line);
        }
    }

    if (is_lvalue) {
        if (info->isConstant()) {
            addError("Nie można modyfikować stałej tablicy: '" + 
                     node->array_name + "'", node->line);
        }
    }
}
// Sprawdź warunek
void SemanticAnalyzer::analyzeCondition(ConditionNode* node) {
    if (!node) return;
    
    if (node->left) {
        analyzeExpression(node->left, false);  
    } else {
        addError("Brak lewego operandu w warunku", node->line);
    }
    
    if (node->right) {
        analyzeExpression(node->right, false);  
    } else {
        addError("Brak prawego operandu w warunku", node->line);
    }
    
    if (node->op != "=" && node->op != "!=" && 
        node->op != ">" && node->op != "<" && 
        node->op != ">=" && node->op != "<=") {
        addError("Nieznany operator porównania: '" + node->op + "'", node->line);
    }
}

// Wypisz ostrzeżenia
void SemanticAnalyzer::printWarnings() const {
    if (warnings.empty()) {
        return;
    }
    std::cout << "\n=== WARNINGI ===" << std::endl;
    for (const auto& warning : warnings) {
        std::cout << warning << std::endl;
    }
    std::cout << "==============================\n" << std::endl;
}

// Wypisz błędy
void SemanticAnalyzer::printErrors() const {
    if (errors.empty()) {
        std::cout << "Brak błędów semantycznych!" << std::endl;
        return;
    }
    
    std::cout << "\n=== BŁĘDY ===" << std::endl;
    for (const auto& error : errors) {
        std::cout << error << std::endl;
    }
    std::cout << "=========================\n" << std::endl;
}