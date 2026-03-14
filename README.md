# Kompilator JFTT

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![Bison](https://img.shields.io/badge/Bison-3.8-green.svg)](https://www.gnu.org/software/bison/)
[![Flex](https://img.shields.io/badge/Flex-2.6-orange.svg)](https://github.com/westes/flex)

Kompilator napisany w języku C++ (C++17), wykorzystujący narzędzia **Bison** oraz **Flex** do analizy składniowej i leksykalnej. Projekt został zrealizowany w ramach przedmiotu **Języki Formalne i Techniki Translacji (JFTT)** na 5. semestrze studiów.

## Wyniki konkursu

Projekt zajął **34. miejsce** na 74 w konkursie.

## Technologie

- **C++**: standard C++17
- **Kompilator**: g++
- **Bison**: generowanie parsera
- **Flex**: generowanie analizatora leksykalnego

## Budowanie projektu
Aby zbudować projekt przejdź do 
```bash
cd kompilator
```
Użyj polecenia:
```bash
make all 
```
 Uruchomienie
Uruchom kompilator, podając plik wejściowy i wyjściowy:
```bash
./kompilator input output
```
Uruchom maszynę wirtualną na wygenerowanym pliku:
```bash
./maszyna-wirtualna-cln output
```

