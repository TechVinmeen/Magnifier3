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
// BlockViewDlg.h : Header for the BlockView dialog class

#pragma once

// Forward declaration - defined in BlockViewDlg.cpp, also used by BlockViewCommands.cpp via StdArx.h
bool CreateAtilImage(AcGsView *pView, int width, int height, int colorDepth,
                     int paletteSize, ACHAR *pFileName, bool renderToImage);

#include "GsPreviewCtrl.h"
#include "resource.h"

/////////////////////////////////////////////////////////////////////////////
// CCrosshairWnd — tiny layered overlay that draws a green + marker.
// Uses MFC class registration (no manual RegisterClassEx needed).
// Black pixels are made transparent via LWA_COLORKEY so the scene shows through.

class CCrosshairWnd : public CWnd
{
public:
    afx_msg void OnPaint()
    {
        CPaintDC dc(this);
        CRect rc;
        GetClientRect(&rc);
        dc.FillSolidRect(&rc, RGB(0, 0, 0));        // black = colour-key transparent
        CPen pen(PS_SOLID, 1, RGB(0, 255, 0));
        CPen* old = dc.SelectObject(&pen);
        int cx = rc.Width() / 2, cy = rc.Height() / 2;
        dc.MoveTo(cx - 7, cy);   dc.LineTo(cx - 1, cy);
        dc.MoveTo(cx + 2, cy);   dc.LineTo(cx + 8, cy);
        dc.MoveTo(cx, cy - 7);   dc.LineTo(cx, cy - 1);
        dc.MoveTo(cx, cy + 2);   dc.LineTo(cx, cy + 8);
        dc.SelectObject(old);
    }
    afx_msg BOOL OnEraseBkgnd(CDC*) { return TRUE; }
    DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CBlockViewDlg dialog

class CBlockViewDlg : public CAcUiDialog
{
public:
    CBlockViewDlg(CWnd *pParent = NULL);

    enum { IDD = IDD_BLOCKVIEW };

    // The graphics preview control (subclasses a static control)
    CGsPreviewCtrl  mPreviewCtrl;
    // Pointer to the currently displayed drawing database
    AcDbDatabase   *mCurrentDwg;
    // View matrix: WCS -> View coordinate space
    AcGeMatrix3d    m_viewMatrix;
    // Green + crosshair overlay (layered, black = transparent)
    CCrosshairWnd   m_crosshair;

protected:
    virtual void DoDataExchange(CDataExchange *pDX);
    virtual BOOL OnInitDialog();
    virtual void PostNcDestroy();
    virtual void OnCancel();

    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnDestroy();
    afx_msg LRESULT OnNcCalcSize(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()

private:
    Acad::ErrorStatus InitDrawingControl(AcDbDatabase *pDb,
                                         const TCHAR *space = ACDB_MODEL_SPACE);
    // Update dialog GsView: 3x zoom, centred on cursor WCS position
    void UpdateDialogView(HWND hwndViewport, POINT cursorScreen);
};
