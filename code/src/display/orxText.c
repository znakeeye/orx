/* Orx - Portable Game Engine
 *
 * Copyright (c) 2008-2017 Orx-Project
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
 * @file orxText.c
 * @date 02/12/2008
 * @author iarwain@orx-project.org
 *
 */


#include "display/orxText.h"

#include "memory/orxMemory.h"
#include "core/orxConfig.h"
#include "core/orxEvent.h"
#include "core/orxLocale.h"
#include "core/orxResource.h"
#include "math/orxVector.h"
#include "object/orxStructure.h"
#include "utils/orxHashTable.h"
#include "utils/orxLinkList.h"


/** Module flags
 */
#define orxTEXT_KU32_STATIC_FLAG_NONE         0x00000000  /**< No flags */

#define orxTEXT_KU32_STATIC_FLAG_READY        0x00000001  /**< Ready flag */

#define orxTEXT_KU32_STATIC_MASK_ALL          0xFFFFFFFF  /**< All mask */

/** orxTEXT flags / masks
 */
#define orxTEXT_KU32_FLAG_NONE                0x00000000  /**< No flags */

#define orxTEXT_KU32_FLAG_INTERNAL            0x10000000  /**< Internal structure handling flag */
#define orxTEXT_KU32_FLAG_FIXED_WIDTH         0x00000001  /**< Fixed width flag */
#define orxTEXT_KU32_FLAG_FIXED_HEIGHT        0x00000002  /**< Fixed height flag */

#define orxTEXT_KU32_MASK_ALL                 0xFFFFFFFF  /**< All mask */


/** Misc defines
 */
#define orxTEXT_KZ_CONFIG_STRING              "String"
#define orxTEXT_KZ_CONFIG_FONT                "Font"

#define orxTEXT_KC_LOCALE_MARKER              '$'

#define orxTEXT_KZ_MARKER_WARNING             "Invalid text marker [%c%s%s] in [%s]!"
#define orxTEXT_KC_MARKER_SYNTAX_START        '`'
/* TODO orxSTRING has alt open/close chars and this should probably follow suit */
/* TODO it would be good to add a generic argument walker to orxString to ease the walking of delimited strings */
#define orxTEXT_KC_MARKER_SYNTAX_OPEN         '('
#define orxTEXT_KC_MARKER_SYNTAX_CLOSE        ')'
#define orxTEXT_KZ_MARKER_TYPE_FONT           "font"
#define orxTEXT_KZ_MARKER_TYPE_COLOR          "color"
#define orxTEXT_KZ_MARKER_TYPE_SCALE          "scale"
#define orxTEXT_KZ_MARKER_TYPE_POP            "!"
#define orxTEXT_KZ_MARKER_TYPE_CLEAR          "*"

#define orxTEXT_KU32_BANK_SIZE                256         /**< Bank size */
#define orxTEXT_KU32_MARKER_CELL_BANK_SIZE    128         /**< Bank size */
#define orxTEXT_KU32_MARKER_DATA_BANK_SIZE    128         /**< Bank size */


/***************************************************************************
 * Structure declaration                                                   *
 ***************************************************************************/

/** Text structure
 */
struct __orxTEXT_t
{
  orxSTRUCTURE      stStructure;                /**< Public structure, first structure member : 40 / 64 */
  orxSTRING         zString;                    /**< String : 44 / 72 */
  orxFONT          *pstFont;                    /**< Font : 48 / 80 */
  orxTEXT_MARKER   *pstMarkerArray;
  orxU32            u32MarkerCounter;
  orxFLOAT          fWidth;                     /**< Width : 52 / 84 */
  orxFLOAT          fHeight;                    /**< Height : 56 / 88 */
  const orxSTRING   zReference;                 /**< Config reference : 60 / 96 */
  orxSTRING         zOriginalString;            /**< Original string : 64 / 104 */
};

/** Static structure
 */
typedef struct __orxTEXT_STATIC_t
{
  orxU32            u32Flags;                   /**< Control flags */

} orxTEXT_STATIC;


/***************************************************************************
 * Module global variable                                                  *
 ***************************************************************************/

static orxTEXT_STATIC sstText;


/***************************************************************************
 * Private functions                                                       *
 ***************************************************************************/

/** Gets corresponding locale key
 * @param[in]   _pstText    Concerned text
 * @param[in]   _zProperty  Property to get
 * @return      orxSTRING / orxNULL
 */
static orxINLINE const orxSTRING orxText_GetLocaleKey(const orxTEXT *_pstText, const orxSTRING _zProperty)
{
  const orxSTRING zResult = orxNULL;

  /* Checks */
  orxSTRUCTURE_ASSERT(_pstText);

  /* Has reference? */
  if(_pstText->zReference != orxNULL)
  {
    const orxSTRING zString;

    /* Pushes its section */
    orxConfig_PushSection(_pstText->zReference);

    /* Gets its string */
    zString = orxConfig_GetString(_zProperty);

    /* Valid? */
    if(zString != orxNULL)
    {
      /* Begins with locale marker? */
      if((*zString == orxTEXT_KC_LOCALE_MARKER) && (*(zString + 1) != orxTEXT_KC_LOCALE_MARKER))
      {
        /* Updates result */
        zResult = zString + 1;
      }
    }

    /* Pops config section */
    orxConfig_PopSection();
  }

  /* Done! */
  return zResult;
}

static orxSTATUS orxFASTCALL orxText_ProcessConfigData(orxTEXT *_pstText)
{
  const orxSTRING zString;
  const orxSTRING zName;
  orxSTATUS       eResult = orxSTATUS_FAILURE;

  /* Pushes its config section */
  orxConfig_PushSection(_pstText->zReference);

  /* Gets font name */
  zName = orxConfig_GetString(orxTEXT_KZ_CONFIG_FONT);

  /* Begins with locale marker? */
  if(*zName == orxTEXT_KC_LOCALE_MARKER)
  {
    /* Gets its locale value */
    zName = (*(zName + 1) == orxTEXT_KC_LOCALE_MARKER) ? zName + 1 : orxLocale_GetString(zName + 1);
  }

  /* Valid? */
  if((zName != orxNULL) && (zName != orxSTRING_EMPTY))
  {
    orxFONT *pstFont;

    /* Creates font */
    pstFont = orxFont_CreateFromConfig(zName);

    /* Valid? */
    if(pstFont != orxNULL)
    {
      /* Stores it */
      if(orxText_SetFont(_pstText, pstFont) != orxSTATUS_FAILURE)
      {
        /* Sets its owner */
        orxStructure_SetOwner(pstFont, _pstText);

        /* Updates flags */
        orxStructure_SetFlags(_pstText, orxTEXT_KU32_FLAG_INTERNAL, orxTEXT_KU32_FLAG_NONE);
      }
      else
      {
        /* Logs message */
        orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Couldn't set font (%s) for text (%s).", zName, _pstText->zReference);

        /* Sets default font */
        orxText_SetFont(_pstText, orxFONT(orxFont_GetDefaultFont()));
      }
    }
    else
    {
      /* Logs message */
      orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Couldn't create font (%s) for text (%s).", zName, _pstText->zReference);

      /* Sets default font */
      orxText_SetFont(_pstText, orxFONT(orxFont_GetDefaultFont()));
    }
  }
  else
  {
    /* Sets default font */
    orxText_SetFont(_pstText, orxFONT(orxFont_GetDefaultFont()));
  }

  /* Gets its string */
  zString = orxConfig_GetString(orxTEXT_KZ_CONFIG_STRING);

  /* Begins with locale marker? */
  if(*zString == orxTEXT_KC_LOCALE_MARKER)
  {
    /* Stores its locale value */
    eResult = orxText_SetString(_pstText, (*(zString + 1) == orxTEXT_KC_LOCALE_MARKER) ? zString + 1 : orxLocale_GetString(zString + 1));
  }
  else
  {
    /* Stores raw text */
    eResult = orxText_SetString(_pstText, zString);
  }

  /* Pops config section */
  orxConfig_PopSection();

  /* Done! */
  return eResult;
}

/** Event handler
 * @param[in]   _pstEvent                     Sent event
 * @return      orxSTATUS_SUCCESS if handled / orxSTATUS_FAILURE otherwise
 */
