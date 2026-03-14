CC = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g
BISON = bison
FLEX = flex

TARGET = kompilator

SRCS = main.cpp symbols.cpp semantic.cpp codegen.cpp
OBJS = $(SRCS:.cpp=.o)
GENERATED = parser.tab.cpp parser.tab.hpp lex.yy.c parser.tab.c parser.tab.h

all: $(TARGET)

bison: parser.tab.cpp parser.tab.hpp

flex: lex.yy.c

$(TARGET): $(OBJS) parser.tab.o lex.yy.o
	$(CXX) $(CXXFLAGS) -o $@ $^

parser.tab.cpp parser.tab.hpp: parser.y
	$(BISON) -d parser.y -o parser.tab.cpp 

lex.yy.c: lexer.l parser.tab.hpp
	$(FLEX) -o lex.yy.c lexer.l

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS) $(GENERATED) *.o output.vm

run: $(TARGET)
	./$(TARGET) test.txt output.vm
	cat output.vm

.PHONY: all clean run bison flex