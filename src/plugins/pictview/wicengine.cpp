// SPDX-FileCopyrightText: 2026 Pavel Stupka
// SPDX-License-Identifier: GPL-2.0-or-later

//*****************************************************************************
//
// wicengine.cpp
//
// In-process image engine backed by the Windows Imaging Component (WIC).
// See wicengine.h and specs/006-fix-pictview-plugin/contracts/ for the
// contract. Struct layouts come from lib/PVW32DLL.h (4-byte packing).
//

#include "precomp.h"

// precomp.h defines INT32/UINT32 as macros for legacy compilers; intsafe.h
// (pulled in by wincodec.h) typedefs the same names - drop the macros here
#ifdef INT32
#undef INT32
#undef UINT32
#endif

#include <initguid.h>
#include <wincodec.h>

#include "lib/pvw32dll.h"
#include "pictview.h"
#include "wicengine.h"

#pragma comment(lib, "ole32.lib")

//*****************************************************************************
//
// COM / WIC factory plumbing (research D9)
//

static IWICImagingFactory* WicFactory = NULL;
static CRITICAL_SECTION WicFactoryCS;
static BOOL WicFactoryCSInited = FALSE;

// COM must be initialized on every thread that decodes (viewer window
// threads, thumbnail loader). We never CoUninitialize - the threads run
// message loops for their whole lifetime and the OS cleans up on exit.
static void EnsureComOnThisThread()
{
    static thread_local bool inited = false;
    if (!inited)
    {
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        // S_OK/S_FALSE = ok; RPC_E_CHANGED_MODE = already MTA, also usable
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
            TRACE_E("WIC engine: CoInitializeEx failed, hr=0x" << std::hex << hr);
        inited = true;
    }
}

static IWICImagingFactory* GetWicFactory()
{
    EnsureComOnThisThread();
    if (WicFactory != NULL)
        return WicFactory; // pointer write is atomic; factory is thread-safe
    if (!WicFactoryCSInited)
        return NULL; // InitWicEngine not called yet
    EnterCriticalSection(&WicFactoryCS);
    if (WicFactory == NULL)
    {
        IWICImagingFactory* f = NULL;
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                      __uuidof(IWICImagingFactory), (void**)&f);
        if (SUCCEEDED(hr))
            WicFactory = f;
        else
            TRACE_E("WIC engine: cannot create IWICImagingFactory, hr=0x" << std::hex << hr);
    }
    LeaveCriticalSection(&WicFactoryCS);
    return WicFactory;
}

//*****************************************************************************
//
// Per-image context (data-model.md #3)
//

struct CWicImage
{
    IWICBitmapDecoder* Decoder; // NULL for attached-HBITMAP images
    IStream* Stream;            // optional backing stream (callback input)

    UINT FrameCount;
    UINT InfoFrame;   // frame whose dimensions are currently reported
    int DecodedFrame; // frame currently held in the DIB; -1 = none

    HBITMAP HDib; // 32bpp top-down DIB section, opaque (composited over BkColor)
    BYTE* DibBits;
    int Width, Height; // dimensions of the decoded/reported image (after rotation)
    BYTE** Lines;      // per-row pointers for PVGetHandles2 (pipette/histogram)
    PVImageHandles Handles;

    DWORD Format; // PVF_*
    DWORD FileSize;
    DWORD DpiX, DpiY;

    int StretchW, StretchH; // signed target size; negative = mirror; 0 = natural
    DWORD StretchMode;      // StretchBlt mode (COLORONCOLOR, ...)
    COLORREF BkColor;       // background for alpha compositing
};

static void FreeDib(CWicImage* img)
{
    if (img->HDib != NULL)
    {
        DeleteObject(img->HDib);
        img->HDib = NULL;
    }
    img->DibBits = NULL;
    if (img->Lines != NULL)
    {
        free(img->Lines);
        img->Lines = NULL;
    }
    img->DecodedFrame = -1;
}

static void DestroyWicImage(CWicImage* img)
{
    FreeDib(img);
    if (img->Decoder != NULL)
        img->Decoder->Release();
    if (img->Stream != NULL)
        img->Stream->Release();
    free(img);
}

// allocate the 32bpp top-down DIB for w x h; returns FALSE on failure
static BOOL AllocDib(CWicImage* img, int w, int h)
{
    FreeDib(img);
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = NULL;
    img->HDib = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (img->HDib == NULL || bits == NULL)
    {
        img->HDib = NULL;
        return FALSE;
    }
    img->DibBits = (BYTE*)bits;
    img->Width = w;
    img->Height = h;
    return TRUE;
}

static BOOL BuildLines(CWicImage* img)
{
    if (img->Lines != NULL)
        free(img->Lines);
    img->Lines = (BYTE**)malloc(img->Height * sizeof(BYTE*));
    if (img->Lines == NULL)
        return FALSE;
    int y;
    for (y = 0; y < img->Height; y++)
        img->Lines[y] = img->DibBits + (size_t)y * img->Width * 4;
    return TRUE;
}

// composite premultiplied BGRA pixels in the DIB over the background colour,
// producing an opaque image (the viewer draws with plain StretchBlt)
static void CompositeOverBackground(CWicImage* img)
{
    BYTE bgR = GetRValue(img->BkColor);
    BYTE bgG = GetGValue(img->BkColor);
    BYTE bgB = GetBValue(img->BkColor);
    size_t count = (size_t)img->Width * img->Height;
    BYTE* px = img->DibBits;
    size_t i;
    for (i = 0; i < count; i++, px += 4)
    {
        BYTE a = px[3];
        if (a != 255)
        {
            // source is premultiplied: out = src + bg * (255 - a) / 255
            px[0] = (BYTE)(px[0] + ((bgB * (255 - a) + 127) / 255));
            px[1] = (BYTE)(px[1] + ((bgG * (255 - a) + 127) / 255));
            px[2] = (BYTE)(px[2] + ((bgR * (255 - a) + 127) / 255));
            px[3] = 255;
        }
    }
}

