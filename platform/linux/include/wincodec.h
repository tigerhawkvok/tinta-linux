// WIC declarations for the Linux compat layer (implemented on gdk-pixbuf and
// Cairo in platform/linux/src/wic_pixbuf.cpp). Pixel data is 32bpp BGRA in
// memory, premultiplied for PBGRA, which is Cairo's ARGB32 on little-endian.
#ifndef TINTA_LINUX_WINCODEC_H
#define TINTA_LINUX_WINCODEC_H

#include "tinta_com.h"

typedef GUID WICPixelFormatGUID;
typedef const GUID& REFWICPixelFormatGUID;
typedef UINT32 WICColor;
struct HPALETTE__;
typedef HPALETTE__* HPALETTE;

TINTA_UUID(WICImagingFactoryClass, 0x01c00000)
#define CLSID_WICImagingFactory tinta_uuidof<WICImagingFactoryClass>()

// Pixel formats and containers by identity; only these are supported
TINTA_UUID(WICFormat32bppPBGRA, 0x01c00001)
TINTA_UUID(WICFormat32bppBGRA, 0x01c00002)
TINTA_UUID(WICFormat24bppBGR, 0x01c00003)
TINTA_UUID(WICFormat32bppRGBA, 0x01c00004)
TINTA_UUID(WICContainerPng, 0x01c00010)
TINTA_UUID(WICContainerJpeg, 0x01c00011)
#define GUID_WICPixelFormat32bppPBGRA tinta_uuidof<WICFormat32bppPBGRA>()
#define GUID_WICPixelFormat32bppBGRA tinta_uuidof<WICFormat32bppBGRA>()
#define GUID_WICPixelFormat24bppBGR tinta_uuidof<WICFormat24bppBGR>()
#define GUID_WICPixelFormat32bppRGBA tinta_uuidof<WICFormat32bppRGBA>()
#define GUID_ContainerFormatPng tinta_uuidof<WICContainerPng>()
#define GUID_ContainerFormatJpeg tinta_uuidof<WICContainerJpeg>()

#define WINCODEC_ERR_IMAGESIZEOUTOFRANGE ((HRESULT)0x88982F51)
#define WINCODEC_ERR_COMPONENTNOTFOUND ((HRESULT)0x88982F50)
#define WINCODEC_ERR_UNKNOWNIMAGEFORMAT ((HRESULT)0x88982F07)

enum WICDecodeOptions { WICDecodeMetadataCacheOnDemand = 0, WICDecodeMetadataCacheOnLoad = 1 };
enum WICBitmapCreateCacheOption { WICBitmapNoCache = 0, WICBitmapCacheOnDemand = 1, WICBitmapCacheOnLoad = 2 };
enum WICBitmapDitherType { WICBitmapDitherTypeNone = 0, WICBitmapDitherTypeSolid = 0, WICBitmapDitherTypeOrdered4x4 = 1 };
enum WICBitmapPaletteType { WICBitmapPaletteTypeCustom = 0, WICBitmapPaletteTypeMedianCut = 1 };
enum WICBitmapInterpolationMode {
    WICBitmapInterpolationModeNearestNeighbor = 0,
    WICBitmapInterpolationModeLinear = 1,
    WICBitmapInterpolationModeCubic = 2,
    WICBitmapInterpolationModeFant = 3,
    WICBitmapInterpolationModeHighQualityCubic = 4,
};
enum WICBitmapEncoderCacheOption { WICBitmapEncoderCacheInMemory = 0, WICBitmapEncoderCacheTempFile = 1, WICBitmapEncoderNoCache = 2 };
enum WICBitmapAlphaChannelOption { WICBitmapUseAlpha = 0, WICBitmapUsePremultipliedAlpha = 1, WICBitmapIgnoreAlpha = 2 };
enum WICBitmapLockFlags { WICBitmapLockRead = 1, WICBitmapLockWrite = 2 };

struct WICRect {
    INT X;
    INT Y;
    INT Width;
    INT Height;
};

struct IPropertyBag2;
struct IWICPalette;

TINTA_UUID(IWICBitmapSource, 0x01c00100)
TINTA_UUID(IWICBitmap, 0x01c00101)
TINTA_UUID(IWICBitmapLock, 0x01c00102)
TINTA_UUID(IWICBitmapFrameDecode, 0x01c00103)
TINTA_UUID(IWICBitmapDecoder, 0x01c00104)
TINTA_UUID(IWICBitmapScaler, 0x01c00105)
TINTA_UUID(IWICFormatConverter, 0x01c00106)
TINTA_UUID(IWICBitmapFrameEncode, 0x01c00107)
TINTA_UUID(IWICBitmapEncoder, 0x01c00108)
TINTA_UUID(IWICStream, 0x01c00109)
TINTA_UUID(IWICImagingFactory, 0x01c0010a)

