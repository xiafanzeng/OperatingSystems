//
//  main.cpp
//  Lab1
//
//  Created by Qinxue Yao on 9/17/18.
//  Copyright © 2018 Qinxue Yao. All rights reserved.
//

#include <iostream>
#include <map>
#include <set>
#include <iterator>
#include <fstream>
#include <string>
#include <cstring>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <regex>
#include <limits.h>
//#include <bits/stdc++.h>
using namespace std;



// read a new line that is not empty
void readNewLine(FILE *file, char * line, int &lineLen, int  &lineNumber) {
    if (fgets(line, INT_MAX, file) != NULL) {
        // keep reading a new line if reach a blank line
        // line length>0, still possible null token
        lineNumber++;
        if (strlen(line) == 0)
        {
            readNewLine(file, line, lineLen, lineNumber);
        } else {
            lineLen = strlen(line);
        }
    } 
}

char * readNewToken(FILE *file, char * line, int &lineLen, int  &lineNumber, int &lineOffset) {
    char * token = strtok(NULL, " \t\n");
    while (token == NULL) {
        readNewLine(file, line, lineLen, lineNumber);
        if (feof(file)) {
            lineOffset = lineLen;
            return token;
        } else {
            token = strtok(line, " \t\n");
        }
    }
    lineOffset = token - line + 1;    
    return token;
}

int readInt(FILE *file, char * line, int &lineLen, int &lineNumber, int &lineOffset) {
    char * token = readNewToken(file, line, lineLen, lineNumber, lineOffset);
    char *endptr;

    if (token == NULL) {//reached the eof
        return -2;
    } else {
        errno = 0;
        long int i = strtol(token, &endptr, 10);
        if (errno == ERANGE || i > INT_MAX || i < INT_MIN) {
            errno = ERANGE;
            i = i > 0 ? INT_MAX : INT_MIN; // int overflow
            return (int) i;
        } else if (endptr == token) {
            return -1;  // no conversion
        } else if (*endptr != '\0') {
            return -1;  // extra junks after number
        } else {
            return (int) i;
        }
    }    
}

string readSymbol(FILE *file, char * line, int &lineLen, int &lineNumber, int &lineOffset) {
    char * token = readNewToken(file, line, lineLen, lineNumber, lineOffset);
    string symbol;
    if (token == NULL) {
        symbol = "";
    } else {
        symbol = token;
    }
    return symbol;
}

void _parseError(int errocode, int lineNumber, int lineOffset) {
    static string errstr[] = {
        "NUM_EXPECTED",             // Number expected
        "SYM_EXPECTED",             // Symbol expected
        "ADDR_EXPECTED",            // Adressing expected which is A/E/I/R
        "SYM_TOO_LONG",             // Symbol name is too long
        "TOO_MANY_DEF_IN_MODULE",   // > 16
        "TOO_MANY_USE_IN_MODULE",   // >16
        "TOO_MANY_INSTR"            // total num_instr exceeds memmory size (512)
    };
    cout <<  "Parse Error line " << lineNumber << " offset " << lineOffset << ": " << errstr[errocode] << "\n";
}