static orxSTATUS orxFASTCALL orxText_EventHandler(const orxEVENT *_pstEvent)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Locale? */
  if(_pstEvent->eType == orxEVENT_TYPE_LOCALE)
  {
    /* Select language event? */
    if(_pstEvent->eID == orxLOCALE_EVENT_SELECT_LANGUAGE)
    {
      orxTEXT *pstText;

      /* For all texts */
      for(pstText = orxTEXT(orxStructure_GetFirst(orxSTRUCTURE_ID_TEXT));
          pstText != orxNULL;
          pstText = orxTEXT(orxStructure_GetNext(pstText)))
      {
        const orxSTRING zLocaleKey;

        /* Gets its corresponding locale string */
        zLocaleKey = orxText_GetLocaleKey(pstText, orxTEXT_KZ_CONFIG_STRING);

        /* Valid? */
        if(zLocaleKey != orxNULL)
        {
          const orxSTRING zText;

          /* Gets its localized value */
          zText = orxLocale_GetString(zLocaleKey);

          /* Valid? */
          if(*zText != orxCHAR_NULL)
          {
            /* Updates text */
            orxText_SetString(pstText, zText);
          }
        }

        /* Gets its corresponding locale font */
        zLocaleKey = orxText_GetLocaleKey(pstText, orxTEXT_KZ_CONFIG_FONT);

        /* Valid? */
        if(zLocaleKey != orxNULL)
        {
          orxFONT *pstFont;

          /* Creates font */
          pstFont = orxFont_CreateFromConfig(orxLocale_GetString(zLocaleKey));

          /* Valid? */
          if(pstFont != orxNULL)
          {
            /* Updates text */
            if(orxText_SetFont(pstText, pstFont) != orxSTATUS_FAILURE)
            {
              /* Sets its owner */
              orxStructure_SetOwner(pstFont, pstText);

              /* Updates flags */
              orxStructure_SetFlags(pstText, orxTEXT_KU32_FLAG_INTERNAL, orxTEXT_KU32_FLAG_NONE);
            }
            else
            {
              /* Sets default font */
              orxText_SetFont(pstText, orxFONT(orxFont_GetDefaultFont()));
            }
          }
        }
      }
    }
  }
  /* Resource */
  else
  {
    /* Checks */
    orxASSERT(_pstEvent->eType == orxEVENT_TYPE_RESOURCE);

    /* Add or update? */
    if((_pstEvent->eID == orxRESOURCE_EVENT_ADD) || (_pstEvent->eID == orxRESOURCE_EVENT_UPDATE))
    {
      orxRESOURCE_EVENT_PAYLOAD *pstPayload;

      /* Gets payload */
      pstPayload = (orxRESOURCE_EVENT_PAYLOAD *)_pstEvent->pstPayload;

      /* Is config group? */
      if(pstPayload->u32GroupID == orxString_ToCRC(orxCONFIG_KZ_RESOURCE_GROUP))
      {
        orxTEXT *pstText;

        /* For all texts */
        for(pstText = orxTEXT(orxStructure_GetFirst(orxSTRUCTURE_ID_TEXT));
            pstText != orxNULL;
            pstText = orxTEXT(orxStructure_GetNext(pstText)))
        {
          /* Match origin? */
          if(orxConfig_GetOriginID(pstText->zReference) == pstPayload->u32NameID)
          {
            /* Re-processes its config data */
            orxText_ProcessConfigData(pstText);
          }
        }
      }
    }
  }

  /* Done! */
  return eResult;
}

/** Marker node
 *  Used specifically for dry run of marker traversal
 */
typedef struct __orxTEXT_MARKER_NODE_t
{
  orxLINKLIST_NODE      stNode;
  orxU32                u32MarkerTally;
  const orxTEXT_MARKER *pstMarker;
} orxTEXT_MARKER_NODE;

typedef struct __orxTEXT_MARKER_PARSER_CONTEXT_t
{
  orxU32    u32OutputSize;           /** The size of the output string */
  orxU32    u32CharacterCodePoint;   /** The current codepoint to analyze */
  orxSTRING zOutputString;           /** The start of the output string so that pointer arithmetic can be performed to attain byte offset */
  orxSTRING zPositionInMarkedString; /** Cursor for the next codepoint in the marked string */
  orxSTRING zPositionInOutputString; /** Cursor for the next empty space in the output string */
} orxTEXT_MARKER_PARSER_CONTEXT;

static orxBOOL orxText_TryParseMarkerType(const orxSTRING _zString, const orxSTRING _zCheckAgainst, const orxSTRING *_pzRemaining)
{
  orxBOOL bResult = orxFALSE;
  orxU32 u32Length = orxString_GetLength(_zCheckAgainst);
  /* Is it the type we're checking against? */
  if (orxString_NCompare(_zString, _zCheckAgainst, u32Length) == 0)
  {
    bResult = orxTRUE;
    /* Store remainder if requested */
    if (_pzRemaining != orxNULL)
    {
      *_pzRemaining = _zString + u32Length;
    }
  }
  return bResult;
}

static orxTEXT_MARKER_TYPE orxText_ParseMarkerType(const orxSTRING _zString, const orxSTRING *_pzRemaining)
{
  orxASSERT(_zString != orxNULL);
  orxTEXT_MARKER_TYPE eType = orxTEXT_MARKER_TYPE_NONE;
  /* Check for valid marker types */
  if (orxText_TryParseMarkerType(_zString, orxTEXT_KZ_MARKER_TYPE_FONT, _pzRemaining))
  {
    eType = orxTEXT_MARKER_TYPE_FONT;
  }
  else if (orxText_TryParseMarkerType(_zString, orxTEXT_KZ_MARKER_TYPE_COLOR, _pzRemaining))
  {
    eType = orxTEXT_MARKER_TYPE_COLOR;
  }
  else if (orxText_TryParseMarkerType(_zString, orxTEXT_KZ_MARKER_TYPE_SCALE, _pzRemaining))
  {
    eType = orxTEXT_MARKER_TYPE_SCALE;
  }
  else if (orxText_TryParseMarkerType(_zString, orxTEXT_KZ_MARKER_TYPE_POP, _pzRemaining))
  {
    eType = orxTEXT_MARKER_TYPE_POP;
  }
  else if (orxText_TryParseMarkerType(_zString, orxTEXT_KZ_MARKER_TYPE_CLEAR, _pzRemaining))
  {
    eType = orxTEXT_MARKER_TYPE_CLEAR;
  }
  else
  {
    /* If no valid types were found this will return the NONE type */
    orxASSERT(eType == orxTEXT_MARKER_TYPE_NONE);
  }
  return eType;
}

static orxTEXT_MARKER_DATA orxText_ParseMarkerValue(orxTEXT_MARKER_TYPE _eType, const orxSTRING _zString, const orxSTRING *_pzRemaining)
{
  orxASSERT(_zString != orxNULL);
  orxTEXT_MARKER_DATA stResult;
  orxMemory_Zero(&stResult, sizeof(stResult));

  stResult.eType = _eType;

  if (_eType == orxTEXT_MARKER_TYPE_FONT)
  {
    /* Is a marker value start character? */
    if (*_zString != orxTEXT_KC_MARKER_SYNTAX_OPEN)
    {
      stResult.eType = orxTEXT_MARKER_TYPE_NONE;
    }
    else
    {
      const orxSTRING zValueStart = _zString + 1;
      const orxSTRING zStringClose = orxString_SearchChar(zValueStart, orxTEXT_KC_MARKER_SYNTAX_CLOSE);
      if (zStringClose == orxNULL)
      {
        stResult.eType = orxTEXT_MARKER_TYPE_NONE;
      }
      else
      {
        /* Make a temporary string to hold the value */
        orxU32 u32ValueStringLength = (zStringClose - zValueStart);
#ifdef __orxMSVC__
        orxCHAR *zValueString = (orxCHAR *)alloca(u32ValueStringLength + 1);
#else /* __orxMSVC__ */
        orxCHAR zValueString[u32ValueStringLength + 1];
#endif /* __orxMSVC__ */
        orxString_NCopy(zValueString, zValueStart, u32ValueStringLength);
        zValueString[u32ValueStringLength] = orxCHAR_NULL;
        /* Try to read the font */
        orxFONT *pstFont = orxFont_CreateFromConfig(zValueString);
        if (pstFont == orxNULL)
        {
          stResult.eType = orxTEXT_MARKER_TYPE_NONE;
          /* Log warning */
          orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Invalid font section name [%s] found for font marker!", zValueString);
        }
        else
        {
          stResult.stFontData.pstMap = orxFont_GetMap(pstFont);
          stResult.stFontData.pstFont = orxTexture_GetBitmap(orxFont_GetTexture(pstFont));
          /* Advance to character after the marker if asked for */
          if (_pzRemaining != orxNULL)
          {
            *_pzRemaining = zStringClose + 1;
          }
        }
      }
    }
  }
  else if (_eType == orxTEXT_MARKER_TYPE_COLOR)
  {
    orxVECTOR vColor = {0};
    orxSTATUS eStatus = orxString_ToVector(_zString, &vColor, _pzRemaining);
    if (eStatus == orxSTATUS_FAILURE)
    {
      stResult.eType = orxTEXT_MARKER_TYPE_NONE;
    }
    else
    {
      orxVector_Mulf(&vColor, &vColor, orxCOLOR_NORMALIZER);
      orxCOLOR stColor = {vColor, 1.0f};
      stResult.stRGBA = orxColor_ToRGBA(&stColor);
    }
  }
  else if (_eType == orxTEXT_MARKER_TYPE_SCALE)
  {
    orxVECTOR vScale = {0};
    orxSTATUS eStatus = orxString_ToVector(_zString, &vScale, _pzRemaining);
    if (eStatus == orxSTATUS_FAILURE)
    {
      stResult.eType = orxTEXT_MARKER_TYPE_NONE;
    }
    else
    {
      stResult.vScale = vScale;
    }
  }
  else if (_eType == orxTEXT_MARKER_TYPE_POP)
  {
    /* Currently marker stack manipulators possess no arguments, so trivially succeed */
  }
  else if (_eType == orxTEXT_MARKER_TYPE_CLEAR)
  {
    /* Currently marker stack manipulators possess no arguments, so trivially succeed */
  }
  else
  {
    orxASSERT(orxFALSE && "Invalid marker type [%d]!", _eType);
  }

  return stResult;
}

