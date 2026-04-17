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
//
// GsPreviewCtrl.cpp : implementation file
//
#include "StdAfx.h"

#if defined(_DEBUG) && !defined(AC_FULL_DEBUG)
//#error _DEBUG should not be defined except in internal Adesk debug builds
#endif

#include "stdarx.h"
#include "resource.h"
#include "GsPreviewCtrl.h"
#include "Ac64bitHelpers.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CGsPreviewCtrl

BEGIN_MESSAGE_MAP(CGsPreviewCtrl, CStatic)
  //{{AFX_MSG_MAP(CGsPreviewCtrl)
  ON_WM_PAINT()
  ON_WM_SIZE()
  ON_WM_NCHITTEST()
  ON_WM_MBUTTONDOWN()
  ON_WM_MBUTTONUP()
  ON_WM_MOUSEMOVE()
  //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGsPreviewCtrl message handlers

void CGsPreviewCtrl::OnPaint()
{
    CPaintDC dc(this);
    if (!mpView) return;
    mpView->invalidate();
    mpView->update();
}

// Erase view and delete model
void CGsPreviewCtrl::erasePreview()
{
  if (mpView)
    mpView->eraseAll();
  if (mpManager && mpModel) {
    mpManager->destroyAutoCADModel (mpModel);
    mpModel = NULL;
  }
}

void CGsPreviewCtrl::setModel(AcGsModel* pModel)
{
  erasePreview();
  mpModel = pModel;
  mbModelCreated =false;
}

void CGsPreviewCtrl::clearAll()
{
  if (mpView)
  {
    mpView->eraseAll();
  }
  if (mpDevice)
  {
    bool b = mpDevice->erase(mpView);
    RXASSERT(b);
  }

  if (mpGraphicsKernel)
  {
    if (mpView)
    {
      // free all of our temporary entities
      freeTempEntities();

      mpGraphicsKernel->deleteView(mpView);
      mpView = NULL;
    }
    mpGraphicsKernel = NULL;
  }

  if (mpManager)
  {
    if (mpModel)
    {
      if (mbModelCreated)
        mpManager->destroyAutoCADModel(mpModel);
      mpModel = NULL;
    }

    if (mpDevice)
    {
      mpManager->destroyAutoCADDevice(mpDevice);
      mpDevice = NULL;
    }
    mpManager = NULL;
  }
}


//////////////////////////////////////////////////////////////////////////
void CGsPreviewCtrl::init(HMODULE hRes, bool bCreateModel)
{
  // Use the Windows default arrow cursor
  SetClassLongPtr(m_hWnd, GCLP_HCURSOR, (LONG_PTR)LoadCursor(NULL, IDC_ARROW));
  //Instantiate view, a device and a model object
  CRect rect;
  if (!mpManager)
  {
    // get the AcGsManager object for a specified AutoCAD MDI Client CView
    mpManager = acgsGetGsManager();
    RXASSERT(mpManager);

	// get the Graphics Kernel
	AcGsKernelDescriptor descriptor;
    descriptor.addRequirement(AcGsKernelDescriptor::k3DDrawing);
	mpGraphicsKernel = AcGsManager::acquireGraphicsKernel(descriptor);
	RXASSERT(mpGraphicsKernel);

    // create an AcGsDevice object. The window handle passed in to this
    // function is the display surface onto which the graphics system draws
    //a device with standard autocad color palette
    mpDevice = mpManager->createAutoCADDevice(*mpGraphicsKernel, m_hWnd);
    RXASSERT(mpDevice);

    // get the size of the window that we are going to draw in
    GetClientRect( &rect);
    // make sure the gs device knows how big the window is
    mpDevice->onSize(rect.Width(), rect.Height());
    // construct a simple view
    mpView = mpGraphicsKernel->createView();
    RXASSERT(mpView);
    if (bCreateModel)
    {
      RXASSERT(mpModel==NULL);
      // create an AcGsModel object with the AcGsModel::kMain RenderType
      // (which is a hint to the graphics system that the geometry in this
      // model should be rasterized into its main frame buffer). This
      // AcGsModel is created with get and release functions that will open and close AcDb objects.
      mpModel = mpManager->createAutoCADModel(*mpGraphicsKernel);
      RXASSERT(mpModel);
      mbModelCreated = true;
    }
    mpDevice->add(mpView);
  }
}


void CGsPreviewCtrl::OnSize(UINT nType, int cx, int cy)
{
  CRect rect;
  if (mpDevice) {
    GetClientRect( &rect);
    mpDevice->onSize(rect.Width(), rect.Height());
  }
}

#if _MSC_VER < 1400
UINT
#else
LRESULT
#endif
CGsPreviewCtrl::OnNcHitTest(CPoint point)
{
  return HTCLIENT;
}


//////////////////////////////////////////////////////////////////////////
// Middle-mouse pan

void CGsPreviewCtrl::OnMButtonDown(UINT nFlags, CPoint point)
{
    mbPanning = true;
    mLastPanPt = point;
    SetCapture();
}

void CGsPreviewCtrl::OnMButtonUp(UINT nFlags, CPoint point)
{
    mbPanning = false;
    ReleaseCapture();
}

void CGsPreviewCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
    if (!mbPanning || mpView == nullptr) return;

    int dx = point.x - mLastPanPt.x;
    int dy = point.y - mLastPanPt.y;
    mLastPanPt = point;

    if (dx == 0 && dy == 0) return;

    CRect rc;
    GetClientRect(&rc);
    if (rc.Width() <= 0 || rc.Height() <= 0) return;

    double fw = mpView->fieldWidth();
    double fh = mpView->fieldHeight();

    AcGePoint3d  eye = mpView->position();
    AcGePoint3d  tgt = mpView->target();
    AcGeVector3d up  = mpView->upVector();
    AcGeVector3d viewDir = (tgt - eye).normal();
    AcGeVector3d right   = viewDir.crossProduct(up).normal();

    // Screen x → world right (negate: drag right moves view left)
    // Screen y → world up (negate: drag down moves view up)
    double worldDx = -(double)dx * fw / (double)rc.Width();
    double worldDy =  (double)dy * fh / (double)rc.Height();

    AcGeVector3d delta = right * worldDx + up * worldDy;

    mpView->setView(eye + delta, tgt + delta, up, fw, fh);
    mpView->invalidate();
    mpView->update();
}

//////////////////////////////////////////////////////////////////////////
// records a temp entity and returns the total number recorded internally
int CGsPreviewCtrl::addTempEntity(AcDbEntity *ent)
{
  // remember this entity
  mTempEnts.append(ent);
  return mTempEnts.length();
}
//////////////////////////////////////////////////////////////////////////
// frees the temporary memory, returns the total number freed
int CGsPreviewCtrl::freeTempEntities()
{
  int count = 0;
  int length = mTempEnts.length();
  // loop through and free the memory
  for (int i=0; i<length; ++i)
  {
    // if ok
    if (mTempEnts[i] != NULL)
    {
      // delete
      delete mTempEnts[i];
      ++count;
    }
  }

  mTempEnts.removeAll();

  return count;
}
