// resource.h : Resource ID definitions for BlockView
//
// (C) Copyright 2002-2007 by Autodesk, Inc.
//

#pragma once

// Dialog IDs
#define IDD_BLOCKVIEW                   200

// Control IDs (Dialog controls)
#define IDC_VIEW                        201
#define IDC_VIEWMODE                    202
#define IDC_ADDENTITY                   203
#define IDC_ADDTEMPENTITY               204
#define IDC_REMAPCOLORS                 205
#define IDC_STANDARDCOLORS              206

// Menu IDs
#define IDR_MAINMENU                    400

// File menu commands
#define ID_TOOLS_PRINT                  298
// ID_FILE_OPEN is already defined in afxres.h (0xE101); undefine and redefine to our value
#ifdef ID_FILE_OPEN
#undef ID_FILE_OPEN
#endif
#define ID_FILE_OPEN                    300
#define ID_FILE_ACGSCONFIG              301

// Renderer type commands (32779-32784)
#define ID_RENDERERTYPE_KDEFAULT                32779
#define ID_RENDERERTYPE_KSOFTWARE               32780
#define ID_RENDERERTYPE_KSOFTWARENEWVIEWSONLY   32781
#define ID_RENDERERTYPE_KFULLRENDER             32782
#define ID_RENDERERTYPE_KSELECTIONRENDER        32783
#define ID_SHOWLINETYPES                        32784

// View commands (32785-32790)
#define ID_SHOWSECTIONING               32785
#define ID_ZOOM_WINDOW                  32786
#define ID_ZOOM_EXTENTS                 32787
#define ID_SETTINGS_VISUALSTYLE         32788

// Next available:
// Next Resource Value:    302
// Next Command Value:     32789
// Next Control Value:     207
// Next Symed Value:       101
