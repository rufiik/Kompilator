#include "symbols.hpp"
#include <sstream>
#include <iomanip>

// Konstruktor
SymbolTable::SymbolTable() : symbol_addresses(), current_scope(0),next_address(6),scopes()  { 
    scopes.push_back(std::map<std::string, SymbolInfo>());
}

// Pobierz adres zmiennej:
uint64_t SymbolTable::getVariableAddress(const std::string& name) const {
    auto it = symbol_addresses.find(name);
    if (it != symbol_addresses.end()) {
        return it->second;
    }
    throw std::runtime_error("Zmienna '" + name + "' nie ma przydzielonego adresu");
}

// Pobierz adres bazowy tablicy:
uint64_t SymbolTable::getArrayBaseAddress(const std::string& name) const {
    auto it = symbol_addresses.find(name);
    if (it != symbol_addresses.end()) {
        return it->second;
    }
    
    const SymbolInfo* info = get(name);
    if (!info || !info->is_array) {
        throw std::runtime_error("'" + name + "' nie jest tablicą");
    }
    
    throw std::runtime_error("Tablica '" + name + "' nie ma przydzielonego adresu");
}

// Wejście do nowego zakresu 
void SymbolTable::enterScope() {
    scopes.push_back(std::map<std::string, SymbolInfo>());
    current_scope++;
}

// Wyjście z zakresu
void SymbolTable::exitScope() {
    if (current_scope > 0) {
        scopes.pop_back();
        current_scope--;
    }
}

// Dodaj symbol (ogólna metoda)
void SymbolTable::addSymbol(const SymbolInfo& info) {
    if (existsInCurrentScope(info.name)) {
        throw SemanticError("Podwójna deklaracja: '" + info.name + "'", info.line);
    }
    SymbolInfo copy = info;
    copy.declared_scope = current_scope; 
    scopes[current_scope][info.name] = copy;
}

// Dodaj zwykłą zmienną
void SymbolTable::addVariable(const std::string& name, int line) {
    uint64_t addr = next_address++;
    symbol_addresses[name] = addr;
    SymbolInfo info;
    info.name = name;
    info.type = SymbolType::VARIABLE;
    info.line = line;
    addSymbol(info);
    
}
// Dodaj tablicę
void SymbolTable::addArray(const std::string& name, uint64_t start, uint64_t end, int line) {
    if (existsInCurrentScope(name)) {
        throw SemanticError("Podwójna deklaracja tablicy: '" + name + "'", line);
    }
    
    if (start > end) {
        throw SemanticError("Nieprawidłowy zakres tablicy " + name + 
                           ": " + std::to_string(start) + 
                           ":" + std::to_string(end), line);
    }
    uint64_t base_addr = next_address;
    symbol_addresses[name] = base_addr;
    uint64_t size = (end - start + 1);
    next_address += size;

    SymbolInfo info;
    info.name = name;
    info.type = SymbolType::ARRAY;
    info.line = line;
    info.array_start = start;
    info.array_end = end;
    info.is_array = true;
    
    scopes[current_scope][name] = info;
}
// Dodaj procedurę
void SymbolTable::addProcedure(const std::string& name, int line) {
    uint64_t addr = next_address++;
    symbol_addresses[name] = addr; 
    SymbolInfo info;
    info.name = name;
    info.type = SymbolType::PROCEDURE;
    info.line = line;
    info.is_procedure = true;
    addSymbol(info);
}
// Dodaj parametr procedury
void SymbolTable::addParameter(const std::string& proc_name, const std::string& param_name, int line, ParamDeclNode::ParamType ptype, bool is_array) {
    std::string unique_name = proc_name + "." + param_name;
    
    uint64_t addr = next_address++;
    symbol_addresses[unique_name] = addr;
    
    SymbolInfo info;
    info.name = unique_name;  
    info.full_name = unique_name;  
    info.type = SymbolType::PARAMETER;
    info.line = line;
    info.is_parameter = true;
    info.param_type = ptype;
    info.is_array = is_array;
    info.is_reference = true;
    info.initialized = (ptype != ParamDeclNode::ParamType::OUT);
    
    addSymbol(info);
}

