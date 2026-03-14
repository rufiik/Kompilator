#include "codegen.hpp"
#include <sstream>
#include <stdexcept>
#include <algorithm>

CodeGenerator::CodeGenerator(SymbolTable& symtab) 
    : symbol_table(symtab) {}

void CodeGenerator::addInstruction(const std::string& instr) {
    code.push_back(instr);
}
// Generowanie kodu z drzewa AST
void CodeGenerator::generate(ASTNode* root) {
    if (!root) {
        throw std::runtime_error("Puste drzewo AST");
    }
    
    code.clear();
    switch (root->type) {
        case NodeType::PROGRAM_ALL:
            generateProgramAll(static_cast<ProgramAllNode*>(root));
            break;
        case NodeType::PROGRAM:
            generateProgram(static_cast<ProgramNode*>(root));
            break;
        default:
            throw std::runtime_error("Nieznany typ węzła AST: " + std::to_string(static_cast<int>(root->type)));
    }
    
    // HALT na końcu
    addInstruction("HALT");
}

void CodeGenerator::generateProgramAll(ProgramAllNode* node) {
    if (!node) return;

    int main_jump_index = -1;
    if (!node->procedures.empty()) {
        main_jump_index = code.size();
        addInstruction("JUMP 0");
    }

    for (auto proc : node->procedures) {
        generateProcedure(proc);
    }

    int main_start_index = code.size();

    if (node->main_program) {
        generateProgram(node->main_program);
    } else {
        throw std::runtime_error("Brak głównego programu");
    }

    if (main_jump_index != -1) {
        code[main_jump_index] = "JUMP " + std::to_string(main_start_index);
    }
}

void CodeGenerator::generateMultiply(ASTNode* left, ASTNode* right) {
    // left -> f, right -> g
    generateValue(left, "f");
    generateValue(right, "g");
    // h = wynik (0)
    addInstruction("RST h");
    int start_loop = code.size();
    // a = g (right)
    addInstruction("RST a");
    addInstruction("ADD g");
    // jeśli a == 0, koniec
    int jump_end = code.size();
    addInstruction("JZERO 0");
    // Sprawdź parzystość: a = g % 2
    addInstruction("SHR a");
    addInstruction("SHL a");
    addInstruction("SWP c"); // c = (g/2)*2
    addInstruction("RST a");
    addInstruction("ADD g");
    addInstruction("SUB c"); // a = g - (g/2)*2
    int jump_even = code.size();
    addInstruction("JZERO 0");
    // Jeśli nieparzyste: h += f
    addInstruction("SWP h");
    addInstruction("ADD f");
    addInstruction("SWP h");
    int skip_add = code.size();
    patchJump(jump_even, skip_add);
    // f *= 2
    addInstruction("SHL f");
    // g /= 2
    addInstruction("SHR g");
    // Skok do początku pętli
    addInstruction("JUMP " + std::to_string(start_loop));
    int end_loop = code.size();
    patchJump(jump_end, end_loop);
    // wynik do a
    addInstruction("SWP h");
}

void CodeGenerator::generateDivide(ASTNode* left, ASTNode* right, bool return_quotient) {
    // left -> f (dzielna), right -> g (dzielnik)
    generateValue(left, "f");
    generateValue(right, "g");
    // h = iloraz = 0 (quotient)
    addInstruction("RST h");
    addInstruction("RST a");
    addInstruction("ADD g");
    // Sprawdź czy dzielnik == 0 -> zwróć 0
    int jump_div_zero = code.size();
    addInstruction("JZERO 0");
    // c = 1 (power)
    addInstruction("RST c");
    addInstruction("INC c");

    //Znalezienie największej wielokrotności dzielnika 
    int find_max_loop = code.size();
    addInstruction("RST a");
    addInstruction("ADD g");             
    addInstruction("SHL a");              
    addInstruction("SUB f");             
    int jump_end_find = code.size();
     // jeśli multiple*2 > dividend, koniec szukania
    addInstruction("JPOS 0");        
    // multiple *= 2
    addInstruction("SHL g");
    // power *= 2
    addInstruction("SHL c");
    addInstruction("JUMP " + std::to_string(find_max_loop));
    int end_find = code.size();
    patchJump(jump_end_find, end_find);

    // Odejmowanie od największej wielokrotności
    int subtract_loop = code.size();
    // Sprawdź czy power == 0 -> koniec
    addInstruction("RST a");
    addInstruction("ADD c");             
    int jump_end_subtract = code.size();
    addInstruction("JZERO 0");
    // Sprawdź czy multiple <= dividend
    addInstruction("RST a");
    addInstruction("ADD g");              // a = multiple
    addInstruction("SUB f");             // a = multiple - dividend
    int jump_skip_sub = code.size();
    addInstruction("JPOS 0");         // jeśli multiple > dividend, pomiń
    // dividend -= multiple
    addInstruction("SWP f");
    addInstruction("SUB g");
    addInstruction("SWP f");
    // quotient += power
    addInstruction("SWP h");
    addInstruction("ADD c");
    addInstruction("SWP h");
    int skip_sub_label = code.size();
    patchJump(jump_skip_sub, skip_sub_label);
    // multiple /= 2
    addInstruction("SHR g");
    // power /= 2
    addInstruction("SHR c");
    addInstruction("JUMP " + std::to_string(subtract_loop));
    int end_subtract = code.size();
    patchJump(jump_end_subtract, end_subtract);
    // Załaduj wynik
    if (return_quotient) {
        addInstruction("SWP h"); // iloraz
    } else {
        addInstruction("SWP f"); // reszta
    }
    // Skok na koniec (pomiń obsługę dzielenia przez 0)
    int jump_finish = code.size();
    addInstruction("JUMP 0");
    // div_by_zero: zwróć 0
    int div_zero_label = code.size();
    patchJump(jump_div_zero, div_zero_label);
    addInstruction("RST a");
    // finish:
    int finish_label = code.size();
    patchJump(jump_finish, finish_label);
}



