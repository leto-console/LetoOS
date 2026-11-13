/**
 * @file KeyCodePC.hpp
 * @date Nov 14, 2026
 * @author Rakhimov T.
 */
#pragma once

#include <cstdint>

// Коды клавиш и кнопок для ПК версий. Значения совпадают с WinAPI VK_*,
// чтобы Windows-бэкенд работал как pass-through, а прикладной код мог
// использовать привычные имена VK_* на любой ОС.
namespace KeyCodePC 
{
    inline constexpr uint32_t KEY_UNKNOWN   = 0x00;

    // Мышь
    inline constexpr uint32_t KEY_LBUTTON   = 0x01;
    inline constexpr uint32_t KEY_RBUTTON   = 0x02;
    inline constexpr uint32_t KEY_CANCEL    = 0x03;
    inline constexpr uint32_t KEY_MBUTTON   = 0x04;

    // Управляющие
    inline constexpr uint32_t KEY_BACK      = 0x08;
    inline constexpr uint32_t KEY_TAB       = 0x09;
    inline constexpr uint32_t KEY_RETURN    = 0x0D;
    inline constexpr uint32_t KEY_SHIFT     = 0x10;
    inline constexpr uint32_t KEY_CONTROL   = 0x11;
    inline constexpr uint32_t KEY_MENU      = 0x12; // Alt
    inline constexpr uint32_t KEY_PAUSE     = 0x13;
    inline constexpr uint32_t KEY_CAPITAL   = 0x14; // CapsLock
    inline constexpr uint32_t KEY_ESCAPE    = 0x1B;
    inline constexpr uint32_t KEY_SPACE     = 0x20;
    inline constexpr uint32_t KEY_PRIOR     = 0x21; // PageUp
    inline constexpr uint32_t KEY_NEXT      = 0x22; // PageDown
    inline constexpr uint32_t KEY_END       = 0x23;
    inline constexpr uint32_t KEY_HOME      = 0x24;
    inline constexpr uint32_t KEY_LEFT      = 0x25;
    inline constexpr uint32_t KEY_UP        = 0x26;
    inline constexpr uint32_t KEY_RIGHT     = 0x27;
    inline constexpr uint32_t KEY_DOWN      = 0x28;
    inline constexpr uint32_t KEY_INSERT    = 0x2D;
    inline constexpr uint32_t KEY_DELETE    = 0x2E;

    // Цифры верхнего ряда
    inline constexpr uint32_t KEY_NUM0 = 0x30, KEY_NUM1 = 0x31, KEY_NUM2 = 0x32, KEY_NUM3 = 0x33,
                            KEY_NUM4 = 0x34, KEY_NUM5 = 0x35, KEY_NUM6 = 0x36, KEY_NUM7 = 0x37,
                            KEY_NUM8 = 0x38, KEY_NUM9 = 0x39;

    // Латиница (значение = ASCII код заглавной буквы)
    inline constexpr uint32_t KEY_A = 0x41, KEY_B = 0x42, KEY_C = 0x43, KEY_D = 0x44, KEY_E = 0x45,
                            KEY_F = 0x46, KEY_G = 0x47, KEY_H = 0x48, KEY_I = 0x49, KEY_J = 0x4A,
                            KEY_K = 0x4B, KEY_L = 0x4C, KEY_M = 0x4D, KEY_N = 0x4E, KEY_O = 0x4F,
                            KEY_P = 0x50, KEY_Q = 0x51, KEY_R = 0x52, KEY_S = 0x53, KEY_T = 0x54,
                            KEY_U = 0x55, KEY_V = 0x56, KEY_W = 0x57, KEY_X = 0x58, KEY_Y = 0x59,
                            KEY_Z = 0x5A;

    // Numpad (только то, что не пересекается с основными)
    inline constexpr uint32_t KEY_MULTIPLY  = 0x6A;
    inline constexpr uint32_t KEY_ADD       = 0x6B;
    inline constexpr uint32_t KEY_SUBTRACT  = 0x6D;
    inline constexpr uint32_t KEY_DECIMAL   = 0x6E;
    inline constexpr uint32_t KEY_DIVIDE    = 0x6F;

    // F1..F12
    inline constexpr uint32_t KEY_F1  = 0x70, KEY_F2  = 0x71, KEY_F3  = 0x72, KEY_F4  = 0x73,
                            KEY_F5  = 0x74, KEY_F6  = 0x75, KEY_F7  = 0x76, KEY_F8  = 0x77,
                            KEY_F9  = 0x78, KEY_F10 = 0x79, KEY_F11 = 0x7A, KEY_F12 = 0x7B;

    // Модификаторы (правая/левая сторона)
    inline constexpr uint32_t KEY_LSHIFT    = 0xA0, KEY_RSHIFT    = 0xA1;
    inline constexpr uint32_t KEY_LCONTROL  = 0xA2, KEY_RCONTROL  = 0xA3;
    inline constexpr uint32_t KEY_LMENU     = 0xA4, KEY_RMENU     = 0xA5; // Alt

    // Пунктуация
    inline constexpr uint32_t KEY_OEM_1  = 0xBA; // ;:
    inline constexpr uint32_t KEY_OEM_PLUS  = 0xBB; // =+
    inline constexpr uint32_t KEY_OEM_COMMA = 0xBC; // ,<
    inline constexpr uint32_t KEY_OEM_MINUS = 0xBD; // -_
    inline constexpr uint32_t KEY_OEM_PERIOD= 0xBE; // .>
    inline constexpr uint32_t KEY_OEM_2  = 0xBF; // /?
    inline constexpr uint32_t KEY_OEM_3  = 0xC0; // `~
    inline constexpr uint32_t KEY_OEM_4  = 0xDB; // [{
    inline constexpr uint32_t KEY_OEM_5  = 0xDC; // \|
    inline constexpr uint32_t KEY_OEM_6  = 0xDD; // ]}
    inline constexpr uint32_t KEY_OEM_7  = 0xDE; // '"

    // Специальный "код устройства" для колеса мыши (вне диапазона VK_*)
    inline constexpr uint32_t MOUSE_WHEEL = 0x1000;

} // namespace KeyCodePC
