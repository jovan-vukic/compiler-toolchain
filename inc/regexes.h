#ifndef REGEXES_H
#define REGEXES_H

#include <regex>
#include <string>

using namespace std;

// regular expressions to find redundancies in the input file
regex commentsRegex("([^#]*)#.*");
regex tabsRegex("\\t");

regex extraSpacesRegex(" {2,}");
regex extraBoundsSpacesRegex("^( *)([^ ].*[^ ])( *)$");
regex commaSpacesRegex(" ?, ?");
regex colonSpacesRegex(" ?: ?");

// useful strings
string decimalPattern = "-?[0-9]+";
string hexadecimalPattern = "0[xX][0-9A-Fa-f]+";
string symbolPattern = "[a-zA-Z][a-zA-Z_0-9]*";

string literalPattern = decimalPattern + "|" + hexadecimalPattern;
string literalOrSymbolPattern = literalPattern + "|" + symbolPattern;

// regular expressions for assembler directives
regex externDirectiveRegex("^\\.extern (" + symbolPattern + "(," + symbolPattern + ")*)$");
regex globalDirectiveRegex("^\\.global (" + symbolPattern + "(," + symbolPattern + ")*)$");

regex sectionDirectiveRegex("^\\.section (" + symbolPattern + ")$");

regex wordDirectiveRegex("^\\.word ((" + literalOrSymbolPattern + ")(,(" + literalOrSymbolPattern + "))*)$");
regex skipDirectiveRegex("^\\.skip (" + literalPattern + ")$");
regex endDirectiveRegex("^\\.end$");

// regular expressions for label recognition
regex labelRegex("^(" + symbolPattern + "):$");
regex labelWithInstructionRegex("^(" + symbolPattern + "):(.*)$");

#endif