struct IWICBitmapSource : IUnknown {
    virtual HRESULT GetSize(UINT* width, UINT* height) = 0;
    virtual HRESULT GetPixelFormat(WICPixelFormatGUID* pixelFormat) = 0;
    virtual HRESULT GetResolution(double* dpiX, double* dpiY) = 0;
    virtual HRESULT CopyPixels(const WICRect* rect, UINT stride, UINT bufferSize, BYTE* buffer) = 0;
};

struct IWICBitmapLock : IUnknown {
    virtual HRESULT GetSize(UINT* width, UINT* height) = 0;
    virtual HRESULT GetStride(UINT* stride) = 0;
    virtual HRESULT GetDataPointer(UINT* bufferSize, BYTE** data) = 0;
};

struct IWICBitmap : IWICBitmapSource {
    virtual HRESULT Lock(const WICRect* lockRect, DWORD flags, IWICBitmapLock** lock) = 0;
    virtual HRESULT SetResolution(double dpiX, double dpiY) = 0;
};

struct IWICBitmapFrameDecode : IWICBitmapSource {};

struct IWICBitmapDecoder : IUnknown {
    virtual HRESULT GetFrameCount(UINT* count) = 0;
    virtual HRESULT GetFrame(UINT index, IWICBitmapFrameDecode** bitmapFrame) = 0;
};

struct IWICBitmapScaler : IWICBitmapSource {
    virtual HRESULT Initialize(IWICBitmapSource* source, UINT width, UINT height,
                               WICBitmapInterpolationMode mode) = 0;
};

struct IWICFormatConverter : IWICBitmapSource {
    virtual HRESULT Initialize(IWICBitmapSource* source, REFWICPixelFormatGUID dstFormat,
                               WICBitmapDitherType dither, IWICPalette* palette, double alphaThresholdPercent,
                               WICBitmapPaletteType paletteTranslate) = 0;
};

struct IWICBitmapFrameEncode : IUnknown {
    virtual HRESULT Initialize(IPropertyBag2* encoderOptions) = 0;
    virtual HRESULT SetSize(UINT width, UINT height) = 0;
    virtual HRESULT SetResolution(double dpiX, double dpiY) = 0;
    virtual HRESULT SetPixelFormat(WICPixelFormatGUID* pixelFormat) = 0;
    virtual HRESULT WritePixels(UINT lineCount, UINT stride, UINT bufferSize, BYTE* pixels) = 0;
    virtual HRESULT WriteSource(IWICBitmapSource* bitmapSource, WICRect* rect) = 0;
    virtual HRESULT Commit() = 0;
};

struct IWICBitmapEncoder : IUnknown {
    virtual HRESULT Initialize(IStream* stream, WICBitmapEncoderCacheOption cacheOption) = 0;
    virtual HRESULT CreateNewFrame(IWICBitmapFrameEncode** frameEncode, IPropertyBag2** encoderOptions) = 0;
    virtual HRESULT Commit() = 0;
};

struct IWICStream : IStream {
    virtual HRESULT InitializeFromIStream(IStream* stream) = 0;
    virtual HRESULT InitializeFromFilename(LPCWSTR fileName, DWORD desiredAccess) = 0;
    virtual HRESULT InitializeFromMemory(BYTE* buffer, DWORD bufferSize) = 0;
};

struct IWICImagingFactory : IUnknown {
    virtual HRESULT CreateDecoderFromFilename(LPCWSTR filename, const GUID* vendor, DWORD desiredAccess,
                                              WICDecodeOptions metadataOptions, IWICBitmapDecoder** decoder) = 0;
    virtual HRESULT CreateDecoderFromStream(IStream* stream, const GUID* vendor, WICDecodeOptions metadataOptions,
                                            IWICBitmapDecoder** decoder) = 0;
    virtual HRESULT CreateFormatConverter(IWICFormatConverter** formatConverter) = 0;
    virtual HRESULT CreateBitmapScaler(IWICBitmapScaler** bitmapScaler) = 0;
    virtual HRESULT CreateEncoder(REFGUID containerFormat, const GUID* vendor, IWICBitmapEncoder** encoder) = 0;
    virtual HRESULT CreateStream(IWICStream** stream) = 0;
    virtual HRESULT CreateBitmap(UINT width, UINT height, REFWICPixelFormatGUID pixelFormat,
                                 WICBitmapCreateCacheOption option, IWICBitmap** bitmap) = 0;
    virtual HRESULT CreateBitmapFromMemory(UINT width, UINT height, REFWICPixelFormatGUID pixelFormat, UINT stride,
                                           UINT bufferSize, BYTE* buffer, IWICBitmap** bitmap) = 0;
    virtual HRESULT CreateBitmapFromHICON(HICON icon, IWICBitmap** bitmap) = 0;
    virtual HRESULT CreateBitmapFromHBITMAP(HBITMAP bitmapHandle, HPALETTE palette,
                                            WICBitmapAlphaChannelOption options, IWICBitmap** bitmap) = 0;
};

#endif  // TINTA_LINUX_WINCODEC_H