static orxTEXT_MARKER * orxFASTCALL orxText_CreateMarker(orxBANK *_pstMarkerBank, orxU32 _u32Offset, orxTEXT_MARKER_DATA _stData)
{
  orxTEXT_MARKER *pstResult = (orxTEXT_MARKER *) orxBank_Allocate(_pstMarkerBank);
  if (pstResult != orxNULL)
  {
    pstResult->u32Offset = _u32Offset;
    pstResult->stData    = _stData;
  }
  return pstResult;
}

static orxTEXT_MARKER * orxFASTCALL orxText_ConvertBankToArray(orxBANK *_pstMarkerBank, orxU32 *_pstArraySizeOut)
{
  orxASSERT(_pstMarkerBank != orxNULL);
  orxASSERT(_pstArraySizeOut != orxNULL);
  orxTEXT_MARKER *pstMarkerArray = orxNULL;
  orxU32 u32MarkerCounter = orxBank_GetCounter(_pstMarkerBank);
  orxU32 u32ArraySize = u32MarkerCounter * sizeof(orxTEXT_MARKER);
  if (u32ArraySize > 0)
  {
    pstMarkerArray = (orxTEXT_MARKER *) orxMemory_Allocate(u32ArraySize, orxMEMORY_TYPE_MAIN);
    for (orxU32 u32Index = 0; u32Index < u32MarkerCounter; u32Index++)
    {
      orxTEXT_MARKER *pstMarker = (orxTEXT_MARKER *) orxBank_GetAtIndex(_pstMarkerBank, u32Index);
      orxASSERT(pstMarker && "There was a rift in the marker bank at index [%u]! Should be impossible.", u32Index);
      orxMemory_Copy(&pstMarkerArray[u32Index], pstMarker, sizeof(orxTEXT_MARKER));
    }
  }
  *_pstArraySizeOut = u32MarkerCounter;
  return pstMarkerArray;
}

static orxU32 orxFASTCALL orxText_WalkCodePoint(orxSTRING *_pzCursor)
{
  orxASSERT(_pzCursor != orxNULL);
  return orxString_GetFirstCharacterCodePoint(*_pzCursor, (const orxSTRING *)_pzCursor);
}

/** Attempt to parse a marker out of the string. If it isn't a marker, store the relevant characters in an output string.
 * @param[in]   _pstMarkerBank       The bank to use for marker allocation.
 * @param[in]   _pstParserContext    The parser context.
 * @return orxTEXT_MARKER pointer if a marker is found / orxNULL if not
 */
static orxTEXT_MARKER * orxFASTCALL orxText_TryParseMarker(orxBANK *_pstMarkerBank, orxTEXT_MARKER_PARSER_CONTEXT *_pstParserContext)
{
  orxTEXT_MARKER *pstResult = orxNULL;
  orxASSERT(_pstParserContext != orxNULL);
  orxASSERT(_pstParserContext->zPositionInMarkedString != orxNULL);
  orxASSERT(_pstParserContext->zPositionInOutputString != orxNULL);

  /* Update the character codepoint and advance to the next */
  _pstParserContext->u32CharacterCodePoint = orxText_WalkCodePoint(&_pstParserContext->zPositionInMarkedString);

  /* Does it look like a marker? */
  if (_pstParserContext->u32CharacterCodePoint == orxTEXT_KC_MARKER_SYNTAX_START)
  {
    /* Peek at the next one */
    orxU32 u32Peek = orxString_GetFirstCharacterCodePoint(_pstParserContext->zPositionInMarkedString, orxNULL);
    /* Is this an escape? */
    if (u32Peek == orxTEXT_KC_MARKER_SYNTAX_START)
    {
      /* Skip over the peeked codepoint */
      _pstParserContext->u32CharacterCodePoint = orxText_WalkCodePoint(&_pstParserContext->zPositionInMarkedString);
      /* Do nothing - allow it to process this codepoint as plaintext */
    }
    else
    {
      /* NOTE The following logic does a lot of "if (eType == orxTEXT_MARKER_TYPE_NONE || zSomeOutputString == orxNULL)" and I'm not sure this is going to always be OK. See next note. */
      orxSTRING zEndOfType = orxNULL;
      /* Attempt to parse marker type, which will also advance the string to the first char after the type (if any) */
      orxTEXT_MARKER_TYPE eType = orxText_ParseMarkerType(_pstParserContext->zPositionInMarkedString, (const orxSTRING *)&zEndOfType);
      /* If it's not a valid marker type */
      if (eType == orxTEXT_MARKER_TYPE_NONE || zEndOfType == orxNULL)
      {
        /* Do nothing - allow it to process this codepoint as plaintext */
      }
      else
      {
        orxSTRING zEndOfValue = orxNULL;
        /* Try to parse marker data if any, which will also advance the string to the first char after the data (if any) */
        orxTEXT_MARKER_DATA stData = orxText_ParseMarkerValue(eType, zEndOfType, (const orxSTRING *)&zEndOfValue);
        /* NOTE I noticed that with font in particular, there are two different behaviours currently specified on failure (though not implemented)
           If the font is invalid it logs a debug message but notes that it should *ignore* the marker as opposed to printing it as plaintext */
        /* If the type was set to an invalid one, it means there was something wrong with the marker data and it must be invalid */
        if (stData.eType == orxTEXT_MARKER_TYPE_NONE)
        {
          /* Do nothing - allow it to process this codepoint as plaintext */
        }
        else
        {
          orxASSERT(stData.eType == eType);
          /* Finally! A valid marker! */
          if (zEndOfValue != orxNULL)
          {
            _pstParserContext->zPositionInMarkedString = zEndOfValue;
          }
          else
          {
            /* NOTE If we start to support other markers without (...) syntax, or make it optional for some markers, we'll need to re-evaluate how this is checked */
            /* A good example would be adding a feature like `!(color) which pops the last added color even if it's not the most recent marker on the stack */
            /* Other good examples would be supporting `color() for pushing the default color to the stack as opposed to clearing all colors with, say, `*(color) */
            orxASSERT(eType == orxTEXT_MARKER_TYPE_POP || eType == orxTEXT_MARKER_TYPE_CLEAR);
            _pstParserContext->zPositionInMarkedString = zEndOfType;
          }
          /* Create the marker */
          orxU32 u32CurrentOffset = (_pstParserContext->zPositionInOutputString - _pstParserContext->zOutputString);
          pstResult = orxText_CreateMarker(_pstMarkerBank, u32CurrentOffset, stData);
          if (pstResult == orxNULL)
          {
            orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Couldn't allocate marker - are we out of memory?");
          }
          return pstResult;
        }
      }
    }
  }
  /* Process plaintext */

  /* Append the codepoint to the output string, having skipped the current one so that the position in the original points to the next codepoint. */
  orxString_PrintUTF8Character(_pstParserContext->zPositionInOutputString, _pstParserContext->u32OutputSize, _pstParserContext->u32CharacterCodePoint);
  /* Now skip the appended codepoint in the output string so that adding to it in the future doesn't overwrite anything. */
  orxU32 u32AddedCodePoint = orxText_WalkCodePoint(&_pstParserContext->zPositionInOutputString);
  orxASSERT(u32AddedCodePoint == _pstParserContext->u32CharacterCodePoint);

  return pstResult;
}

/** Takes style markers out of the text string and places them into an array.
 * @param[in]   _pstText      Concerned text
 * @return      A cleaned version of the current string for _pstText
 */