uint64_t SymbolTable::getProcedureAddress(const std::string& name) const {
    auto it = symbol_addresses.find(name);
    if (it != symbol_addresses.end()) {
        return it->second;
    }
    throw std::runtime_error("Procedura '" + name + "' nie ma przydzielonego adresu");
}

uint64_t SymbolTable::getParameterAddress(const std::string& proc_name, const std::string& param_name) const {
    std::string unique_name = proc_name + "." + param_name;
    auto it = symbol_addresses.find(unique_name);
    if (it != symbol_addresses.end()) {
        return it->second;
    }
    throw std::runtime_error("Parametr '" + unique_name + "' nie ma przydzielonego adresu");
}

// Sprawdź czy symbol istnieje (we wszystkich zakresach)
bool SymbolTable::exists(const std::string& name) const {
    return findSymbol(name) != nullptr;
}

// Sprawdź czy symbol istnieje w bierzącym zakresie
bool SymbolTable::existsInCurrentScope(const std::string& name) const {
    return scopes[current_scope].find(name) != scopes[current_scope].end();
}

// Pobierz symbol (modyfikowalny)
SymbolInfo* SymbolTable::get(const std::string& name) {
    return findSymbol(name);
}

// Pobierz symbol (tylko do odczytu)
const SymbolInfo* SymbolTable::get(const std::string& name) const {
    return findSymbol(name);
}
// Sprawdź czy zmienna jest zadeklarowana
void SymbolTable::checkVariableDeclared(const std::string& name, int line) const {
    if (!exists(name)) {
        throw SemanticError("Użycie niezadeklarowanej zmiennej: '" + name + "'", line);
    }
}
// Sprawdź czy zmienna NIE jest zadeklarowana w bieżącym zakresie
void SymbolTable::checkNotDeclaredInCurrentScope(const std::string& name, int line) const {
    if (existsInCurrentScope(name)) {
        throw SemanticError("Podwójna deklaracja: '" + name + "'", line);
    }
}
// Sprawdź czy nie modyfikujemy stałej
void SymbolTable::checkNotConstant(const std::string& name, int line) const {
    const SymbolInfo* info = get(name);
    if (info && info->isConstant()) {
        throw SemanticError("Nie można modyfikować stałej: '" + name + "'", line);
    }
}
// Sprawdź czy nie czytamy niezainicjalizowanej
void SymbolTable::checkNotUninitialized(const std::string& name, int line) const {
    const SymbolInfo* info = get(name);
    if (info && info->isUninitialized()) {
        throw SemanticError("Czytanie niezainicjalizowanej zmiennej: '" + name + "'", line);
    }
}
// Sprawdź zakres tablicy
void SymbolTable::checkArrayIndex(const std::string& name, uint64_t index, int line) const {
    const SymbolInfo* info = get(name);
    
    if (!info) {
        throw SemanticError("Tablica '" + name + "' nie zadeklarowana", line);
    }
    
    if (!info->is_array) {
        throw SemanticError("'" + name + "' nie jest tablicą", line);
    }
    
    if (index < info->array_start || index > info->array_end) {
        throw SemanticError("Indeks " + std::to_string(index) + 
                           " poza zakresem tablicy " + name + 
                           "[" + std::to_string(info->array_start) + 
                           ":" + std::to_string(info->array_end) + "]", line);
    }
}
// Sprawdź czy procedura istnieje
void SymbolTable::checkProcedureExists(const std::string& name, int line) const {
    const SymbolInfo* info = get(name);
    if (!info || !info->isProcedure()) {
        throw SemanticError("Wywołanie nieistniejącej procedury: '" + name + "'", line);
    }
}
// Wyczyść wszystko
void SymbolTable::clear() {
    scopes.clear();
    scopes.push_back(std::map<std::string, SymbolInfo>());
    current_scope = 0;
}
// Policz wszystkie symbole
size_t SymbolTable::getSymbolCount() const {
    size_t count = 0;
    for (const auto& scope : scopes) {
        count += scope.size();
    }
    return count;
}
// Wyszukaj symbol (implementacja prywatna)
const SymbolInfo* SymbolTable::findSymbol(const std::string& name) const {
    for (long long i = current_scope; i >= 0; i--) {
        auto it = scopes[i].find(name);
        if (it != scopes[i].end()) {
            return &(it->second);
        }
    }
    return nullptr;
}