// Token Parser
int pass1(FILE *file, map<string, int> &symbolTable) {
    int currentModule = 1;
    int moduleBase = 0;
    
    int totalNumInstr = 0;

    char line[LINE_MAX];
    int lineLen;
    int lineNumber = 0;
    int lineOffset = 1;

    while (!feof(file)) {
        int defCount = readInt(file, line, lineLen, lineNumber, lineOffset);
        if (defCount == -2) { //reach end of file
            break;
        }
        if (defCount == -1) {
            _parseError(0, lineNumber, lineOffset); // num expected
            return 1;
        }
        if (defCount > 16) {
            _parseError(4, lineNumber, lineOffset); // too many def in the module
            return 1;
        }
        map<string, int> defList; // deflist of this module
        for (int i = 0; i < defCount; i++) {
            string symbol = readSymbol(file, line, lineLen, lineNumber, lineOffset);
            // cout << symbol << "\n";
            if (symbol.empty() || !regex_match(symbol, regex("[A-Za-z][A-Za-z0-9]*"))) {
                _parseError(1, lineNumber, lineOffset); // symbol expected
                return 1;
            }
            if (symbol.length() > 16) {
                _parseError(3, lineNumber, lineOffset);  // symbol name too long
                return 1;
            }
            int val = readInt(file, line, lineLen, lineNumber, lineOffset);
            // cout << val << "\n";
            if (val == -1) {
                _parseError(0, lineNumber, lineOffset);  // num expected
                return 1;
            }
                
            int absVal = val + moduleBase;
            // if not defined, add to the symbol table
            if (symbolTable.count(symbol) == 0) {
                symbolTable.insert(pair <string, int> (symbol, absVal));
                defList.insert(pair <string, int> (symbol, val));
            } else {
                // if defined, use the first value, add 1000 to the first value as a mark
                symbolTable[symbol] += 1000;
                defList.insert(pair <string, int> (symbol, val + 1000));
            }                
        }
            
        // if defCount == 0, then no symbol defined in this module
        int useCount = readInt(file, line, lineLen, lineNumber, lineOffset);
        if (useCount == -1) {
            _parseError(0, lineNumber, lineOffset);   // num expected
            return 1;
        }
        if (useCount > 16) {
            _parseError(5, lineNumber, lineOffset);   // too many use in the module
            return 1;
        }
        for (int i = 0; i < useCount; i++) {
            string symbol = readSymbol(file, line, lineLen, lineNumber, lineOffset);
            if (symbol.empty() || !regex_match(symbol, regex("[A-Za-z][A-Za-z0-9]*"))) {
                _parseError(1, lineNumber, lineOffset);  // symbol expected
                return 1;
            }
            if (symbol.length() > 16) {
                _parseError(3, lineNumber, lineOffset);  // symbol name too long
                return 1;
            }
        }
            
        int codeCount = readInt(file, line, lineLen, lineNumber, lineOffset);
        totalNumInstr += codeCount;
        if (totalNumInstr > 512) {
            _parseError(6, lineNumber, lineOffset);  // total num_instr exceeds memmory size (512)
            return 1;
        }
        for (int i = 0; i < codeCount; i++) {
            string addressing = readSymbol(file, line, lineLen, lineNumber, lineOffset);
            /*
            if (addressing == NULL || (strcmp(addressing, "A") != 0 && strcmp(addressing, "E") != 0 && strcmp(addressing, "I") != 0 && strcmp(addressing, "R") != 0)) {
                _parseError(2, lineNumber, lineOffset);  // Adressing expected which is A/E/I/R
                return 1;
            }
            */

            if (addressing == "") {
                _parseError(2, lineNumber, lineOffset);  // Adressing expected which is A/E/I/R
                return 1;
            } else if (addressing != "A" && addressing != "E" && addressing != "I" && addressing != "R") {
                _parseError(2, lineNumber, lineOffset);  // Adressing expected which is A/E/I/R
                return 1;
            }
            int instr = readInt(file, line, lineLen, lineNumber, lineOffset);
            // cout << instr << "\n";
            if (instr == -1) {
                _parseError(0, lineNumber, lineOffset);  // num expected
                return 1;
            }
        }
            
        // rule 5: address in the def < size of the module
        for (map<string, int>::iterator i = defList.begin(); i != defList.end(); ++i) {
            if (i->second >= 1000) { // defined in the previous module
                break;
            } else if (i->second >= codeCount) { // not defined before
                printf("Warning: Module %d: %s too big %d (max=%d) assume zero relative\n", currentModule, (i->first).c_str(), i->second, codeCount - 1);
                symbolTable[i->first] = moduleBase;
            }
        }
        currentModule++;
        moduleBase = totalNumInstr;        
    }
    return 0;
}


