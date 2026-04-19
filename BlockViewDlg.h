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
#include <acedinpt.h>

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
// CGridDrawable — AcGiDrawable that emits pre-computed WCS grid lines
// directly into the AcGs scene, rendered before the entity model so grid
// lines appear behind drawing objects (depth-buffer handles 3D correctly).

// COsnapMonitor — captures the active OSNAP snap point each cursor move.
// Registered with acedGetInputPointManager() while the magnifier is open.
// Thread-safe reads: written on the ARX/main thread, read on the same timer thread.

class COsnapMonitor : public AcEdInputPointMonitor
{
public:
    ACRX_DECLARE_MEMBERS(COsnapMonitor);

    AcGePoint3d     m_snapPt;
    AcDb::OsnapMask m_snapMask = static_cast<AcDb::OsnapMask>(0);
    bool            m_hasSnap  = false;

    Acad::ErrorStatus monitorInputPoint(const AcEdInputPoint& input,
                                        AcEdInputPointMonitorResult&) override
    {
        m_snapMask = input.osnapMask();
        m_hasSnap  = (m_snapMask != 0);
        if (m_hasSnap)
            m_snapPt = input.osnappedPoint();
        return Acad::eOk;
    }
};

/////////////////////////////////////////////////////////////////////////////
// CGridDrawable — AcGiDrawable that emits pre-computed WCS grid lines
// directly into the AcGs scene, rendered before the entity model so grid
// lines appear behind drawing objects (depth-buffer handles 3D correctly).

class CGridDrawable : public AcGiDrawable
{
public:
    struct Line { AcGePoint3d p1, p2; bool major; };
    AcGsModel* m_pModel = nullptr;

    void SetLines(const AcArray<Line>& lines) { m_lines = lines; }

    // Snap marker — set each tick by UpdateDialogView from COsnapMonitor data.
    // snapColor: read from AcColorSettings in UpdateDialogView (safe timer context).
    // pixelSize: WCS units per screen pixel — used to simulate line thickness.
    void SetSnapPoint(bool hasSnap, const AcGePoint3d& pt, AcDb::OsnapMask mask,
                      double markerHalfSize, BYTE r, BYTE g, BYTE b, double pixelSize)
    {
        m_hasSnap        = hasSnap;
        m_snapPt         = pt;
        m_snapMask       = mask;
        m_markerHalfSize = markerHalfSize;
        m_snapR = r; m_snapG = g; m_snapB = b;
        m_pixelSize      = pixelSize;
    }

protected:
    Adesk::UInt32 subSetAttributes(AcGiDrawableTraits*) override
        { return kDrawableRegenDraw; }

