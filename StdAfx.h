//
// (C) Copyright 2002-2007 by Autodesk, Inc.
//
// Permission to use, copy, modify, and distribute this software in
// object code form for any purpose and without fee is hereby granted,
// provided that the above copyright notice appears in all copies and
// that both that copyright notice and the limited warranty and
// restricted rights notice below appear in all supporting
// documentation.
//
// AUTODESK PROVIDES THIS PROGRAM "AS IS" AND WITH ALL FAULTS.
// AUTODESK SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTY OF
// MERCHANTABILITY OR FITNESS FOR A PARTICULAR USE.  AUTODESK, INC.
// DOES NOT WARRANT THAT THE OPERATION OF THE PROGRAM WILL BE
// UNINTERRUPTED OR ERROR FREE.
//
// Use, duplication, or disclosure by the U.S. Government is subject to
// restrictions set forth in FAR 52.227-19 (Commercial Computer
// Software - Restricted Rights) and DFAR 252.227-7013(c)(1)(ii)
// (Rights in Technical Data and Computer Software), as applicable.
//
// StdAfx.h : Precompiled header for BlockView ARX plugin
//            Targeting AutoCAD 2022 (ObjectARX R24)

#pragma once

#pragma pack (push, 8)
#pragma warning(disable: 4786 4996)

//-----------------------------------------------------------------------------
#define STRICT

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN        // Exclude rarely-used stuff from Windows headers
#endif

#ifndef WINVER
#define WINVER 0x0A00       // Windows 10
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#ifndef _WIN32_WINDOWS
#define _WIN32_WINDOWS 0x0A00
#endif

#ifndef _WIN32_IE
#define _WIN32_IE 0x0A00
#endif

//-----------------------------------------------------------------------------
// ObjectARX and OMF headers need this
#include <map>

//-----------------------------------------------------------------------------
// Temporarily undefine _DEBUG for MFC headers to avoid forcing debug CRT linkage
#ifdef _DEBUG
#define _WAS_DEBUG_STDAFX
#undef _DEBUG
#endif

#include <afxwin.h>             // MFC core and standard components
#include <afxext.h>             // MFC extensions

#ifndef _AFX_NO_OLE_SUPPORT
#include <afxole.h>             // MFC OLE classes
#include <afxodlgs.h>           // MFC OLE dialog classes
#include <afxdisp.h>            // MFC Automation classes
#endif

#ifndef _AFX_NO_DB_SUPPORT
#include <afxdb.h>              // MFC ODBC database classes
#endif

#ifndef _AFX_NO_DAO_SUPPORT
#include <afxdao.h>             // MFC DAO database classes
#endif

#include <afxdtctl.h>           // MFC support for IE 4 Common Controls
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>             // MFC support for Windows Common Controls
#endif

#include <afxcontrolbars.h>

// Restore _DEBUG
#ifdef _WAS_DEBUG_STDAFX
#define _DEBUG
#undef _WAS_DEBUG_STDAFX
#endif

//-----------------------------------------------------------------------------
// ObjectARX / ObjectDBX headers
#include "arxHeaders.h"         // Comprehensive ObjectARX include
#include "axlock.h"             // AcAxDocLock

//-----------------------------------------------------------------------------
// rxdebug utilities
#include "rxdebug.h"

//-----------------------------------------------------------------------------
// Free function declarations (defined in BlockViewDlg.cpp)
// Set the AcGsView to match the *Active* AutoCAD viewport
extern AcDbObjectId SetViewTo(AcGsView *pView, AcDbDatabase *pDb, AcGeMatrix3d& viewMat);
// Invalidate and update an AcGsView
extern void refreshView(AcGsView *pView);
// Query the active viewport info from a database
extern bool GetActiveViewPortInfo(AcDbDatabase *pDb, ads_real &height, ads_real &width,
                                  AcGePoint3d &target, AcGeVector3d &viewDir,
                                  ads_real &viewTwist, AcDbObjectId &currentVsId,
                                  bool getViewCenter);

#pragma pack (pop)
