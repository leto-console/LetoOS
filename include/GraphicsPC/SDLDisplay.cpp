#ifdef __LINUX__

#include "SDLDisplay.hpp"

#include <SDL2/SDL.h>

#include <GraphicsPC/BufferScreen.hpp>

#include <GraphicsPC/KeyEventPC.hpp>
#include <Input/KeyCodePC.hpp>

static bool closeFlag = false;

// Минимальный маппинг SDL_Scancode -> VK_*. Расширять по мере надобности.
static uint64_t ScancodeToVK(SDL_Scancode sc)
{
    switch (sc)
    {
    case SDL_SCANCODE_ESCAPE:    return KeyCodePC::KEY_ESCAPE;
    case SDL_SCANCODE_RETURN:    return KeyCodePC::KEY_RETURN;
    case SDL_SCANCODE_SPACE:     return KeyCodePC::KEY_SPACE;
    case SDL_SCANCODE_TAB:       return KeyCodePC::KEY_TAB;
    case SDL_SCANCODE_BACKSPACE: return KeyCodePC::KEY_BACK;

    case SDL_SCANCODE_LEFT:      return KeyCodePC::KEY_LEFT;
    case SDL_SCANCODE_RIGHT:     return KeyCodePC::KEY_RIGHT;
    case SDL_SCANCODE_UP:        return KeyCodePC::KEY_UP;
    case SDL_SCANCODE_DOWN:      return KeyCodePC::KEY_DOWN;
    case SDL_SCANCODE_A:         return KeyCodePC::KEY_A;
    case SDL_SCANCODE_B:         return KeyCodePC::KEY_B;
    // ...
    default:                     return 0;
    }
}

SDLDisplay::SDLDisplay(int width, int height, int scale)
    : m_window{nullptr}, m_renderer{nullptr}, m_texture{nullptr}
    , m_width{width}, m_height{height}, m_scale{scale}
{
    closeFlag = false;
}

SDLDisplay::~SDLDisplay()
{
    if (m_texture)  SDL_DestroyTexture(static_cast<SDL_Texture*>(m_texture));
    if (m_renderer) SDL_DestroyRenderer(static_cast<SDL_Renderer*>(m_renderer));
    if (m_window)   SDL_DestroyWindow(static_cast<SDL_Window*>(m_window));
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

bool SDLDisplay::Init()
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) return false;

    m_window = SDL_CreateWindow("STM_INS",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        m_width * m_scale, m_height * m_scale, 0);
    if (!m_window) return false;

    m_renderer = SDL_CreateRenderer(static_cast<SDL_Window*>(m_window), -1,
                                    SDL_RENDERER_ACCELERATED);
    if (!m_renderer) return false;

    m_texture = SDL_CreateTexture(static_cast<SDL_Renderer*>(m_renderer),
        SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        m_width, m_height);
    if (!m_texture) return false;

    return true;
}

void SDLDisplay::Loop()
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        switch (e.type)
        {
        case SDL_QUIT:
            closeFlag = true;
            break;

        case SDL_KEYDOWN:
        {
            const uint64_t vk = ScancodeToVK(e.key.keysym.scancode);
            if (vk) KeyEventsPCMap[vk].push_back({vk, KeyEventPC::PRESSED});
            break;
        }
        case SDL_KEYUP:
        {
            const uint64_t vk = ScancodeToVK(e.key.keysym.scancode);
            if (vk) KeyEventsPCMap[vk].push_back({vk, KeyEventPC::RELEASED});
            break;
        }

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        {
            uint64_t vk = 0;
            switch (e.button.button) {
            case SDL_BUTTON_LEFT:   vk = KeyCodePC::KEY_LBUTTON; break;
            case SDL_BUTTON_RIGHT:  vk = KeyCodePC::KEY_RBUTTON; break;
            case SDL_BUTTON_MIDDLE: vk = KeyCodePC::KEY_MBUTTON; break;
            }
            if (vk)
            {
                const auto ev = (e.type == SDL_MOUSEBUTTONDOWN)
                    ? KeyEventPC::PRESSED : KeyEventPC::RELEASED;
                KeyEventsPCMap[vk].push_back({vk, ev});
            }
            break;
        }

        case SDL_MOUSEWHEEL:
            if (e.wheel.y > 0)
                KeyEventsPCMap[KeyCodePC::MOUSE_WHEEL].push_back(
                    {KeyCodePC::MOUSE_WHEEL, KeyEventPC::ROTATE_LEFT});
            else if (e.wheel.y < 0)
                KeyEventsPCMap[KeyCodePC::MOUSE_WHEEL].push_back(
                    {KeyCodePC::MOUSE_WHEEL, KeyEventPC::ROTATE_RIGHT});
            break;
        }
    }
}

bool SDLDisplay::Render(IDrawer* drawer)
{
    static BufferScreen screen;
    screen.SetSize(m_width, m_height);
    drawer->Draw(screen);

    SDL_UpdateTexture(static_cast<SDL_Texture*>(m_texture),
        nullptr, screen.image, m_width * sizeof(uint32_t));
    SDL_RenderClear(static_cast<SDL_Renderer*>(m_renderer));
    SDL_RenderCopy(static_cast<SDL_Renderer*>(m_renderer),
        static_cast<SDL_Texture*>(m_texture), nullptr, nullptr);
    SDL_RenderPresent(static_cast<SDL_Renderer*>(m_renderer));
    return true;
}

bool SDLDisplay::IsOpen()       { return !closeFlag; }
int  SDLDisplay::Width()  const { return m_width; }
int  SDLDisplay::Height() const { return m_height; }

#endif
