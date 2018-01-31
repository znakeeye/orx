/* Orx - Portable Game Engine
 *
 * Copyright (c) 2008-2010 Orx-Project
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
 * @file 13_Text.c
 * @date 12/03/2017
 * @author sam@codedothing.com
 *
 * Object creation tutorial
 */

#include "orx.h"

/* This is a basic C tutorial creating a viewport and an object.
 *
 * As orx is data driven, here we just write 2 lines of code to create a viewport
 * and an object. All their properties are defined in the config file (13_Text.ini).
 * As a matter of fact, the viewport is associated with a camera implicitly created from the
 * info given in the config file. You can also set their sizes, positions, the object colors,
 * scales, rotations, animations, physical properties, and so on. You can even request
 * random values for these without having to add a single line of code.
 * In a later tutorial we'll see how to generate your whole scene (all background
 * and landscape objects for example) with a simple for loop written in 3 lines of code.
 *
 * For now, you can try to uncomment some of the lines of 13_Text.ini, play with them,
 * then relaunch this tutorial. For an exhaustive list of options, please look at CreationTemplate.ini.
 */

static orxOBJECT *pstScene = orxNULL;
static orxOBJECT *pstLabel = orxNULL;
static orxOBJECT *pstCurrentText = orxNULL;

void DebugText(const orxTEXT *_pstText)
{
  const orxSTRING zString = orxText_GetString(_pstText);
  orxLOG("String: %s", zString);
  orxLOG("Markers:");
  const orxTEXT_MARKER *pstMarkerArray = orxText_GetMarkerArray(_pstText);
  orxU32 u32MarkerCount = orxText_GetMarkerCounter(_pstText);
  orxU32 u32Index;
  for (u32Index = 0; u32Index < u32MarkerCount; u32Index++)
  {
    const orxTEXT_MARKER stMarker = pstMarkerArray[u32Index];
    switch (stMarker.stData.eType)
    {
    case orxTEXT_MARKER_TYPE_FONT:
      orxLOG("@%3u Font = (%p, %p, %f)", stMarker.u32Offset, stMarker.stData.stFontData.pstMap, stMarker.stData.stFontData.pstFont, stMarker.stData.stFontData.pstMap->fCharacterHeight);
      break;
    case orxTEXT_MARKER_TYPE_COLOR:
      orxLOG("@%3u Color = (%u, %u, %u)", stMarker.u32Offset, stMarker.stData.stRGBA.u8R, stMarker.stData.stRGBA.u8G, stMarker.stData.stRGBA.u8B);
      break;
    case orxTEXT_MARKER_TYPE_SCALE:
      orxLOG("@%3u Scale = (%f, %f, %f)", stMarker.u32Offset, stMarker.stData.vScale.fX, stMarker.stData.vScale.fY, stMarker.stData.vScale.fZ);
      break;
    case orxTEXT_MARKER_TYPE_DEFAULT:
      orxLOG("@%3u Default = (%d)", stMarker.u32Offset, stMarker.stData.eTypeOfDefault);
      break;
    case orxTEXT_MARKER_TYPE_LINE:
      orxLOG("@%3u Line Height = %f", stMarker.u32Offset, stMarker.stData.fLineHeight);
      break;
    default:
      orxLOG("@%3u Invalid Type", stMarker.u32Offset);
    }
  }
  orxU32 u32LineCount = orxText_GetLineCount(_pstText);
  orxLOG("Line Sizes:");
  for(u32Index = 0; u32Index < u32LineCount; u32Index++)
  {
    orxFLOAT fWidth, fHeight;
    orxSTATUS eResult = orxText_GetLineSize(_pstText, u32Index, &fWidth, &fHeight);
    orxLOG("#%3u Line Size = (%f, %f)", u32Index, fWidth, fHeight);
  }
}

void ResetText()
{
  if (pstCurrentText != orxNULL)
  {
    orxObject_SetLifeTime(pstCurrentText, orxFLOAT_0);
    pstCurrentText = orxObject_CreateFromConfig(orxObject_GetName(pstCurrentText));
  }
}