//*****************************************************************************
//
// Format mapping and error codes
//

static DWORD MapContainerToPVF(REFGUID g, const char** shortName)
{
    if (IsEqualGUID(g, GUID_ContainerFormatJpeg))
    {
        *shortName = "JPEG";
        return PVF_JPG;
    }
    if (IsEqualGUID(g, GUID_ContainerFormatPng))
    {
        *shortName = "PNG";
        return PVF_PNG;
    }
    if (IsEqualGUID(g, GUID_ContainerFormatBmp))
    {
        *shortName = "BMP";
        return PVF_BMP;
    }
    if (IsEqualGUID(g, GUID_ContainerFormatGif))
    {
        *shortName = "GIF";
        return PVF_GIF;
    }
    if (IsEqualGUID(g, GUID_ContainerFormatTiff))
    {
        *shortName = "TIFF";
        return PVF_TIFF;
    }
    if (IsEqualGUID(g, GUID_ContainerFormatIco))
    {
        *shortName = "ICO";
        return PVF_ICO;
    }
    *shortName = "WIC";
    return 0; // no special-case handling in the viewer
}

static PVCODE MapOpenHResult(HRESULT hr)
{
    if (hr == E_OUTOFMEMORY)
        return PVC_OOM;
    if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
        hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND) ||
        hr == HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED) ||
        hr == HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION))
        return PVC_CANNOT_OPEN_FILE;
    if (hr == WINCODEC_ERR_COMPONENTNOTFOUND)
        return PVC_UNSUP_FILE_TYPE; // no codec for this data
    return PVC_UNKNOWN_FILE_STRUCT;
}

//*****************************************************************************
//
// Open paths
//

static PVCODE OpenFromFileU8(CWicImage* img, const char* u8Path)
{
    IWICImagingFactory* factory = GetWicFactory();
    if (factory == NULL)
        return PVC_EXCEPTION;
    // UTF-8 -> UTF-16 with \\?\ prefix so long paths work (feature 004)
    WCHAR* w = SplU8ToWExtAlloc(u8Path);
    if (w == NULL)
        return PVC_CANNOT_OPEN_FILE;

    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (GetFileAttributesExW(w, GetFileExInfoStandard, &fad))
        img->FileSize = fad.nFileSizeLow; // informational only

    IWICBitmapDecoder* dec = NULL;
    HRESULT hr = factory->CreateDecoderFromFilename(w, NULL, GENERIC_READ,
                                                    WICDecodeMetadataCacheOnDemand, &dec);
    free(w);
    if (FAILED(hr))
        return MapOpenHResult(hr);
    img->Decoder = dec;
    return PVC_OK;
}

static PVCODE OpenFromCallbacks(CWicImage* img, TPVReadFunc readFunc, TPVSeekFunc seekFunc,
                                void* appSpecific, DWORD dataSize)
{
    IWICImagingFactory* factory = GetWicFactory();
    if (factory == NULL)
        return PVC_EXCEPTION;
    if (readFunc == NULL)
        return PVC_INCORRECT_PARAMETER;
    if (seekFunc != NULL)
        seekFunc(appSpecific, 0, FILE_BEGIN);

    // read everything into memory (thumbnail/embedded inputs are small)
    DWORD cap = dataSize > 0 ? dataSize : 64 * 1024;
    BYTE* buf = (BYTE*)malloc(cap);
    if (buf == NULL)
        return PVC_OOM;
    DWORD total = 0;
    for (;;)
    {
        if (total == cap)
        {
            DWORD newCap = cap * 2;
            BYTE* nb = (BYTE*)realloc(buf, newCap);
            if (nb == NULL)
            {
                free(buf);
                return PVC_OOM;
            }
            buf = nb;
            cap = newCap;
        }
        DWORD got = readFunc(appSpecific, buf + total, cap - total);
        if (got == 0 || got == (DWORD)-1)
            break;
        total += got;
        if (dataSize > 0 && total >= dataSize)
            break;
    }
    if (total == 0)
    {
        free(buf);
        return PVC_READING_ERROR;
    }

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, total);
    if (hMem == NULL)
    {
        free(buf);
        return PVC_OOM;
    }
    void* p = GlobalLock(hMem);
    memcpy(p, buf, total);
    GlobalUnlock(hMem);
    free(buf);

    IStream* stream = NULL;
    if (FAILED(CreateStreamOnHGlobal(hMem, TRUE /*free on release*/, &stream)))
    {
        GlobalFree(hMem);
        return PVC_OOM;
    }
    IWICBitmapDecoder* dec = NULL;
    HRESULT hr = factory->CreateDecoderFromStream(stream, NULL, WICDecodeMetadataCacheOnDemand, &dec);
    if (FAILED(hr))
    {
        stream->Release();
        return MapOpenHResult(hr);
    }
    img->Stream = stream;
    img->Decoder = dec;
    img->FileSize = total;
    return PVC_OK;
}