    Adesk::Boolean subWorldDraw(AcGiWorldDraw* wd) override
    {
        AcCmEntityColor minorCol, majorCol;
        minorCol.setRGB(70, 70, 70);
        majorCol.setRGB(120, 120, 120);
        AcGePoint3d pts[2];
        for (int i = 0; i < m_lines.length(); ++i)
        {
            const Line& ln = m_lines[i];
            wd->subEntityTraits().setTrueColor(ln.major ? majorCol : minorCol);
            pts[0] = ln.p1; pts[1] = ln.p2;
            wd->geometry().polyline(2, pts);
        }

        // OSNAP marker — shapes and color match AutoCAD's standard snap glyphs.
        // Color stored by UpdateDialogView; thickness by 3-pass pixel offset rendering.
        if (m_hasSnap && m_markerHalfSize > 0.0)
        {
            AcCmEntityColor snapColor;
            snapColor.setRGB(m_snapR, m_snapG, m_snapB);
            wd->subEntityTraits().setTrueColor(snapColor);

            double h  = m_markerHalfSize;
            double px = m_pixelSize;   // 1 WCS pixel — used for thickness passes

            // Draw the shape 3 times at (0,0), (+px,0), (0,+px) to fake ~2px thickness.
            const double offX[3] = { 0.0,  px, 0.0 };
            const double offY[3] = { 0.0, 0.0,  px };
            for (int pass = 0; pass < 3; ++pass)
            {
            double x = m_snapPt.x + offX[pass];
            double y = m_snapPt.y + offY[pass];
            double z = m_snapPt.z;

            auto polyline = [&](std::initializer_list<AcGePoint3d> pts) {
                AcArray<AcGePoint3d> arr;
                for (auto& p : pts) arr.append(p);
                wd->geometry().polyline(arr.length(), arr.asArrayPtr());
            };

            auto circle = [&](int N = 12) {
                AcArray<AcGePoint3d> pts;
                for (int i = 0; i <= N; ++i) {
                    double a = i * 6.28318530717958647 / N;
                    pts.append(AcGePoint3d(x + h * cos(a), y + h * sin(a), z));
                }
                wd->geometry().polyline(pts.length(), pts.asArrayPtr());
            };

            if (m_snapMask & AcDb::kOsMaskEnd)
            {
                // Endpoint: hollow square
                polyline({{x-h,y-h,z},{x+h,y-h,z},{x+h,y+h,z},{x-h,y+h,z},{x-h,y-h,z}});
            }
            else if (m_snapMask & AcDb::kOsMaskMid)
            {
                // Midpoint: hollow triangle tip-up
                polyline({{x,y+h,z},{x-h,y-h,z},{x+h,y-h,z},{x,y+h,z}});
            }
            else if (m_snapMask & AcDb::kOsMaskCen)
            {
                // Center: hollow circle
                circle(12);
            }
            else if (m_snapMask & AcDb::kOsMaskNode)
            {
                // Node: circle + X through it
                circle(12);
                polyline({{x-h,y,z},{x+h,y,z}});
                polyline({{x,y-h,z},{x,y+h,z}});
            }
            else if (m_snapMask & AcDb::kOsMaskQuad)
            {
                // Quadrant: hollow diamond
                polyline({{x,y+h,z},{x+h,y,z},{x,y-h,z},{x-h,y,z},{x,y+h,z}});
            }
            else if (m_snapMask & AcDb::kOsMaskInt)
            {
                // Intersection: X
                polyline({{x-h,y-h,z},{x+h,y+h,z}});
                polyline({{x+h,y-h,z},{x-h,y+h,z}});
            }
            else if (m_snapMask & AcDb::kOsMaskIns)
            {
                // Insertion: square with tick marks at midpoints of each side
                polyline({{x-h,y-h,z},{x+h,y-h,z},{x+h,y+h,z},{x-h,y+h,z},{x-h,y-h,z}});
                double t = h * 0.4;
                polyline({{x-h,y-t,z},{x-h-t,y,z},{x-h,y+t,z}});
                polyline({{x+h,y-t,z},{x+h+t,y,z},{x+h,y+t,z}});
                polyline({{x-t,y+h,z},{x,y+h+t,z},{x+t,y+h,z}});
                polyline({{x-t,y-h,z},{x,y-h-t,z},{x+t,y-h,z}});
            }
            else if (m_snapMask & AcDb::kOsMaskPerp)
            {
                // Perpendicular: right-angle symbol
                polyline({{x-h,y+h,z},{x-h,y-h,z},{x+h,y-h,z}});
                polyline({{x-h,y-h*0.3,z},{x-h*0.3,y-h*0.3,z},{x-h*0.3,y-h,z}});
            }
            else if (m_snapMask & AcDb::kOsMaskTan)
            {
                // Tangent: circle with horizontal tangent line at bottom
                circle(12);
                polyline({{x-h,y-h,z},{x+h,y-h,z}});
            }
            else if (m_snapMask & AcDb::kOsMaskNear)
            {
                // Nearest: hourglass (two triangles, tips touching at centre)
                polyline({{x-h,y+h,z},{x+h,y+h,z},{x,y,z},{x-h,y+h,z}});
                polyline({{x-h,y-h,z},{x+h,y-h,z},{x,y,z},{x-h,y-h,z}});
            }
            else if (m_snapMask & AcDb::kOsMaskApint)
            {
                // Apparent intersection: X with a horizontal bar below
                polyline({{x-h,y-h,z},{x+h,y+h,z}});
                polyline({{x+h,y-h,z},{x-h,y+h,z}});
                polyline({{x-h,y-h*1.4,z},{x+h,y-h*1.4,z}});
            }
            else
            {
                // Fallback: hollow square
                polyline({{x-h,y-h,z},{x+h,y-h,z},{x+h,y+h,z},{x-h,y+h,z},{x-h,y-h,z}});
            }
            } // end thickness pass loop
        }

        return Adesk::kTrue;
    }

