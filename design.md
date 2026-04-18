# BlockView2022 — Design Document

## Purpose

AutoCAD 2022 ObjectARX plugin (`AsdkBlockView.arx`). A floating modeless magnifier that follows the crosshair cursor and shows the drawing at **3× zoom** relative to the current editor viewport. Intended to help with precise point picking.

---

## Build

| Item | Value |
|---|---|
| Project file | `BlockView.vcxproj` |
| Configuration | `Debug2022 \| x64` |
| Output | `Debug2022\AsdkBlockView.arx` |
| SDK | `C:\Autodesk\ObjectARX 2022` (macro `$(ARXSDK)` in `sdk.props`) |
| Toolset | v145 (VS 2022 Community) |
| Extra libs | `gdiplus.lib` (GDI+), `AdImaging.lib`, `AdIntImgServices.lib`, `AcSceneOE.lib` |

---

## Source Files

| File | Role |
|---|---|
| `BlockView.cpp` | ARX entry point, command registration |
| `BlockViewCommands.cpp` | `AsdkBlockView()` toggle command, global `g_pBlockViewDlg` |
| `BlockViewDlg.h / .cpp` | Main dialog class + all supporting classes (`COsnapMonitor`, `CGridDrawable`, `CCrosshairWnd`, `CDbChangeReactor`) |
| `GsPreviewCtrl.h / .cpp` | MFC static subclass wrapping AcGs objects |
| `BlockView.rc` | Dialog resource `IDD_BLOCKVIEW` |

---

## Commands

| Command | Alias | Flag | Behaviour |
|---|---|---|---|
| `BLOCKVIEW` | `BVIEW` | `ACRX_CMD_TRANSPARENT` | Toggle magnifier on/off |
| `CONFIGGS` | — | — | Open AcGs configuration dialog |
| `ROS` / `RenderOffScreen` | — | — | Off-screen render to image file |

---

## Class Overview

### `CBlockViewDlg : public CAcUiDialog`

The main floating dialog. Modeless, popup, no caption (`WS_THICKFRAME` + `OnNcCalcSize → 0`).

**Key members:**

| Member | Type | Purpose |
|---|---|---|
| `mPreviewCtrl` | `CGsPreviewCtrl` | AcGs-backed preview, inset 4 px from edges |
| `m_crosshair` | `CCrosshairWnd` | Layered green + marker, centred at (100,100) |
| `m_gridDrawable` | `CGridDrawable` | AcGiDrawable — grid lines + OSNAP marker in AcGs scene |
| `m_pGridModel` | `AcGsModel*` | Model for `m_gridDrawable`, added before entity model |
| `m_dbReactor` | `CDbChangeReactor` | Forwards DB changes to AcGsModel incrementally |
| `m_osnapMonitor` | `COsnapMonitor` | Captures live snap point from `AcEdInputPointManager` |
| `m_gdipToken` | `ULONG_PTR` | GDI+ lifetime token |
| `mCurrentDwg` | `AcDbDatabase*` | Database being displayed |
| `m_viewMatrix` | `AcGeMatrix3d` | WCS → view coordinate space |

**`OnInitDialog` sequence (order matters):**

1. `GdiplusStartup` — must precede base class (base can trigger `WM_ERASEBKGND`)
2. `CAcUiDialog::OnInitDialog()`
3. `SubclassDlgItem(IDC_VIEW)` — attach `CGsPreviewCtrl`
4. `SetWindowPos(200, 200)` — enforce exact pixel size
5. `SetWindowRgn(CreateRoundRectRgn(0,0,201,201,18,18))` — 9 px rounded corners
6. `InitDrawingControl` — create AcGs model, wire reactor, register OSNAP monitor
7. Create `m_crosshair` layered child window with `LWA_COLORKEY` black=transparent
8. Pre-render: `invalidate() + update()` while hidden — prevents blank flash on first show
9. `GetCursorPos + SetWindowPos` — position at cursor before `ShowWindow` — prevents position jump
10. `SetTimer(1, 16)` — 60 fps cursor-follow loop

---

### `OnTimer` — cursor-follow loop (16 ms)