static PVCODE AttachBitmap(CWicImage* img, HBITMAP hBmp)
{
    BITMAP bm;
    if (hBmp == NULL || GetObject(hBmp, sizeof(bm), &bm) == 0 || bm.bmWidth <= 0 || bm.bmHeight <= 0)
        return PVC_INCORRECT_PARAMETER;
    if (!AllocDib(img, bm.bmWidth, bm.bmHeight))
        return PVC_OOM;

    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = bm.bmWidth;
    bi.bmiHeader.biHeight = -bm.bmHeight; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    HDC dc = GetDC(NULL);
    int ok = GetDIBits(dc, hBmp, 0, bm.bmHeight, img->DibBits, &bi, DIB_RGB_COLORS);
    ReleaseDC(NULL, dc);
    if (ok == 0)
    {
        FreeDib(img);
        return PVC_GDI_ERROR;
    }
    // DDB has no alpha - force opaque
    size_t count = (size_t)img->Width * img->Height;
    BYTE* px = img->DibBits;
    size_t i;
    for (i = 0; i < count; i++, px += 4)
        px[3] = 255;

    if (!BuildLines(img))
    {
        FreeDib(img);
        return PVC_OOM;
    }
    img->FrameCount = 1;
    img->InfoFrame = 0;
    img->DecodedFrame = 0;
    img->Format = PVF_BMP;
    return PVC_OK;
}

//*****************************************************************************
//
// Frame info + decode
//

// fill frame dimensions/DPI for 'frame' without a full decode
static PVCODE QueryFrameInfo(CWicImage* img, UINT frame, UINT* w, UINT* h)
{
    if (img->Decoder == NULL) // attached bitmap
    {
        *w = img->Width;
        *h = img->Height;
        return PVC_OK;
    }
    if (frame >= img->FrameCount)
        return PVC_NO_MORE_IMAGES;
    IWICBitmapFrameDecode* fr = NULL;
    if (FAILED(img->Decoder->GetFrame(frame, &fr)))
        return PVC_READING_ERROR;
    UINT fw = 0, fh = 0;
    fr->GetSize(&fw, &fh);
    double dx = 0, dy = 0;
    fr->GetResolution(&dx, &dy);
    fr->Release();
    if (fw == 0 || fh == 0)
        return PVC_INVALID_DIMENSIONS;
    img->DpiX = dx > 1 ? (DWORD)(dx + 0.5) : 96;
    img->DpiY = dy > 1 ? (DWORD)(dy + 0.5) : 96;
    *w = fw;
    *h = fh;
    return PVC_OK;
}

// decode 'frame' into the context DIB (PBGRA composited over BkColor)
static PVCODE DecodeFrame(CWicImage* img, int frame, TProgressProc progress, void* appSpecific)
{
    if (img->Decoder == NULL) // attached bitmap is always "decoded"
        return img->DecodedFrame >= 0 ? PVC_OK : PVC_INVALID_HANDLE;
    if (frame < 0)
        frame = 0;
    if ((UINT)frame >= img->FrameCount)
        return PVC_NO_MORE_IMAGES;
    if (img->DecodedFrame == frame)
        return PVC_OK;

    IWICImagingFactory* factory = GetWicFactory();
    if (factory == NULL)
        return PVC_EXCEPTION;

    // PictView's TProgressProc returns TRUE to CANCEL (e.g. the user pressed a
    // page/file-navigation key) and FALSE to continue - the opposite of the
    // header's "return FALSE to cancel" comment. Honor a real cancel before we
    // start decoding; the inverted check here was skipping the very first draw,
    // leaving the window blank until a resize/zoom forced a repaint.
    if (progress != NULL && progress(0, appSpecific))
        return PVC_CANCELED;

    IWICBitmapFrameDecode* fr = NULL;
    if (FAILED(img->Decoder->GetFrame(frame, &fr)))
        return PVC_READING_ERROR;

    IWICFormatConverter* conv = NULL;
    HRESULT hr = factory->CreateFormatConverter(&conv);
    if (SUCCEEDED(hr))
        hr = conv->Initialize(fr, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
                              NULL, 0.0, WICBitmapPaletteTypeCustom);
    UINT w = 0, h = 0;
    if (SUCCEEDED(hr))
        hr = fr->GetSize(&w, &h);
    double dx = 0, dy = 0;
    if (SUCCEEDED(hr))
        fr->GetResolution(&dx, &dy);

    PVCODE code = PVC_OK;
    if (FAILED(hr) || w == 0 || h == 0)
        code = (hr == E_OUTOFMEMORY) ? PVC_OOM : PVC_READING_ERROR;
    else if (!AllocDib(img, (int)w, (int)h))
        code = PVC_OOM;
    else
    {
        hr = conv->CopyPixels(NULL, w * 4, w * 4 * h, img->DibBits);
        if (FAILED(hr))
        {
            FreeDib(img);
            code = (hr == E_OUTOFMEMORY) ? PVC_OOM : PVC_READING_ERROR;
        }
    }
    if (conv != NULL)
        conv->Release();
    fr->Release();
    if (code != PVC_OK)
        return code;

    CompositeOverBackground(img);
    if (!BuildLines(img))
    {
        FreeDib(img);
        return PVC_OOM;
    }
    img->DpiX = dx > 1 ? (DWORD)(dx + 0.5) : 96;
    img->DpiY = dy > 1 ? (DWORD)(dy + 0.5) : 96;
    img->DecodedFrame = frame;
    img->InfoFrame = (UINT)frame;

    // decode is complete; report 100% but never abort a finished frame (a late
    // cancel must not suppress drawing the image we already have in the DIB)
    if (progress != NULL)
        progress(100, appSpecific);
    return PVC_OK;
}

//*****************************************************************************
//
// Thumbnail fast path (feature 064, contract C4)
//