// Generowanie kodu dla głównego programu
void CodeGenerator::generateProgram(ProgramNode* node) {
    if (!node) return;
    // Przydziel adresy zmiennym i tablicom
    if (node->declarations) {
        DeclarationsNode* decls = static_cast<DeclarationsNode*>(node->declarations);
        for (auto decl : decls->declarations) {
            if (!decl) continue;
            if (decl->type == NodeType::DECLARATION) {
                VariableDeclNode* var_decl = static_cast<VariableDeclNode*>(decl);
                getVarAddress(var_decl->name);
            }
            if (decl->type == NodeType::ARRAY_DECL) {
                ArrayDeclNode* arr_decl = static_cast<ArrayDeclNode*>(decl);
                getVarAddress(arr_decl->name); 
            }
        }
    }
    // Generuj kod
    if (node->commands) {
        generateBlock(node->commands);
    }
}
// Generowanie kodu dla procedury
void CodeGenerator::generateProcedure(ProcedureNode* node) {
    if (!node) return;
    current_procedure = node->header->name;
    procedure_addresses[node->header->name] = code.size();
    uint64_t return_addr = symbol_table.getProcedureAddress(node->header->name);
    // Zapisz adres powrotu
    addInstruction("STORE " + std::to_string(return_addr));

    if (node->body) {
        generateBlock(node->body);
    }

    // Załaduj adres powrotu i wróć
    addInstruction("LOAD " + std::to_string(return_addr));
    addInstruction("RTRN");
    current_procedure = "";
}

void CodeGenerator::generateProcedureCall(ProcedureCallNode* node) {
    if (!node) return;
    const SymbolInfo* proc_info = symbol_table.get(node->proc_name);
    if (!proc_info || !proc_info->is_procedure) {
        throw std::runtime_error("Nieznana procedura: " + node->proc_name);
    }
    const std::vector<std::string>& param_names = proc_info->param_names;
    for (size_t i = 0; i < node->args->args.size(); ++i) {
        ASTNode* arg = node->args->args[i];
        std::string param_name = param_names[i];
        // Adres parametru WYWOŁYWANEJ procedury
        uint64_t param_addr = symbol_table.getParameterAddress(node->proc_name, param_name);
        if (arg->type == NodeType::VARIABLE) {
            VariableNode* var = static_cast<VariableNode*>(arg);
            // Sprawdź czy argument jest parametrem BIEŻĄCEJ procedury
            const SymbolInfo* arg_info = symbol_table.get(current_procedure + "." + var->name);
            if (arg_info && arg_info->is_parameter) {
                // Argument jest PARAMETREM bieżącej procedury - przekaż jego zawartość
                uint64_t arg_param_addr = symbol_table.getParameterAddress(current_procedure, var->name);
                addInstruction("LOAD " + std::to_string(arg_param_addr));
                addInstruction("STORE " + std::to_string(param_addr));
            } else {
                arg_info = symbol_table.get(var->name);
                if (arg_info && arg_info->is_array) {
                    // Argument to TABLICA
                    uint64_t base_addr = symbol_table.getArrayBaseAddress(var->name);
                    uint64_t start_idx = arg_info->array_start;
                    uint64_t effective_base = base_addr - start_idx;
                    loadConstant(effective_base, "a");
                    addInstruction("STORE " + std::to_string(param_addr));
                } else {
                    // Argument to ZWYKŁA ZMIENNA
                    uint64_t var_addr = getVarAddress(var->name);
                    loadConstant(var_addr, "a");
                    addInstruction("STORE " + std::to_string(param_addr));
                }
            }
        }
        else if (arg->type == NodeType::ARRAY_ACCESS) {
            ArrayAccessNode* arr = static_cast<ArrayAccessNode*>(arg);
            generateArrayAccess(arr, "a", false);
            addInstruction("STORE " + std::to_string(param_addr));
        }
        else {
            throw std::runtime_error("Nieprawidłowy argument procedury");
        }
    }

    uint64_t proc_addr = procedure_addresses.at(node->proc_name);
    addInstruction("CALL " + std::to_string(proc_addr));
}

