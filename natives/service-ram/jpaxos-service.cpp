#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sendfile.h>

#include <fstream>
#include <regex>
#include <unordered_map>

#include <jni.h>

#include "jpaxos-service.h"

Config cfg;
jint localId;

#ifndef NDEBUG
#include <signal.h>
void (*original_sigabrt)(int);
void on_SIGABRT(int){
    // java handles abrt by aborting self, and handles sevg by dumping usefull info and calling abort
    // and assert calls abrt
    // so we catch first abrt and change it to segv, then resore abrt handler.
    // This does not work well, but at least good enough to see what went wrong.
    signal(SIGABRT, original_sigabrt);
    raise(SIGSEGV);
}
#endif

__attribute__((constructor))
void libInit() {
}

void loadConfig(JNIEnv* env, jint localId){
    std::ifstream cfgFile(CONFIG_FILE_NAME);
    if(!cfgFile)
        env->ThrowNew(env->FindClass("java/io/FileNotFoundException"), "Configuration file " CONFIG_FILE_NAME " cannot be read");
    
    std::regex ignoreLineRegex("^\\s*(#.*)?$");
    std::regex kvRegex("^\\s*([^=]*?)\\s*=\\s*(.*?)\\s*$");
    
    std::unordered_map<std::string, std::string> entries;
    std::string line;
    while(std::getline(cfgFile, line)){
        if(std::regex_search(line, ignoreLineRegex))
            continue;
        std::smatch m;
        if(! std::regex_search(line, m, kvRegex))
            env->ThrowNew(env->FindClass("java/util/IllegalFormatException"),
                          ("Malformed line in " CONFIG_FILE_NAME ":" + line).c_str());
        if(entries.contains(m[1]))
            env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"),
                          ("Multiple definitions of " + (std::string)m[1] + " in " CONFIG_FILE_NAME).c_str());
        entries[m[1]] = m[2];
    }
    
    cfg.path = entries.contains("path") ?  entries["path"] : "/mnt/pmem/service";
    cfg.path += "." + std::to_string(localId);
    entries.erase("path");
    
    if(!entries.empty()){
        std::string errormsg {"Unknown keys in config file " CONFIG_FILE_NAME ": "};
        for(const auto & [k, v] : entries)
            errormsg.append(k).append(", ");
        env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), errormsg.c_str());
    }
}

void initialize(JNIEnv* env, jint localId){
    #ifndef NDEBUG
    original_sigabrt = signal(SIGABRT, on_SIGABRT);
    #endif
    
    loadConfig(env, localId); 
    ::localId = localId;
}
