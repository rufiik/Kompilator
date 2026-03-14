# JFTT Compiler

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![Bison](https://img.shields.io/badge/Bison-3.8-green.svg)](https://www.gnu.org/software/bison/)
[![Flex](https://img.shields.io/badge/Flex-2.6-orange.svg)](https://github.com/westes/flex)

A compiler written in C++ (C++17), utilizing **Bison** and **Flex** tools for syntactic and lexical analysis. The project was developed as part of the **Formal Languages and Translation Techniques (JFTT)** course in the 5th semester of university studies.

## Competition Results

The project ranked **34th place** out of 74 in the competition.

## Technologies

- **C++**: C++17 standard
- **Compiler**: g++
- **Bison**: Parser generation
- **Flex**: Lexical analyzer generation

## Building the Project

To build the project, navigate to:

```bash
cd kompilator
```
Use the following command:
```bash
make all 
```
## Running the Compiler

Run the compiler by providing input and output files:
```bash
./kompilator input output
```
## Running the Virtual Machine

Execute the generated file on the virtual machine:
```bash
./maszyna-wirtualna-cln output
```

