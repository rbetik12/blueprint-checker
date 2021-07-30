#ifndef __EASYARGS_H_INCLUDED__
#define __EASYARGS_H_INCLUDED__

#include <algorithm>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

enum ArgType {
    flagShrt,
    flagLng,
    val,
    pos
};

class EasyArgs {
   public:
    EasyArgs(int argc, char *argv[]);
    EasyArgs(int argc, std::string argv[]);
    EasyArgs(std::vector<std::string> argv);
    void Initialize(std::string name);

    ~EasyArgs();

    EasyArgs *Version(std::string version);
    EasyArgs *Description(std::string desc);
    EasyArgs *Flag(std::string shrt, std::string lng, std::string helptext);
    EasyArgs *Value(std::string shrt, std::string lng, std::string helptext, bool required);
    EasyArgs *Positional(std::string name, std::string helptext);

    bool IsSet(std::string arg);
    std::string GetValueFor(std::string arg);
    std::string GetPositional(std::string name);

    void PrintUsage();

   private:
    std::vector<std::string> args;

    std::vector<std::string> flagsSet;
    std::map<std::string, std::string> valuesGiven;
    std::map<std::string, std::string> posGiven;
    int posArgCount;

    std::string version;
    std::string description;
    std::string valueDelimiter;

    std::stringstream helpOpt;
    std::stringstream helpPos;
    std::stringstream usageSummary;

    ArgType FindArgType(std::string arg);

    bool FlagIsSet(std::string shrt, std::string lng);
    std::string ExtractKey(std::string arg);
    std::string ExtractValue(std::string arg);
    std::string GetValueFromArgs(std::string shrt, std::string lng);
    std::string FetchPositional();

    void Error(std::string err);
};

#endif  //__EASYARGS_H_INCLUDED__