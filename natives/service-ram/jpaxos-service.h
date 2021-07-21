#ifndef JPAXOS_SERVICE_H_INCLUDED
#define JPAXOS_SERVICE_H_INCLUDED

#include <jni.h>
#include <string>

#include "simple-kv-service.h"

#define CONFIG_FILE_NAME "service.conf"

struct Config {
    /// path where to ut snapshots
    std::string path;
};

extern Config cfg;
extern jint localId;

void libInit();

void initialize(JNIEnv * env, jint localId);

void loadConfig(JNIEnv* env, jint localId);

#endif // JPAXOS_SERVICE_H_INCLUDED
