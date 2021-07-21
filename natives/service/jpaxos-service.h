#ifndef JPAXOS_SERVICE_H_INCLUDED
#define JPAXOS_SERVICE_H_INCLUDED

#include <jni.h>
#include <string>

#include "simple-kv-service.h"

#define CONFIG_FILE_NAME "service.conf"

struct Config {
    /// path to persistent memory file 
    std::string path;
    /// size of persistent memory file 
    uint64_t size;
};

void libInit();

void initialize(JNIEnv * env, jint localId);

std::string getSnapshotFile();

void releaseSnapshotFile();

void updateToSnapshot(JNIEnv * env, std::string filename);

void loadConfig(JNIEnv* env, jint localId);

#endif // JPAXOS_SERVICE_H_INCLUDED
