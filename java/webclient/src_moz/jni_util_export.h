/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s): Ed Burns <edburns@acm.org>
 *
 */


/**

 * Exported Util methods, called from webclient uno.

 */

#ifndef jni_util_export_h
#define jni_util_export_h

#ifdef __cplusplus
extern "C" {
#endif

#include <jni.h>


JNIEXPORT const char * JNICALL util_GetStringUTFChars(JNIEnv *env, 
                                                      jstring inString);

JNIEXPORT void JNICALL util_ReleaseStringUTFChars(JNIEnv *env, 
                                                  jstring inString, 
                                                  const char *stringFromGet);

JNIEXPORT const jchar * JNICALL util_GetStringChars(JNIEnv *env, 
                                                    jstring inString);

JNIEXPORT void JNICALL util_ReleaseStringChars(JNIEnv *env, jstring inString, 
                                               const jchar *stringFromGet);

JNIEXPORT jstring JNICALL util_NewStringUTF(JNIEnv *env, 
                                            const char * inString);

JNIEXPORT  void JNICALL util_DeleteStringUTF(JNIEnv *env, jstring toDelete); 

JNIEXPORT jstring JNICALL util_NewString(JNIEnv *env, const jchar *inString, 
                                         jsize len);

JNIEXPORT  void JNICALL util_DeleteString(JNIEnv *env, jstring toDelete); 

//
// BAL methods
//

/*

 * The following methods are used by non Java JNI clients, such a
 * StarOfficeDesktop.

 */

/**

 * This method is used to store a mapping from a jniClass Name, such as
 * "org/mozilla/webclient/DocumentLoadListener" to some external class
 * type, such as
 * org::mozilla::webclient::wrapper_native::uno::DocumentLoadListener.

 * This table is used in util_IsInstanceOf.

 * @see util_SetInstanceOfFunction

 * @ret 0 on success

 */

JNIEXPORT jint JNICALL util_StoreClassMapping(const char* jniClassName,
                                              jclass yourClassType);

JNIEXPORT jclass JNICALL util_GetClassMapping(const char* jniClassName);


/**

 * Function declaration for the user defined InstanceOf function.  It
 * tells whether the second argument, which is an instance, is an
 * instance of the type in the third argument.

 * @see util_SetInstanceOfFunction

 */


typedef JNIEXPORT jboolean (JNICALL *fpInstanceOfType) (JNIEnv *env,
                                                        jobject obj,
                                                        jclass clazz);

/**

 * Function declaration for the user defined EventOccurred function.  It
 * is called when an event occurrs.  The second argument is the context
 * for the event, passed in by the user as the second argument to
 * NativeEventThreadImpl_nativeAddListener().  The third arcument is the
 * listener object, passed in as the last argument to
 * NativeEventThreadImpl_nativeAddListener().  The last argument is a
 * listener specific type field, to indicate what kind of sub-event
 * within the listener has occurred.

 */

typedef JNIEXPORT void (JNICALL * fpEventOccurredType) (JNIEnv *env, 
                                                        jobject nativeEventThread,
                                                        jobject webclientEventListener,
                                                        jlong eventType);

/**

 * This function must be called at app initialization.

 * @see fpInstanceOfType

 */

JNIEXPORT void JNICALL util_SetInstanceOfFunction(fpInstanceOfType fp);

/**

 * This function must be called at app initialization.

 * @see fpEventOccurredType

 */

JNIEXPORT void JNICALL util_SetEventOccurredFunction(fpEventOccurredType fp);

/**

 * defined in jni_util_export.cpp

 * The function pointer set with util_SetEventOccurredFunction.

 */

extern fpEventOccurredType externalEventOccurred;

/**

 * defined in jni_util_export.cpp

 * The function pointer set with util_SetInstanceOfFunction.

 */

extern fpInstanceOfType externalInstanceOf;

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif // jni_util_export_h
