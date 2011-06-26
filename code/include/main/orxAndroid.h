/* Orx - Portable Game Engine
 *
 * Copyright (c) 2008-2011 Orx-Project
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 *    1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 *
 *    2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 *    3. This notice may not be removed or altered from any source
 *    distribution.
 */

/**
 * @file orxAndroid.h
 * @date 26/06/2011
 * @author iarwain@orx-project.org
 *
 * @todo
 */

/**
 * @addtogroup orxAndroid
 * 
 * Android support module
 *
 * @{
 */


#ifndef _orxANDROID_H_
#define _orxANDROID_H_

#if defined(__orxANDROID__)

#include <jni.h>
#include <android/log.h>

/* defined in orxModule.c */
extern orxMODULE_RUN_FUNCTION  spfnRun;
extern orxMODULE_EXIT_FUNCTION spfnExit;
extern orxMODULE_INIT_FUNCTION spfnInit;
extern orxSYSTEM_EVENT_PAYLOAD sstPayload;

/* defined in orxDisplay.c */
extern int32_t s32DisplayWidth;
extern int32_t s32DisplayHeight;

/* defined in orxAndSupport.c */
extern jobject     oActivity;
extern JNIEnv     *poJEnv;
extern orxS32      s32NbParams;
extern orxSTRING  *azParams;

void orxAndroid_GetMainArgs();
void orxAndroid_ReleaseMainArgs();


static orxINLINE void orx_Init()
{
  orxASSERT(spfnRun != orxNULL);

  orxDEBUG_INIT();
  
  /* retrieve orx cmd-line arguments */
  orxAndroid_GetMainArgs();
  
  /* Registers main module */
  orxModule_Register(orxMODULE_ID_MAIN, orx_MainSetup, spfnInit, spfnExit);

  /* Registers all other modules */
  orxModule_RegisterAll();

  /* Calls all modules setup */
  orxModule_SetupAll();

  /* Sends the command line arguments to orxParam module */
  if(orxParam_SetArgs(s32NbParams, azParams) != orxSTATUS_FAILURE)
  {
    /* Inits the engine */
    if(orxModule_Init(orxMODULE_ID_MAIN) != orxSTATUS_FAILURE)
    {
      /* Registers default event handler */
      orxEvent_AddHandler(orxEVENT_TYPE_SYSTEM, orx_DefaultEventHandler);

      /* Displays help */
      if(orxParam_DisplayHelp() != orxSTATUS_FAILURE)
      {
        /* Clears payload */
        orxMemory_Zero(&sstPayload, sizeof(orxSYSTEM_EVENT_PAYLOAD));
      }
    }
  }

  /* Inits stop condition */
  sbStopByEvent = orxFALSE;
}

static orxINLINE orxBOOL orx_Step()
{
  orxSTATUS eClockStatus, eMainStatus;
  orxBOOL   bStop;
  
  /* Sends frame start event */
  orxEVENT_SEND(orxEVENT_TYPE_SYSTEM, orxSYSTEM_EVENT_GAME_LOOP_START, orxNULL, orxNULL, &sstPayload);

  /* Runs the engine */
  eMainStatus = spfnRun();

  /* Updates clock system */
  eClockStatus = orxClock_Update();

  /* Sends frame stop event */
  orxEVENT_SEND(orxEVENT_TYPE_SYSTEM, orxSYSTEM_EVENT_GAME_LOOP_STOP, orxNULL, orxNULL, &sstPayload);

  /* Updates frame counter */
  sstPayload.u32FrameCounter++;

  /* Updates stop condition */
  bStop = ((sbStopByEvent != orxFALSE) || (eMainStatus == orxSTATUS_FAILURE) || (eClockStatus == orxSTATUS_FAILURE)) ? orxTRUE : orxFALSE;

  /* Done! */
  return bStop;
}

static orxINLINE void orx_Exit()
{
  /* Removes event handler */
  orxEvent_RemoveHandler(orxEVENT_TYPE_SYSTEM, orx_DefaultEventHandler);

  /* Exits from engine */
  orxModule_Exit(orxMODULE_ID_MAIN);

  /* Exits from all modules */
  orxModule_ExitAll();

  /* Exits from the Debug system */
  orxDEBUG_EXIT();
  
  /* free cmd-line arguments */
  orxAndroid_ReleaseMainArgs();
}

  #ifdef __cplusplus
  extern "C" {
  #endif

    JNIEXPORT void JNICALL Java_org_orx_android_OrxLib_init(JNIEnv * env, jobject obj, jobject activity, jint width, jint height);
    JNIEXPORT jboolean JNICALL Java_org_orx_android_OrxLib_step(JNIEnv * env, jobject obj);
    JNIEXPORT void JNICALL Java_org_orx_android_OrxLib_exit(JNIEnv * env, jobject obj);
    JNIEXPORT void JNICALL Java_org_orx_android_OrxLib_send_1orxSYSTEM_1EVENT_1CLOSE(JNIEnv * env, jobject obj);
    JNIEXPORT void JNICALL Java_org_orx_android_OrxLib_send_1orxDISPLAY_1EVENT_1SAVE_1CONTEXT(JNIEnv * env, jobject obj);
    JNIEXPORT void JNICALL Java_org_orx_android_OrxLib_send_1orxDISPLAY_1EVENT_1RESTORE_1CONTEXT(JNIEnv * env, jobject obj);
    JNIEXPORT void JNICALL Java_org_orx_android_OrxLib_onNativeTouch(JNIEnv* env, jobject obj, jint action, int pointId, jfloat x, jfloat y, jfloat p);
    JNIEXPORT void JNICALL Java_org_orx_android_OrxLib_onNativeAccel(JNIEnv* env, jobject obj, jfloat x, jfloat y, jfloat z);
    JNIEXPORT void JNICALL Java_org_orx_android_OrxLib_send_1orxSYSTEM_1EVENT_1BACKGROUND(JNIEnv* env, jobject obj);
    JNIEXPORT void JNICALL Java_org_orx_android_OrxLib_send_1orxSYSTEM_1EVENT_1FOREGROUND(JNIEnv* env, jobject obj);
    
  #ifdef __cplusplus
  };
  #endif