// Generowanie kodu dla bloku instrukcji
void CodeGenerator::generateBlock(BlockNode* node) {
    if (!node) return;
    
    for (auto stmt : node->statements) {
        if (!stmt) continue;
        
        switch (stmt->type) {
            case NodeType::ASSIGNMENT:
                generateAssignment(static_cast<AssignmentNode*>(stmt));
                break;
            case NodeType::READ:
                generateRead(static_cast<ReadNode*>(stmt));
                break;
            case NodeType::WRITE:
                generateWrite(static_cast<WriteNode*>(stmt));
                break;
            case NodeType::BLOCK:
                generateBlock(static_cast<BlockNode*>(stmt));
                break;
            case NodeType::PROCEDURE_CALL:
                generateProcedureCall(static_cast<ProcedureCallNode*>(stmt));
                break;
            case NodeType::IF_ELSE:
                generateIf(static_cast<IfNode*>(stmt));
                break;
            case NodeType::WHILE:
                generateWhile(static_cast<WhileNode*>(stmt));
                break;
            case NodeType::REPEAT:
                generateRepeat(static_cast<RepeatNode*>(stmt));
                break;
            case NodeType::FOR_TO:
            case NodeType::FOR_DOWNTO:
                generateFor(static_cast<ForNode*>(stmt));
                break;
            default:
                throw std::runtime_error("Nieobsługiwany typ instrukcji: " + std::to_string(static_cast<int>(stmt->type)));
        }
    }
}
// Wczytywanie stałej do rejestru
void CodeGenerator::loadConstant(uint64_t value, const std::string& reg) {
    addInstruction("RST " + reg);
    if (value == 0) return;

    // Limit dla optymalizacji "potęga 2 - diff"
    const uint64_t DIFF_OPT_LIMIT = 1 << 20;

    if (value < DIFF_OPT_LIMIT) {
        // potęga 2 - diff (jeśli korzystne)
        uint64_t next_power = 1;
        int shifts_needed = 0;
        while (next_power <= value) {
            next_power <<= 1;
            shifts_needed++;
        }
        uint64_t diff = next_power - value;

        uint64_t standard_cost = 2; // RST + INC
        uint64_t temp_val = value;
        int highest = 0;
        while (temp_val > 1) { temp_val >>= 1; highest++; }
        standard_cost += highest; // SHL-e
        standard_cost += __builtin_popcountll(value) - 1; // INC-e (oprócz pierwszego bitu)

        // Koszt metody (2^n - diff)
        uint64_t subtract_cost = 2 + shifts_needed + diff;
        if (subtract_cost < standard_cost) {
            // Użyj 2^n - diff
            addInstruction("INC " + reg);
            for (int i = 0; i < shifts_needed; i++) {
                addInstruction("SHL " + reg);
            }
            for (uint64_t i = 0; i < diff; i++) {
                addInstruction("DEC " + reg);
            }
            return;
        }
    }

    // Standardowa metoda binarna (najlepsza dla dużych liczb)
    int highest_bit = 0;
    uint64_t temp_val = value;
    while (temp_val > 1) {
        temp_val >>= 1;
        highest_bit++;
    }
    addInstruction("INC " + reg);
    for (int bit = highest_bit - 1; bit >= 0; bit--) {
        addInstruction("SHL " + reg);
        if ((value >> bit) & 1) {
            addInstruction("INC " + reg);
        }
    }
}
// Generowanie kodu dla wartości wyrażenia
void CodeGenerator::generateValue(ASTNode* node, const std::string& target_reg) {
    if (!node) return;

    if (node->type == NodeType::NUMBER) {
        NumberNode* num = static_cast<NumberNode*>(node);
        uint64_t value = std::stoull(num->value);  
        loadConstant(value, "a");
    }
    else if (node->type == NodeType::VARIABLE) {
        VariableNode* var = static_cast<VariableNode*>(node);
        bool found = false;
        
        if (!current_procedure.empty()) {
            // Sprawdź czy to parametr
            std::string param_name = current_procedure + "." + var->name;
            const SymbolInfo* info = symbol_table.get(param_name);
            if (info && info->is_parameter) {
                found = true;
                uint64_t param_addr = symbol_table.getParameterAddress(current_procedure, var->name);
                addInstruction("LOAD " + std::to_string(param_addr));
                addInstruction("RLOAD a");
            }
            // Sprawdź czy to zmienna lokalna procedury
            else if (info && !info->is_parameter) {
                found = true;
                uint64_t address = symbol_table.getVariableAddress(param_name);
                addInstruction("LOAD " + std::to_string(address));
            }
        }
        if (!found) {
            // Zmienna globalna
            const SymbolInfo* info = symbol_table.get(var->name);
            if (!info) {
                throw std::runtime_error("Nieznana zmienna: '" + var->name + "'");
            }
            uint64_t address = symbol_table.getVariableAddress(var->name);
            addInstruction("LOAD " + std::to_string(address));
        }
    }
    else if (node->type == NodeType::BIN_OP) {
        generateBinaryOp(static_cast<BinaryOpNode*>(node), "a");
    }
    else if (node->type == NodeType::ARRAY_ACCESS) {
        ArrayAccessNode* array_access = static_cast<ArrayAccessNode*>(node);
        generateArrayAccess(array_access, target_reg);
        return;
    }

    if (target_reg != "a") {
        addInstruction("SWP " + target_reg);
    }
}
// Generowanie kodu dla przypisania
void CodeGenerator::generateAssignment(AssignmentNode* node) {
    if (!node || !node->lhs || !node->rhs) return;

    if (node->lhs->type == NodeType::VARIABLE) {
        VariableNode* var = static_cast<VariableNode*>(node->lhs);
        generateValue(node->rhs, "a");
        
        if (!current_procedure.empty()) {
            std::string full_name = current_procedure + "." + var->name;
            const SymbolInfo* info = symbol_table.get(full_name);
            
            if (info && info->is_parameter) {
                // PARAMETR – zapis przez referencję
                uint64_t param_addr = symbol_table.getParameterAddress(current_procedure, var->name);
                addInstruction("SWP b");
                addInstruction("LOAD " + std::to_string(param_addr));
                addInstruction("SWP b");
                addInstruction("RSTORE b");
                return;
            }
            else if (info && !info->is_parameter) {
                // ZMIENNA LOKALNA procedury
                uint64_t address = symbol_table.getVariableAddress(full_name);
                addInstruction("STORE " + std::to_string(address));
                return;
            }
        }
        // ZMIENNA GLOBALNA
        uint64_t address = getVarAddress(var->name);
        addInstruction("STORE " + std::to_string(address));
    }
    else if (node->lhs->type == NodeType::ARRAY_ACCESS) {
        ArrayAccessNode* array_access = static_cast<ArrayAccessNode*>(node->lhs);
        generateValue(node->rhs, "a");
        addInstruction("SWP b");
        generateArrayStore(array_access, "b");
    }
}

