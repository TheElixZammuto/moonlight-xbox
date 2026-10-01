//--------------------------------------------------------------------------------------
// File: TextConsole.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "TextConsole.h"
#include "../Utils.hpp"

#include "DirectXHelper.h"
#include "SimpleMath.h"
#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"
#include "VertexTypes.h"

#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <limits>
#include <stdexcept>
#include <utility>

namespace
{
// The DirectXTK project generates these blobs before the app project builds.
#include "../third_party/DirectXTK/Src/Shaders/Compiled/SpriteEffect_SpriteVertexShader.inc"
#include "../third_party/DirectXTK/Src/Shaders/Compiled/SpriteEffect_SpritePixelShader.inc"
}

using Microsoft::WRL::ComPtr;

using namespace DirectX;
using namespace DX;
using namespace moonlight_xbox_dx;

TextConsole::TextConsole() noexcept
    : m_layout{},
    m_textColor(1.f, 1.f, 1.f, 1.f),
    m_debugOutput(false),
    m_columns(0),
    m_rows(0),
    m_fixedWidth(0.0f)
{
    Clear();
}


_Use_decl_annotations_
TextConsole::TextConsole(ID3D11DeviceContext* context, const wchar_t* fontName) noexcept(false)
    : m_layout{},
    m_textColor(1.f, 1.f, 1.f, 1.f),
    m_debugOutput(false),
    m_columns(0),
    m_rows(0),
    m_fixedWidth(0.0)
{
    RestoreDevice(context, fontName);

    Clear();
}


void TextConsole::Render()
{
    if (!m_lines)
        return;

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_cacheGeometry && m_rotation == DXGI_MODE_ROTATION_IDENTITY)
    {
        if (m_cacheDirty)
        {
            RebuildCachedGeometry();
        }

        DrawCachedGeometry();
        return;
    }

    const float lineSpacing = m_font->GetLineSpacing();

    const float x = float(m_layout.left);
    const float y = float(m_layout.top);

    const XMVECTOR color = XMLoadFloat4(&m_textColor);

    m_batch->Begin();

    auto textLine = static_cast<unsigned int>(m_currentLine + 1) % m_rows;

    for (unsigned int line = 0; line < m_rows; ++line)
    {
        const XMFLOAT2 pos(x, y + lineSpacing * float(line));

        if (*m_lines[textLine])
        {
            m_font->DrawString(m_batch.get(), m_lines[textLine], pos, color);
        }

        textLine = static_cast<unsigned int>(textLine + 1) % m_rows;
    }

    m_batch->End();
}


void TextConsole::Clear() noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_buffer)
    {
        memset(m_buffer.get(), 0, sizeof(wchar_t) * (m_columns + 1) * m_rows);
    }

    m_currentColumn = m_currentLine = 0;
    m_cacheDirty = true;
}


_Use_decl_annotations_
void TextConsole::Write(const wchar_t* str)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    ProcessString(str);
    m_cacheDirty = true;

#ifndef NDEBUG
    if (m_debugOutput)
    {
        OutputDebugStringW(str);
    }
#endif
}


_Use_decl_annotations_
void TextConsole::WriteLine(const wchar_t* str)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    ProcessString(str);
    IncrementLine();
    m_cacheDirty = true;

#ifndef NDEBUG
    if (m_debugOutput)
    {
        OutputDebugStringW(str);
        OutputDebugStringW(L"\n");
    }
#endif
}


_Use_decl_annotations_
void TextConsole::Format(const wchar_t* strFormat, ...)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    va_list argList;
    va_start(argList, strFormat);

    const auto len = size_t(_vscwprintf(strFormat, argList) + 1);

    if (m_tempBuffer.size() < len)
        m_tempBuffer.resize(len);

    memset(m_tempBuffer.data(), 0, sizeof(wchar_t) * len);

    vswprintf_s(m_tempBuffer.data(), m_tempBuffer.size(), strFormat, argList);

    va_end(argList);

    ProcessString(m_tempBuffer.data());
    m_cacheDirty = true;

#ifndef NDEBUG
    if (m_debugOutput)
    {
        OutputDebugStringW(m_tempBuffer.data());
    }
#endif
}