static void orxFASTCALL orxText_ProcessMarkedString(orxTEXT *_pstText)
{
  if (_pstText->zString == orxNULL || *(_pstText->zString) == orxCHAR_NULL)
  {
    return;
  }
  /* Stacks for each marker type - most recent marker is derived from the max character index among the tops of each stack. */
  orxLINKLIST  stMarkerStacks[orxTEXT_MARKER_TYPE_NUMBER_STYLES] = {0};
  orxU32       u32StyleMarkerTally = 0;
  /* Bank for marker allocation */
  orxBANK     *pstMarkerBank     = orxBank_Create(orxTEXT_KU32_MARKER_DATA_BANK_SIZE, sizeof(orxTEXT_MARKER),
                                                  orxBANK_KU32_FLAG_NONE, orxMEMORY_TYPE_MAIN);
  /* Memory bank for marker nodes that are put in the marker stacks */
  orxBANK     *pstMarkerNodeBank = orxBank_Create(orxTEXT_KU32_MARKER_DATA_BANK_SIZE, sizeof(orxTEXT_MARKER_NODE),
                                                  orxBANK_KU32_FLAG_NONE, orxMEMORY_TYPE_MAIN);
  /* Initialize the output string */
  orxSTRING zOutputString       = orxString_Duplicate(_pstText->zString);
  orxU32    u32OutputStringSize = orxString_GetLength(zOutputString) + 1;
  orxMemory_Zero(zOutputString, u32OutputStringSize);

  orxTEXT_MARKER_PARSER_CONTEXT stContext = {0};
  stContext.u32OutputSize = u32OutputStringSize;
  stContext.u32CharacterCodePoint = orxU32_UNDEFINED;
  stContext.zOutputString = zOutputString;
  stContext.zPositionInMarkedString = _pstText->zString;
  stContext.zPositionInOutputString = zOutputString;

  /* Walk UTF-8 encoded string */
  while (stContext.u32CharacterCodePoint != orxCHAR_NULL)
  {
    orxTEXT_MARKER *pstNewMarker = orxText_TryParseMarker(pstMarkerBank, &stContext);
    /* Hit a marker? */
    if (pstNewMarker != orxNULL)
    {
      orxASSERT(orxDisplay_MarkerTypeIsParsed(pstNewMarker->stData.eType));
      if (orxDisplay_MarkerTypeIsStyle(pstNewMarker->stData.eType))
      {
        /* Push marker onto the relevant stack */
        orxTEXT_MARKER_NODE *pstNode = (orxTEXT_MARKER_NODE *) orxBank_Allocate(pstMarkerNodeBank);
        if (pstNode == orxNULL)
        {
          orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Couldn't allocate marker node - are we out of memory?");
          break;
        }
        u32StyleMarkerTally++;
        pstNode->pstMarker = pstNewMarker;
        pstNode->u32MarkerTally = u32StyleMarkerTally;

        orxLinkList_AddEnd(&stMarkerStacks[pstNewMarker->stData.eType], (orxLINKLIST_NODE *)pstNode);
      }
      else if (orxDisplay_MarkerTypeIsManipulator(pstNewMarker->stData.eType))
      {
        /* Manipulate relevant marker stack based on the manipulator */

        /* Is it a pop manipulation? */
        if (pstNewMarker->stData.eType == orxTEXT_MARKER_TYPE_POP)
        {
          /* Find the most recently added marker */
          orxU32 u32MaxTally = 0;
          orxTEXT_MARKER_TYPE ePopThisType = orxTEXT_MARKER_TYPE_NONE;
          for (orxENUM eType = 0; eType < orxTEXT_MARKER_TYPE_NUMBER_STYLES; eType++)
          {
            orxTEXT_MARKER_NODE *pstTopNode = (orxTEXT_MARKER_NODE *) orxLinkList_GetLast(&stMarkerStacks[eType]);
            if (pstTopNode != orxNULL)
            {
              if (pstTopNode->u32MarkerTally > u32MaxTally)
              {
                u32MaxTally = pstTopNode->u32MarkerTally;
                const orxTEXT_MARKER *pstNodeMarker = pstTopNode->pstMarker;
                orxASSERT(pstNodeMarker != orxNULL);
                ePopThisType = pstNodeMarker->stData.eType;
              }
            }
          }
          /* No markers to pop from any stacks? */
          if (ePopThisType == orxTEXT_MARKER_TYPE_NONE)
          {
            /* TODO log warning */
            /* Free the useless marker */
            orxBank_Free(pstMarkerBank, pstNewMarker);
            pstNewMarker = orxNULL;
            continue;
          }
          orxASSERT(orxDisplay_MarkerTypeIsStyle(ePopThisType) && "Most recently pushed marker type [%d] is not a style? How?", ePopThisType);
          /* Get the marker that we'll popping. */
          orxTEXT_MARKER_NODE *pstPoppedNode = (orxTEXT_MARKER_NODE *) orxLinkList_GetLast(&stMarkerStacks[ePopThisType]);
          orxASSERT((pstPoppedNode != orxNULL) && "Marker type [%d] was ostensibly valid, how can the top node for it be null?", ePopThisType);
          /* Pop stack */
          orxSTATUS eOK = orxLinkList_Remove((orxLINKLIST_NODE *) pstPoppedNode);
          orxASSERT(eOK == orxSTATUS_SUCCESS);
          orxBank_Free(pstMarkerNodeBank, pstPoppedNode);
          pstPoppedNode = orxNULL;
          /* Now get the node that's being fallen back to (if any) */
          orxTEXT_MARKER_NODE *pstFallbackNode = (orxTEXT_MARKER_NODE *) orxLinkList_GetLast(&stMarkerStacks[ePopThisType]);
          orxTEXT_MARKER_DATA stFallbackData;
          orxMemory_Zero(&stFallbackData, sizeof(stFallbackData));
          /* Change the pop marker into whatever it needs to fall back to */
          if (pstFallbackNode == orxNULL)
          {
            /* If we ran out of markers of the type we're popping, we must add a marker with data that indicates that we're reverting to a default value. */
            stFallbackData.eType = orxTEXT_MARKER_TYPE_STYLE_DEFAULT;
            stFallbackData.eTypeOfDefault = ePopThisType;
          }
          else
          {
            /* If we have a previous marker of this type, we must add a new marker with the data of that previous marker */
            stFallbackData = pstFallbackNode->pstMarker->stData;
          }
          /* Modify the new marker to change from a stack pop to whatever it translates into before adding it to the marker array */
          pstNewMarker->stData = stFallbackData;
        }
        else if (pstNewMarker->stData.eType == orxTEXT_MARKER_TYPE_CLEAR)
        {
          /* Free the manipulator */
          orxBank_Free(pstMarkerBank, pstNewMarker);
          pstNewMarker = orxNULL;
          /* Add a default marker for each style type to the marker array */
          for (orxENUM eType = 0; eType < orxTEXT_MARKER_TYPE_NUMBER_STYLES; eType++)
          {
            if (orxLinkList_GetCounter(&stMarkerStacks[eType]) > 0)
            {
              orxTEXT_MARKER_DATA stData;
              orxMemory_Zero(&stData, sizeof(stData));
              stData.eType = orxTEXT_MARKER_TYPE_STYLE_DEFAULT;
              stData.eTypeOfDefault = (orxTEXT_MARKER_TYPE) eType;
              orxU32 u32CurrentOffset = (stContext.zPositionInOutputString - stContext.zOutputString);
              orxTEXT_MARKER *pstMarker = orxText_CreateMarker(pstMarkerBank, u32CurrentOffset, stData);
            }
          }
        }
        else
        {
          orxASSERT(orxFALSE && "Unimplemented marker type [%d]", pstNewMarker->stData.eType);
        }
      }
      else
      {
        orxASSERT(orxFALSE && "Impossible marker type [%d]", pstNewMarker->stData.eType);
      }
    }
  }

  /* Store the marker array from the bank */
  _pstText->pstMarkerArray = orxText_ConvertBankToArray(pstMarkerBank, &_pstText->u32MarkerCounter);

  orxBank_Delete(pstMarkerBank);
  orxBank_Delete(pstMarkerNodeBank);
  /* TODO double check whether escapes screw up offsets for markers */
  orxString_Delete(_pstText->zString);
  _pstText->zString = zOutputString;
}

/** Updates text size
 * @param[in]   _pstText      Concerned text
 */
