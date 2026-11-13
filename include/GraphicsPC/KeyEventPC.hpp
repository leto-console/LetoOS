/**
 * @file KeyEventsPC.hpp
 * @date Nov 14, 2026
 * @author Rakhimov T.
 */
#pragma once

#include "LetoOSExport.h"

#include <cstdint>
#include <deque>
#include <map>

struct KeyEventPC
{
	enum { IDLE, PRESSED, RELEASED, ROTATE_LEFT, ROTATE_RIGHT };
	uint64_t key;
	int event;
};


extern LETO_OS_EXPORT std::map<uint64_t, std::deque<KeyEventPC>> KeyEventsPCMap;