1. `GetCursorPos` → `WindowFromPoint`
2. Detect AutoCAD editor: `GetClassLongPtr(hwnd, GCLP_HCURSOR) == NULL`
3. Not in editor → `SW_HIDE`, return
4. In editor → `SW_SHOWNOACTIVATE`
5. `SetWindowPos(pt.x+5, pt.y-5-height)` — track cursor, above-right
6. `UpdateDialogView(hwndUnder, pt)`

---

### `UpdateDialogView` — per-tick view update

Reads all view state via `acedGetVar` (safe from timer — no DB lock needed):

| Sysvar | Used for |
|---|---|
| `VIEWSIZE` | WCS height of main viewport |
| `VIEWCTR` | View centre in UCS |
| `VIEWDIR` | View direction in WCS |
| `VIEWTWIST` | View rotation angle |

Pixel dimensions: `acgsGetViewportInfo(vportNumber, L, B, R, T)`.

**View coordinate frame:**
```
vpRight = vpDir.perpVector().rotateBy(viewTwist, -vpDir)
vpUp    = vpDir × vpRight
```

**Cursor WCS mapping:**
```
nx = screenX/wndW - 0.5        // normalised, -0.5 … +0.5
ny = 0.5 - screenY/wndH
cursorWCS = vpCenter + vpRight*(nx*vpWidth) + vpUp*(ny*vpHeight)
```

**True 3× zoom formula:**
```
dlgH = vpHeight * (dlgPixH / vpPixH) / 3
```

**OSNAP color read (safe here — timer context, not renderer):**
```cpp
AcColorSettings cs;
acedGetCurrentColors(&cs);            // reads Options > Drafting snap marker color
DWORD cr = cs.dwModelASnapMarkerColor;
// fallback to yellow if call fails or returns black
```

**After computing the view:**
1. `mpView->setView(eye, cursorWCS, vpUp, dlgW, dlgH)`
2. Feed snap data: `m_gridDrawable.SetSnapPoint(hasSnap, pt, mask, halfSize, R, G, B, pixelSize)`
3. `mpView->invalidate(); mpView->update()`

---

### `COsnapMonitor : public AcEdInputPointMonitor`

Captures the active snap point on every cursor move. Registered with `acDocManager->curDocument()->inputPointManager()->addPointMonitor()` during `InitDrawingControl`; removed in `OnDestroy`.

| Member | Purpose |
|---|---|
| `m_snapPt` | Last snapped WCS point |
| `m_snapMask` | `AcDb::OsnapMask` — which snap type is active |
| `m_hasSnap` | True when `osnapMask != 0` |

`monitorInputPoint(const AcEdInputPoint& input, ...)` — uses the new single-argument overload (old multi-arg overload is `final` in 2022 SDK).

---

### `CGridDrawable : public AcGiDrawable`

AcGiDrawable added to a dedicated `AcGsModel` (created before the entity model so grid renders first, entities paint over it at the same z).

**`SetLines`** — called each tick with pre-computed WCS line data (grid lines).

**`SetSnapPoint(hasSnap, pt, mask, halfSize, R, G, B, pixelSize)`** — stores snap state for `subWorldDraw`.

**`subWorldDraw`** draws:

1. **Grid lines** — minor `RGB(70,70,70)`, major `RGB(120,120,120)`, z-offset `−0.001` so entities render in front.
2. **OSNAP marker** — if `m_hasSnap`:
   - Color: from stored `m_snapR/G/B` (read via `acedGetCurrentColors` each tick).
   - Thickness: 3 drawing passes at offsets `(0,0)`, `(+1px,0)`, `(0,+1px)` in WCS space. `setLineWeight` has no effect on secondary views.
   - Shape per snap type:

| Snap | Shape |
|---|---|
| `kOsMaskEnd` | Hollow square |
| `kOsMaskMid` | Hollow triangle (tip up) |
| `kOsMaskCen` | Hollow circle (12-gon) |
| `kOsMaskNode` | Circle + X through centre |
| `kOsMaskQuad` | Hollow diamond |
| `kOsMaskInt` | X (two diagonal lines) |
| `kOsMaskIns` | Square + tick marks at side midpoints |
| `kOsMaskPerp` | Right-angle L with corner square |
| `kOsMaskTan` | Circle + horizontal tangent line at bottom |
| `kOsMaskNear` | Hourglass (two triangles tips-touching) |
| `kOsMaskApint` | X + horizontal bar below |
| fallback | Hollow square |

---

### `CDbChangeReactor : public AcDbDatabaseReactor`

Forwards per-entity changes directly to `AcGsModel` for incremental cache updates. Avoids `kInvalidateAll` (which caused a full scene rebuild and visible freeze).

**Filtering:** only entities in model space (`pObj->ownerId() == m_spaceId`).

| DB event | AcGsModel call |
|---|---|
| `objectAppended` | `onAdded(entity, parentId)` |
| `objectModified` | `onModified(entity, parentId)` |
| `objectErased` | `onErased(entity, parentId)` |

`parentId = pObj->ownerId().asOldId()` — the model space block's `IntDbId`.

Wired in `InitDrawingControl` after the model is created; removed in `OnDestroy`.

---

### `CCrosshairWnd : public CWnd`

20×20 px layered child window centred at (90, 90) in the 200×200 dialog.

- `WS_EX_LAYERED` + `LWA_COLORKEY(RGB(0,0,0))` — black pixels are transparent
- `OnPaint`: fill black, draw green lines with 2 px gap at centre (7 px arms)
- Flicker-free: separate HWND composited by DWM on top of the scene; AcGs cannot overwrite it

---

### `CGsPreviewCtrl : public CStatic`

Subclasses the `IDC_VIEW` static control. Owns all AcGs objects.

**Members:** `mpManager`, `mpGraphicsKernel`, `mpDevice`, `mpView`, `mpModel`

**`init()`:** creates AcGsManager → Kernel → Device → View → Model chain; sets cursor to `IDC_ARROW`.

**Message handlers:**
- `OnPaint`: `invalidate() + update()`
- `OnSize`: `mpDevice->onSize(w, h)`
- `OnNcHitTest`: returns `HTCLIENT`
- `OnMButtonDown/Up/Move`: middle-mouse pan (kept for testing)

---

## Visual Design

| Property | Value |
|---|---|
| Window size | 200 × 200 px (enforced in `OnInitDialog`) |
| Rounded corners | 9 px radius (`SetWindowRgn`) |
| Border width | 4 px (IDC_VIEW inset by 4 px each side) |
| Border style | GDI+ linear gradient, icy blue-white (top) → vivid blue (bottom) |
| Border top glint | 1 px white arc — Vista Aero style |
| NC area | Collapsed to zero via `OnNcCalcSize → 0` (removes DWM grey strip) |
| Crosshair | 15 px green + marker, flicker-free layered HWND |

**Border colours:**
- Top: `Color(255, 200, 225, 255)` — pale icy blue
- Bottom: `Color(255, 55, 120, 215)` — vivid blue
- Glint: `Color(180, 255, 255, 255)` — semi-transparent white

---

## Critical Implementation Rules

1. **Never open AcDb viewport table from `WM_TIMER`** — use `acedGetVar` instead (no doc lock required)
2. **`SWP_NOACTIVATE` on all `SetWindowPos` / `ShowWindow`** — must never steal focus from AutoCAD
3. **`GdiplusStartup` before `CAcUiDialog::OnInitDialog()`** — base class can fire `WM_ERASEBKGND` before init
4. **No `WS_CLIPSIBLINGS` on `IDC_VIEW`** — required so GsView renders into the crosshair area; DWM composites the layered overlay on top
5. **Pre-render in `OnInitDialog`** — `invalidate+update` while window is hidden prevents blank blue flash on first show
6. **Initial cursor position in `OnInitDialog`** — `GetCursorPos+SetWindowPos` before `ShowWindow` prevents position jump
7. **`ACRX_CMD_TRANSPARENT`** on `BVIEW` — allows invoking during another active command
8. **`<OutputFile>` in `<Link>` section** — `<TargetExt>` alone does not force `.arx` extension
9. **`AcGsModel::onAdded/onModified/onErased`** instead of `kInvalidateAll` — full rebuild causes visible freeze on object add/erase
10. **`acedGetCurrentColors` must be called from timer context, not from `subWorldDraw`** — calling it inside AcGi callbacks may fail silently; read color in `UpdateDialogView` and pass it to the drawable
11. **`setLineWeight` has no effect on secondary AcGsViews** — line weight display is not enabled on manually created views; simulate thickness with multiple drawing passes at pixel offsets
12. **`AcGsModel::enableLinetypes(true)` after `createAutoCADModel`** — secondary models default to linetypes disabled; without this call dashed/dotted entities render as solid
13. **`monitorInputPoint` new overload only** — the old multi-argument overload is marked `final` in ObjectARX 2022; override `monitorInputPoint(const AcEdInputPoint&, AcEdInputPointMonitorResult&)` instead