static void orxFASTCALL orxText_UpdateSize(orxTEXT *_pstText)
{
  /* Checks */
  orxSTRUCTURE_ASSERT(_pstText);

  /* Has original string? */
  if(_pstText->zOriginalString != orxNULL)
  {
    /* Has current string? */
    if(_pstText->zString != orxNULL)
    {
      /* Deletes it */
      orxString_Delete(_pstText->zString);
    }

    /* Restores string from original */
    _pstText->zString = _pstText->zOriginalString;
    _pstText->zOriginalString = orxNULL;
  }

  /* Has string and font? */
  if((_pstText->zString != orxNULL) && (_pstText->zString != orxSTRING_EMPTY) && (_pstText->pstFont != orxNULL))
  {
    orxFLOAT        fWidth, fHeight, fCharacterHeight;
    orxU32          u32CharacterCodePoint;

    /* Used for building a new marker array that contains line markers */
    orxBANK        *pstNewMarkerBank;
    /* Needed for keeping track of what marker we're on */
    orxU32 u32MarkerIndex;

    /* Gets character height */
    fCharacterHeight = orxFont_GetCharacterHeight(_pstText->pstFont);

    pstNewMarkerBank = orxBank_Create(orxTEXT_KU32_MARKER_DATA_BANK_SIZE, sizeof(orxTEXT_MARKER),
                                      orxBANK_KU32_FLAG_NONE, orxMEMORY_TYPE_MAIN);

    /* Add a line height marker for the first line, which will get updated with a running maximum height
       for the current line until next line is hit, at which point a new line marker will be added and
       replace it repeating the same process. */
    orxTEXT_MARKER_DATA stLineData;
    orxMemory_Zero(&stLineData, sizeof(stLineData));
    stLineData.eType = orxTEXT_MARKER_TYPE_LINE_HEIGHT;
    stLineData.fLineHeight = orxFLOAT_0;
    orxTEXT_MARKER *pstLineMarker = orxText_CreateMarker(pstNewMarkerBank, 0, stLineData);

    /* No fixed size? */
    if(orxStructure_TestFlags(_pstText, orxTEXT_KU32_FLAG_FIXED_WIDTH | orxTEXT_KU32_FLAG_FIXED_HEIGHT) == orxFALSE)
    {
      const orxCHAR  *pc;
      orxFLOAT        fMaxWidth;

      orxENUM             eType;
      orxTEXT_MARKER_DATA astAppliedStyles[orxTEXT_MARKER_TYPE_NUMBER_STYLES];
      for (eType = 0; eType < orxTEXT_MARKER_TYPE_NUMBER_STYLES; eType++)
      {
        astAppliedStyles[eType].eType = orxTEXT_MARKER_TYPE_STYLE_DEFAULT;
        astAppliedStyles[eType].eTypeOfDefault = (orxTEXT_MARKER_TYPE)eType;
      }

      /* For all characters */
      for(u32CharacterCodePoint = orxString_GetFirstCharacterCodePoint(_pstText->zString, &pc), fHeight = fWidth = fMaxWidth = orxFLOAT_0, u32MarkerIndex = 0;
          u32CharacterCodePoint != orxCHAR_NULL;
          u32CharacterCodePoint = orxString_GetFirstCharacterCodePoint(pc, &pc))
      {
        /* Calculate what the byte index of the current codepoint is. Since pc has since advanced past the current codepoint, we must subtract the length of the current codepoint from the pointer difference. */
        orxU32 u32CurrentOffset = (pc - _pstText->zString) - orxString_GetUTF8CharacterLength(u32CharacterCodePoint);
        /* Are there any markers at all? Have we traversed all of them? */
        if ((_pstText->pstMarkerArray != orxNULL) && (_pstText->u32MarkerCounter > 0) && (u32MarkerIndex < _pstText->u32MarkerCounter))
        {
          orxTEXT_MARKER stMarker = _pstText->pstMarkerArray[u32MarkerIndex];
          /* There may be more than one marker at this offset. */
          while (stMarker.u32Offset == u32CurrentOffset)
          {
            /* Update the currently applied marker of this type */
            orxTEXT_MARKER_TYPE eResolvedStyle = stMarker.stData.eType == orxTEXT_MARKER_TYPE_STYLE_DEFAULT ? stMarker.stData.eTypeOfDefault : stMarker.stData.eType;
            orxASSERT(orxDisplay_MarkerTypeIsStyle(eResolvedStyle) && "Resolved style is [%u]", eResolvedStyle);
            astAppliedStyles[eResolvedStyle] = _pstText->pstMarkerArray[u32MarkerIndex].stData;
            /* Create a copy of the marker for the rebuilt marker array */
            orxTEXT_MARKER *pstNewMarker = orxText_CreateMarker(pstNewMarkerBank, stMarker.u32Offset, stMarker.stData);
            pstNewMarker->u32Offset = stMarker.u32Offset;
            u32MarkerIndex++;
            if(u32MarkerIndex >= _pstText->u32MarkerCounter)
            {
              break;
            }
            /* Move on to the next possible marker */
            stMarker = _pstText->pstMarkerArray[u32MarkerIndex];
          }
        }

        /* Calculate the size of this glyph */
        orxVECTOR               vSize          = orxVECTOR_0;
        orxVECTOR               vCurrentScale  = orxVECTOR_1;
        const orxCHARACTER_MAP *pstCurrentMap  = orxFont_GetMap(_pstText->pstFont);
        orxLOG("1 %p", pstCurrentMap);
        /* Grab the values for the latest scale and font markers for size calculation */
        if (astAppliedStyles[orxTEXT_MARKER_TYPE_SCALE].eType != orxTEXT_MARKER_TYPE_STYLE_DEFAULT)
        {
          orxASSERT(astAppliedStyles[orxTEXT_MARKER_TYPE_SCALE].eType == orxTEXT_MARKER_TYPE_SCALE);
          vCurrentScale = astAppliedStyles[orxTEXT_MARKER_TYPE_SCALE].vScale;
        }
        if (astAppliedStyles[orxTEXT_MARKER_TYPE_FONT].eType != orxTEXT_MARKER_TYPE_STYLE_DEFAULT)
        {
          orxASSERT(astAppliedStyles[orxTEXT_MARKER_TYPE_FONT].eType == orxTEXT_MARKER_TYPE_FONT);
          pstCurrentMap = astAppliedStyles[orxTEXT_MARKER_TYPE_FONT].stFontData.pstMap;
          orxLOG("2 %p", pstCurrentMap);
        }
        orxLOG("3 %p", pstCurrentMap);
        orxASSERT(pstCurrentMap != orxNULL);
        orxASSERT(pstCurrentMap->pstCharacterTable != orxNULL);
        /* Gets glyph from UTF-8 table */
        orxCHARACTER_GLYPH *pstGlyph = (orxCHARACTER_GLYPH *)orxHashTable_Get(pstCurrentMap->pstCharacterTable, u32CharacterCodePoint);
        /* Compute size */
        if (pstGlyph != orxNULL)
        {
          vSize.fX = pstGlyph->fWidth                * vCurrentScale.fX;
          vSize.fY = pstCurrentMap->fCharacterHeight * vCurrentScale.fY;
        }
        else
        {
          vSize.fX = pstCurrentMap->fCharacterHeight * vCurrentScale.fX;
          vSize.fY = pstCurrentMap->fCharacterHeight * vCurrentScale.fY;
        }

        /* Update current line height if necessary */
        pstLineMarker->stData.fLineHeight = orxMAX(pstLineMarker->stData.fLineHeight, vSize.fY);
        orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Updated line height to %f", pstLineMarker->stData.fLineHeight);

        /* Depending on character */
        switch(u32CharacterCodePoint)
        {
          case orxCHAR_CR:
          {
            /* Half EOL? */
            if(*pc == orxCHAR_LF)
            {
              /* Updates pointer and index */
              pc++;
              u32CurrentOffset++;
            }

            /* Fall through */
          }

          case orxCHAR_LF:
          {
            /* Updates height */
            fHeight += pstLineMarker->stData.fLineHeight;

            /* Updates max width */
            fMaxWidth = orxMAX(fMaxWidth, fWidth);

            /* Resets width */
            fWidth = orxFLOAT_0;

            /* Add a new line marker, replacing the reference to the previous one */
            orxTEXT_MARKER_DATA stData;
            orxMemory_Zero(&stData, sizeof(stData));
            stData.eType = orxTEXT_MARKER_TYPE_LINE_HEIGHT;
            stData.fLineHeight = orxFLOAT_0;
            orxASSERT(*(_pstText->zString + u32CurrentOffset) == orxCHAR_LF);
            /* We add one here because we want the marker to start on the first character of the next line.
               Line height is only meaningful to the glyphs of that line, and allowing it to show up sooner will throw off the renderer. */
            pstLineMarker = orxText_CreateMarker(pstNewMarkerBank, u32CurrentOffset + 1, stData);

            break;
          }

          default:
          {
            /* Updates width */
            fWidth += vSize.fX;

            break;
          }
        }
      }
      /* Line height still needs to be applied for the final line */
      fHeight += pstLineMarker->stData.fLineHeight;

      /* Stores values */
      _pstText->fWidth  = orxMAX(fWidth, fMaxWidth);
      _pstText->fHeight = fHeight;
    }
    else
    {
    /* NOTE it looks to me like the height is not expanding properly when line breaks get added, at least according to physics debugging */
      orxCHAR  *pc;
      orxSTRING zLastSpace;

      /* For all characters */
      for(u32CharacterCodePoint = orxString_GetFirstCharacterCodePoint(_pstText->zString, (const orxCHAR **)&pc), fHeight = fCharacterHeight, fWidth = orxFLOAT_0, zLastSpace = orxNULL;
          u32CharacterCodePoint != orxCHAR_NULL;
          u32CharacterCodePoint = orxString_GetFirstCharacterCodePoint(pc, (const orxCHAR **)&pc))
      {
        /* Depending on the character */
        switch(u32CharacterCodePoint)
        {
          case orxCHAR_CR:
          {
            /* Half EOL? */
            if(*pc == orxCHAR_LF)
            {
              /* Updates pointer */
              pc++;
            }

            /* Fall through */
          }

          case orxCHAR_LF:
          {
            /* Updates height */
            fHeight += fCharacterHeight;

            /* Should truncate? */
            if((orxStructure_TestFlags(_pstText, orxTEXT_KU32_FLAG_FIXED_HEIGHT) != orxFALSE) && (fHeight > _pstText->fHeight))
            {
              /* Truncates the string */
              *pc = orxCHAR_NULL;
            }
            else
            {
              /* Resets width */
              fWidth = orxFLOAT_0;
            }

            break;
          }

          /* TODO There are other characters that are worth breaking lines on that span more than one byte. Unicode spaces/punctuation are good examples. If anyone were to write an interpreter for text wrapping in another language, this would break pretty badly. */
          case ' ':
          case '\t':
          {
            /* Updates width */
            fWidth += orxFont_GetCharacterWidth(_pstText->pstFont, u32CharacterCodePoint);

            /* Updates last space */
            zLastSpace = pc - 1;

            break;
          }

          default:
          {
            /* Finds end of word */
            for(; (u32CharacterCodePoint != ' ') && (u32CharacterCodePoint != '\t') && (u32CharacterCodePoint != orxCHAR_NULL); u32CharacterCodePoint = orxString_GetFirstCharacterCodePoint(pc, (const orxCHAR **)&pc))
            {
              fWidth += orxFont_GetCharacterWidth(_pstText->pstFont, u32CharacterCodePoint);
            }

            /* Gets back to previous character */
            pc--;

            break;
          }
        }

        /* Doesn't fit inside the line? */
        if(fWidth > _pstText->fWidth)
        {
          /* No original string copy yet? */
          if(_pstText->zOriginalString == orxNULL)
          {
            /* Stores a copy of the original */
            _pstText->zOriginalString = orxString_Duplicate(_pstText->zString);
          }

          /* Has last space? */
          if(zLastSpace != orxNULL)
          {
            /* Inserts end of line */
            *zLastSpace = orxCHAR_LF;

            /* Updates cursor */
            pc = zLastSpace;

            /* Clears last space */
            zLastSpace = orxNULL;

            /* Updates width */
            fWidth = orxFLOAT_0;
          }
          else
          {
            orxCHAR cBackup, *pcDebug;

            /* Logs message */
            cBackup = *pc;
            *pc = orxCHAR_NULL;
            for(pcDebug = pc - 1; (pcDebug >= _pstText->zString) && (*pcDebug != ' ') && (*pcDebug != '\t') && (*pcDebug != orxCHAR_LF) && (*pcDebug != orxCHAR_CR); pcDebug--)
              ;
            orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "[%s] Word <%s> is too long to fit inside the requested <%g> pixels!", (_pstText->zReference != orxNULL) ? _pstText->zReference : orxSTRING_EMPTY, pcDebug + 1, _pstText->fWidth);
            *pc = cBackup;
          }
        }
      }

      /* Isn't height fixed? */
      if(orxStructure_TestFlags(_pstText, orxTEXT_KU32_FLAG_FIXED_HEIGHT) == orxFALSE)
      {
        /* Stores it */
        _pstText->fHeight = fHeight;
      }
    }

    /* Replace the old marker array with the new one */
    orxMemory_Free(_pstText->pstMarkerArray);
    _pstText->pstMarkerArray = orxText_ConvertBankToArray(pstNewMarkerBank, &_pstText->u32MarkerCounter);
  }
  else
  {
    /* Isn't width fixed? */
    if(orxStructure_TestFlags(_pstText, orxTEXT_KU32_FLAG_FIXED_WIDTH) == orxFALSE)
    {
      /* Clears it */
      _pstText->fWidth = orxFLOAT_0;
    }

    /* Isn't height fixed? */
    if(orxStructure_TestFlags(_pstText, orxTEXT_KU32_FLAG_FIXED_HEIGHT) == orxFALSE)
    {
      /* Clears it */
      _pstText->fHeight = orxFLOAT_0;
    }
  }

  /* Done! */
  return;
}

