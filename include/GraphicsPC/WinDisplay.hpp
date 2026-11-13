/**
 * @file WinDisplay.hpp
 * @date Apr 23, 2026
 * @author Rakhimov T.
 */

#ifdef __WIN__

#ifndef GRAPHICS_PC_WIN_DISPLAY_HPP_
#define GRAPHICS_PC_WIN_DISPLAY_HPP_

#include "LetoOSExport.h"

#include <Graphics/IDisplay.hpp>

class LETO_OS_EXPORT WinDisplay : public IDisplay
{
public:
	WinDisplay(int width, int height, int scale);
	~WinDisplay();

	WinDisplay(const WinDisplay&) = delete;
    WinDisplay& operator =(const WinDisplay&) = delete;

    int Width() const override;
    int Height() const override;

	bool IsOpen() override;
	bool Init() override;
	void Loop() override;
    bool Render(IDrawer* drawer) override;

private:
	void* m_instance;
	void* m_window;
};

#endif /* GRAPICS_PC_WIN_DISPLAY_HPP_ */

#endif /* __WIN__ */