SymbolInfo* SymbolTable::findSymbol(const std::string& name) {
    for (long long i = current_scope; i >= 0; i--) {
        auto it = scopes[i].find(name);
        if (it != scopes[i].end()) {
            return &(it->second);
        }
    }
    return nullptr;
}

uint64_t SymbolTable::getArrayElementAddress(const std::string& name, uint64_t index) const {
    const SymbolInfo* info = get(name);
    if (!info || !info->is_array) {
        return -1;
    }
    
    uint64_t base = getArrayBaseAddress(name);
    return base + (index - info->array_start);
}
// Dodaj tymczasową zmienną
void SymbolTable::addTempVariable(const std::string& name, uint64_t addr) {
    SymbolInfo info;
    info.name = name;
    info.type = SymbolType::VARIABLE;
    info.line = -1;
    symbol_addresses[name] = addr;
    scopes[current_scope][name] = info;
}
void SymbolTable::removeTempVariable(const std::string& name) {
    symbol_addresses.erase(name);
    scopes[current_scope].erase(name);
}
uint64_t SymbolTable::allocateTemp() {
    return next_address++;
}

// Debug: wypisz całą tablicę symboli
void SymbolTable::print() const {
    std::cout << "\n=== TABLICA SYMBOLI ===" << std::endl;
    std::cout << "Liczba zakresów: " << scopes.size() << std::endl;
    std::cout << "Bieżący zakres: " << current_scope << std::endl;

    for (size_t i = 0; i < scopes.size(); ++i) {
        std::cout << "\n--- Zakres " << i << " ---" << std::endl;

        if (scopes[i].empty()) {
            std::cout << "  (pusty)" << std::endl;
            continue;
        }

        for (const auto& pair : scopes[i]) {
            const SymbolInfo& info = pair.second;
            std::cout << "  " << std::left << std::setw(15) << info.name;
            std::cout << " (";

            switch (info.type) {
                case SymbolType::VARIABLE:
                    std::cout << "zmienna";
                    try {
                        std::cout << ") -> adres: " << getVariableAddress(info.name);
                    } catch (...) {
                        std::cout << ") -> adres: [brak]";
                    }
                    break;
                case SymbolType::ARRAY:
                    std::cout << "tablica";
                    try {
                        std::cout << ") -> base_addr: " << getArrayBaseAddress(info.name);
                    } catch (...) {
                        std::cout << ") -> base_addr: [brak]";
                    }
                    break;
                case SymbolType::PROCEDURE:
                    std::cout << "procedura";
                    try {
                        std::cout << ") -> adres powrotu: " << getProcedureAddress(info.name);
                    } catch (...) {
                        std::cout << ") -> adres powrotu: [brak]";
                    }
                    break;
                case SymbolType::PARAMETER:
                    std::cout << "parametr";
                    try {
                        std::string full = info.full_name.empty() ? info.name : info.full_name;
                        size_t pos = full.find('.');
                        std::string proc_name = (pos != std::string::npos) ? full.substr(0, pos) : full;
                        std::string param_name = (pos != std::string::npos) ? full.substr(pos + 1) : info.name;
                        std::cout << ") -> adres: " << getParameterAddress(proc_name, param_name);
                    } catch (...) {
                        std::cout << ") -> adres: [brak]";
                    }
                    break;
                default:
                    std::cout << "inne";
                    break;
            }

            if (info.isConstant()) std::cout << " (stała)";
            if (info.isUninitialized()) std::cout << " (niezainicjalizowana)";

            std::cout << " linia: " << info.line << std::endl;
        }
    }

    std::cout << "======================\n" << std::endl;
}