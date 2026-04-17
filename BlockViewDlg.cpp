//
// (C) Copyright 2002-2008 by Autodesk, Inc.
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
//

#include "StdAfx.h"
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#if defined(_DEBUG) && !defined(AC_FULL_DEBUG)
//#error _DEBUG should not be defined except in internal Adesk debug builds
#endif

#include "stdarx.h"
#include "RgbModel.h"
#include "BlockViewDlg.h"
#include "resource.h"
#include ".\blockviewdlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

BEGIN_MESSAGE_MAP(CCrosshairWnd, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CBlockViewDlg dialog

extern CAcExtensionModule BlockViewDLL;

// Global pointer — defined in BlockViewCommands.cpp
extern CBlockViewDlg* g_pBlockViewDlg;

CBlockViewDlg::CBlockViewDlg(CWnd *pParent /*=NULL*/)
: CAcUiDialog(CBlockViewDlg::IDD, pParent)
{
}

void CBlockViewDlg::DoDataExchange(CDataExchange *pDX)
{
    CAcUiDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CBlockViewDlg, CAcUiDialog)
    ON_WM_SIZE()
    ON_WM_TIMER()
    ON_WM_DESTROY()
    ON_WM_ERASEBKGND()
    ON_MESSAGE(WM_NCCALCSIZE, OnNcCalcSize)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CBlockViewDlg message handlers

BOOL CBlockViewDlg::OnInitDialog()
{
    // GDI+ must be ready before the base class call, which can trigger
    // WM_ERASEBKGND before we get a chance to initialise it ourselves.
    Gdiplus::GdiplusStartupInput gdipInput;
    Gdiplus::GdiplusStartup(&m_gdipToken, &gdipInput, nullptr);

    CAcUiDialog::OnInitDialog();

    if (!mPreviewCtrl.SubclassDlgItem(IDC_VIEW, this))
        return FALSE;

    // Exact 200×200 pixel window (RC dialog units are approximate).
    SetWindowPos(nullptr, 0, 0, 200, 200, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    // Rounded corners (radius 9px) — clip the window shape itself.
    HRGN hRgn = CreateRoundRectRgn(0, 0, 201, 201, 18, 18);
    SetWindowRgn(hRgn, FALSE);   // region handle is consumed by the OS

    InitDrawingControl(acdbHostApplicationServices()->workingDatabase());

    // Green + crosshair overlay: 20×20 layered child, centred in 200×200 client.
    // MFC registers the window class via AfxRegisterWndClass — no manual RegisterClassEx.
    // DWM composites this on top of the scene; black pixels are transparent (LWA_COLORKEY).
    LPCTSTR cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW);
    m_crosshair.CreateEx(WS_EX_LAYERED, cls, nullptr,
                         WS_CHILD | WS_VISIBLE,
                         90, 90, 20, 20,
                         m_hWnd, (HMENU)nullptr);
    if (m_crosshair.m_hWnd)
        ::SetLayeredWindowAttributes(m_crosshair.m_hWnd, RGB(0, 0, 0), 255, LWA_COLORKEY);

    // Pre-render the scene now, while the window is still hidden.
    // Without this the dialog flashes the blank gradient border for ~1 s
    // on first show while the GsView builds its display list.
    if (mPreviewCtrl.mpView)
    {
        mPreviewCtrl.mpView->invalidate();
        mPreviewCtrl.mpView->update();
    }

    // Position next to the cursor right now so the window never appears at
    // the RC template origin (0,0) before the first timer tick moves it.
    POINT pt;
    ::GetCursorPos(&pt);
    SetWindowPos(nullptr, pt.x + 5, pt.y - 5 - 200, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

    // Start cursor-follow timer (~60 fps)
    SetTimer(1, 16, nullptr);

    return TRUE;
}

// Collapse the non-client area to zero so the grey DWM caption strip
// that Windows 10 adds to WS_THICKFRAME popup windows disappears.
LRESULT CBlockViewDlg::OnNcCalcSize(WPARAM wParam, LPARAM lParam)
{
    if (wParam)
        return 0;   // client rect == window rect
    return DefWindowProc(WM_NCCALCSIZE, wParam, lParam);
}

void CBlockViewDlg::PostNcDestroy()
{
    if (m_gdipToken) { Gdiplus::GdiplusShutdown(m_gdipToken); m_gdipToken = 0; }
    g_pBlockViewDlg = nullptr;
    delete this;
}

void CBlockViewDlg::OnCancel()
{
    DestroyWindow();
}

void CBlockViewDlg::OnSize(UINT nType, int cx, int cy)
{
    CWnd *wnd = GetDlgItem(IDC_VIEW);
    if (wnd != NULL)
    {
        wnd->MoveWindow(4, 4, cx - 8, cy - 8);   // 4px border strip all round
        // Round the control corners to match the dialog shape.
        // Inner radius 5px (= outer 9px - 4px border) keeps corners concentric.
        HRGN hRgn = CreateRoundRectRgn(0, 0, cx - 7, cy - 7, 10, 10);
        wnd->SetWindowRgn(hRgn, TRUE);
    }
}

// Vista-style gradient border: GDI+ linear gradient top-to-bottom,
// with anti-aliased rounded path matching the window region.
BOOL CBlockViewDlg::OnEraseBkgnd(CDC* pDC)
{
    CRect rc;
    GetClientRect(&rc);

    Gdiplus::Graphics g(pDC->m_hDC);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    // Gradient: icy blue-white at top → richer blue at bottom (Vista Aero palette)
    Gdiplus::LinearGradientBrush brush(
        Gdiplus::Point(0, 0),
        Gdiplus::Point(0, rc.bottom),
        Gdiplus::Color(255, 200, 225, 255),   // top  — pale icy blue
        Gdiplus::Color(255,  55, 120, 215)    // bottom — vivid blue
    );

    // Rounded-rectangle path that matches the window region (radius 9px)
    const Gdiplus::REAL R = 9.0f;
    const Gdiplus::REAL W = (Gdiplus::REAL)rc.right;
    const Gdiplus::REAL H = (Gdiplus::REAL)rc.bottom;
    Gdiplus::GraphicsPath path;
    path.AddArc(0.0f,  0.0f,  R*2, R*2, 180.0f, 90.0f);
    path.AddArc(W-R*2, 0.0f,  R*2, R*2, 270.0f, 90.0f);
    path.AddArc(W-R*2, H-R*2, R*2, R*2,   0.0f, 90.0f);
    path.AddArc(0.0f,  H-R*2, R*2, R*2,  90.0f, 90.0f);
    path.CloseFigure();

    g.FillPath(&brush, &path);

    // 1px bright highlight along the top edge — the Vista "glass edge" glint
    Gdiplus::Pen highlight(Gdiplus::Color(180, 255, 255, 255), 1.0f);
    g.DrawArc(&highlight,  1.0f, 1.0f, R*2-1.0f, R*2-1.0f, 180.0f, 90.0f);
    g.DrawLine(&highlight, R,    1.0f, W-R,       1.0f);
    g.DrawArc(&highlight,  W-R*2, 1.0f, R*2-1.0f, R*2-1.0f, 270.0f, 90.0f);

    return TRUE;
}

void CBlockViewDlg::OnTimer(UINT_PTR nIDEvent)
{
    POINT pt;
    ::GetCursorPos(&pt);

    // Show only while the AutoCAD drawing-editor crosshair is active.
    // AutoCAD's viewport window sets GCLP_HCURSOR = NULL so it can draw its
    // own crosshair via SetCursor().  Every other surface (ribbon, menus,
    // tool palettes, file dialogs, etc.) has a non-NULL class cursor.
    HWND hwndUnder = ::WindowFromPoint(pt);
    bool inAcadEditor = false;
    if (hwndUnder != NULL &&
        hwndUnder != m_hWnd &&
        !::IsChild(m_hWnd, hwndUnder))
    {
        HCURSOR hCur = (HCURSOR)::GetClassLongPtr(hwndUnder, GCLP_HCURSOR);
        inAcadEditor = (hCur == NULL);
    }

    if (!inAcadEditor)
    {
        if (IsWindowVisible())
            ShowWindow(SW_HIDE);
        return;
    }

    if (!IsWindowVisible())
        ShowWindow(SW_SHOWNOACTIVATE);

    // Position dialog above-right of cursor.
    CRect rc;
    GetWindowRect(&rc);
    SetWindowPos(nullptr,
                 pt.x + 5,
                 pt.y - 5 - rc.Height(),
                 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

    UpdateDialogView(hwndUnder, pt);
}

void CBlockViewDlg::OnDestroy()
{
    KillTimer(1);
    if (mCurrentDwg)
    {
        mCurrentDwg->removeReactor(&m_dbReactor);
        m_dbReactor.m_pModel = nullptr;
    }
    if (m_pGridModel)
    {
        if (mPreviewCtrl.mpView)
            mPreviewCtrl.mpView->erase(&m_gridDrawable);
        if (mPreviewCtrl.mpManager)
            mPreviewCtrl.mpManager->destroyAutoCADModel(m_pGridModel);
        m_pGridModel = nullptr;
    }
    CAcUiDialog::OnDestroy();
}

void CBlockViewDlg::UpdateDialogView(HWND hwndViewport, POINT cursorScreen)
{
    if (mPreviewCtrl.mpView == nullptr) return;

    // ── View parameters via system variables (safe from timer, no DB lock) ──
    struct resbuf rb;

    memset(&rb, 0, sizeof(rb));
    if (acedGetVar(_T("VIEWSIZE"), &rb) != RTNORM) return;
    double vpHeight = rb.resval.rreal;
    if (vpHeight < 1e-10) return;

    // VIEWCTR: center of view in UCS (treat as WCS for plan/default UCS)
    memset(&rb, 0, sizeof(rb));
    if (acedGetVar(_T("VIEWCTR"), &rb) != RTNORM) return;
    AcGePoint3d vpCenter(rb.resval.rpoint[X], rb.resval.rpoint[Y], 0.0);

    // VIEWDIR: view direction in WCS
    memset(&rb, 0, sizeof(rb));
    if (acedGetVar(_T("VIEWDIR"), &rb) != RTNORM) return;
    AcGeVector3d vpDir(rb.resval.rpoint[X], rb.resval.rpoint[Y], rb.resval.rpoint[Z]);
    vpDir = vpDir.normal();

    memset(&rb, 0, sizeof(rb));
    if (acedGetVar(_T("VIEWTWIST"), &rb) != RTNORM) return;
    double viewTwist = rb.resval.rreal;

    // ── Viewport pixel dimensions via AcGs ──────────────────────────────────
    AcDbObjectId curVportId = acedActiveViewportId();
    if (curVportId.isNull()) return;
    int vpPixW = 0, vpPixH = 0;
    {
        AcDbObjectPointer<AcDbViewportTableRecord> curVport(curVportId, AcDb::kForRead);
        if (curVport.openStatus() != Acad::eOk) return;
        int vpL, vpB, vpR, vpT;
        acgsGetViewportInfo(curVport->number(), vpL, vpB, vpR, vpT);
        vpPixW = vpR - vpL;
        vpPixH = vpT - vpB;
    }
    if (vpPixW <= 0 || vpPixH <= 0) return;

    // ── Build view coordinate frame ──────────────────────────────────────────
    AcGeVector3d vpRight = vpDir.perpVector().normal().rotateBy(viewTwist, -vpDir);
    AcGeVector3d vpUp    = vpDir.crossProduct(vpRight);
    double vpWidth = vpHeight * (double)vpPixW / (double)vpPixH;

    // ── Cursor screen → viewport client → normalized → WCS ──────────────────
    // GetClientRect gives the window client size (0,0 top-left).
    // ScreenToClient converts screen cursor to that space.
    RECT vpWndRect = {};
    ::GetClientRect(hwndViewport, &vpWndRect);
    int wndW = vpWndRect.right;
    int wndH = vpWndRect.bottom;
    if (wndW <= 0 || wndH <= 0) return;

    POINT cpt = cursorScreen;
    ::ScreenToClient(hwndViewport, &cpt);
    double nx =  ((double)cpt.x / (double)wndW) - 0.5;   // -0.5 left … +0.5 right
    double ny = -((double)cpt.y / (double)wndH) + 0.5;   // +0.5 top  … -0.5 bottom

    AcGePoint3d cursorWCS = vpCenter
                          + vpRight * (nx * vpWidth)
                          + vpUp    * (ny * vpHeight);

    // ── Dialog field dimensions: true 3× magnification relative to editor ───
    CRect dlgPx;
    mPreviewCtrl.GetClientRect(&dlgPx);
    if (dlgPx.bottom <= 0) return;
    double dlgH = vpHeight * (double)dlgPx.bottom / (3.0 * (double)vpPixH);
    double dlgAspect = (double)dlgPx.right / (double)dlgPx.bottom;
    double dlgW = dlgH * dlgAspect;
    if (dlgW < 1e-10 || dlgH < 1e-10) return;

    // ── Pre-compute grid lines in WCS (sysvar reads safe here in timer context) ─
    AcArray<CGridDrawable::Line> gridLines;
    {
        struct resbuf rb2;
        memset(&rb2, 0, sizeof(rb2));
        if (acedGetVar(_T("GRIDMODE"), &rb2) == RTNORM && rb2.resval.rint != 0)
        {
            memset(&rb2, 0, sizeof(rb2));
            if (acedGetVar(_T("GRIDUNIT"), &rb2) == RTNORM)
            {
                double gx = rb2.resval.rpoint[X], gy = rb2.resval.rpoint[Y];
                if (gx < 1e-10) gx = gy;
                if (gy < 1e-10) gy = gx;
                if (gx > 1e-10 && gy > 1e-10)
                {
                    int major = 5;
                    memset(&rb2, 0, sizeof(rb2));
                    if (acedGetVar(_T("GRIDMAJOR"), &rb2) == RTNORM) major = max(1, rb2.resval.rint);

                    int gridDisp = 0;
                    memset(&rb2, 0, sizeof(rb2));
                    if (acedGetVar(_T("GRIDDISPLAY"), &rb2) == RTNORM) gridDisp = rb2.resval.rint;

                    double ox = 0.0, oy = 0.0;
                    memset(&rb2, 0, sizeof(rb2));
                    if (acedGetVar(_T("SNAPBASE"), &rb2) == RTNORM) {
                        ox = rb2.resval.rpoint[X];
                        oy = rb2.resval.rpoint[Y];
                    }

                    if (gridDisp & 1) {
                        double ppu = vpPixH > 0 ? (double)vpPixH / vpHeight : 1.0;
                        while (gx * ppu < 8.0) gx *= 2.0;
                        while (gy * ppu < 8.0) gy *= 2.0;
                    }

                    // Half-diagonal of the view covers all visible area regardless of twist.
                    double halfDiag = 0.5 * sqrt(dlgW * dlgW + dlgH * dlgH) + max(gx, gy);
                    double cx = cursorWCS.x, cy = cursorWCS.y;

                    int xi = max((int)floor((cx - ox - halfDiag) / gx), -1000);
                    int xe = min((int)ceil ((cx - ox + halfDiag) / gx),  1000);
                    int yi = max((int)floor((cy - oy - halfDiag) / gy), -1000);
                    int ye = min((int)ceil ((cy - oy + halfDiag) / gy),  1000);

                    // Slight z offset puts grid just behind z=0 entities in the
                    // depth buffer so text, lines, arcs all render in front.
                    const double kGridZ = -0.001;
                    for (int n = xi; n <= xe; ++n) {
                        double wx = ox + n * gx;
                        CGridDrawable::Line ln;
                        ln.p1 = AcGePoint3d(wx, cy - halfDiag * 2.0, kGridZ);
                        ln.p2 = AcGePoint3d(wx, cy + halfDiag * 2.0, kGridZ);
                        ln.major = (major > 1 && (n % major == 0));
                        gridLines.append(ln);
                    }
                    for (int m = yi; m <= ye; ++m) {
                        double wy = oy + m * gy;
                        CGridDrawable::Line ln;
                        ln.p1 = AcGePoint3d(cx - halfDiag * 2.0, wy, kGridZ);
                        ln.p2 = AcGePoint3d(cx + halfDiag * 2.0, wy, kGridZ);
                        ln.major = (major > 1 && (m % major == 0));
                        gridLines.append(ln);
                    }
                }
            }
        }
    }
    m_gridDrawable.SetLines(gridLines);

    // ── Update dialog GsView ─────────────────────────────────────────────────
    AcGePoint3d eye = cursorWCS + vpDir;
    mPreviewCtrl.mpView->setView(eye, cursorWCS, vpUp, dlgW, dlgH);
    mPreviewCtrl.mpView->invalidate();
    mPreviewCtrl.mpView->update();

}

//***************************************************************************************
// get the view port information - see parameter list
bool GetActiveViewPortInfo(AcDbDatabase *pDb, ads_real &height, ads_real &width,
                            AcGePoint3d &target, AcGeVector3d &viewDir,
                            ads_real &viewTwist,
                            AcDbObjectId &currentVsId,
                            bool getViewCenter)
{
    if (pDb == NULL)
        return false;

    AcApDocument *pDoc = acDocManager->document(pDb);
    acDocManager->setCurDocument(pDoc);
    acDocManager->lockDocument(pDoc);

    acedVports2VportTableRecords();

    AcDbViewportTablePointer pVTable(pDb->viewportTableId(), AcDb::kForRead);
    if (pVTable.openStatus() == Acad::eOk)
    {
        AcDbViewportTableRecord *pViewPortRec = NULL;
        Acad::ErrorStatus es = pVTable->getAt(_T("*Active"), pViewPortRec, AcDb::kForRead);
        if (es == Acad::eOk)
        {
            height = pViewPortRec->height();
            width  = pViewPortRec->width();
            if (getViewCenter == true)
            {
                struct resbuf rb;
                memset(&rb, 0, sizeof(struct resbuf));
                acedGetVar(_T("VIEWCTR"), &rb);
                target = AcGePoint3d(rb.resval.rpoint[X], rb.resval.rpoint[Y], rb.resval.rpoint[Z]);
            }
            else
            {
                target = pViewPortRec->target();
            }
            viewDir   = pViewPortRec->viewDirection();
            viewTwist = pViewPortRec->viewTwist();
            currentVsId = pViewPortRec->visualStyle();
        }
        pViewPortRec->close();
    }

    acDocManager->unlockDocument(pDoc);
    acDocManager->setCurDocument(acDocManager->mdiActiveDocument());

    return true;
}

//////////////////////////////////////////////////////////////////////////////
// takes a drawing and updates the GsView with it
Acad::ErrorStatus CBlockViewDlg::InitDrawingControl(AcDbDatabase *pDb, const TCHAR *space)
{
    if (pDb == NULL)
        return Acad::eNullBlockName;

    mCurrentDwg = pDb;

    AcDbBlockTableRecordPointer spaceRec(space, pDb, AcDb::kForRead);
    if (spaceRec.openStatus() == Acad::eOk)
    {
        mPreviewCtrl.init(BlockViewDLL.ModuleResourceInstance(), true);
        mPreviewCtrl.SetFocus();
        AcDbObjectId currentVsId;
        currentVsId = SetViewTo(mPreviewCtrl.mpView, pDb, m_viewMatrix);

        // Grid model added BEFORE entity model so AcGs renders it first;
        // entities paint over grid pixels at the same z — grid appears behind.
        m_pGridModel = mPreviewCtrl.mpManager->createAutoCADModel(*mPreviewCtrl.mpGraphicsKernel);
        if (m_pGridModel)
            mPreviewCtrl.view()->add(&m_gridDrawable, m_pGridModel);

        mPreviewCtrl.view()->add(spaceRec, mPreviewCtrl.model());
        mPreviewCtrl.mpView->setVisualStyle(currentVsId);

        // Wire reactor after model exists; filter to model-space entities only.
        m_dbReactor.m_pModel  = mPreviewCtrl.mpModel;
        m_dbReactor.m_spaceId = spaceRec->objectId();
        pDb->addReactor(&m_dbReactor);
    }
    return spaceRec.openStatus();
}

//////////////////////////////////////////////////////////////////////////
// set the passed AcGsView to the *Active* AutoCAD AcDbDatabase view
AcDbObjectId SetViewTo(AcGsView *pView, AcDbDatabase *pDb, AcGeMatrix3d& viewMat)
{
    AcGePoint3d extMax = pDb->extmax();
    AcGePoint3d extMin = pDb->extmin();

    if (extMin.distanceTo(extMax) > 1e20)
    {
        extMin.set(0, 0, 0);
        extMax.set(100, 100, 100);
    }

    ads_real height = 0.0, width = 0.0, viewTwist = 0.0;
    AcGePoint3d targetView;
    AcGeVector3d viewDir;
    AcDbObjectId currentVsId;
    GetActiveViewPortInfo(pDb, height, width, targetView, viewDir, viewTwist, currentVsId, true);

    viewDir = viewDir.normal();

    AcGeVector3d viewXDir = viewDir.perpVector().normal();
    viewXDir = viewXDir.rotateBy(viewTwist, -viewDir);
    AcGeVector3d viewYDir = viewDir.crossProduct(viewXDir);

    AcGePoint3d boxCenter = extMin + 0.5 * (extMax - extMin);

    viewMat = AcGeMatrix3d::alignCoordSys(boxCenter, AcGeVector3d::kXAxis, AcGeVector3d::kYAxis, AcGeVector3d::kZAxis,
        boxCenter, viewXDir, viewYDir, viewDir).inverse();

    AcDbExtents wcsExtents(extMin, extMax);
    AcDbExtents viewExtents = wcsExtents;
    viewExtents.transformBy(viewMat);

    double xMax = fabs(viewExtents.maxPoint().x - viewExtents.minPoint().x);
    double yMax = fabs(viewExtents.maxPoint().y - viewExtents.minPoint().y);

    AcGePoint3d eye = boxCenter + viewDir;
    pView->setView(eye, boxCenter, viewYDir, xMax, yMax);
    refreshView(pView);

    return currentVsId;
}

//////////////////////////////////////////////////////////////////////////
// updates the gsView
void refreshView(AcGsView *pView)
{
    if (pView != NULL)
    {
        pView->invalidate();
        pView->update();
    }
}

//////////////////////////////////////////////////////////////////////////////
// creates an Atil image using AcGsView::getSnapShot() or AcGsView::renderToImage()

#include <dbrender.h>

#include "FileWriteDescriptor.h"
#include "RowProviderInterface.h"
#include "FileSpecifier.h"
#include "DataModelAttributes.h"
#include "DataModel.h"
#include "Image.h"

#include <JfifFormatCodec.h>
#include <PngFormatCodec.h>
#include <TiffFormatCodec.h>
#include <BmpFormatCodec.h>
#include <TiffCustomProperties.h>
#include <PngCustomProperties.h>
#include <FileReadDescriptor.h>

#pragma comment (lib, "AdImaging.lib")
#pragma comment (lib, "AdIntImgServices.lib")
#pragma comment (lib, "AcSceneOE.lib")

bool CreateAtilImage(AcGsView *pView,
                     int width, int height,
                     int colorDepth, int paletteSize,
                     ACHAR *pFileName,
                     bool renderToImage)
{
    bool done = false;

    AcGsDCRect screenRect(0, width, 0, height);
    if (!renderToImage)
        pView->getViewport(screenRect);

    try
    {
        if (colorDepth < 24) colorDepth = 24;
        if (colorDepth > 24) colorDepth = 32;

        Atil::RgbModel rgbModel(colorDepth);
        Atil::ImagePixel initialColor(rgbModel.pixelType());
        Atil::Image imgSource(Atil::Size(width, height), &rgbModel, initialColor);

        if (renderToImage)
        {
            AcDbMentalRayRenderSettings mentalRayRenderer;
            bool ok = pView->renderToImage(&imgSource, &mentalRayRenderer, NULL, screenRect);
            if (!ok)
                AfxMessageBox(_T("Failed to RenderToImage"));
        }
        else
        {
            pView->getSnapShot(&imgSource, screenRect.m_min);
        }

        if (imgSource.isValid())
        {
            Atil::RowProviderInterface *pPipe = imgSource.read(imgSource.size(), Atil::Offset(0, 0), Atil::kBottomUpLeftRight);
            if (pPipe != NULL)
            {
                TCHAR drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT];
                _tsplitpath_s(pFileName, drive, dir, fname, ext);

                Atil::ImageFormatCodec *pCodec = NULL;
                if      (CString(ext) == _T(".jpg")) pCodec = new JfifFormatCodec();
                else if (CString(ext) == _T(".png")) pCodec = new PngFormatCodec();
                else if (CString(ext) == _T(".tif")) pCodec = new TiffFormatCodec();
                else if (CString(ext) == _T(".bmp")) pCodec = new BmpFormatCodec();

                if (pCodec != NULL)
                {
                    if (Atil::FileWriteDescriptor::isCompatibleFormatCodec(pCodec, &(pPipe->dataModel()), pPipe->size()))
                    {
                        Atil::FileWriteDescriptor fileWriter(pCodec);
                        Atil::FileSpecifier fs(Atil::StringBuffer((lstrlen(pFileName) + 1) * sizeof(TCHAR),
                            (const Atil::Byte *)pFileName, Atil::StringBuffer::kUTF_16), Atil::FileSpecifier::kFilePath);

                        _tremove(pFileName);

                        if (fileWriter.setFileSpecifier(fs))
                        {
                            fileWriter.createImageFrame(pPipe->dataModel(), pPipe->size());

                            Atil::FormatCodecPropertyInterface *pProp = fileWriter.getProperty(Atil::FormatCodecPropertyInterface::kCompression);
                            if (pProp != NULL)
                            {
                                if (CString(ext) == _T(".jpg"))
                                {
                                    Atil::FormatCodecIntProperty *pComp = dynamic_cast<Atil::FormatCodecIntProperty*>(pProp);
                                    if (pComp != NULL)
                                    {
                                        int min = 0, max = 0;
                                        pComp->getPropertyLimits(min, max);
                                        pComp->setValue((int)((double)max * .9));
                                        fileWriter.setProperty(pComp);
                                    }
                                }
                                else if (CString(ext) == _T(".png"))
                                {
                                    PngCompression *pComp = dynamic_cast<PngCompression*>(pProp);
                                    if (pComp != NULL)
                                    {
                                        pComp->selectCompression(PngCompressionType::kHigh);
                                        fileWriter.setProperty(pComp);
                                    }
                                }
                                else if (CString(ext) == _T(".tif"))
                                {
                                    TiffCompression *pComp = dynamic_cast<TiffCompression*>(pProp);
                                    if (pComp != NULL)
                                    {
                                        if (pComp->selectCompression(TiffCompressionType::kCCITT_FAX4) == false)
                                            if (pComp->selectCompression(TiffCompressionType::kLZW) == false)
                                                pComp->selectCompression(TiffCompressionType::kNone);
                                        fileWriter.setProperty(pComp);
                                    }
                                }
                                delete pProp;
                                pProp = NULL;
                            }
                        }

                        Atil::FormatCodecPropertySetIterator* pPropsIter = fileWriter.newPropertySetIterator();
                        if (pPropsIter)
                        {
                            for (pPropsIter->start(); !pPropsIter->endOfList(); pPropsIter->step())
                            {
                                Atil::FormatCodecPropertyInterface* pProp = pPropsIter->openProperty();
                                if (pProp->isRequired())
                                    fileWriter.setProperty(pProp);
                                pPropsIter->closeProperty();
                            }
                            delete pPropsIter;
                        }

                        fileWriter.writeImageFrame(pPipe);
                        done = true;
                    }
                }
                delete pCodec;
            }
        }
    }
    catch (Atil::ATILException e)
    {
        const Atil::StringBuffer *pStringMsg = e.getMessage();
    }

    return done;
}