/** Deletes all texts
 */
static orxINLINE void orxText_DeleteAll()
{
  orxTEXT *pstText;

  /* Gets first text */
  pstText = orxTEXT(orxStructure_GetFirst(orxSTRUCTURE_ID_TEXT));

  /* Non empty? */
  while(pstText != orxNULL)
  {
    /* Deletes text */
    orxText_Delete(pstText);

    /* Gets first text */
    pstText = orxTEXT(orxStructure_GetFirst(orxSTRUCTURE_ID_TEXT));
  }

  return;
}


/***************************************************************************
 * Public functions                                                        *
 ***************************************************************************/

/** Setups the text module
 */
void orxFASTCALL orxText_Setup()
{
  /* Adds module dependencies */
  orxModule_AddDependency(orxMODULE_ID_TEXT, orxMODULE_ID_MEMORY);
  orxModule_AddDependency(orxMODULE_ID_TEXT, orxMODULE_ID_CONFIG);
  orxModule_AddDependency(orxMODULE_ID_TEXT, orxMODULE_ID_EVENT);
  orxModule_AddDependency(orxMODULE_ID_TEXT, orxMODULE_ID_FONT);
  orxModule_AddDependency(orxMODULE_ID_TEXT, orxMODULE_ID_LOCALE);
  orxModule_AddDependency(orxMODULE_ID_TEXT, orxMODULE_ID_STRUCTURE);

  return;
}

/** Inits the text module
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxText_Init()
{
  orxSTATUS eResult;

  /* Not already Initialized? */
  if(!(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY))
  {
    /* Cleans static controller */
    orxMemory_Zero(&sstText, sizeof(orxTEXT_STATIC));

    /* Adds event handler */
    eResult = orxEvent_AddHandler(orxEVENT_TYPE_LOCALE, orxText_EventHandler);

    /* Valid? */
    if(eResult != orxSTATUS_FAILURE)
    {
      /* Registers structure type */
      eResult = orxSTRUCTURE_REGISTER(TEXT, orxSTRUCTURE_STORAGE_TYPE_LINKLIST, orxMEMORY_TYPE_MAIN, orxTEXT_KU32_BANK_SIZE, orxNULL);

      /* Success? */
      if(eResult != orxSTATUS_FAILURE)
      {
        /* Updates flags for screen text creation */
        sstText.u32Flags = orxTEXT_KU32_STATIC_FLAG_READY;

        /* Adds event handler for resources */
        orxEvent_AddHandler(orxEVENT_TYPE_RESOURCE, orxText_EventHandler);
      }
      else
      {
        /* Removes event handler */
        orxEvent_RemoveHandler(orxEVENT_TYPE_LOCALE, orxText_EventHandler);
      }
    }
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Tried to initialize text module when it was already initialized.");

    /* Already initialized */
    eResult = orxSTATUS_SUCCESS;
  }

  /* Not initialized? */
  if(eResult == orxSTATUS_FAILURE)
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Initializing text module failed.");

    /* Updates Flags */
    sstText.u32Flags &= ~orxTEXT_KU32_STATIC_FLAG_READY;
  }

  /* Done! */
  return eResult;
}

/** Exits from the text module
 */