// Generowanie kodu dla READ
void CodeGenerator::generateRead(ReadNode* node) {
    if (!node || !node->target) return;

    if (node->target->type == NodeType::VARIABLE) {
        VariableNode* var = static_cast<VariableNode*>(node->target);
        
        if (!current_procedure.empty()) {
            std::string full_name = current_procedure + "." + var->name;
            const SymbolInfo* info = symbol_table.get(full_name);
            
            if (info && info->is_parameter) {
                // PARAMETR – zapis przez referencję
                uint64_t param_addr = symbol_table.getParameterAddress(current_procedure, var->name);
                addInstruction("READ");
                addInstruction("SWP b");
                addInstruction("LOAD " + std::to_string(param_addr));
                addInstruction("SWP b");
                addInstruction("RSTORE b");
                return;
            }
            else if (info && !info->is_parameter) {
                // ZMIENNA LOKALNA procedury
                uint64_t address = symbol_table.getVariableAddress(full_name);
                addInstruction("READ");
                addInstruction("STORE " + std::to_string(address));
                return;
            }
        }
        // ZMIENNA GLOBALNA
        uint64_t address = getVarAddress(var->name);
        addInstruction("READ");
        addInstruction("STORE " + std::to_string(address));
    } else if (node->target->type == NodeType::ARRAY_ACCESS) {
        ArrayAccessNode* array_access = static_cast<ArrayAccessNode*>(node->target);
        addInstruction("READ");
        generateArrayStore(array_access, "a");
    }
}
// Generowanie kodu dla WRITE
void CodeGenerator::generateWrite(WriteNode* node) {
    if (!node || !node->value) return;
    
    generateValue(node->value, "a");
    addInstruction("WRITE");
}
// Generowanie kodu dla dostępu do tablicy
void CodeGenerator::generateArrayAccess(ArrayAccessNode* node, const std::string& target_reg, bool load_value) {
    if (!node) return;

    // Sprawdź czy tablica jest parametrem w aktualnej procedurze
    const SymbolInfo* info = symbol_table.get(current_procedure + "." + node->array_name);
    
    if (info && info->is_parameter) {
        // Tablica jest PARAMETREM (T)
        // Parametr zawiera "effective base" = base_addr - start_idx
        // Więc adres elementu = param + index
        uint64_t param_addr = symbol_table.getParameterAddress(current_procedure, node->array_name);
        // Załaduj effective base z parametru
        addInstruction("LOAD " + std::to_string(param_addr));
        addInstruction("SWP c"); // c = effective base
        // Załaduj wartość indeksu
        generateValue(node->index, "a"); // a = indeks
        // Oblicz adres elementu: effective_base + index
        addInstruction("ADD c"); // a = adres elementu
        if (load_value) {
            addInstruction("SWP d");
            addInstruction("RLOAD d");
        }
    } else {
        // Tablica LOKALNA - sprawdź w globalnym scope
        info = symbol_table.get(node->array_name);
        if (!info) {
            throw std::runtime_error("Nieznana tablica: '" + node->array_name + "'");
        }
        
        uint64_t base_addr = symbol_table.getArrayBaseAddress(node->array_name);
        uint64_t start_idx = info->array_start;

        if (node->index->type == NodeType::NUMBER) {
            NumberNode* num = static_cast<NumberNode*>(node->index);
            uint64_t idx = std::stoull(num->value);  
            uint64_t offset = idx - start_idx;
            uint64_t element_addr = base_addr + offset;
            
            if (load_value) {
                addInstruction("LOAD " + std::to_string(element_addr));
            } else {
                loadConstant(element_addr, "a");
            }
        } else {
            generateValue(node->index, "a");
            loadConstant(start_idx, "c");
            addInstruction("SUB c");
            loadConstant(base_addr, "c");
            addInstruction("ADD c");
            
            if (load_value) {
                addInstruction("SWP d");
                addInstruction("RLOAD d");
            }
        }
    }

    if (target_reg != "a") {
        addInstruction("SWP " + target_reg);
    }
}
// Generowanie kodu dla zapisu do tablicy
void CodeGenerator::generateArrayStore(ArrayAccessNode* node, const std::string& value_reg) {
    if (!node) return;

    // Sprawdź czy tablica jest parametrem w aktualnej procedurze
    const SymbolInfo* info = symbol_table.get(current_procedure + "." + node->array_name);

    if (info && info->is_parameter) {
        // Tablica jest PARAMETREM (T)
        uint64_t param_addr = symbol_table.getParameterAddress(current_procedure, node->array_name);
        
        // Zapisz wartość do e (żeby nie stracić)
        if (value_reg != "e") {
            if (value_reg == "a") {
                addInstruction("SWP e");
            } else {
                addInstruction("SWP " + value_reg);
                addInstruction("SWP e");
            }
        }
        // e = wartość do zapisania
        
        // Załaduj effective base z parametru
        addInstruction("LOAD " + std::to_string(param_addr));
        addInstruction("SWP c"); // c = effective base
        
        // Załaduj wartość indeksu
        generateValue(node->index, "a"); // a = indeks
        
        // Oblicz adres elementu: effective_base + index
        addInstruction("ADD c"); // a = adres elementu
        addInstruction("SWP d"); // d = adres
        
        // Przywróć wartość i zapisz
        addInstruction("SWP e"); // a = wartość
        addInstruction("RSTORE d"); // zapisz wartość pod adres w d
        
    } else {
        // Tablica LOKALNA
        info = symbol_table.get(node->array_name);
        if (!info) {
            throw std::runtime_error("Nieznana tablica: '" + node->array_name + "'");
        }
        
        uint64_t base_addr = symbol_table.getArrayBaseAddress(node->array_name);
        uint64_t start_idx = info->array_start;

        if (node->index->type == NodeType::NUMBER) {
            NumberNode* num = static_cast<NumberNode*>(node->index);
            uint64_t idx = std::stoull(num->value); 
            uint64_t offset = idx - start_idx;
            uint64_t element_addr = base_addr + offset;
        
            if (value_reg != "a") {
                addInstruction("SWP " + value_reg);
            }
            addInstruction("STORE " + std::to_string(element_addr));
        } else {
            // Zapisz wartość do e
            if (value_reg != "e") {
                if (value_reg == "a") {
                    addInstruction("SWP e");
                } else {
                    addInstruction("SWP " + value_reg);
                    addInstruction("SWP e");
                }
            }
            
            generateValue(node->index, "a");
            loadConstant(start_idx, "c");
            addInstruction("SUB c");
            loadConstant(base_addr, "c");
            addInstruction("ADD c");
            addInstruction("SWP d");
            addInstruction("SWP e");
            addInstruction("RSTORE d");
        }
    }
}
// Generowanie kodu dla operacji binarnych
void CodeGenerator::generateBinaryOp(BinaryOpNode* node, const std::string& target_reg) {
    if (!node || !node->left || !node->right) return;
    
    switch(node->op) {
        case BinaryOpNode::OpType::ADD: {
            generateValue(node->left, "b");
            generateValue(node->right, "a");
            addInstruction("ADD b");
            if (target_reg != "a") {
                addInstruction("SWP " + target_reg);
            }
            break;
        }
        
        case BinaryOpNode::OpType::SUB: {
            generateValue(node->left, "b");
            generateValue(node->right, "a");
            addInstruction("SWP b");
            addInstruction("SUB b");
            if (target_reg != "a") {
                addInstruction("SWP " + target_reg);
            }
            break;
        }
        
        case BinaryOpNode::OpType::MUL: {
            // Optymalizacja: mnożenie przez potęgę 2
            auto is_pow2 = [](const std::string& val, int& shift) {
                uint64_t v = std::stoull(val);
                if (v == 0) return false;
                if ((v & (v - 1)) == 0) {
                    shift = 0;
                    while (v > 1) { v >>= 1; ++shift; }
                    return true;
                }
                return false;
            };
            int shift = 0;
            if (node->left->type == NodeType::NUMBER && is_pow2(static_cast<NumberNode*>(node->left)->value, shift)) {
                generateValue(node->right, "a");
                for (int i = 0; i < shift; ++i) addInstruction("SHL a");
                if (target_reg != "a") addInstruction("SWP " + target_reg);
                break;
            }
            if (node->right->type == NodeType::NUMBER && is_pow2(static_cast<NumberNode*>(node->right)->value, shift)) {
                generateValue(node->left, "a");
                for (int i = 0; i < shift; ++i) addInstruction("SHL a");
                if (target_reg != "a") addInstruction("SWP " + target_reg);
                break;
            }
            generateMultiply(node->left, node->right);
            if (target_reg != "a") addInstruction("SWP " + target_reg);
            break;
        }
        case BinaryOpNode::OpType::DIV: {
            // Optymalizacja: dzielenie przez potęgę 2
            auto is_pow2 = [](const std::string& val, int& shift) {
                uint64_t v = std::stoull(val);
                if (v == 0) return false;
                if ((v & (v - 1)) == 0) {
                    shift = 0;
                    while (v > 1) { v >>= 1; ++shift; }
                    return true;
                }
                return false;
            };
            int shift = 0;
            if (node->right->type == NodeType::NUMBER && is_pow2(static_cast<NumberNode*>(node->right)->value, shift)) {
                generateValue(node->left, "a");
                for (int i = 0; i < shift; ++i) addInstruction("SHR a");
                if (target_reg != "a") addInstruction("SWP " + target_reg);
                break;
            }
            generateDivide(node->left, node->right, true);
            if (target_reg != "a") addInstruction("SWP " + target_reg);
            break;
        }
        
        case BinaryOpNode::OpType::MOD: {
            generateDivide(node->left, node->right, false);
            if (target_reg != "a") {
                addInstruction("SWP " + target_reg);
            }
            break;
        }
        
        default:
            throw std::runtime_error("Nieobsługiwany operator: " + std::to_string(static_cast<int>(node->op)));
    }
}
// Poprawianie adresu skoku
void CodeGenerator::patchJump(int jump_index, uint64_t target_address) {
    // Zamień instrukcję JUMP/JZERO/JPOS na właściwy adres
    std::string& instr = code[jump_index];
    size_t space_pos = instr.find(' ');
    if (space_pos != std::string::npos) {
        std::string opcode = instr.substr(0, space_pos);
        instr = opcode + " " + std::to_string(target_address);
    }
}
// Generowanie kodu dla warunku
long long CodeGenerator::generateCondition(ConditionNode* node) {
    if (!node) return -1;
    // Dla a = b:  jeśli a-b != 0 LUB b-a != 0, to fałsz
    // Dla a != b: jeśli a-b == 0 I b-a == 0, to fałsz
    // Dla a > b:  jeśli a-b <= 0 (czyli a-b == 0), to fałsz 
    // Dla a < b:  jeśli b-a <= 0, to fałsz
    // Dla a >= b: jeśli a < b, to fałsz
    // Dla a <= b: jeśli a > b, to fałsz
    
    if (node->op == "=") {
        generateValue(node->left, "b");          
        generateValue(node->right, "c");  
        addInstruction("RST a");
        addInstruction("ADD b");
        addInstruction("SUB c"); 
        addInstruction("SWP d");   
        
        addInstruction("RST a");
        addInstruction("ADD c");
        addInstruction("SUB b");         
        addInstruction("ADD d");          
        // Jeśli suma > 0, różne
        int jump_false = code.size();
        addInstruction("JPOS 0");
        return jump_false;
    } else if (node->op == "!=") {
        generateValue(node->right, "b");
        generateValue(node->left, "c");
        addInstruction("RST a");
        addInstruction("ADD b");
        addInstruction("SUB c");
        // Jeśli a > 0, warunek prawdziwy 
        int jump_true = code.size();
        addInstruction("JPOS 0");  
        addInstruction("RST a");
        addInstruction("ADD c");
        addInstruction("SUB b");
        // Jeśli a == 0, warunek fałszywy
        int jump_false = code.size();
        addInstruction("JZERO 0");  
        

        int body_addr = code.size();
        patchJump(jump_true, body_addr);
        
        return jump_false;
        
    } else if (node->op == ">") {
        generateValue(node->right, "b");
        generateValue(node->left, "a");
        addInstruction("SUB b");  
        // Jeśli a <= 0, to fałsz
        int jump_false = code.size();
        addInstruction("JZERO 0");
        
        return jump_false;
        
    } else if (node->op == "<") {
        generateValue(node->left, "b");
        generateValue(node->right, "a");
        addInstruction("SUB b");
        // Jeśli a <= 0, to fałsz
        int jump_false = code.size();
        addInstruction("JZERO 0");
        return jump_false;
        
    } else if (node->op == ">=") {
        generateValue(node->left, "b");
        generateValue(node->right, "a");
        addInstruction("SUB b");  
        // Jeśli a > 0, to fałsz
        int jump_false = code.size();
        addInstruction("JPOS 0");  
        
        return jump_false;
        
    } else if (node->op == "<=") {
        generateValue(node->right, "b");
        generateValue(node->left, "a");
        addInstruction("SUB b"); 
        // jeśli a > 0, to fałsz
        int jump_false = code.size();
        addInstruction("JPOS 0");  
        
        return jump_false;
    }
    
    return -1;
}

