/*
 * ButtonDevice.cpp
 *
 *  Created on: Oct 20, 2025
 *      Author: Timur
 */

#include <Input/Devices/ButtonDevice.hpp>
#include <Input/ButtonEvent.hpp>

#include <Time/TimeUtils.hpp>

#ifndef USE_HAL_DRIVER
#include <Utils/FileConfig.hpp>
#endif

#include <cstring>

const long long DEBOUNCE_MS = 15;

ButtonDevice::ButtonDevice()
	: UserInputDevice{ AE_Button }
{
}

#ifdef USE_HAL_DRIVER
void ButtonDevice::Init(uint8_t id, GPIO_TypeDef* gpio_port, uint16_t gpio_pin)
{
	this->gpio_port = gpio_port;
	this->gpio_pin = gpio_pin;
	UserInputDevice::Init(id);
}
#else
void ButtonDevice::Init(uint8_t id, uint64_t key)
{
	this->keys.push_back(key);
	this->key = key;
	UserInputDevice::Init(id);
	// Нужно для управления приложением с помощью конфигурационного файла
	// Пока что для отладки хватает и нажатия на клавиатуру
	//FileConfig::InitBool("buttons", name);
}
#endif


void ButtonDevice::Tick()
{
	if (!initialized)
		return;

	uint32_t now = TimeUtils::GetCurrentMs();

	bool raw = current_state;

#ifdef USE_HAL_DRIVER

	raw = !HAL_GPIO_ReadPin(gpio_port, gpio_pin);

#else

	if (KeyEventsPCMap.find(key) != KeyEventsPCMap.end() &&
		!KeyEventsPCMap[key].empty())
	{
		KeyEventPC& event = KeyEventsPCMap[key].front();
		int winAPIKeyEvent = event.event;
		KeyEventsPCMap[key].pop_front();

		if		(winAPIKeyEvent == KeyEventPC::PRESSED)		raw = true;
		else if (winAPIKeyEvent == KeyEventPC::RELEASED)	raw = false;
		else 	return;
	}

#endif

	if (raw != current_state)
    {
        current_state = raw;
        last_change_time = now;
    }

    // Debounce
    if (current_state != stable_state && 
        (now - last_change_time) >= DEBOUNCE_MS)
    {
        stable_state = current_state;

        if (stable_state)
            PushEvent(ButtonEvent::PRESSED);
        else
            PushEvent(ButtonEvent::RELEASED);
    }
}