void TextConsole::SetWindow(const RECT& layout)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_layout = layout;
    m_cacheDirty = true;

    assert(m_font != nullptr);

    const float lineSpacing = m_font->GetLineSpacing();
    unsigned int rows = std::max<unsigned int>(1, static_cast<unsigned int>(float(layout.bottom - layout.top) / lineSpacing));

    const RECT fontLayout = m_font->MeasureDrawBounds(L"X", XMFLOAT2(0, 0));
    unsigned int columns = std::max<unsigned int>(1, static_cast<unsigned int>(float(layout.right - layout.left) / float(fontLayout.right - fontLayout.left)));

    auto buffer = std::make_unique<wchar_t[]>((columns + 1) * rows);
    memset(buffer.get(), 0, sizeof(wchar_t) * (columns + 1) * rows);

    auto lines = std::make_unique<wchar_t* []>(rows);
    for (unsigned int line = 0; line < rows; ++line)
    {
        lines[line] = buffer.get() + (columns + 1) * line;
    }

    if (m_lines)
    {
        const unsigned int c = std::min<unsigned int>(columns, m_columns);
        const unsigned int r = std::min<unsigned int>(rows, m_rows);

        for (unsigned int line = 0; line < r; ++line)
        {
            memcpy(lines[line], m_lines[line], c * sizeof(wchar_t));
        }
    }

    std::swap(columns, m_columns);
    std::swap(rows, m_rows);
    std::swap(buffer, m_buffer);
    std::swap(lines, m_lines);

    Utils::Logf("TextConsole initialized at (%d,%d) - (%d,%d) with %u rows, %u columns\n",
                layout.left, layout.top, layout.right, layout.bottom, m_rows, m_columns);

    if ((m_currentColumn >= m_columns) || (m_currentLine >= m_rows))
    {
        IncrementLine();
    }
}


void TextConsole::ReleaseDevice() noexcept
{
    m_cachedVertices.Reset();
    m_cachedTransform.Reset();
    m_cachedInputLayout.Reset();
    m_cachedVS.Reset();
    m_cachedPS.Reset();
    m_fontSheet.Reset();
    m_cachedStates.reset();
    m_cachedVertexCount = 0;
    m_cachedVertexCapacity = 0;
    m_inverseSheetWidth = m_inverseSheetHeight = 0;
    m_transformWidth = m_transformHeight = 0;
    m_cacheDirty = true;
    m_batch.reset();
    m_font.reset();
    m_context.Reset();
}

_Use_decl_annotations_
void TextConsole::RestoreDevice(ID3D11DeviceContext* context, const wchar_t* fontName)
{
    ReleaseDevice();
    m_context = context;

    m_batch = std::make_unique<SpriteBatch>(context);

    ComPtr<ID3D11Device> device;
    context->GetDevice(device.GetAddressOf());

    m_font = std::make_unique<SpriteFont>(device.Get(), fontName);

    m_font->SetDefaultCharacter(L' ');
}

_Use_decl_annotations_
void TextConsole::RestoreDevice(ID3D11DeviceContext* context, const uint8_t* fontBytes, size_t fontSize)
{
    ReleaseDevice();
    m_context = context;

    m_batch = std::make_unique<SpriteBatch>(context);

    ComPtr<ID3D11Device> device;
    context->GetDevice(device.GetAddressOf());

    m_font = std::make_unique<SpriteFont>(device.Get(), fontBytes, fontSize);

    m_font->SetDefaultCharacter(L' ');
}

void TextConsole::SetViewport(const D3D11_VIEWPORT& viewPort)
{
    m_viewport = viewPort;
    m_hasViewport = true;
    m_transformWidth = m_transformHeight = 0;
    if (m_batch)
    {
        m_batch->SetViewport(viewPort);
    }
}


void TextConsole::SetRotation(DXGI_MODE_ROTATION rotation)
{
    m_rotation = rotation;
    if (m_batch)
    {
        m_batch->SetRotation(rotation);
    }
}

void XM_CALLCONV TextConsole::SetForegroundColor(FXMVECTOR color)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    XMStoreFloat4(&m_textColor, color);
    m_cacheDirty = true;
}


void TextConsole::SetCachedGeometry(bool enabled)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cacheGeometry = enabled;
    m_cacheDirty = true;
}