void orxFASTCALL orxText_Exit()
{
  /* Initialized? */
  if(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY)
  {
    /* Deletes text list */
    orxText_DeleteAll();

    /* Removes event handlers */
    orxEvent_RemoveHandler(orxEVENT_TYPE_RESOURCE, orxText_EventHandler);
    orxEvent_RemoveHandler(orxEVENT_TYPE_LOCALE, orxText_EventHandler);

    /* Unregisters structure type */
    orxStructure_Unregister(orxSTRUCTURE_ID_TEXT);

    /* Updates flags */
    sstText.u32Flags &= ~orxTEXT_KU32_STATIC_FLAG_READY;
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Tried to exit text module when it wasn't initialized.");
  }

  return;
}

/** Creates an empty text
 * @return      orxTEXT / orxNULL
 */
orxTEXT *orxFASTCALL orxText_Create()
{
  orxTEXT *pstResult;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);

  /* Creates text */
  pstResult = orxTEXT(orxStructure_Create(orxSTRUCTURE_ID_TEXT));

  /* Created? */
  if(pstResult != orxNULL)
  {
    /* Inits it */
    pstResult->zString          = orxNULL;
    pstResult->pstFont          = orxNULL;
    pstResult->zOriginalString  = orxNULL;
    pstResult->pstMarkerArray   = orxNULL;
    pstResult->u32MarkerCounter = 0;

    /* Inits flags */
    orxStructure_SetFlags(pstResult, orxTEXT_KU32_FLAG_NONE, orxTEXT_KU32_MASK_ALL);

    /* Increases counter */
    orxStructure_IncreaseCounter(pstResult);
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Failed to create structure for text.");
  }

  /* Done! */
  return pstResult;
}

/** Creates a text from config
 * @param[in]   _zConfigID    Config ID
 * @return      orxTEXT / orxNULL
 */
orxTEXT *orxFASTCALL orxText_CreateFromConfig(const orxSTRING _zConfigID)
{
  orxTEXT *pstResult;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxASSERT((_zConfigID != orxNULL) && (_zConfigID != orxSTRING_EMPTY));

  /* Pushes section */
  if((orxConfig_HasSection(_zConfigID) != orxFALSE)
  && (orxConfig_PushSection(_zConfigID) != orxSTATUS_FAILURE))
  {
    /* Creates text */
    pstResult = orxText_Create();

    /* Valid? */
    if(pstResult != orxNULL)
    {
      /* Stores its reference key */
      pstResult->zReference = orxConfig_GetCurrentSection();

      /* Processes its config data */
      if(orxText_ProcessConfigData(pstResult) == orxSTATUS_FAILURE)
      {
        /* Logs message */
        orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Couldn't process config data for text <%s>.", _zConfigID);

        /* Deletes it */
        orxText_Delete(pstResult);

        /* Updates result */
        pstResult = orxNULL;
      }
    }

    /* Pops previous section */
    orxConfig_PopSection();
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "Couldn't find config section named (%s).", _zConfigID);

    /* Updates result */
    pstResult = orxNULL;
  }

  /* Done! */
  return pstResult;
}

/** Deletes a text
 * @param[in]   _pstText      Concerned text
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxText_Delete(orxTEXT *_pstText)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);

  /* Decreases counter */
  orxStructure_DecreaseCounter(_pstText);

  /* Not referenced? */
  if(orxStructure_GetRefCounter(_pstText) == 0)
  {
    /* Removes string */
    orxText_SetString(_pstText, orxNULL);

    /* Removes font */
    orxText_SetFont(_pstText, orxNULL);

    /* Deletes structure */
    orxStructure_Delete(_pstText);
  }
  else
  {
    /* Referenced by others */
    eResult = orxSTATUS_FAILURE;
  }

  /* Done! */
  return eResult;
}

/** Gets text name
 * @param[in]   _pstText      Concerned text
 * @return      Text name / orxNULL
 */
const orxSTRING orxFASTCALL orxText_GetName(const orxTEXT *_pstText)
{
  const orxSTRING zResult;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);

  /* Updates result */
  zResult = (_pstText->zReference != orxNULL) ? _pstText->zReference : orxSTRING_EMPTY;

  /* Done! */
  return zResult;
}

/** Gets text's line count
 * @param[in]   _pstText      Concerned text
 * @return      orxU32
 */
orxU32 orxFASTCALL orxText_GetLineCount(const orxTEXT *_pstText)
{
  orxU32 u32Result;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);

  /* Is not empty? */
  if((_pstText->zString != orxNULL) && (*(_pstText->zString) != orxCHAR_NULL))
  {
    const orxCHAR *pc;

    /* For all characters */
    for(pc = _pstText->zString, u32Result = 1; *pc != orxCHAR_NULL; pc++)
    {
      /* Depending on character */
      switch(*pc)
      {
        case orxCHAR_CR:
        {
          /* Half EOL? */
          if(*(pc + 1) == orxCHAR_LF)
          {
            /* Updates pointer */
            pc++;
          }

          /* Fall through */
        }

        case orxCHAR_LF:
        {
          /* Updates result */
          u32Result++;
        }

        default:
        {
          break;
        }
      }
    }
  }
  else
  {
    /* Updates result */
    u32Result = 0;
  }

  /* Done! */
  return u32Result;
}

/** Gets text's line size
 * @param[in]   _pstText      Concerned text
 * @param[out]  _u32Line      Line index
 * @param[out]  _pfWidth      Line's width
 * @param[out]  _pfHeight     Line's height
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxText_GetLineSize(const orxTEXT *_pstText, orxU32 _u32Line, orxFLOAT *_pfWidth, orxFLOAT *_pfHeight)
{
  orxSTATUS eResult = orxSTATUS_FAILURE;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);
  orxASSERT(_pfWidth != orxNULL);
  orxASSERT(_pfHeight != orxNULL);

  /* Has font? */
  if(_pstText->pstFont != orxNULL)
  {
    /* Has text? */
    if(_pstText->zString != orxNULL)
    {
      const orxCHAR  *pc;
      orxU32          u32Line;

      /* Skips to requested line */
      for(pc = _pstText->zString, u32Line = 0; (u32Line < _u32Line) && (*pc != orxCHAR_NULL); pc++)
      {
        /* Depending on character */
        switch(*pc)
        {
          case orxCHAR_CR:
          {
            /* Half EOL? */
            if(*(pc + 1) == orxCHAR_LF)
            {
              /* Updates pointer */
              pc++;
            }

            /* Fall through */
          }

          case orxCHAR_LF:
          {
            /* Updates line counter */
            u32Line++;
          }

          default:
          {
            break;
          }
        }
      }

      /* Valid? */
      if(*pc != orxCHAR_NULL)
      {
        orxU32    u32CharacterCodePoint;
        orxFLOAT  fWidth;

        /* For all characters in the line */
        for(u32CharacterCodePoint = orxString_GetFirstCharacterCodePoint(pc, &pc), fWidth = orxFLOAT_0;
            (u32CharacterCodePoint != orxCHAR_CR) && (u32CharacterCodePoint != orxCHAR_LF) && (u32CharacterCodePoint != orxCHAR_NULL);
            u32CharacterCodePoint = orxString_GetFirstCharacterCodePoint(pc, &pc))
        {
          /* Updates width */
          fWidth += orxFont_GetCharacterWidth(_pstText->pstFont, u32CharacterCodePoint);
        }

        /* Stores dimensions */
        *_pfWidth   = fWidth;
        *_pfHeight  = orxFont_GetCharacterHeight(_pstText->pstFont);

        /* Updates result */
        eResult = orxSTATUS_SUCCESS;
      }
      else
      {
        /* Logs message */
        orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "[%s:%u]: Couldn't get text line size, invalid line number.", (_pstText->zReference != orxNULL) ? _pstText->zReference : orxSTRING_EMPTY, _u32Line);
      }
    }
    else
    {
      /* Logs message */
      orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "[%s:%u]: Couldn't get text line size as no string is set.", (_pstText->zReference != orxNULL) ? _pstText->zReference : orxSTRING_EMPTY, _u32Line);
    }
  }
  else
  {
    /* Logs message */
    orxDEBUG_PRINT(orxDEBUG_LEVEL_DISPLAY, "[%s:%u]: Couldn't get text line size as no font is set.", (_pstText->zReference != orxNULL) ? _pstText->zReference : orxSTRING_EMPTY, _u32Line);
  }

  /* Done! */
  return eResult;
}

/** Is text's size fixed? (ie. manually constrained with orxText_SetSize())
 * @param[in]   _pstText      Concerned text
 * @return      orxTRUE / orxFALSE
 */
orxBOOL orxFASTCALL orxText_IsFixedSize(const orxTEXT *_pstText)
{
  orxBOOL bResult;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);

  /* Updates result */
  bResult = orxStructure_TestFlags(_pstText, orxTEXT_KU32_FLAG_FIXED_WIDTH | orxTEXT_KU32_FLAG_FIXED_HEIGHT);

  /* Done! */
  return bResult;
}

