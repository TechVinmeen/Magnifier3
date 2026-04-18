# BlockView2022

AutoCAD 2022 ObjectARX plugin — a floating magnifier window that follows the crosshair and shows the drawing at **3× zoom**, making precise point picking easier.

![BlockView magnifier](ss-2.png)

---

## Features

- **3× live zoom** — magnification is exact and updates instantly as you pan/zoom the main viewport
- **Cursor tracking** — window follows the crosshair, positioned above-right of the cursor
- **OSNAP markers** — snap glyphs (endpoint, midpoint, center, etc.) rendered in the magnifier matching AutoCAD's Options > Drafting color setting
- **Grid overlay** — grid lines with adaptive spacing, major/minor distinction, matching the main viewport grid
- **Linetypes** — dashed and dotted linetypes render correctly in the magnifier
- **Incremental updates** — entity add/modify/erase reflected immediately without freezing
- **Hide/show** — automatically hides when the cursor leaves the drawing editor
- **Vista-style border** — rounded corners, GDI+ gradient border

---

## Requirements

| Item | Version |
|---|---|
| AutoCAD | 2022 (64-bit) |
| ObjectARX SDK | 2022 (`C:\Autodesk\ObjectARX 2022`) |
| Visual Studio | 2022 Community (toolset v145) |
| Platform | Windows 10/11 x64 |

---

## Building

1. Open `BlockView.vcxproj` in Visual Studio 2022.
2. Select configuration **`Debug2022 | x64`**.
3. Verify `sdk.props` points to your ObjectARX 2022 SDK root (`$(ARXSDK)`).
4. Build — output: `Debug2022\AsdkBlockView.arx`.

Or from PowerShell:
```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" `
    BlockView.vcxproj /p:Configuration=Debug2022 /p:Platform=x64
```

---

## Loading in AutoCAD

```
APPLOAD → browse to Debug2022\AsdkBlockView.arx → Load
```

Or drag-and-drop the `.arx` onto the AutoCAD window.

---

## Commands

| Command | Alias | Description |
|---|---|---|
| `BLOCKVIEW` | `BVIEW` | Toggle magnifier on/off (works during any active command) |
| `CONFIGGS` | — | Open AcGs graphics configuration dialog |
| `ROS` | — | Off-screen render to image file |

---

## How It Works

See [design.md](design.md) for full implementation details. Key points:

- The magnifier is a modeless `CAcUiDialog` with `WS_POPUP | WS_THICKFRAME`, NC area collapsed to zero.
- An `AcGsView` renders the drawing into the dialog at 3× the main viewport's zoom level.
- View parameters (center, direction, twist, size) are read every 16 ms via `acedGetVar` — safe from a timer callback without acquiring a document lock.
- **3× formula:** `dlgH = vpHeight × (dlgPixH / vpPixH) / 3` — correct for any combination of dialog and editor pixel sizes.
- Grid lines are rendered as an `AcGiDrawable` in the AcGs scene (not a GDI overlay), placed in a dedicated model drawn before the entity model so grid lines appear behind entities.
- OSNAP markers are drawn by the same `AcGiDrawable`. The snap point is captured via `AcEdInputPointMonitor`; the marker color is read each frame from `acedGetCurrentColors()` to match Options > Drafting.

---

## Repository

`https://github.com/TechVinmeen/Magnifier3`
