#include "headers/lsr_paxos_test_GenericJniService.h"
#include "jpaxos-service.h"

#ifdef __cplusplus
extern "C" {
#endif
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *, void *){
    return JNI_VERSION_1_8;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *, void *){
}

JNIEXPORT void JNICALL Java_lsr_paxos_test_GenericJniService_init (JNIEnv * env, jobject, jint localId){
    try {
        initialize(env, localId);
    } catch(const std::exception & e){
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
    }    
}

JNIEXPORT jbyteArray JNICALL Java_lsr_paxos_test_GenericJniService_execute_1 (JNIEnv * env, jclass, jlong ssn, jbyteArray request){
    try {
        auto reqLen = env->GetArrayLength(request);
        auto elements = env->GetByteArrayElements(request, nullptr);
        auto [responseC, resLen, shallDelete] = execute(env, ssn, (const char*) elements, reqLen);
        env->ReleaseByteArrayElements(request, elements, JNI_ABORT);
        auto responseJ = env->NewByteArray(resLen);
        env->SetByteArrayRegion(responseJ, 0, resLen, (const jbyte*) responseC);
        if(shallDelete) delete [] responseC;
        return responseJ;
    } catch(const std::exception & e){
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), ("Service execute() failed: "s + e.what() + + " (" + typeid(e).name() + ")").c_str());
        return env->NewByteArray(0);
    }
}

JNIEXPORT jstring JNICALL Java_lsr_paxos_test_GenericJniService_getSnapshotFile (JNIEnv * env, jclass){
    std::string filename = getSnapshotFile();
    return env->NewStringUTF(filename.data());
}

JNIEXPORT void JNICALL Java_lsr_paxos_test_GenericJniService_releaseSnapshotFile (JNIEnv *, jclass){
    releaseSnapshotFile();
}

JNIEXPORT void JNICALL Java_lsr_paxos_test_GenericJniService_updateToSnapshot (JNIEnv * env, jobject, jstring filenameJ){
    auto len = env->GetStringUTFLength(filenameJ);
    auto filenameJdata = env->GetStringUTFChars(filenameJ, nullptr);
    std::string filenameC(filenameJdata, len);
    env->ReleaseStringUTFChars(filenameJ, filenameJdata);
    updateToSnapshot(env, filenameC);
}

#ifdef __cplusplus
}
#endif