// Generowanie kodu dla IF-ELSE
void CodeGenerator::generateIf(IfNode* node) {
    if (!node || !node->condition) return;
    
    ConditionNode* cond = static_cast<ConditionNode*>(node->condition);
    
    if (node->else_block) { 
        int jump_to_else = generateCondition(cond);
        // Generuj blok THEN
        generateBlock(static_cast<BlockNode*>(node->then_block));
        
        // Skok na koniec (pomiń ELSE)
        int jump_to_end = code.size();
        addInstruction("JUMP 0");
        
        // Etykieta ELSE
        int else_label = code.size();
        patchJump(jump_to_else, else_label);
        
        // Generuj blok ELSE
        generateBlock(static_cast<BlockNode*>(node->else_block));
        
        // Etykieta END
        int end_label = code.size();
        patchJump(jump_to_end, end_label);
        
    } else {
        int jump_to_end = generateCondition(cond);
        
        // Generuj blok THEN
        generateBlock(static_cast<BlockNode*>(node->then_block));
        
        // Etykieta END
        int end_label = code.size();
        patchJump(jump_to_end, end_label);
    }
}
// Generowanie kodu dla WHILE
void CodeGenerator::generateWhile(WhileNode* node) {
    if (!node || !node->condition) return;
    
    ConditionNode* cond = static_cast<ConditionNode*>(node->condition);
    int start_label = code.size();
    int jump_to_end = generateCondition(cond);
    
    // Generuj ciało pętli
    generateBlock(static_cast<BlockNode*>(node->body));
    
    // Skok powrotny do początku
    addInstruction("JUMP " + std::to_string(start_label));
    
    // Etykieta końca
    int end_label = code.size();
    patchJump(jump_to_end, end_label);
}