JNIEXPORT void JNICALL Java_org_orx_android_OrxLib_init(JNIEnv * env, jobject obj, jobject activity, jint width, jint height)
{
	DisplayWidth = s32width;
	DisplayHeight = s32height;
	
	/* save env and activity */
	poJEnv = env;
  #ifdef __cplusplus
    oActivity = env->NewGlobalRef(activity);
  #else /* __cplusplus */
	  oActivity = (*env)->NewGlobalRef(env, activity);
  #endif /* __cplusplus */
	
	orx_Init();
}

JNIEXPORT jboolean JNICALL Java_org_orx_android_OrxLib_step(JNIEnv * env, jobject obj)
{
	return (orx_Step() == orxTRUE ? JNI_FALSE : JNI_TRUE);
}

JNIEXPORT void JNICALL Java_org_orx_android_OrxLib_exit(JNIEnv * env, jobject obj)
{
  
	orx_Exit();
	
	/* release oActivity reference */
#ifdef __cplusplus
  env->DeleteGlobalRef(oActivity);
#else
	(*env)->DeleteGlobalRef(env, oActivity);
#endif
}

JNIEXPORT void JNICALL Java_org_orx_android_OrxLib_send_1orxSYSTEM_1EVENT_1CLOSE(JNIEnv * env, jobject obj)
{
  orxEvent_SendShort(orxEVENT_TYPE_SYSTEM, orxSYSTEM_EVENT_CLOSE);
}

JNIEXPORT void JNICALL Java_org_orx_android_OrxLib_send_1orxDISPLAY_1EVENT_1SAVE_1CONTEXT(JNIEnv * env, jobject obj)
{
  orxEvent_SendShort(orxEVENT_TYPE_DISPLAY, orxDISPLAY_EVENT_SAVE_CONTEXT);
}

JNIEXPORT void JNICALL Java_org_orx_android_OrxLib_send_1orxDISPLAY_1EVENT_1RESTORE_1CONTEXT(JNIEnv * env, jobject obj)
{
  orxEvent_SendShort(orxEVENT_TYPE_DISPLAY, orxDISPLAY_EVENT_RESTORE_CONTEXT);
}

JNIEXPORT void JNICALL Java_org_orx_android_OrxLib_onNativeTouch(JNIEnv* env, jobject obj,	jint action, int pointId, jfloat x, jfloat y, jfloat p)
{
	orxSYSTEM_EVENT_PAYLOAD stPayload;

	orxSYSTEM_EVENT android_event;
	switch (action) {
	case 0:
		android_event = orxSYSTEM_EVENT_TOUCH_BEGIN;
		break;
	case 1:
		android_event = orxSYSTEM_EVENT_TOUCH_MOVE;
		break;
	case 2:
		android_event = orxSYSTEM_EVENT_TOUCH_END;
		break;
	default:
		//error.....
		return;
	}

	/* Inits event's payload */
	orxMemory_Zero(&stPayload, sizeof(orxSYSTEM_EVENT_PAYLOAD));
	stPayload.stTouch.u32ID = (orxU32) pointId;
	stPayload.stTouch.fX = (orxFLOAT) x;
	stPayload.stTouch.fY = (orxFLOAT) y;
	stPayload.stTouch.fPressure = (orxFLOAT) p;

	/* Sends it */
	orxEVENT_SEND(orxEVENT_TYPE_SYSTEM, android_event, orxNULL, orxNULL, &stPayload);
}

JNIEXPORT void JNICALL Java_org_orx_android_OrxLib_onNativeAccel(JNIEnv* env, jobject obj, jfloat x, jfloat y, jfloat z)
{
  orxSYSTEM_EVENT_PAYLOAD stPayload;

	/* Inits event's payload */
	orxMemory_Zero(&stPayload, sizeof(orxSYSTEM_EVENT_PAYLOAD));
	stPayload.stAccelerometer.pAccelerometer = orxNULL;
	stPayload.stAccelerometer.fX = (orxFLOAT) x;
	stPayload.stAccelerometer.fY = (orxFLOAT) y;
	stPayload.stAccelerometer.fZ = (orxFLOAT) z;

	/* Sends it */
	orxEVENT_SEND(orxEVENT_TYPE_SYSTEM, orxSYSTEM_EVENT_ACCELERATE,	orxNULL, orxNULL, &stPayload);
}

JNIEXPORT void JNICALL Java_org_orx_android_OrxLib_send_1orxSYSTEM_1EVENT_1BACKGROUND(JNIEnv * env, jobject obj)
{
  orxEvent_SendShort(orxEVENT_TYPE_SYSTEM, orxSYSTEM_EVENT_BACKGROUND);
}

JNIEXPORT void JNICALL Java_org_orx_android_OrxLib_send_1orxSYSTEM_1EVENT_1FOREGROUND(JNIEnv * env, jobject obj)
{
  orxEvent_SendShort(orxEVENT_TYPE_SYSTEM, orxSYSTEM_EVENT_FOREGROUND);
}

#elif defined (__orxANDROID_NATIVE__)


#endif

/** @} */
