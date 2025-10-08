//--------------------------------------------------------------------------------------
// File: TextConsole.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "TextConsole.h"
#include "../Utils.hpp"

#include "SimpleMath.h"
#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"

#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cwchar>
#include <utility>

using Microsoft::WRL::ComPtr;

using namespace DirectX;
using namespace DX;
using namespace moonlight_xbox_dx;

TextConsole::TextConsole() noexcept
    : m_layout{},
    m_textColor(1.f, 1.f, 1.f, 1.f),
    m_debugOutput(false),
    m_columns(0),
    m_rows(0)
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
}


_Use_decl_annotations_
void TextConsole::Write(const wchar_t* str)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    ProcessString(str);

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

    Utils::Logf("TextConsole initialized at (%d,%d) - (%d,%d) with %d rows, %d columns\n",
                layout.left, layout.top, layout.right, layout.bottom, m_rows, m_columns);

    if ((m_currentColumn >= m_columns) || (m_currentLine >= m_rows))
    {
        IncrementLine();
    }
}


void TextConsole::ReleaseDevice() noexcept
{
    m_batch.reset();
    m_font.reset();
    m_context.Reset();
}

_Use_decl_annotations_
void TextConsole::RestoreDevice(ID3D11DeviceContext* context, const wchar_t* fontName)
{
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
    m_context = context;

    m_batch = std::make_unique<SpriteBatch>(context);

    ComPtr<ID3D11Device> device;
    context->GetDevice(device.GetAddressOf());

    m_font = std::make_unique<SpriteFont>(device.Get(), fontBytes, fontSize);

    m_font->SetDefaultCharacter(L' ');
}

void TextConsole::SetViewport(const D3D11_VIEWPORT& viewPort)
{
    if (m_batch)
    {
        m_batch->SetViewport(viewPort);
    }
}


void TextConsole::SetRotation(DXGI_MODE_ROTATION rotation)
{
    if (m_batch)
    {
        m_batch->SetRotation(rotation);
    }
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