    void           subViewportDraw(AcGiViewportDraw*) override {}
    Adesk::Boolean isPersistent() const override { return Adesk::kFalse; }
    AcDbObjectId   id()           const override { return AcDbObjectId::kNull; }

private:
    AcArray<Line>   m_lines;
    bool            m_hasSnap        = false;
    AcGePoint3d     m_snapPt;
    AcDb::OsnapMask m_snapMask       = static_cast<AcDb::OsnapMask>(0);
    double          m_markerHalfSize = 0.0;
    BYTE            m_snapR = 255, m_snapG = 255, m_snapB = 0;
    double          m_pixelSize      = 1.0;
};

/////////////////////////////////////////////////////////////////////////////
// CDbChangeReactor — forwards per-entity add/modify/erase directly to the
// AcGsModel so it can update incrementally without a full kInvalidateAll.

class CDbChangeReactor : public AcDbDatabaseReactor
{
public:
    AcGsModel*   m_pModel  = nullptr;
    AcDbObjectId m_spaceId;

    void objectAppended(const AcDbDatabase*, const AcDbObject* p) override
        { notify(p, 0); }
    void objectModified(const AcDbDatabase*, const AcDbObject* p) override
        { notify(p, 1); }
    void objectErased  (const AcDbDatabase*, const AcDbObject* p, Adesk::Boolean) override
        { notify(p, 2); }

private:
    void notify(const AcDbObject* pObj, int type)
    {
        if (!m_pModel || !pObj) return;
        if (pObj->ownerId() != m_spaceId) return;
        AcDbEntity* pEnt = AcDbEntity::cast(const_cast<AcDbObject*>(pObj));
        if (!pEnt) return;
        Adesk::IntDbId pid = pObj->ownerId().asOldId();
        switch (type)
        {
        case 0: m_pModel->onAdded   (pEnt, pid); break;
        case 1: m_pModel->onModified(pEnt, pid); break;
        case 2: m_pModel->onErased  (pEnt, pid); break;
        }
    }
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
    CCrosshairWnd    m_crosshair;
    // Grid lines rendered as an AcGiDrawable in the AcGs scene (before entity model)
    CGridDrawable    m_gridDrawable;
    AcGsModel*       m_pGridModel = nullptr;
    // GDI+ token (initialised in OnInitDialog, shut down in PostNcDestroy)
    ULONG_PTR        m_gdipToken = 0;
    CDbChangeReactor m_dbReactor;
    COsnapMonitor    m_osnapMonitor;

protected:
    virtual void DoDataExchange(CDataExchange *pDX);
    virtual BOOL OnInitDialog();
    virtual void PostNcDestroy();
    virtual void OnCancel();

    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnDestroy();
    afx_msg LRESULT OnNcCalcSize(WPARAM wParam, LPARAM lParam);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);

    DECLARE_MESSAGE_MAP()

    static void CALLBACK TimerProc(HWND, UINT, UINT_PTR, DWORD);
    static UINT_PTR s_timerID;

private:
    Acad::ErrorStatus InitDrawingControl(AcDbDatabase *pDb,
                                         const TCHAR *space = ACDB_MODEL_SPACE);
    // Update dialog GsView: 3x zoom, centred on cursor WCS position
    void UpdateDialogView(HWND hwndViewport, POINT cursorScreen);
};
