/*
 * LwsService.cpp - libwebsockets test service for Android
 *
 * Copyright (C) 2016 Alexander Bruines <alexander.bruines@gmail.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 *
 * The person who associated a work with this deed has dedicated
 * the work to the public domain by waiving all of his or her rights
 * to the work worldwide under copyright law, including all related
 * and neighboring rights, to the extent allowed by law. You can copy,
 * modify, distribute and perform the work, even for commercial purposes,
 * all without asking permission.
 *
 * The test apps are intended to be adapted for use in your code, which
 * may be proprietary.  So unlike the library itself, they are licensed
 * Public Domain.
 */

#include <libwebsockets.h>

#include <jni.h>
#include <android/log.h>
#define printf(...) __android_log_print(ANDROID_LOG_VERBOSE, "LwsService", ##__VA_ARGS__)

/////////////////////////////////////////////////////////
// Code executed when loading the dynamic link library //
/////////////////////////////////////////////////////////

// The Java class the native functions shall be part of
#define JNIREG_CLASS "org/libwebsockets/client/LwsService"

JavaVM* gJvm = NULL;
JNIEnv* gEnv = 0;

JNIEXPORT jboolean JNICALL jni_initLws(JNIEnv *env, jobject obj);
JNIEXPORT void JNICALL jni_exitLws(JNIEnv *env, jobject obj);
JNIEXPORT void JNICALL jni_serviceLws(JNIEnv *env, jobject obj);
JNIEXPORT void JNICALL jni_setConnectionParameters(JNIEnv *env, jobject obj, jstring serverAddress, jint serverPort);
JNIEXPORT jboolean JNICALL jni_connectLws(JNIEnv *env, jobject obj);

static JNINativeMethod gMethods[] = {
    { "initLws", "()Z", (void*)jni_initLws },
    { "exitLws", "()V", (void*)jni_exitLws },
    { "serviceLws", "()V", (void*)jni_serviceLws },
    { "setConnectionParameters", "(Ljava/lang/String;I)V", (void*)jni_setConnectionParameters },
    { "connectLws", "()Z", (void*)jni_connectLws },
};