void TextConsole::CreateCachedResources()
{
    if (m_cachedStates)
        return;

    ComPtr<ID3D11Device> device;
    m_context->GetDevice(device.GetAddressOf());

    DX::ThrowIfFailed(device->CreateVertexShader(SpriteEffect_SpriteVertexShader,
        sizeof(SpriteEffect_SpriteVertexShader), nullptr, m_cachedVS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(device->CreatePixelShader(SpriteEffect_SpritePixelShader,
        sizeof(SpriteEffect_SpritePixelShader), nullptr, m_cachedPS.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(device->CreateInputLayout(VertexPositionColorTexture::InputElements,
        VertexPositionColorTexture::InputElementCount, SpriteEffect_SpriteVertexShader,
        sizeof(SpriteEffect_SpriteVertexShader), m_cachedInputLayout.ReleaseAndGetAddressOf()));

    D3D11_BUFFER_DESC transformDesc = {};
    transformDesc.ByteWidth = sizeof(XMFLOAT4X4);
    transformDesc.Usage = D3D11_USAGE_DEFAULT;
    transformDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    DX::ThrowIfFailed(device->CreateBuffer(&transformDesc, nullptr, m_cachedTransform.ReleaseAndGetAddressOf()));

    m_font->GetSpriteSheet(m_fontSheet.ReleaseAndGetAddressOf());
    ComPtr<ID3D11Resource> sheetResource;
    m_fontSheet->GetResource(sheetResource.GetAddressOf());
    ComPtr<ID3D11Texture2D> sheet;
    DX::ThrowIfFailed(sheetResource.As(&sheet));
    D3D11_TEXTURE2D_DESC sheetDesc;
    sheet->GetDesc(&sheetDesc);
    m_inverseSheetWidth = 1.f / float(sheetDesc.Width);
    m_inverseSheetHeight = 1.f / float(sheetDesc.Height);
    m_cachedStates = std::make_unique<CommonStates>(device.Get());
}


void TextConsole::RebuildCachedGeometry()
{
    CreateCachedResources();

    std::vector<VertexPositionColorTexture> vertices;
    size_t characterCount = 0;
    for (unsigned int line = 0; line < m_rows; ++line)
        characterCount += std::wcslen(m_lines[line]);
    vertices.reserve(characterCount * 6);
    const float lineSpacing = m_font->GetLineSpacing();
    auto textLine = static_cast<unsigned int>(m_currentLine + 1) % m_rows;

    for (unsigned int line = 0; line < m_rows; ++line)
    {
        float penX = 0.f;
        float penY = 0.f;
        const float lineY = float(m_layout.top) + lineSpacing * float(line);

        for (const wchar_t* ch = m_lines[textLine]; *ch; ++ch)
        {
            if (*ch == L'\r')
                continue;
            if (*ch == L'\n')
            {
                penX = 0.f;
                penY += lineSpacing;
                continue;
            }

            const auto glyph = m_font->FindGlyph(*ch);
            penX = std::max(0.f, penX + glyph->XOffset);
            const float width = float(glyph->Subrect.right - glyph->Subrect.left);
            const float height = float(glyph->Subrect.bottom - glyph->Subrect.top);

            // Match SpriteFont's whitespace handling and glyph advances.
            if (!std::iswspace(*ch) || width > 1.f || height > 1.f)
            {
                const float left = float(m_layout.left) + penX;
                const float top = lineY + penY + glyph->YOffset;
                const float right = left + width;
                const float bottom = top + height;
                const float u0 = float(glyph->Subrect.left) * m_inverseSheetWidth;
                const float v0 = float(glyph->Subrect.top) * m_inverseSheetHeight;
                const float u1 = float(glyph->Subrect.right) * m_inverseSheetWidth;
                const float v1 = float(glyph->Subrect.bottom) * m_inverseSheetHeight;

                const VertexPositionColorTexture topLeft(XMFLOAT3(left, top, 0.f), m_textColor, XMFLOAT2(u0, v0));
                const VertexPositionColorTexture topRight(XMFLOAT3(right, top, 0.f), m_textColor, XMFLOAT2(u1, v0));
                const VertexPositionColorTexture bottomLeft(XMFLOAT3(left, bottom, 0.f), m_textColor, XMFLOAT2(u0, v1));
                const VertexPositionColorTexture bottomRight(XMFLOAT3(right, bottom, 0.f), m_textColor, XMFLOAT2(u1, v1));

                vertices.insert(vertices.end(), {topLeft, topRight, bottomLeft,
                                                 topRight, bottomRight, bottomLeft});
            }

            penX += width + glyph->XAdvance;
        }

        textLine = static_cast<unsigned int>(textLine + 1) % m_rows;
    }

    if (vertices.size() > std::numeric_limits<UINT>::max() / sizeof(VertexPositionColorTexture))
        throw std::length_error("Cached text geometry is too large");

    const UINT vertexCount = static_cast<UINT>(vertices.size());
    if (vertexCount)
    {
        const UINT requiredBytes = static_cast<UINT>(vertices.size() * sizeof(VertexPositionColorTexture));
        if (requiredBytes > m_cachedVertexCapacity)
        {
            // Grow infrequently; Map(WRITE_DISCARD) lets D3D11 rename storage
            // while the previous frame is still using the old contents.
            UINT capacity = std::max<UINT>(4096, m_cachedVertexCapacity);
            while (capacity < requiredBytes)
            {
                if (capacity > std::numeric_limits<UINT>::max() / 2)
                {
                    capacity = requiredBytes;
                    break;
                }
                capacity *= 2;
            }

            ComPtr<ID3D11Device> device;
            m_context->GetDevice(device.GetAddressOf());
            D3D11_BUFFER_DESC vertexDesc = {};
            vertexDesc.ByteWidth = capacity;
            vertexDesc.Usage = D3D11_USAGE_DYNAMIC;
            vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            vertexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            DX::ThrowIfFailed(device->CreateBuffer(&vertexDesc, nullptr, m_cachedVertices.ReleaseAndGetAddressOf()));
            m_cachedVertexCapacity = capacity;
        }

        D3D11_MAPPED_SUBRESOURCE mapped = {};
        DX::ThrowIfFailed(m_context->Map(m_cachedVertices.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
        std::memcpy(mapped.pData, vertices.data(), requiredBytes);
        m_context->Unmap(m_cachedVertices.Get(), 0);
    }
    m_cachedVertexCount = vertexCount;
    m_cacheDirty = false;
}


void TextConsole::DrawCachedGeometry()
{
    if (!m_cachedVertexCount)
        return;

    D3D11_VIEWPORT viewport = m_viewport;
    if (!m_hasViewport)
    {
        UINT count = 1;
        m_context->RSGetViewports(&count, &viewport);
        if (count != 1)
            throw std::runtime_error("No viewport is set for cached text");
    }

    if (viewport.Width != m_transformWidth || viewport.Height != m_transformHeight)
    {
        const float xScale = viewport.Width > 0.f ? 2.f / viewport.Width : 0.f;
        const float yScale = viewport.Height > 0.f ? 2.f / viewport.Height : 0.f;
        const XMMATRIX matrix(xScale, 0, 0, 0,
                              0, -yScale, 0, 0,
                              0, 0, 1, 0,
                              -1, 1, 0, 1);
        XMFLOAT4X4 transform;
        XMStoreFloat4x4(&transform, matrix);
        m_context->UpdateSubresource(m_cachedTransform.Get(), 0, nullptr, &transform, 0, 0);
        m_transformWidth = viewport.Width;
        m_transformHeight = viewport.Height;
    }

    auto blend = m_cachedStates->AlphaBlend();
    auto depth = m_cachedStates->DepthNone();
    auto rasterizer = m_cachedStates->CullCounterClockwise();
    auto sampler = m_cachedStates->LinearClamp();
    m_context->OMSetBlendState(blend, nullptr, 0xffffffff);
    m_context->OMSetDepthStencilState(depth, 0);
    m_context->RSSetState(rasterizer);
    m_context->PSSetSamplers(0, 1, &sampler);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->IASetInputLayout(m_cachedInputLayout.Get());
    auto vertexBuffer = m_cachedVertices.Get();
    const UINT stride = sizeof(VertexPositionColorTexture);
    const UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    m_context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    m_context->VSSetShader(m_cachedVS.Get(), nullptr, 0);
    m_context->PSSetShader(m_cachedPS.Get(), nullptr, 0);
    auto transformBuffer = m_cachedTransform.Get();
    m_context->VSSetConstantBuffers(0, 1, &transformBuffer);
    auto sheet = m_fontSheet.Get();
    m_context->PSSetShaderResources(0, 1, &sheet);
    m_context->Draw(m_cachedVertexCount, 0);
}


void TextConsole::ProcessString(_In_z_ const wchar_t* str)
{
    if (!m_lines)
        return;

    float width = float(m_layout.right - m_layout.left);

    for (const wchar_t* ch = str; *ch != 0; ++ch)
    {
        if (*ch == '\n')
        {
            IncrementLine();
            continue;
        }

        bool increment = false;

        if (m_currentColumn >= m_columns)
        {
            increment = true;
        }
        else
        {
            m_lines[m_currentLine][m_currentColumn] = *ch;

            float lineWidth = 0.0;
            if (m_fixedWidth) {
                // fixed-width font optimization
                lineWidth = std::wcslen(m_lines[m_currentLine]) * m_fixedWidth;
            }
            else {
                auto fontSize = m_font->MeasureString(m_lines[m_currentLine]);
                lineWidth = XMVectorGetX(fontSize);
            }

            if (lineWidth > width)
            {
                m_lines[m_currentLine][m_currentColumn] = L'\0';

                increment = true;
            }
        }

        if (increment)
        {
            IncrementLine();
            m_lines[m_currentLine][0] = *ch;
        }

        ++m_currentColumn;
    }
}


void TextConsole::IncrementLine()
{
    if (!m_lines)
        return;

    m_currentLine = (m_currentLine + 1) % m_rows;
    m_currentColumn = 0;
    memset(m_lines[m_currentLine], 0, sizeof(wchar_t) * (m_columns + 1));
}

void TextConsole::SetFixedWidthFont(bool isFixedWidth) {
    if (isFixedWidth) {
        // If we know we're using a fixed-width font, we can avoid a lot of MeasureString calls
        m_fixedWidth = XMVectorGetX(m_font->MeasureString(L"M"));
    }
    else {
        m_fixedWidth = 0.0;
    }
 }
