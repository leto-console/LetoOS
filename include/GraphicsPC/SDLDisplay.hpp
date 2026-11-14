/**
 * @file SDLDisplay.hpp
 * @date Nov 13, 2026
 * @author Rakhimov T.
 */
#pragma once

#ifdef __LINUX__

#include "LetoOSExport.h"

#include <Graphics/IDrawer.hpp>
#include <Graphics/IDisplay.hpp>

class LETO_OS_EXPORT SDLDisplay : public IDisplay
{
public:
    SDLDisplay(int width, int height, int scale);
    ~SDLDisplay();

	SDLDisplay(const SDLDisplay&) = delete;
    SDLDisplay& operator =(const SDLDisplay&) = delete;

    bool Init() override;
    void Loop() override;
    bool Render(IDrawer* drawer) override;

    bool IsOpen() override;
    int  Width()  const override;
    int  Height() const override;

private:
    void* m_window;
    void* m_renderer;
    void* m_texture;
    int   m_width;
    int   m_height;
    int   m_scale;
};

#endif