// convert an arbitrary WIC source into the context DIB (PBGRA composited over
// BkColor); shared by the embedded-thumbnail and reduced-decode paths
static PVCODE DecodeSourceToDib(CWicImage* img, IWICBitmapSource* src)
{
    IWICImagingFactory* factory = GetWicFactory();
    if (factory == NULL)
        return PVC_EXCEPTION;
    UINT w = 0, h = 0;
    if (FAILED(src->GetSize(&w, &h)) || w == 0 || h == 0)
        return PVC_READING_ERROR;
    IWICFormatConverter* conv = NULL;
    HRESULT hr = factory->CreateFormatConverter(&conv);
    if (SUCCEEDED(hr))
        hr = conv->Initialize(src, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
                              NULL, 0.0, WICBitmapPaletteTypeCustom);
    PVCODE code = PVC_OK;
    if (FAILED(hr))
        code = (hr == E_OUTOFMEMORY) ? PVC_OOM : PVC_READING_ERROR;
    else if (!AllocDib(img, (int)w, (int)h))
        code = PVC_OOM;
    else
    {
        hr = conv->CopyPixels(NULL, w * 4, w * 4 * h, img->DibBits);
        if (FAILED(hr))
        {
            FreeDib(img);
            code = (hr == E_OUTOFMEMORY) ? PVC_OOM : PVC_READING_ERROR;
        }
    }
    if (conv != NULL)
        conv->Release();
    if (code == PVC_OK)
        CompositeOverBackground(img);
    return code;
}

// embedded decoder thumbnail (typically the EXIF preview, ~160x120): orders of
// magnitude cheaper than any decode; may be smaller than the requested box
static PVCODE TryEmbeddedThumbnail(CWicImage* img, IWICBitmapFrameDecode* fr,
                                   int maxW, int maxH, int* onlyPreview)
{
    IWICBitmapSource* th = NULL;
    if (FAILED(fr->GetThumbnail(&th)) || th == NULL)
        return PVC_UNSUP_FILE_TYPE;
    UINT w = 0, h = 0;
    th->GetSize(&w, &h);
    PVCODE code = (w == 0 || h == 0) ? PVC_READING_ERROR : DecodeSourceToDib(img, th);
    th->Release();
    if (code == PVC_OK)
        *onlyPreview = (w < (UINT)maxW && h < (UINT)maxH); // same rule the driver uses
    return code;
}

// reduced-resolution decode through IWICBitmapSourceTransform (JPEG decodes in
// the DCT domain at 1/2..1/8): full final quality at a fraction of the cost
static PVCODE TryReducedDecode(CWicImage* img, IWICBitmapFrameDecode* fr,
                               int maxW, int maxH, UINT fullW, UINT fullH)
{
    IWICImagingFactory* factory = GetWicFactory();
    if (factory == NULL)
        return PVC_EXCEPTION;

    IWICBitmapSourceTransform* st = NULL;
    if (FAILED(fr->QueryInterface(IID_PPV_ARGS(&st))) || st == NULL)
        return PVC_UNSUP_FILE_TYPE;

    PVCODE code = PVC_UNSUP_FILE_TYPE;
    // largest power-of-two reduction whose result still covers the box
    UINT n = 1;
    while (n < 8 && fullW / (n * 2) >= (UINT)maxW && fullH / (n * 2) >= (UINT)maxH)
        n *= 2;
    UINT w = fullW / n;
    UINT h = fullH / n;
    if (n > 1 && SUCCEEDED(st->GetClosestSize(&w, &h)) &&
        w != 0 && h != 0 && (w < fullW || h < fullH))
    {
        WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppPBGRA;
        UINT bpp = 0;
        if (SUCCEEDED(st->GetClosestPixelFormat(&fmt)))
        {
            IWICComponentInfo* ci = NULL;
            if (SUCCEEDED(factory->CreateComponentInfo(fmt, &ci)) && ci != NULL)
            {
                IWICPixelFormatInfo* pfi = NULL;
                if (SUCCEEDED(ci->QueryInterface(IID_PPV_ARGS(&pfi))) && pfi != NULL)
                {
                    pfi->GetBitsPerPixel(&bpp);
                    pfi->Release();
                }
                ci->Release();
            }
        }
        if (bpp >= 8)
        {
            UINT stride = (w * bpp + 7) / 8;
            stride = (stride + 3) & ~3u; // DWORD-aligned rows for CreateBitmapFromMemory
            UINT size = stride * h;
            BYTE* buf = (BYTE*)malloc(size);
            if (buf != NULL)
            {
                HRESULT hr = st->CopyPixels(NULL, w, h, &fmt, WICBitmapTransformRotate0,
                                            stride, size, buf);
                if (SUCCEEDED(hr))
                {
                    IWICBitmap* bmp = NULL;
                    hr = factory->CreateBitmapFromMemory(w, h, fmt, stride, size, buf, &bmp);
                    if (SUCCEEDED(hr) && bmp != NULL)
                    {
                        code = DecodeSourceToDib(img, bmp);
                        bmp->Release();
                    }
                }
                free(buf);
            }
            else
                code = PVC_OOM;
        }
    }
    st->Release();
    return code;
}

DWORD WicPrepareThumbnailSource(void* hPVImage, int maxW, int maxH, int fastMode,
                                DWORD* effWidth, DWORD* effHeight, int* onlyPreview)
{
    CWicImage* img = (CWicImage*)hPVImage;
    *onlyPreview = FALSE;
    if (img == NULL || maxW <= 0 || maxH <= 0)
        return PVC_INVALID_HANDLE;
    if (img->Decoder == NULL || img->DecodedFrame >= 0)
    {
        // attached bitmap or already decoded: report what is there
        if (img->DibBits == NULL)
            return PVC_INVALID_HANDLE;
        *effWidth = img->Width;
        *effHeight = img->Height;
        return PVC_OK;
    }

    IWICBitmapFrameDecode* fr = NULL;
    if (FAILED(img->Decoder->GetFrame(0, &fr)) || fr == NULL)
        return PVC_READING_ERROR;
    UINT fullW = 0, fullH = 0;
    fr->GetSize(&fullW, &fullH);

    PVCODE code = PVC_UNSUP_FILE_TYPE;
    if (fastMode)
        code = TryEmbeddedThumbnail(img, fr, maxW, maxH, onlyPreview);
    if (code != PVC_OK)
    {
        *onlyPreview = FALSE;
        code = TryReducedDecode(img, fr, maxW, maxH, fullW, fullH);
    }
    fr->Release();
    if (code != PVC_OK && code != PVC_OOM)
        return code; // caller silently keeps the classic full-decode path
    if (code != PVC_OK)
        return code;

    if (!BuildLines(img)) // keep the handle fully usable (pipette path expects Lines)
    {
        FreeDib(img);
        return PVC_OOM;
    }
    img->DecodedFrame = 0; // PVSaveImage's DecodeFrame becomes a no-op
    img->InfoFrame = 0;
    *effWidth = (DWORD)img->Width;
    *effHeight = (DWORD)img->Height;
    return PVC_OK;
}