/* TODO Find a better way of writing this */
void CycleText(orxBOOL _bNext)
{
  orxLOG("Cycling to %s text object", (_bNext ? "next" : "previous"));
  static orxOBJECT *pstObject = orxNULL;
  static orxS32     s32Index = -1; /* We start at negative one so it increments to 0 on startup */
  s32Index += (_bNext ? 1 : -1);
  orxConfig_PushSection("Scene");
  orxU32 u32Size = orxConfig_GetListCounter("TextList");
  orxConfig_PopSection();
  if(s32Index < 0)
  {
    s32Index = u32Size - 1;
  }
  s32Index = s32Index % u32Size;
  orxLOG("Index is now %d", s32Index);
  orxConfig_PushSection("Scene");
  const orxSTRING zObjectName = orxConfig_GetListString("TextList", s32Index);
  orxConfig_PopSection();
  if (orxConfig_HasSection(zObjectName))
  {
    orxObject_SetTextString(pstLabel, zObjectName);
    orxLOG("Text object will be %s", zObjectName);
    if (pstCurrentText != orxNULL)
    {
      orxObject_SetLifeTime(pstCurrentText, orxFLOAT_0);
    }
    pstCurrentText = orxObject_CreateFromConfig(zObjectName);
    /* Output debug data */
    orxGRAPHIC *pstGraphic = orxGRAPHIC(orxOBJECT_GET_STRUCTURE(pstCurrentText, GRAPHIC)) ;
    orxSTRUCTURE *pstStructure = orxGraphic_GetData(pstGraphic);
    orxTEXT *pstText = orxTEXT(pstStructure);
    DebugText(pstText);
  }
}

orxSTATUS orxFASTCALL ConfigEventHandler(const orxEVENT *_pstEvent) {
  orxSTATUS eResult = orxSTATUS_SUCCESS;
  if (_pstEvent->eID == orxRESOURCE_EVENT_UPDATE)
  {
    ResetText();
  }
  return eResult;
}

/** Inits the tutorial
 */
orxSTATUS orxFASTCALL Init()
{
  /* Displays a small hint in console */
  orxLOG("\n* This tutorial creates a viewport/camera couple and multiple objects that display text"
         "\n* You can play with the config parameters in ../13_Text.ini"
         "\n* After changing them, relaunch the tutorial to see their effects");

  orxEvent_AddHandler(orxEVENT_TYPE_RESOURCE, ConfigEventHandler);

  /* Creates viewport */
  orxViewport_CreateFromConfig("Viewport");

  /* Creates object */
  pstScene = orxObject_CreateFromConfig("Scene");
  pstLabel = orxObject_CreateFromConfig("Label");

  CycleText(orxTRUE);

  /* Done! */
  return orxSTATUS_SUCCESS;
}
/** Run function
 */
orxSTATUS orxFASTCALL Run()
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Cycle next? */
  if (orxInput_IsActive("Next") && orxInput_HasNewStatus("Next"))
  {
    orxLOG("NEXT");
    CycleText(orxTRUE);
  }

  /* Cycle previous? */
  if (orxInput_IsActive("Prev") && orxInput_HasNewStatus("Prev"))
  {
    orxLOG("PREVIOUS");
    CycleText(orxFALSE);
  }

  /* Screenshot? */
  if (orxInput_IsActive("Screenshot") && orxInput_HasNewStatus("Screenshot"))
  {
    /* Captures it */
    orxScreenshot_Capture();
  }

  /* Should quit? */
  if (orxInput_IsActive("Quit"))
  {
    /* Updates result */
    eResult = orxSTATUS_FAILURE;
  }

  /* Done! */
  return eResult;
}

/** Exit function
 */
void orxFASTCALL Exit()
{
  /* We're a bit lazy here so we let orx clean all our mess! :) */
}

/** Main function
 */
int main(int argc, char **argv)
{
  /* Executes a new instance of tutorial */
  orx_Execute(argc, argv, Init, Run, Exit);

  return EXIT_SUCCESS;
}


#ifdef __orxMSVC__

// Here's an example for a console-less program under windows with visual studio
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  // Inits and executes orx
  orx_WinExecute(Init, Run, Exit);

  // Done!
  return EXIT_SUCCESS;
}

#endif // __orxMSVC__
