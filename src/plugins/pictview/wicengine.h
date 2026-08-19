// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//*****************************************************************************
//
// wicengine.h
//
// In-process image engine backed by the Windows Imaging Component (WIC).
// Replaces the proprietary PVW32Cnv.dll, which was removed from the
// repository for GPL reasons (feature 006-fix-pictview-plugin). The engine
// fills the existing CPVW32DLL function-pointer table with functions of
// identical signatures, so all viewer call sites stay unchanged.
//
// VIEW-CRITICAL functions are implemented fully (open / info / decode /
// draw / stretch / background / close / error text). Out-of-scope
// operations (save, crop, clipboard, image sequences) are non-crashing
// stubs returning a clean PVCODE. PVChangeImage (lossless 90deg rotation)
// IS implemented so EXIF auto-rotate keeps working.
//
// FileName passed to PVOpenImageEx is UTF-8 (plugin interface 104); the
// engine converts it to UTF-16 with the \\?\ long-path prefix internally.
//

struct CPVW32DLL;

// fills all PV* members of the table with the WIC engine implementations
// (the four plugin-internal helpers GetRGBAtCursor/CalculateHistogram/
// CreateThumbnail/SimplifyImageSequence are assigned by the caller)
void InitWicEngine(CPVW32DLL* table);

//*****************************************************************************
//
// Thumbnail fast path (feature 064, contract C4)
//
// WicPrepareThumbnailSource decodes the cheapest pixel source that can fill a
// thumbnail bounded by maxW x maxH into the image's DIB, so the following
// PVSaveImage streams a small image instead of the full frame:
//   fast mode:  embedded decoder thumbnail (EXIF preview) first,
//               then reduced-resolution decode (JPEG DCT-domain scaling),
//   slow mode:  reduced-resolution decode only (final quality).
// On PVC_OK (DWORD; PVCODE value) *effWidth/*effHeight carry the decoded
// dimensions the caller must hand to the thumbnail maker, and *onlyPreview is
// nonzero when the pixels came from a preview smaller than the requested box
// (the caller then flags SSTHUMB_ONLY_PREVIEW so the core schedules a quality
// round). Any failure leaves the image undecoded - the caller just keeps the
// classic full-decode path. Set the background (PVSetBkHandle) BEFORE calling:
// alpha sources are composited here.
//
// hPVImage is the LPPVHandle from PVOpenImageEx (typed void* to keep this
// header independent of pvw32dll.h).
DWORD WicPrepareThumbnailSource(void* hPVImage, int maxW, int maxH, int fastMode,
                                DWORD* effWidth, DWORD* effHeight, int* onlyPreview);

// EXIF orientation of frame 0 (1..8 per the TIFF/EXIF spec); 0 = absent or
// unknown. Cheap: metadata only, no pixel decode (feature 064, contract C5).
int WicGetExifOrientation(void* hPVImage);