int WicGetExifOrientation(void* hPVImage)
{
    CWicImage* img = (CWicImage*)hPVImage;
    if (img == NULL || img->Decoder == NULL)
        return 0;
    IWICBitmapFrameDecode* fr = NULL;
    if (FAILED(img->Decoder->GetFrame(0, &fr)) || fr == NULL)
        return 0;
    int orient = 0;
    IWICMetadataQueryReader* q = NULL;
    if (SUCCEEDED(fr->GetMetadataQueryReader(&q)) && q != NULL)
    {
        PROPVARIANT v;
        PropVariantInit(&v);
        // JPEG stores the IFD under APP1; TIFF-family containers at the root
        if (SUCCEEDED(q->GetMetadataByName(L"/app1/ifd/{ushort=274}", &v)) ||
            SUCCEEDED(q->GetMetadataByName(L"/ifd/{ushort=274}", &v)))
        {
            if (v.vt == VT_UI2)
                orient = v.uiVal;
            else if (v.vt == VT_UI4)
                orient = (int)v.ulVal;
        }
        PropVariantClear(&v);
        q->Release();
    }
    fr->Release();
    return (orient >= 1 && orient <= 8) ? orient : 0;
}

// blit the decoded (already composited) DIB to 'dc' with the image origin at
// (X, Y), honoring the stretch/mirror state, clipped to 'clip' when given
static PVCODE BlitTo(CWicImage* img, HDC dc, int X, int Y, const RECT* clip)
{
    if (img->HDib == NULL)
        return PVC_INVALID_HANDLE;
    int dw = img->StretchW != 0 ? abs(img->StretchW) : img->Width;
    int dh = img->StretchH != 0 ? abs(img->StretchH) : img->Height;
    BOOL mirH = img->StretchW < 0;
    BOOL mirV = img->StretchH < 0;

    int saved = SaveDC(dc);
    if (clip != NULL)
        IntersectClipRect(dc, clip->left, clip->top, clip->right, clip->bottom);
    SetStretchBltMode(dc, img->StretchMode != 0 ? img->StretchMode : COLORONCOLOR);

    HDC mem = CreateCompatibleDC(dc);
    PVCODE code = PVC_GDI_ERROR;
    if (mem != NULL)
    {
        HGDIOBJ old = SelectObject(mem, img->HDib);
        if (StretchBlt(dc, X, Y, dw, dh, mem,
                       mirH ? img->Width - 1 : 0, mirV ? img->Height - 1 : 0,
                       mirH ? -img->Width : img->Width, mirV ? -img->Height : img->Height,
                       SRCCOPY))
            code = PVC_OK;
        SelectObject(mem, old);
        DeleteDC(mem);
    }
    RestoreDC(dc, saved);
    return code;
}

//*****************************************************************************
//
// PVImageInfo filling
//

static const char* FormatShortName(DWORD pvf)
{
    switch (pvf)
    {
    case PVF_JPG:
        return "JPEG";
    case PVF_PNG:
        return "PNG";
    case PVF_BMP:
        return "BMP";
    case PVF_GIF:
        return "GIF";
    case PVF_TIFF:
        return "TIFF";
    case PVF_ICO:
        return "ICO";
    }
    return "WIC";
}

static void FillInfo(CWicImage* img, UINT frame, UINT w, UINT h, LPPVImageInfo out, int cbSize)
{
    PVImageInfo pv;
    memset(&pv, 0, sizeof(pv));
    pv.cbSize = sizeof(pv);
    pv.Width = w;
    pv.Height = h;
    pv.BytesPerLine = w * 4;
    pv.FileSize = img->FileSize;
    pv.Colors = PV_COLOR_TC32;
    pv.Format = img->Format;
    pv.Flags = 0; // no PVFF_IMAGESEQUENCE in v1 (animation shows frame 0)
    pv.ColorModel = PVCM_RGB;
    pv.NumOfImages = img->FrameCount != 0 ? img->FrameCount : 1;
    pv.CurrentImage = frame;
    lstrcpynA(pv.Info1, FormatShortName(img->Format), PV_MAX_INFO_LEN);
    pv.StretchedWidth = img->StretchW != 0 ? abs(img->StretchW) : w;
    pv.StretchedHeight = img->StretchH != 0 ? abs(img->StretchH) : h;
    pv.StretchMode = img->StretchMode;
    pv.HorDPI = img->DpiX != 0 ? img->DpiX : 96;
    pv.VerDPI = img->DpiY != 0 ? img->DpiY : 96;
    pv.FSI = NULL;
    pv.Compression = PVCS_DEFAULT;
    pv.CommentSize = 0;
    pv.Comment = NULL;
    pv.TotalBitDepth = 24;

    if (out != NULL && cbSize > 0)
        memcpy(out, &pv, min((size_t)cbSize, sizeof(pv)));
}

//*****************************************************************************
//
// Engine entry points (table implementations)
//

