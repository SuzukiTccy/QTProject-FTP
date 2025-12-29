#pragma once

#include <iostream>
using namespace std;

#ifdef TEST
#define testout(msg) cout << msg << endl << flush
#else 
#define testout(msg) 
#endif

class Logger{
public:
    template <typename ...Args>
    static void info(Args&& ...args){
        cout << "[INFO] ";
        (cout << ... << args) << endl << flush;
    }

    template <typename ...Args>
    static void error(Args&& ...args){
        cout << "[ERROR] ";
        (cout << ... << args) << endl << flush;
    }

    template <typename ...Args>
    static void debug(Args&& ...args){
        cout << "[DEBUG] ";
        (cout << ... << args) << endl << flush;
    }

    template <typename ...Args>
    static void warning(Args&& ...args){
        cout << "[DEBUG] ";
        (cout << ... << args) << endl << flush;
    }
};