---

## Transient Graphics — Implementation Research

Research completed April 2026. APIs located in `C:/Autodesk/ObjectARX 2022/inc/`.

### Command-preview transients

`AcGiTransientManager` (`acgitransient.h`) — global via `acgiGetTransientManager()`.

| Method | Purpose |
|---|---|
| `addTransient(drawable, mode, subMode, vpNums)` | Register drawable in a transient layer |
| `updateTransient(drawable, vpNums)` | Force redraw |
| `eraseTransient(drawable, vpNums)` | Remove |

Best modes for magnifier: `kAcGiDirectShortTerm` (rubber-band previews, no Z-test) and `kAcGiDirectTopmost` (always on top).

To mirror main-viewport transients into the magnifier view, register both viewport numbers:
```cpp
AcArray<int> vpNums;
vpNums.append(1);             // main model-space viewport
vpNums.append(magnifierVp);   // magnifier AcGsView
pMgr->addTransient(drawable, kAcGiDirectShortTerm, 0, vpNums);
```

Custom `AcGiDrawable` subclass must implement `subViewportDraw(vd)` (not `subWorldDraw`) to emit geometry and read per-viewport camera state.

### Rollover hover glow

`kAcGiHighlightGlow` is internal to AutoCAD's selection system — no public subscribe API.

Workaround: `AcEdInputPointMonitor` (`acedinpt.h`) — `monitorInputPoint` fires on every cursor move and provides entities under the aperture. Register via `acedGetInputPointManager()->addPointMonitor()`. On hover enter/leave, manually apply a glow pass using `AcDbHighlightOverrule` (`dbentityoverrule.h`) — `highlight/unhighlight/highlightState`.

Global highlight style controls (`acgs.h`): `acgsSetHighlightStyle`, `acgsSetHighlightColor`, `acgsSetHighlightLinePattern`.

### Jig detection

`AcEdInputContextReactor` (`acedinpt.h`) — `beginGetPoint/endGetPoint` callbacks signal active jig/command input. Use to show/hide transient geometry in the magnifier during rubber-band operations. Register via `acedGetInputPointManager()->addInputContextReactor()`.

### AcGsModel render types (for overlay ordering)

| `AcGsModel::RenderType` | Behaviour |
|---|---|
| `kMain` | Standard Z-buffer |
| `kDirect` | Direct render, no Z-test |
| `kDirectTopmost` | Topmost, no Z-test — use for grid/crosshair overlays |
| `kHighlight` | Highlight layer |

---

## Known Limitations

| Issue | Root cause | Status |
|---|---|---|
| OSNAP for transient/command-preview geometry | Rubber-band/jig entities are in open write transactions; `AcGiTransientManager` needed | Deferred |
| Trim/command highlight previews not shown | Same as above | Deferred |
| Rollover hover glow not shown | Applied by AutoCAD selection system internally; no public subscribe API | Deferred — would need `AcEdInputPointMonitor` + custom highlight pass |
| IDC_VIEW inner rounded corners | `SetWindowRgn` in `OnSize` partially working | Revisit later |
| Middle-mouse pan in `CGsPreviewCtrl` | Kept for testing | Remove before release |