int pass2(FILE *file, map<string, int>  &symbolTable) {
    //printing map symbolTable
    cout << "Symbol Table\n";
    map<string, int> :: iterator itr;
    for (itr = symbolTable.begin(); itr != symbolTable.end(); ++itr) {
        if (itr->second < 1000) {// only defined once
            cout << itr->first << "=" << itr->second <<'\n';
        } else {//defined mutiple times
            symbolTable[itr->first] = (itr->second) % 1000;
            cout << itr->first << "=" <<  (itr->second) % 1000
                << " Error: This variable is multiple times defined; first value used\n";
        }            
    }
    cout << "\nMemory Map\n";

    int currentModule = 1;
    int moduleBase = 0;
    vector<int> modules;
    modules.push_back(moduleBase);
    int totalNumInstr = 0;

    char line[LINE_MAX];
    int lineLen;
    int lineNumber = 0;
    int lineOffset = 1;
    set<string> usedSet;

    while (!feof(file)) {

        int defCount = readInt(file, line, lineLen, lineNumber, lineOffset);
            if (defCount == -2) { //reach end of file
                break;
            }
            for (int i = 0; i < defCount; i++) {
                string symbol = readSymbol(file, line, lineLen, lineNumber, lineOffset);
                int val = readInt(file, line, lineLen, lineNumber, lineOffset);
                int absVal = val + moduleBase;     
            }
            
            // if defCount == 0, then no symbol defined in this module
            int useCount = readInt(file, line, lineLen, lineNumber, lineOffset);
            string useList[useCount]; // in each module

            for (int i = 0; i < useCount; i++) {
                string symbol = readSymbol(file, line, lineLen, lineNumber, lineOffset);
                useList[i] = symbol;
                
            }
            
            int codeCount = readInt(file, line, lineLen, lineNumber, lineOffset);
            totalNumInstr += codeCount;
            set<string> externalSet; // in each module
            for (int i = 0; i < codeCount; i++) {
                string addressing = readSymbol(file, line, lineLen, lineNumber, lineOffset);
                int instr = readInt(file, line, lineLen, lineNumber, lineOffset);
                if (addressing == "I") {
                    // rule 10: instr >= 10000
                    if (instr >= 10000) {
                        instr = 9999;
                        printf("%03d: %04d ", moduleBase + i, instr);
                        printf("Error: Illegal immediate value; treated as 9999\n");
                    } else 
                        printf("%03d: %04d\n", moduleBase + i, instr);
                } else if (addressing =="A") {
                    if (instr >= 10000) {
                        instr = 9999;
                        printf("%03d: %04d ", moduleBase + i, instr);
                        printf("Error: Illegal opcode; treated as 9999\n");
                    } else if (instr % 1000 >= 512) {
                        // rule 8: absolute address cannot exceed machine size
                        printf("%03d: %04d ", moduleBase + i, (instr/1000)*1000 + 0);
                        printf("Error: Absolute address exceeds machine size; zero used\n");
                    } else 
                        printf("%03d: %04d\n", moduleBase + i, instr);
                } else if (addressing == "R") {
                    if (instr >= 10000) {
                        instr = 9999;
                        printf("%03d: %04d ", moduleBase + i, instr);
                        printf("Error: Illegal opcode; treated as 9999\n");
                    } else if (instr % 1000 >= codeCount) {
                        // rule 9: relative address exceeds moduel size, use 0                    
                        printf("%03d: %04d ", moduleBase + i, (instr/1000)*1000 + 0 + moduleBase);
                        printf("Error: Relative address exceeds module size; zero used\n");
                    } else 
                        printf("%03d: %04d\n", moduleBase + i, instr + moduleBase);
                } else if (addressing == "E") {
                    if (instr >= 10000) {
                        instr = 9999;
                        printf("%03d: %04d ", moduleBase + i, instr);
                        printf("Error: Illegal opcode; treated as 9999\n");
                    } else {
                        int order = instr % 1000;
                        // rule 6: external address exceeds length of uselist, treat ad immediate
                        if (order >= useCount) {
                            printf("%03d: %04d ", moduleBase + i, instr);
                            printf("Error: External address exceeds length of uselist; treated as immediate\n");
                        } else {                            
                            string symbol1 = useList[order];
                            externalSet.insert(symbol1);
                            usedSet.insert(symbol1);
                            if (symbolTable.count(symbol1)) {
                                printf("%03d: %04d\n", moduleBase + i, (instr/1000)*1000 + symbolTable.at(symbol1));
                            } else {
                                printf("%03d: %04d ", moduleBase + i, (instr/1000)*1000 + 0);
                            // rule 3
                                printf("Error: %s is not defined; zero used\n", symbol1.c_str());
                            }
                        }  
                    }
                    
                } 
                
            }
            // rule 7: appear in the uselist, but not used in E
            for (int i = 0; i < useCount; i++) {
                if (externalSet.count(useList[i]) == 0) {
                    printf("Warning: Module %d: %s appeared in the uselist but was not actually used\n", currentModule, useList[i].c_str());
                }   
            }
            currentModule++;
            moduleBase = totalNumInstr;
            modules.push_back(moduleBase);
    }
    cout << "\n";
    // rule 4: check if all defined symbols are used
    map<string, int>::iterator symbolIt;
    vector<int>::iterator upper;
    for (symbolIt = symbolTable.begin(); symbolIt != symbolTable.end(); ++symbolIt) {
        if (usedSet.count(symbolIt->first) == 0) {
            upper = upper_bound(modules.begin(), modules.end(), symbolIt->second);
            int moduleNum = (int)(upper - modules.begin());
            printf("Warning: Module %d: %s was defined but never used\n", moduleNum, (symbolIt->first).c_str());
        }
    }
    cout << "\n";
    return 0;
}

int main(int argc, const char * argv[]) {
    FILE *file;
    
    file = fopen(argv[1], "r");
    
    map <string, int> symbolTable;
    
    int res1;
    if (file != NULL) {
        res1 = pass1(file, symbolTable);
        fclose(file);
    } else {
        cout << "Unable to open file";
        return 1;
    }
        
    if (res1 == 0) {
        file = fopen(argv[1], "r");
        pass2(file, symbolTable);
        fclose(file);
    }
    return 0;
}