// Generowanie kodu dla REPEAT
void CodeGenerator::generateRepeat(RepeatNode* node) {
    if (!node || !node->condition) return;
    
    ConditionNode* cond = static_cast<ConditionNode*>(node->condition);
    int start_label = code.size();
    
    // Generuj ciało pętli
    generateBlock(static_cast<BlockNode*>(node->body));
    
    // generateCondition zwraca indeks skoku gdy warunek FAŁSZYWY
    int jump_if_false = generateCondition(cond);
    
    // Patch: jeśli fałsz, skocz do start
    patchJump(jump_if_false, start_label);
}
// Generowanie kodu dla FOR
void CodeGenerator::generateFor(ForNode* node) {
    if (!node) return;
    
    int iter_addr = symbol_table.allocateTemp();
    int counter_addr = symbol_table.allocateTemp();
    
    uint64_t old_addr = 0;
    bool has_global_var = false;
    try {
        old_addr = symbol_table.getVariableAddress(node->iterator);
        has_global_var = true;
    } catch (...) {}
    
    int from_addr = symbol_table.allocateTemp();
    int to_addr = symbol_table.allocateTemp();
    
    generateValue(node->from, "a");
    addInstruction("STORE " + std::to_string(from_addr));
    addInstruction("STORE " + std::to_string(iter_addr));  // iterator = from
    
    generateValue(node->to, "a");
    addInstruction("STORE " + std::to_string(to_addr));
    

    if (node->descending) {
        // DOWNTO: wykonuj tylko jeśli from >= to
        addInstruction("LOAD " + std::to_string(to_addr));
        addInstruction("SWP b");
        addInstruction("LOAD " + std::to_string(from_addr));
        addInstruction("SUB b");  // a = from - to
        int jump_valid = code.size();
        addInstruction("JPOS 0");  // jeśli from > to, pętla OK
        
        // Sprawdź from == to
        addInstruction("LOAD " + std::to_string(from_addr));
        addInstruction("SWP b");
        addInstruction("LOAD " + std::to_string(to_addr));
        addInstruction("SUB b");  // a = to - from
        int jump_also_valid = code.size();
        addInstruction("JZERO 0");  // jeśli to == from, też OK (1 iteracja)
        
        // from < to - counter = 0, skocz na koniec
        addInstruction("RST a");
        addInstruction("STORE " + std::to_string(counter_addr));
        int jump_to_end_invalid = code.size();
        addInstruction("JUMP 0");
        
        // Pętla jest valid
        int valid_label = code.size();
        patchJump(jump_valid, valid_label);
        patchJump(jump_also_valid, valid_label);
        
        // counter = from - to + 1
        addInstruction("LOAD " + std::to_string(from_addr));
        addInstruction("SWP b");
        addInstruction("LOAD " + std::to_string(to_addr));
        addInstruction("SWP b");
        addInstruction("SUB b");  // a = from - to
        addInstruction("INC a");
        addInstruction("STORE " + std::to_string(counter_addr));
        
        // 3. PĘTLA
        int start_label = code.size();
        
        addInstruction("LOAD " + std::to_string(counter_addr));
        int jump_to_end = code.size();
        addInstruction("JZERO 0");
        
        // Ciało pętli
        symbol_table.addTempVariable(node->iterator, iter_addr);
        generateBlock(static_cast<BlockNode*>(node->body));
        symbol_table.removeTempVariable(node->iterator);
        
        // Dekrementuj counter
        addInstruction("LOAD " + std::to_string(counter_addr));
        addInstruction("DEC a");
        addInstruction("STORE " + std::to_string(counter_addr));
        
        // Dekrementuj iterator (DOWNTO)
        addInstruction("LOAD " + std::to_string(iter_addr));
        addInstruction("DEC a");
        addInstruction("STORE " + std::to_string(iter_addr));
        
        addInstruction("JUMP " + std::to_string(start_label));
        
        int end_label = code.size();
        patchJump(jump_to_end, end_label);
        patchJump(jump_to_end_invalid, end_label);
        
    } else {
        addInstruction("LOAD " + std::to_string(from_addr));
        addInstruction("SWP b");
        addInstruction("LOAD " + std::to_string(to_addr));
        addInstruction("SUB b");  // a = to - from
        int jump_valid = code.size();
        addInstruction("JPOS 0");  // jeśli to > from, pętla OK
        
        // Sprawdź from == to
        addInstruction("LOAD " + std::to_string(to_addr));
        addInstruction("SWP b");
        addInstruction("LOAD " + std::to_string(from_addr));
        addInstruction("SUB b");  // a = from - to
        int jump_also_valid = code.size();
        addInstruction("JZERO 0");  // jeśli from == to, też OK (1 iteracja)
        
        // from > to - counter = 0, skocz na koniec
        addInstruction("RST a");
        addInstruction("STORE " + std::to_string(counter_addr));
        int jump_to_end_invalid = code.size();
        addInstruction("JUMP 0");
        
        // Pętla jest valid
        int valid_label = code.size();
        patchJump(jump_valid, valid_label);
        patchJump(jump_also_valid, valid_label);
        
        // counter = to - from + 1
        addInstruction("LOAD " + std::to_string(to_addr));
        addInstruction("SWP b");
        addInstruction("LOAD " + std::to_string(from_addr));
        addInstruction("SWP b");
        addInstruction("SUB b");  // a = to - from
        addInstruction("INC a");
        addInstruction("STORE " + std::to_string(counter_addr));
        
        // 3. PĘTLA
        int start_label = code.size();
        
        addInstruction("LOAD " + std::to_string(counter_addr));
        int jump_to_end = code.size();
        addInstruction("JZERO 0");
        
        // Ciało pętli
        symbol_table.addTempVariable(node->iterator, iter_addr);
        generateBlock(static_cast<BlockNode*>(node->body));
        symbol_table.removeTempVariable(node->iterator);
        
        // Dekrementuj counter
        addInstruction("LOAD " + std::to_string(counter_addr));
        addInstruction("DEC a");
        addInstruction("STORE " + std::to_string(counter_addr));
        
        // Inkrementuj iterator (TO)
        addInstruction("LOAD " + std::to_string(iter_addr));
        addInstruction("INC a");
        addInstruction("STORE " + std::to_string(iter_addr));
        
        addInstruction("JUMP " + std::to_string(start_label));
        
        int end_label = code.size();
        patchJump(jump_to_end, end_label);
        patchJump(jump_to_end_invalid, end_label);
    }
    
    if (has_global_var) {
        symbol_table.addTempVariable(node->iterator, old_addr);
    }
}
// Pobierz adres zmiennej/tablicy
uint64_t CodeGenerator::getVarAddress(const std::string& name) {
    if (!current_procedure.empty()) {
        std::string full_name = current_procedure + "." + name;
        const SymbolInfo* info = symbol_table.get(full_name);
        
        if (info && info->is_parameter) {
            return symbol_table.getParameterAddress(current_procedure, name);
        }
        else if (info && !info->is_parameter) {
            // Zmienna lokalna procedury
            return symbol_table.getVariableAddress(full_name);
        }
    }
    
    // Zmienna globalna
    const SymbolInfo* info = symbol_table.get(name);
    if (!info) {
        throw std::runtime_error("Nieznana zmienna/tablica: '" + name + "'");
    }
    
    if (info->is_array) {
        return symbol_table.getArrayBaseAddress(name);
    } else {
        return symbol_table.getVariableAddress(name);
    }
}

// Emitowanie kodu do strumienia
void CodeGenerator::emit(std::ostream& out) const {
    for (const auto& instr : code) {
        out << instr << "\n";
    }
}
// Emitowanie kodu do pliku
void CodeGenerator::emitToFile(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out) {
        throw std::runtime_error("Nie można otworzyć pliku: " + filename);
    }
    emit(out);
    out.close();
}

void CodeGenerator::printCode() const {
    std::cout << "\n=== WYGENEROWANY KOD ===" << std::endl;
    for (const auto& instr : code) {
        std::cout << instr << std::endl;
    }
    std::cout << "=======================\n" << std::endl;
}