static PVCODE WINAPI WicOpenImageEx(LPPVHandle* Img, LPPVOpenImageExInfo oi, LPPVImageInfo pInfo, int cbSize)
{
    if (Img == NULL || oi == NULL)
        return PVC_INCORRECT_PARAMETER;
    *Img = NULL;

    CWicImage* img = (CWicImage*)calloc(1, sizeof(CWicImage));
    if (img == NULL)
        return PVC_OOM;
    img->DecodedFrame = -1;
    img->FrameCount = 1;
    img->BkColor = RGB(255, 255, 255);
    img->StretchMode = COLORONCOLOR;
    img->DpiX = img->DpiY = 96;

    PVCODE code;
    if (oi->Flags & PVOF_ATTACH_TO_HANDLE)
        code = AttachBitmap(img, (HBITMAP)oi->Handle);
    else if (oi->Flags & PVOF_USERDEFINED_INPUT)
        code = OpenFromCallbacks(img, oi->ReadFunc, oi->SeekFunc, (void*)oi->Handle, oi->DataSize);
    else if (oi->FileName != NULL)
        code = OpenFromFileU8(img, oi->FileName); // FileName carries UTF-8 (feature 006)
    else
        code = PVC_INCORRECT_PARAMETER;

    UINT w = 0, h = 0;
    if (code == PVC_OK && img->Decoder != NULL)
    {
        UINT frames = 0;
        if (FAILED(img->Decoder->GetFrameCount(&frames)) || frames == 0)
            code = PVC_UNKNOWN_FILE_STRUCT;
        else
        {
            img->FrameCount = frames;
            GUID container;
            const char* shortName;
            if (SUCCEEDED(img->Decoder->GetContainerFormat(&container)))
                img->Format = MapContainerToPVF(container, &shortName);
            code = QueryFrameInfo(img, 0, &w, &h);
        }
    }
    else if (code == PVC_OK) // attached bitmap
    {
        w = img->Width;
        h = img->Height;
    }

    if (code != PVC_OK)
    {
        DestroyWicImage(img);
        return code;
    }
    img->InfoFrame = 0;
    FillInfo(img, 0, w, h, pInfo, cbSize);
    *Img = (LPPVHandle)img;
    return PVC_OK;
}

static PVCODE WINAPI WicGetImageInfo(LPPVHandle Img, LPPVImageInfo pInfo, int cbSize, int ImageIndex)
{
    CWicImage* img = (CWicImage*)Img;
    if (img == NULL || pInfo == NULL)
        return PVC_INVALID_HANDLE;
    if (ImageIndex < 0)
        ImageIndex = 0;
    UINT w, h;
    if (img->DecodedFrame == ImageIndex)
    {
        // report the decoded (possibly rotated) dimensions
        w = img->Width;
        h = img->Height;
    }
    else
    {
        PVCODE code = QueryFrameInfo(img, (UINT)ImageIndex, &w, &h);
        if (code != PVC_OK)
            return code;
    }
    img->InfoFrame = (UINT)ImageIndex;
    FillInfo(img, (UINT)ImageIndex, w, h, pInfo, cbSize);
    return PVC_OK;
}

static PVCODE WINAPI WicReadImage2(LPPVHandle Img, HDC PaintDC, RECT* pDRect,
                                   TProgressProc Progress, void* AppSpecific, int ImageIndex)
{
    CWicImage* img = (CWicImage*)Img;
    if (img == NULL)
        return PVC_INVALID_HANDLE;
    PVCODE code = DecodeFrame(img, ImageIndex, Progress, AppSpecific);
    if (code != PVC_OK)
        return code;
    if (PaintDC != NULL)
    {
        // the viewer passes the image's on-screen rectangle; its top-left is
        // the image origin (render1.cpp OnPaint first-load path)
        int x = pDRect != NULL ? pDRect->left : 0;
        int y = pDRect != NULL ? pDRect->top : 0;
        code = BlitTo(img, PaintDC, x, y, pDRect);
    }
    return code;
}

static PVCODE WINAPI WicDrawImage(LPPVHandle Img, HDC PaintDC, int X, int Y, LPRECT rect)
{
    CWicImage* img = (CWicImage*)Img;
    if (img == NULL || PaintDC == NULL)
        return PVC_INVALID_HANDLE;
    if (img->HDib == NULL)
    {
        // background colour changed after decode (or draw before read):
        // re-decode the current info frame without progress UI
        PVCODE code = DecodeFrame(img, (int)img->InfoFrame, NULL, NULL);
        if (code != PVC_OK)
            return code;
    }
    return BlitTo(img, PaintDC, X, Y, rect);
}

static PVCODE WINAPI WicSetStretchParameters(LPPVHandle Img, DWORD Width, DWORD Height, DWORD Mode)
{
    CWicImage* img = (CWicImage*)Img;
    if (img == NULL)
        return PVC_INVALID_HANDLE;
    img->StretchW = (int)Width;  // negative encodes horizontal mirror
    img->StretchH = (int)Height; // negative encodes vertical mirror
    img->StretchMode = Mode;
    return PVC_OK;
}

static PVCODE WINAPI WicSetBkHandle(LPPVHandle Img, COLORREF BkColor)
{
    CWicImage* img = (CWicImage*)Img;
    if (img == NULL)
        return PVC_INVALID_HANDLE;
    if (img->BkColor != BkColor)
    {
        img->BkColor = BkColor;
        // alpha was flattened against the old colour - re-decode lazily
        if (img->Decoder != NULL && img->DecodedFrame >= 0)
            FreeDib(img);
    }
    return PVC_OK;
}

static PVCODE WINAPI WicCloseImage(LPPVHandle Img)
{
    if (Img != NULL)
        DestroyWicImage((CWicImage*)Img);
    return PVC_OK;
}