/** Gets text size
 * @param[in]   _pstText      Concerned text
 * @param[out]  _pfWidth      Text's width
 * @param[out]  _pfHeight     Text's height
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxText_GetSize(const orxTEXT *_pstText, orxFLOAT *_pfWidth, orxFLOAT *_pfHeight)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);
  orxASSERT(_pfWidth != orxNULL);
  orxASSERT(_pfHeight != orxNULL);

  /* Updates result */
  *_pfWidth   = _pstText->fWidth;
  *_pfHeight  = _pstText->fHeight;

  /* Done! */
  return eResult;
}

/** Gets text string
 * @param[in]   _pstText      Concerned text
 * @return      Text string / orxSTRING_EMPTY
 */
const orxSTRING orxFASTCALL orxText_GetString(const orxTEXT *_pstText)
{
  const orxSTRING zResult;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);

  /* Has string? */
  if(_pstText->zString != orxNULL)
  {
    /* Updates result */
    zResult = _pstText->zString;
  }
  else
  {
    /* Updates result */
    zResult = orxSTRING_EMPTY;
  }

  /* Done! */
  return zResult;
}

/** Gets text font
 * @param[in]   _pstText      Concerned text
 * @return      Text font / orxNULL
 */
orxFONT *orxFASTCALL orxText_GetFont(const orxTEXT *_pstText)
{
  orxFONT *pstResult;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);

  /* Updates result */
  pstResult = _pstText->pstFont;

  /* Done! */
  return pstResult;
}

/** Sets text's size, will lead to reformatting if text doesn't fit (pass width = -1.0f to restore text's original size, ie. unconstrained)
 * @param[in]   _pstText      Concerned text
 * @param[in]   _fWidth       Max width for the text, remove any size constraint if negative
 * @param[in]   _fHeight      Max height for the text, ignored if negative (ie. unconstrained height)
 * @param[in]   _pzExtra      Text that wouldn't fit inside the box if height is provided, orxSTRING_EMPTY if no extra, orxNULL to ignore
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxText_SetSize(orxTEXT *_pstText, orxFLOAT _fWidth, orxFLOAT _fHeight, const orxSTRING *_pzExtra)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);
  orxASSERT (_fWidth > orxFLOAT_0);

  /* NOTE This needs to remove line height markers and allow updatesize to replace them */

  /* Unconstrained? */
  if(_fWidth <= orxFLOAT_0)
  {
    /* Was constrained? */
    if(orxStructure_TestFlags(_pstText, orxTEXT_KU32_FLAG_FIXED_WIDTH) != orxFALSE)
    {
      /* Clears dimensions */
      _pstText->fWidth = _pstText->fHeight = orxFLOAT_0;

      /* Updates status */
      orxStructure_SetFlags(_pstText, orxTEXT_KU32_FLAG_NONE, orxTEXT_KU32_FLAG_FIXED_WIDTH | orxTEXT_KU32_FLAG_FIXED_HEIGHT);

      /* Updates size */
      orxText_UpdateSize(_pstText);
    }

    /* Asked for extra? */
    if(_pzExtra != orxNULL)
    {
      /* Updates it */
      *_pzExtra = orxSTRING_EMPTY;
    }
  }
  else
  {
    /* Stores dimensions */
    _pstText->fWidth  = _fWidth;
    _pstText->fHeight = (_fHeight > orxFLOAT_0) ? _fHeight : orxFLOAT_0;

    /* Updates status */
    orxStructure_SetFlags(_pstText, (_fHeight > orxFLOAT_0) ? orxTEXT_KU32_FLAG_FIXED_WIDTH | orxTEXT_KU32_FLAG_FIXED_HEIGHT : orxTEXT_KU32_FLAG_FIXED_WIDTH, orxTEXT_KU32_FLAG_FIXED_WIDTH | orxTEXT_KU32_FLAG_FIXED_HEIGHT);

    /* Updates size */
    orxText_UpdateSize(_pstText);

    /* Asked for extra? */
    if(_pzExtra != orxNULL)
    {
      /* Has original string? */
      if(_pstText->zOriginalString != orxNULL)
      {
        const orxCHAR *pcSrc, *pcDst;

        /* Finds end of new string */
        for(pcSrc = _pstText->zString, pcDst = _pstText->zOriginalString; *pcSrc != orxCHAR_NULL; pcSrc++, pcDst++)
          ;

        /* Updates extra */
        *_pzExtra = (pcDst != orxCHAR_NULL) ? pcDst : orxSTRING_EMPTY;
      }
      else
      {
        /* Updates extra */
        *_pzExtra = orxSTRING_EMPTY;
      }
    }
  }

  /* Done! */
  return eResult;
}

/** Sets text string
 * @param[in]   _pstText      Concerned text
 * @param[in]   _zString      String to contain
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxText_SetString(orxTEXT *_pstText, const orxSTRING _zString)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);

  /* Has current string? */
  if(_pstText->zString != orxNULL)
  {
    /* Deletes it */
    orxString_Delete(_pstText->zString);
    _pstText->zString = orxNULL;

    /* Has original? */
    if(_pstText->zOriginalString != orxNULL)
    {
      /* Deletes it */
      orxString_Delete(_pstText->zOriginalString);
      _pstText->zOriginalString = orxNULL;
    }
  }

  if (_pstText->pstMarkerArray != orxNULL)
  {
    orxMemory_Free(_pstText->pstMarkerArray);
    _pstText->pstMarkerArray = orxNULL;
    _pstText->u32MarkerCounter = 0;
  }

  /* Has new string? */
  if((_zString != orxNULL) && (_zString != orxSTRING_EMPTY))
  {
    /* Stores a duplicate */
    _pstText->zString = orxString_Duplicate(_zString);
  }

  /* Update markers, removing all markers from the string */
  orxText_ProcessMarkedString(_pstText);

  /* Updates text size */
  orxText_UpdateSize(_pstText);

  /* Done! */
  return eResult;
}

/** Sets text font
 * @param[in]   _pstText      Concerned text
 * @param[in]   _pstFont      Font / orxNULL to use default
 * @return      orxSTATUS_SUCCESS / orxSTATUS_FAILURE
 */
orxSTATUS orxFASTCALL orxText_SetFont(orxTEXT *_pstText, orxFONT *_pstFont)
{
  orxSTATUS eResult = orxSTATUS_SUCCESS;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);

  /* Different? */
  if(_pstText->pstFont != _pstFont)
  {
    /* Has current font? */
    if(_pstText->pstFont != orxNULL)
    {
      /* Updates structure reference counter */
      orxStructure_DecreaseCounter(_pstText->pstFont);

      /* Internally handled? */
      if(orxStructure_TestFlags(_pstText, orxTEXT_KU32_FLAG_INTERNAL) != orxFALSE)
      {
        /* Removes its owner */
        orxStructure_SetOwner(_pstText->pstFont, orxNULL);

        /* Deletes it */
        orxFont_Delete(_pstText->pstFont);

        /* Updates flags */
        orxStructure_SetFlags(_pstText, orxTEXT_KU32_FLAG_NONE, orxTEXT_KU32_FLAG_INTERNAL);
      }

      /* Cleans it */
      _pstText->pstFont = orxNULL;
    }

    /* Has new font? */
    if(_pstFont != orxNULL)
    {
      /* Stores it */
      _pstText->pstFont = _pstFont;

      /* Updates its reference counter */
      orxStructure_IncreaseCounter(_pstFont);
    }

    /* Updates text's size */
    orxText_UpdateSize(_pstText);
  }

  /* Done! */
  return eResult;
}

/** Gets number of markers
  * @param[in]   _pstText      Concerned text
  * @return      Text marker counter
  */
orxU32 orxFASTCALL orxText_GetMarkerCounter(const orxTEXT *_pstText)
{
    orxU32 u32Result;

    /* Checks */
    orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
    orxSTRUCTURE_ASSERT(_pstText);

    /* Updates result */
    u32Result = _pstText->u32MarkerCounter;

    /* Done! */
    return u32Result;
}

/** Gets marker array
 * @param[in] _pstText  Concerned text
 * @return Pointer to orxTEXT_MARKER / orxNULL if no markers
 */
const orxTEXT_MARKER *orxFASTCALL orxText_GetMarkerArray(const orxTEXT *_pstText)
{
  const orxTEXT_MARKER *pstResult = orxNULL;

  /* Checks */
  orxASSERT(sstText.u32Flags & orxTEXT_KU32_STATIC_FLAG_READY);
  orxSTRUCTURE_ASSERT(_pstText);

  /* Update result */
  pstResult = _pstText->pstMarkerArray;

  /* Done! */
  return pstResult;
}