static int registerNativeMethods(JNIEnv* env, const char* className, JNINativeMethod* gMethods, int numMethods)
{
    jclass cls;
    cls = env->FindClass(className);
    if(cls == NULL) {
        return JNI_FALSE;
    }
    if (env->RegisterNatives(cls, gMethods, numMethods) < 0) {
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

static int registerNatives(JNIEnv* env)
{
    if(!registerNativeMethods(env, JNIREG_CLASS, gMethods, sizeof(gMethods) / sizeof(gMethods[0]))) {
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void * reserved) {
    jint result = -1;

    gJvm = vm;
    if(vm->GetEnv((void**)&gEnv, JNI_VERSION_1_6) != JNI_OK) goto bail;
    if(vm->AttachCurrentThread(&gEnv, NULL) < 0) goto bail;
    if(registerNatives(gEnv) != JNI_TRUE) goto bail;

    result = JNI_VERSION_1_6;

bail:
    return result;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved) {
    gJvm = NULL;
}

////////////////////////////////////////////////////
// JNI functions to export:                       //
////////////////////////////////////////////////////

static jclass gLwsServiceCls;
static jobject gLwsServiceObj;
static jmethodID sendMessageId;

static const int MSG_DUMB_INCREMENT_PROTOCOL_COUNTER = 1;
static const int MSG_LWS_CALLBACK_CLIENT_CONNECTION_ERROR = 2;
static const int MSG_LWS_CALLBACK_CLIENT_ESTABLISHED = 3;

#define BUFFER_SIZE 4096

static struct lws_context *context = NULL;
static struct lws_context_creation_info info;
static struct lws *wsi = NULL;

// prevents sending messages after jni_exitLws had been called
static int isExit = 0;

enum websocket_protocols {
  PROTOCOL_DUMB_INCREMENT = 0,
  PROTOCOL_COUNT
};

struct per_session_data {
  ;// no data
};

static int callback( struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len );

static struct lws_protocols protocols[] = {
  {
    "dumb-increment-protocol",
    callback,
    sizeof( struct per_session_data ),
    BUFFER_SIZE,
  },
  { NULL, NULL, 0, 0 } // end of list
};

static const struct lws_extension exts[] = {
  {
    "deflate-frame",
    lws_extension_callback_pm_deflate,
    "deflate_frame"
  },
  { NULL, NULL, NULL }
};

static int port = 0;
static int use_ssl = 0;
static int use_ssl_client = 0;
static char address[8192];

static char ca_cert[8192];
static char client_cert[8192];
static char client_cert_key[8192];

static int deny_deflate = 0;
static int deny_mux = 0;

// Logging function for libwebsockets
static void emit_log(int level, const char *msg)
{
    printf("%s", msg);
}


JNIEXPORT jboolean JNICALL jni_initLws(JNIEnv *env, jobject obj)
{
    if(context) return JNI_TRUE;

    // Attach the java virtual machine to this thread
    gJvm->AttachCurrentThread(&gEnv, NULL);

    // Set java global references to the class and object
    jclass cls = env->GetObjectClass(obj);
    gLwsServiceCls = (jclass) env->NewGlobalRef(cls);
    gLwsServiceObj = env->NewGlobalRef(obj);

    // Get the sendMessage method from the LwsService class (inherited from class ThreadService)
    sendMessageId = gEnv->GetMethodID(gLwsServiceCls, "sendMessage", "(ILjava/lang/Object;)V");

    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
#if !defined(LWS_WITHOUT_EXTENSIONS)
    info.extensions = exts;
#endif
    info.gid = -1;
    info.uid = -1;

    lws_set_log_level( LLL_NOTICE | LLL_INFO | LLL_ERR | LLL_WARN | LLL_CLIENT, emit_log );

    context = lws_create_context(&info);
    if( context == NULL ){
        emit_log(LLL_ERR, "Creating libwebsocket context failed");
        return JNI_FALSE;
    }

    isExit = 0;

    return JNI_TRUE;
}

// Send a message to the client of the service
// (must call jni_initLws() first)
static inline void sendMessage(int id, jobject obj)
{
  if(!isExit) gEnv->CallVoidMethod(gLwsServiceObj, sendMessageId, id, obj);
}

JNIEXPORT void JNICALL jni_exitLws(JNIEnv *env, jobject obj)
{
    if(context){
        isExit = 1;
        lws_context_destroy(context);
        context = NULL;
        env->DeleteGlobalRef(gLwsServiceObj);
        env->DeleteGlobalRef(gLwsServiceCls);
    }
}

static int callback(
  struct lws *wsi,
  enum lws_callback_reasons reason,
  void *user,
  void *in,
  size_t len
)
{
  switch(reason){

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
      sendMessage(MSG_LWS_CALLBACK_CLIENT_CONNECTION_ERROR, NULL);
      break;

    case LWS_CALLBACK_CLIENT_ESTABLISHED:
      sendMessage(MSG_LWS_CALLBACK_CLIENT_ESTABLISHED, NULL);
      break;

    case LWS_CALLBACK_CLIENT_RECEIVE:
        ((char *)in)[len] = '\0';
        sendMessage(MSG_DUMB_INCREMENT_PROTOCOL_COUNTER, gEnv->NewStringUTF((const char*)in));
        break;

    case LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED:
      if ((strcmp((const char*)in, "deflate-stream") == 0) && deny_deflate) {
        emit_log(LLL_ERR, "websocket: denied deflate-stream extension");
        return 1;
      }
      if ((strcmp((const char*)in, "deflate-frame") == 0) && deny_deflate) {
        emit_log(LLL_ERR, "websocket: denied deflate-frame extension");
        return 1;
      }
      if ((strcmp((const char*)in, "x-google-mux") == 0) && deny_mux) {
        emit_log(LLL_ERR, "websocket: denied x-google-mux extension");
        return 1;
      }
      break;

    default:
      break;
  }

  return 0;
}

JNIEXPORT void JNICALL jni_serviceLws(JNIEnv *env, jobject obj)
{
  if(context){
    lws_service( context, 0 );
  }
}

JNIEXPORT void JNICALL jni_setConnectionParameters(
  JNIEnv *env,
  jobject obj,
  jstring serverAddress,
  jint serverPort
)
{
  address[0] = 0;
  port = serverPort;
  use_ssl = 0;
  use_ssl_client = 0;
  snprintf(address, sizeof(address), "%s", env->GetStringUTFChars(serverAddress, 0));
}

JNIEXPORT jboolean JNICALL jni_connectLws(JNIEnv *env, jobject obj)
{
  struct lws_client_connect_info info_ws;
  memset(&info_ws, 0, sizeof(info_ws));

  info_ws.port = port;
  info_ws.address = address;
  info_ws.path = "/";
  info_ws.context = context;
  info_ws.ssl_connection = use_ssl;
  info_ws.host = address;
  info_ws.origin = address;
  info_ws.ietf_version_or_minus_one = -1;
  info_ws.client_exts = exts;
  info_ws.protocol = protocols[PROTOCOL_DUMB_INCREMENT].name;

  // connect
  wsi = lws_client_connect_via_info(&info_ws);
  if(wsi == NULL ){
    // Error
    emit_log(LLL_ERR, "Protocol failed to connect.");
    return JNI_FALSE;
  }

  return JNI_TRUE;
}
