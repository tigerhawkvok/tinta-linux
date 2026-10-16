#include "image_loader.h"

#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <new>
#include <stdexcept>
#include <utility>

using Microsoft::WRL::ComPtr;

ImageDecodeSize boundedImageSize(UINT width, UINT height, UINT maxDimension) {
    if (!width || !height || !maxDimension) return {};
    const double limit = std::min(maxDimension, IMAGE_DECODE_MAX_DIMENSION);
    const double scale = std::min({1.0, limit / width, limit / height,
        std::sqrt(static_cast<double>(IMAGE_DECODE_MAX_PIXELS) /
                  (static_cast<double>(width) * height))});
    return {std::max(1u, static_cast<UINT>(std::floor(width * scale))),
            std::max(1u, static_cast<UINT>(std::floor(height * scale)))};
}

namespace {

// A decoder's first frame, bounded, as premultiplied BGRA
HRESULT decodeFirstFrame(IWICImagingFactory* factory, IWICBitmapDecoder* decoder,
                         UINT maxDimension, DecodedImage& output) {
    ComPtr<IWICBitmapFrameDecode> frame;
    HRESULT hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr)) return hr;
    UINT sourceW = 0, sourceH = 0;
    hr = frame->GetSize(&sourceW, &sourceH);
    if (FAILED(hr)) return hr;
    if (!sourceW || !sourceH || sourceW > IMAGE_SOURCE_MAX_DIMENSION ||
        sourceH > IMAGE_SOURCE_MAX_DIMENSION ||
        static_cast<uint64_t>(sourceW) * sourceH > IMAGE_SOURCE_MAX_PIXELS)
        return WINCODEC_ERR_IMAGESIZEOUTOFRANGE;

    const auto size = boundedImageSize(sourceW, sourceH, maxDimension);
    ComPtr<IWICBitmapSource> source = frame;
    ComPtr<IWICBitmapScaler> scaler;
    if (size.width != sourceW || size.height != sourceH) {
        hr = factory->CreateBitmapScaler(scaler.GetAddressOf());
        if (FAILED(hr)) return hr;
        // Scale before format conversion/CopyPixels; never request an
        // unbounded full-resolution BGRA buffer from WIC.
        hr = scaler->Initialize(frame.Get(), size.width, size.height,
                                WICBitmapInterpolationModeFant);
        if (FAILED(hr)) return hr;
        source = scaler;
    }
    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(converter.GetAddressOf());
    if (FAILED(hr)) return hr;
    hr = converter->Initialize(source.Get(), GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) return hr;

    DecodedImage decoded;
    decoded.width = size.width;
    decoded.height = size.height;
    double dpiX = 96.0, dpiY = 96.0;
    if (FAILED(frame->GetResolution(&dpiX, &dpiY))) dpiX = dpiY = 96.0;
    if (!std::isfinite(dpiX) || dpiX < 1.0 || dpiX > 9600.0) dpiX = 96.0;
    if (!std::isfinite(dpiY) || dpiY < 1.0 || dpiY > 9600.0) dpiY = 96.0;
    // Preserve the intrinsic size in DIPs even when the pixel copy shrinks.
    decoded.dpiX = static_cast<float>(dpiX * size.width / sourceW);
    decoded.dpiY = static_cast<float>(dpiY * size.height / sourceH);
    const UINT stride = size.width * 4;
    const UINT bytes = stride * size.height;  // bounded to 64 MiB above
    decoded.pixels.resize(bytes);
    hr = converter->CopyPixels(nullptr, stride, bytes, decoded.pixels.data());
    if (FAILED(hr)) return hr;
    output = std::move(decoded);
    return S_OK;
}

}  // namespace

HRESULT decodeImageFile(IWICImagingFactory* factory, const wchar_t* path,
                       UINT maxDimension, DecodedImage& output) noexcept {
    output = {};
    if (!factory || !path || !maxDimension) return E_INVALIDARG;
    try {
        ComPtr<IWICBitmapDecoder> decoder;
        HRESULT hr = factory->CreateDecoderFromFilename(path, nullptr, GENERIC_READ,
            WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf());
        if (FAILED(hr)) return hr;
        return decodeFirstFrame(factory, decoder.Get(), maxDimension, output);
    } catch (const std::bad_alloc&) {
        return E_OUTOFMEMORY;
    } catch (const std::length_error&) {
        return E_OUTOFMEMORY;
    }
}

HRESULT decodeImageMemory(IWICImagingFactory* factory, const std::string& bytes,
                         UINT maxDimension, DecodedImage& output) noexcept {
    output = {};
    if (!factory || bytes.empty() || bytes.size() > 0xffffffffu || !maxDimension) return E_INVALIDARG;
    try {
        ComPtr<IWICStream> stream;
        HRESULT hr = factory->CreateStream(stream.GetAddressOf());
        if (SUCCEEDED(hr)) {
            hr = stream->InitializeFromMemory(reinterpret_cast<BYTE*>(const_cast<char*>(bytes.data())),
                                              static_cast<DWORD>(bytes.size()));
        }
        ComPtr<IWICBitmapDecoder> decoder;
        if (SUCCEEDED(hr)) {
            hr = factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand,
                                                  decoder.GetAddressOf());
        }
        if (FAILED(hr)) return hr;
        return decodeFirstFrame(factory, decoder.Get(), maxDimension, output);
    } catch (const std::bad_alloc&) {
        return E_OUTOFMEMORY;
    } catch (const std::length_error&) {
        return E_OUTOFMEMORY;
    }
}

bool imageDataUriBytes(const std::string& src, std::string& bytes) {
    bytes.clear();
    if (src.compare(0, 11, "data:image/") != 0) return false;
    size_t comma = src.find(',');
    if (comma == std::string::npos) return false;
    // Only base64: a percent-encoded raster is not something anyone pastes
    if (src.rfind(";base64", comma) == std::string::npos) return false;
    auto value = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+' || c == '-') return 62;
        if (c == '/' || c == '_') return 63;
        return -1;
    };
    bytes.reserve((src.size() - comma) / 4 * 3);
    unsigned buffer = 0;
    int bits = 0;
    for (size_t i = comma + 1; i < src.size(); i++) {
        char c = src[i];
        if (c == '=') break;
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
        int v = value(c);
        if (v < 0) {
            bytes.clear();
            return false;
        }
        buffer = (buffer << 6) | (unsigned)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            bytes += (char)((buffer >> bits) & 0xff);
        }
    }
    return !bytes.empty();
}
