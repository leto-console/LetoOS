#include <Input/Devices/EncoderDevice.hpp>

#include <Input/EncoderEvent.hpp>

EncoderDevice::EncoderDevice()
	: UserInputDevice{ AE_Encoder }
{
}

#ifdef USE_HAL_DRIVER
void EncoderDevice::Init(uint8_t id, GPIO_TypeDef* gpio_clk_port, uint16_t gpio_clk_pin, GPIO_TypeDef* gpio_dt_port, uint16_t gpio_dt_pin)
{
	this->gpio_clk_port = gpio_clk_port;
	this->gpio_clk_pin = gpio_clk_pin;
	this->gpio_dt_port = gpio_dt_port;
	this->gpio_dt_pin = gpio_dt_pin;

	UserInputDevice::Init(id);
}
#else
void EncoderDevice::Init(uint8_t id, uint64_t key_left, uint64_t key_right, bool enable_wheel)
{
	this->keys.push_back(key_left);
	this->keys.push_back(key_right);

	this->key_left = key_left;
	this->key_right = key_right;
	this->enable_wheel = enable_wheel;

	UserInputDevice::Init(id);
}
#endif

void EncoderDevice::Tick()
{
	if (!initialized)
		return;

#ifdef USE_HAL_DRIVER
	stateCLK = !HAL_GPIO_ReadPin(gpio_clk_port, gpio_clk_pin);
	stateDT = !HAL_GPIO_ReadPin(gpio_dt_port, gpio_dt_pin);

	if (stateCLK && stateCLK != lastStateCLK) {
		if ((stateDT != stateCLK) != reverse)
			OnLeftTurn();
		else
			OnRightTurn();
	}

	lastStateCLK = stateCLK;

#else

	if (KeyEventsPCMap.find(key_left) != KeyEventsPCMap.end() &&
		!KeyEventsPCMap[key_left].empty())
	{
		KeyEventPC& event = KeyEventsPCMap[key_left].front();
		if (event.event == KeyEventPC::PRESSED) OnLeftTurn();
		KeyEventsPCMap[key_left].pop_front();
	}

	if (KeyEventsPCMap.find(key_right) != KeyEventsPCMap.end() &&
		!KeyEventsPCMap[key_right].empty())
	{
		KeyEventPC& event = KeyEventsPCMap[key_right].front();
		if (event.event == KeyEventPC::PRESSED) OnRightTurn();
		KeyEventsPCMap[key_right].pop_front();
	}

	if (enable_wheel)
	{
		if (KeyEventsPCMap.find(KeyCodePC::MOUSE_WHEEL) != KeyEventsPCMap.end() &&
		!KeyEventsPCMap[KeyCodePC::MOUSE_WHEEL].empty())
		{
			KeyEventPC& event = KeyEventsPCMap[KeyCodePC::MOUSE_WHEEL].front();
			if      (event.event == KeyEventPC::ROTATE_LEFT)  OnLeftTurn();
			else if (event.event == KeyEventPC::ROTATE_RIGHT) OnRightTurn();
			KeyEventsPCMap[KeyCodePC::MOUSE_WHEEL].pop_front();
		}
	}

#endif
}

void EncoderDevice::SetReverse(bool reverse)
{
	this->reverse = reverse;
}

void EncoderDevice::OnLeftTurn(uint8_t count)
{
	PushEvent((uint8_t) EncoderEvent::TURN_LEFT);
}

void EncoderDevice::OnRightTurn(uint8_t count)
{
	PushEvent((uint8_t) EncoderEvent::TURN_RIGHT);
}