static const char* WINAPI WicGetErrorText(DWORD ErrorCode)
{
    switch ((PVCODE)ErrorCode)
    {
    case PVC_OK:
        return "No error.";
    case PVC_OOM:
    case PVC_OUT_OF_MEMORY:
        return "Not enough memory to process the image.";
    case PVC_CANCELED:
        return "Operation was canceled.";
    case PVC_INVALID_HANDLE:
        return "Invalid image handle.";
    case PVC_GDI_ERROR:
        return "A graphics (GDI) operation failed.";
    case PVC_NO_MORE_IMAGES:
        return "No more images in the file.";
    case PVC_CANNOT_OPEN_FILE:
        return "The file cannot be opened.";
    case PVC_UNSUP_FILE_TYPE:
        return "This image is in a format that is not supported by the built-in Windows imaging component.";
    case PVC_UNKNOWN_FILE_STRUCT:
        return "The image file is damaged or has an unknown structure.";
    case PVC_READING_ERROR:
        return "An error occurred while reading or decoding the image.";
    case PVC_INVALID_DIMENSIONS:
        return "The image has invalid dimensions.";
    case PVC_INCORRECT_PARAMETER:
        return "Incorrect parameter.";
    case PVC_UNSUP_OUT_PARAMS:
        return "This operation is not supported by the built-in image engine.";
    }
    return "Unknown image engine error.";
}

static DWORD WINAPI WicGetDLLVersion(void)
{
    return (1 << 16) | 0; // built-in WIC engine 1.00
}

static PVCODE WINAPI WicSetParam(LPPVHandle Img)
{
    // the proprietary engine registered a text callback here; no-op for WIC
    UNREFERENCED_PARAMETER(Img);
    return PVC_OK;
}

//*****************************************************************************
//
// Lossless rotation (kept real so EXIF auto-rotate works)
//

static PVCODE WINAPI WicChangeImage(LPPVHandle Img, DWORD Flags)
{
    CWicImage* img = (CWicImage*)Img;
    if (img == NULL)
        return PVC_INVALID_HANDLE;
    if (Flags != PVCF_ROTATE90CW && Flags != PVCF_ROTATE90CCW)
        return PVC_UNSUP_OUT_PARAMS;
    if (img->HDib == NULL)
    {
        PVCODE code = DecodeFrame(img, (int)img->InfoFrame, NULL, NULL);
        if (code != PVC_OK)
            return code;
    }

    int w = img->Width;
    int h = img->Height;
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = h; // swapped
    bi.bmiHeader.biHeight = -w;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* newBits = NULL;
    HBITMAP newDib = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &newBits, NULL, 0);
    if (newDib == NULL || newBits == NULL)
        return PVC_OOM;

    const DWORD* src = (const DWORD*)img->DibBits;
    DWORD* dst = (DWORD*)newBits;
    int y;
    for (y = 0; y < h; y++)
    {
        const DWORD* srcRow = src + (size_t)y * w;
        if (Flags == PVCF_ROTATE90CW)
        {
            // (x, y) -> (h-1-y, x): column h-1-y of the new image
            DWORD* dstCol = dst + (h - 1 - y);
            int x;
            for (x = 0; x < w; x++)
                dstCol[(size_t)x * h] = srcRow[x];
        }
        else
        {
            // (x, y) -> (y, w-1-x)
            DWORD* dstCol = dst + (size_t)(w - 1) * h + y;
            int x;
            for (x = 0; x < w; x++)
                dstCol[-(LONG_PTR)x * h] = srcRow[x];
        }
    }

    DeleteObject(img->HDib);
    img->HDib = newDib;
    img->DibBits = (BYTE*)newBits;
    img->Width = h;
    img->Height = w;
    DWORD dpi = img->DpiX;
    img->DpiX = img->DpiY;
    img->DpiY = dpi;
    if (!BuildLines(img))
    {
        FreeDib(img);
        return PVC_OOM;
    }
    return PVC_OK;
}

//*****************************************************************************
//
// Pixel access for the pipette / histogram tools
//

static PVCODE WINAPI WicGetHandles2(LPPVHandle Img, LPPVImageHandles* pHandles)
{
    CWicImage* img = (CWicImage*)Img;
    if (img == NULL || pHandles == NULL)
        return PVC_INVALID_HANDLE;
    if (img->HDib == NULL)
    {
        PVCODE code = DecodeFrame(img, (int)img->InfoFrame, NULL, NULL);
        if (code != PVC_OK)
            return code;
    }
    memset(&img->Handles, 0, sizeof(img->Handles));
    img->Handles.pLines = img->Lines;
    img->Handles.Palette = NULL;
    *pHandles = &img->Handles;
    return PVC_OK;
}

//*****************************************************************************
//
// Raw pixel export - backs the panel-thumbnail path (feature 048)
//

