# ObjectARX 2022 Transient Graphics Research

## Summary of Findings

This comprehensive audit of the ObjectARX 2022 SDK headers has located all relevant APIs for implementing:

1. Command-preview transients (rubber-bands, jig previews)
2. Rollover highlight glow effects
3. Grid display overlays
4. Secondary magnifier AcGsView

---

## 1. COMMAND-PREVIEW TRANSIENTS

### Primary API: AcGiTransientManager

File: acgitransient.h

Methods:
- addTransient(drawable, mode, subMode, viewportNumbers)
- eraseTransient(drawable, viewportNumbers)
- updateTransient(drawable, viewportNumbers)
- addChildTransient(drawable, parentDrawable)
- eraseChildTransient(drawable, parentDrawable)
- updateChildTransient(drawable, parentDrawable)
- getFreeSubDrawingMode(mode, subMode, viewportNumbers)

Global: acgiGetTransientManager(), acgiSetTransientManager()

### Transient Drawing Modes

Enum AcGiTransientDrawingMode:
- kAcGiMain - Main scene
- kAcGiSprite - UI layer
- kAcGiDirectShortTerm - Rubber-band previews (NO Z-test)
- kAcGiHighlight - Highlight layer
- kAcGiDirectTopmost - Topmost layer
- kAcGiContrast - Contrast style

Best for magnifier: kAcGiDirectShortTerm and kAcGiDirectTopmost

---

## 2. ROLLOVER HIGHLIGHT GLOW

### Highlight Style Control

File: acgidefs.h, acgs.h

Enum AcGiHighlightStyle:
- kAcGiHighlightNone
- kAcGiHighlightCustom
- kAcGiHighlightDashedAndThicken
- kAcGiHighlightDim
- kAcGiHighlightThickDim
- kAcGiHighlightGlow (target for magnifier)

Global functions:
- acgsSetHighlightStyle(style)
- acgsGetHighlightStyle()
- acgsSetHighlightColor(colorIndex)
- acgsGetHighlightColor()
- acgsSetHighlightLinePattern(pattern)
- acgsGetHighlightLinePattern()
- acgsSetHighlightLineWeight(weight)
- acgsGetHighlightLineWeight()

Issue: kHighlightGlow is INTERNAL to AutoCAD selection system.
Workaround: Use AcEdInputPointMonitor to detect hovers, render glow manually.

### Entity Highlight API

File: dbentityoverrule.h

Class AcDbHighlightOverrule:
- highlight(entity, subId, highlightAll)
- unhighlight(entity, subId, highlightAll)
- highlightState(entity, subId) -> AcGiHighlightStyle

---

## 3. HOVER DETECTION

### AcEdInputPointMonitor

File: acedinpt.h

Class methods:
- monitorInputPoint(input, output) - Called on every cursor move
- mouseHasMoved() - Query mouse state

Register with: acedGetInputPointManager()->addPointMonitor()
Deregister with: removePointMonitor()

Input data includes:
- Cursor position (raw and snapped)
- OSNAP information
- Entities under aperture
- History flags

### AcEdInputContextReactor

File: acedinpt.h

Callbacks:
- beginGetPoint/endGetPoint
- beginGetAngle/endGetAngle
- beginGetDistance/endGetDistance
- beginGetString/endGetString

Register: acedGetInputPointManager()->addInputContextReactor()
Deregister: removeInputContextReactor()

Use to detect command start/end and manage transient visibility.

---

## 4. GRID DISPLAY

### Grid System Variables

Grid is NOT exposed as drawable or model.
Access via AutoCAD system variables:

- GRIDMODE: 0=off, 1=on
- GRIDUNIT: AcGePoint3d with X, Y spacing

Query via: acedGetVar(_T("GRIDMODE"), &gridMode)

### Grid Rendering in Magnifier

Implement in custom AcGiDrawable::subViewportDraw():
1. Query GRIDMODE and GRIDUNIT
2. Calculate grid line positions
3. Draw polylines via AcGiViewportGeometry
4. Use kAcGiDirectTopmost transient mode

---

## 5. SECONDARY VIEW CREATION

### Creating Magnifier AcGsView

File: AcGsManager.h

Steps:
1. Get AcGsManager via acgsGetCurrentGsManager()
2. Acquire kernel: AcGsManager::acquireGraphicsKernel(descriptor)
3. Create device: pMgr->createAutoCADOffScreenDevice(kernel)
4. Create view: pMgr->createView(device)
5. Set viewport: pView->setViewport(rect)
6. Set camera: pView->setView(pos, target, up, w, h)
7. Add model: pView->add(drawable, model)
8. Update: pView->update()

### View Synchronization Methods

File: gs.h

Class AcGsView key methods:
- setView(position, target, upVector, fieldWidth, fieldHeight, projection)
- position(), target(), upVector()
- fieldWidth(), fieldHeight()
- zoom(factor), pan(x,y), dolly(vector)
- zoomWindow(ll, ur)
- setViewport(rect), getViewport(rect)
- invalidate(), update()
- add(drawable, model), erase(drawable)
- getDevice(), getModel(drawable), getModelList(models)

---

## 6. RENDER TYPES FOR OVERLAYS

File: gs.h

Enum AcGsModel::RenderType:
- kMain - Standard Z-buffer
- kSprite - UI layer Z-buffer
- kDirect - Direct render no Z-test
- kHighlight - Highlight layer
- kHighlightSelection - Internal
- kDirectTopmost - Topmost, no Z-test
- kContrast - Contrast style

For magnifier: Use kDirectTopmost transient mode.

---

## 7. CUSTOM TRANSIENT DRAWABLE

File: drawable.h, acgi.h

Class AcGiDrawable requirements:
- subSetAttributes(traits)
- subWorldDraw(wd)
- subViewportDraw(vd) <- PRIMARY for magnifier
- isPersistent() -> false
- id() -> kNull

Implement subViewportDraw() to emit:
- Grid lines via geometry.polyline()
- Highlight effects via traits.setTrueColor()
- Preview geometry from jigs

---

## 8. VIEWPORT NUMBERS

Main viewport: 1 (model space)
Paperspace viewports: > 1

When registering transients:
  AcArray<int> vpNums;
  vpNums.append(1);           // Main
  vpNums.append(magnifierVp); // Magnifier
  
  pMgr->addTransient(drawable, mode, 0, vpNums);

---

## 9. KEY HEADER FILES

All at: C:/Autodesk/ObjectARX 2022/inc/

Critical headers:
- acgitransient.h - Transient manager
- acgs.h - Graphics functions
- gs.h - View/Device/Model classes
- AcGsManager.h - Manager for creation
- acgi.h - Drawing contexts
- drawable.h - AcGiDrawable base
- acedinpt.h - Input monitoring
- dbjig.h - Jig API
- dbentityoverrule.h - Highlight override
- acgidefs.h - Highlight enums
- dbmain.h - Entity methods

---

Research Date: April 2026
SDK Version: ObjectARX 2022
Scope: Complete API audit for magnifier implementation
