#include "easyargs.h"

EasyArgs::EasyArgs(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++)
        this->args.push_back(std::string(argv[i]));

    this->Initialize(std::string(argv[0]));
}

EasyArgs::EasyArgs(int argc, std::string argv[]) {
    for (int i = 1; i < argc; i++)
        this->args.push_back(argv[i]);

    this->Initialize(argv[0]);
}

EasyArgs::EasyArgs(std::vector<std::string> argv) {
    this->args = argv;

    this->Initialize(argv[0]);
}

void EasyArgs::Initialize(std::string name) {
    this->valueDelimiter = '=';
    this->posArgCount = 0;
    this->usageSummary << "usage: " << name << " [OPTIONS] ";
}

EasyArgs::~EasyArgs() {}

EasyArgs *EasyArgs::Version(std::string _version) {
    this->version = _version;
    return this;
}

EasyArgs *EasyArgs::Description(std::string desc) {
    this->description = desc;
    return this;
}

EasyArgs *EasyArgs::Flag(std::string shrt, std::string lng, std::string helptext) {
    if (this->FlagIsSet(shrt, lng)) {
        this->flagsSet.push_back(shrt);
        this->flagsSet.push_back(lng);
    }

    this->helpOpt << shrt + ", " + lng + "\t"
                  << "no\t" << helptext << std::endl;

    return this;
}

EasyArgs *EasyArgs::Value(std::string shrt, std::string lng, std::string helptext, bool required) {
    std::string val = this->GetValueFromArgs(shrt, lng);

    if (val.empty() && required)
        this->Error("argument " + shrt + "," + lng + " is required");
    else {
        this->valuesGiven.insert(std::pair<std::string, std::string>(shrt, val));
        this->valuesGiven.insert(std::pair<std::string, std::string>(lng, val));
    }

    this->helpOpt << shrt + "=, " + lng + "=\t" << (required ? "yes\t" : "no\t") << helptext << std::endl;

    return this;
}

EasyArgs *EasyArgs::Positional(std::string name, std::string helptext) {
    this->posArgCount++;
    std::string pos = this->FetchPositional();
    if (pos.empty())
        this->Error("positional argument " + name + " is required");

    this->posGiven.insert(std::pair<std::string, std::string>(name, pos));

    this->usageSummary << name + ' ';
    this->helpPos << name << "\t\t" << helptext << std::endl;

    return this;
}

bool EasyArgs::IsSet(std::string arg) {
    return std::find(this->flagsSet.begin(), this->flagsSet.end(), arg) != this->flagsSet.end();
}

std::string EasyArgs::GetValueFor(std::string arg) {
    return this->valuesGiven[arg];
}

std::string EasyArgs::GetPositional(std::string name) {
    return this->posGiven[name];
}

void EasyArgs::PrintUsage() {
    std::cout << std::endl;
    std::cout << "version: " << this->version << std::endl;
    std::cout << "description: " << this->description << std::endl;
    std::cout << this->usageSummary.str() << std::endl
              << std::endl;

    std::cout << "OPTION       REQUIRED   DESCRIPTION\n===========================================" << std::endl;
    std::cout << this->helpOpt.str() << std::endl
              << std::endl;

    if (this->posArgCount > 0) {
        std::cout << "POSITIONAL      DESCRIPTION\n===========================================" << std::endl;
        std::cout << this->helpPos.str() << std::endl;
    }
}

bool EasyArgs::FlagIsSet(std::string shrt, std::string lng) {
    for (std::string rawArg : this->args) {
        ArgType typ = this->FindArgType(rawArg);

        if (typ == ArgType::flagLng && rawArg == lng)
            return true;

        if (typ == ArgType::flagShrt && rawArg.find(shrt[1]) != std::string::npos)
            return true;
    }
    return false;
}

// instead of looking up each time, I should maybe find the types once and be done with it.
ArgType EasyArgs::FindArgType(std::string arg) {
    if (arg[0] != '-')
        return ArgType::pos;

    size_t pos = 0;
    if ((pos = arg.find(this->valueDelimiter)) != std::string::npos)
        return ArgType::val;

    return arg.size() > 2 && arg[1] == '-' ? ArgType::flagLng : ArgType::flagShrt;
}

std::string EasyArgs::GetValueFromArgs(std::string shrt, std::string lng) {
    for (std::string rawArg : this->args) {
        ArgType typ = this->FindArgType(rawArg);

        std::string key = this->ExtractKey(rawArg);

        if (typ == ArgType::val && (key == lng || key == shrt))
            return this->ExtractValue(rawArg);
    }

    return "";
}

std::string EasyArgs::ExtractKey(std::string arg) {
    return arg.substr(0, arg.find(this->valueDelimiter));
}

std::string EasyArgs::ExtractValue(std::string arg) {
    size_t pos = arg.find(this->valueDelimiter);
    return arg.substr(pos + 1, arg.length() - pos - 1);
}

std::string EasyArgs::FetchPositional() {
    int i = 0;
    for (std::string rawArg : this->args) {
        ArgType typ = this->FindArgType(rawArg);
        if (typ == ArgType::pos)
            i++;
        if (i == this->posArgCount)
            return rawArg;
    }
    return "";
}

void EasyArgs::Error(std::string err) {
    throw std::runtime_error("incorrect arguments: " + err);
}