// The panel thumbnail loader (thumbs.cpp CreateThumbnail) asks PVSaveImage
// for the decoded image as raw 32bpp rows pushed through pSii->WriteFunc at
// natural size; the core's CSalamanderThumbnailMaker does the shrinking.
// Exactly that subset is implemented here. Encoder formats, scaling and
// cropping stay unsupported, so Save As, print preview and the batch JPEG
// thumbnail operation keep their pre-048 behavior (PVC_UNSUP_OUT_PARAMS).
static PVCODE WINAPI WicSaveImage(LPPVHandle Img, const char* OutFName, LPPVSaveImageInfo pSii,
                                  TProgressProc Progress, void* AppSpecific, int ImageIndex)
{
    UNREFERENCED_PARAMETER(OutFName); // user-defined output only

    CWicImage* img = (CWicImage*)Img;
    if (img == NULL)
        return PVC_INVALID_HANDLE;
    if (pSii == NULL)
        return PVC_INCORRECT_PARAMETER;

    // Salamander-proprietary decode-speed hint set by thumbs.cpp for heavily
    // oversized images (PVSF_SUPERFAST there); accepted and ignored
    const DWORD PVSF_SUPERFAST_HINT = 0x8000000;

    if ((pSii->Flags & PVSF_USERDEFINED_OUTPUT) == 0 || pSii->WriteFunc == NULL ||
        pSii->Format != PVF_RAW || pSii->Colors != PV_COLOR_TC32 ||
        pSii->Width != 0 || pSii->Height != 0 ||         // no scaling in this subset
        pSii->CropWidth != 0 || pSii->CropHeight != 0 || // no cropping either
        (pSii->Flags & ~(PVSF_USERDEFINED_OUTPUT | PVSF_FLIP_VERT | PVSF_SUPERFAST_HINT)) != 0)
        return PVC_UNSUP_OUT_PARAMS;

    // decoded rows are opaque BGRX composited over BkColor - the exact
    // PV_COLOR_TC32 row layout the consumer expects (see print.cpp usage)
    PVCODE code = DecodeFrame(img, ImageIndex, Progress, AppSpecific);
    if (code != PVC_OK)
        return code;
    if (img->DibBits == NULL || img->Width <= 0 || img->Height <= 0)
        return PVC_INVALID_DIMENSIONS;

    DWORD stride = (DWORD)img->Width * 4;
    // whole rows per WriteFunc call; bounded batches keep the consumer's
    // cancellation (short write) responsive on large images
    int rowsPerBatch = (int)(256 * 1024 / stride);
    if (rowsPerBatch < 1)
        rowsPerBatch = 1;
    BOOL flip = (pSii->Flags & PVSF_FLIP_VERT) != 0;
    if (flip)
        rowsPerBatch = 1; // rows leave in reverse order -> one row per call

    int y = 0;
    while (y < img->Height)
    {
        // between batches only: the consumer's progress hook probes its own
        // completion state and would report a false error after the last row
        if (y > 0 && Progress != NULL && Progress(MulDiv(y, 100, img->Height), AppSpecific))
            return PVC_OK; // consumer cancel; it judges completeness itself

        int rows = min(rowsPerBatch, img->Height - y);
        BYTE* p = img->DibBits + (size_t)(flip ? img->Height - y - 1 : y) * stride;
        DWORD bytes = (DWORD)rows * stride;
        if (pSii->WriteFunc(AppSpecific, p, bytes) != bytes)
        {
            // short write is the normal end: the thumbnail maker returns FALSE
            // (-> 0 bytes) both on cancellation and after consuming the final
            // rows, and ThumbnailReady() arbitrates which one it was
            return PVC_OK;
        }
        y += rows;
    }
    return PVC_OK;
}

//*****************************************************************************
//
// Graceful stubs - operations with no engine backing (feature 006 scope)
//

static PVCODE WINAPI WicLoadFromClipboard(LPPVHandle* Img, LPPVImageInfo pInfo, int cbSize)
{
    UNREFERENCED_PARAMETER(pInfo);
    UNREFERENCED_PARAMETER(cbSize);
    if (Img != NULL)
        *Img = NULL;
    return PVC_UNSUP_FILE_TYPE;
}

static DWORD WINAPI WicIsOutCombSupported(int Fmt, int Compr, int Colors, int ColorModel)
{
    UNREFERENCED_PARAMETER(Fmt);
    UNREFERENCED_PARAMETER(Compr);
    UNREFERENCED_PARAMETER(Colors);
    UNREFERENCED_PARAMETER(ColorModel);
    return 0; // no output combination supported (Save As is disabled)
}

static PVCODE WINAPI WicReadImageSequence(LPPVHandle Img, LPPVImageSequence* ppSeq)
{
    // never called in v1: the engine does not set PVFF_IMAGESEQUENCE
    if (ppSeq != NULL)
        *ppSeq = NULL;
    UNREFERENCED_PARAMETER(Img);
    return PVC_UNSUP_FILE_TYPE;
}

static PVCODE WINAPI WicCropImage(LPPVHandle Img, int Left, int Top, int Width, int Height)
{
    UNREFERENCED_PARAMETER(Img);
    UNREFERENCED_PARAMETER(Left);
    UNREFERENCED_PARAMETER(Top);
    UNREFERENCED_PARAMETER(Width);
    UNREFERENCED_PARAMETER(Height);
    return PVC_UNSUP_OUT_PARAMS;
}

//*****************************************************************************
//
// InitWicEngine
//

void InitWicEngine(CPVW32DLL* table)
{
    if (!WicFactoryCSInited)
    {
        InitializeCriticalSection(&WicFactoryCS);
        WicFactoryCSInited = TRUE;
    }
    table->PVReadImage2 = WicReadImage2;
    table->PVCloseImage = WicCloseImage;
    table->PVDrawImage = WicDrawImage;
    table->PVGetErrorText = WicGetErrorText;
    table->PVOpenImageEx = WicOpenImageEx;
    table->PVSetBkHandle = WicSetBkHandle;
    table->PVGetDLLVersion = WicGetDLLVersion;
    table->PVSetStretchParameters = WicSetStretchParameters;
    table->PVLoadFromClipboard = WicLoadFromClipboard;
    table->PVGetImageInfo = WicGetImageInfo;
    table->PVSetParam = WicSetParam;
    table->PVGetHandles2 = WicGetHandles2;
    table->PVSaveImage = WicSaveImage;
    table->PVChangeImage = WicChangeImage;
    table->PVIsOutCombSupported = WicIsOutCombSupported;
    table->PVReadImageSequence = WicReadImageSequence;
    table->PVCropImage = WicCropImage;
    table->Handle = NULL; // no external DLL
}
