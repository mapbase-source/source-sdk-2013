//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 =================
//
// Purpose: Contains the standard color format for the tools.
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "Color.h"

// Insted of using ansi values, we the valve implementation of the color format.
// So we dont have issues with standard hammer compile window/log format & linux.

// We will use the next color format in the tools.
//	- Green: process done.
//	- Dark yellow: indicates a path.
//	- Purple: indicates a number of a funtion.
//	- Red: process failed.
//	- Cyan: Only for header strings.

// +--------------------+------------------+
// |       Color        | ANSI Code       |
// +--------------------+------------------+
// | Black              | \033[30m        |
// | Red                | \033[31m        |
// | Green              | \033[32m        |
// | Yellow             | \033[33m        |
// | Blue               | \033[34m        |
// | Magenta            | \033[35m        |
// | Cyan               | \033[36m        |
// | White              | \033[37m        |
// +--------------------+------------------+
// | Bright Black       | \033[90m        |
// | Bright Red         | \033[91m        |
// | Bright Green       | \033[92m        |
// | Bright Yellow      | \033[93m        |
// | Bright Blue        | \033[94m        |
// | Bright Magenta     | \033[95m        |
// | Bright Cyan        | \033[96m        |
// | Bright White       | \033[97m        |
// +--------------------+------------------+
// | Reset              | \033[0m         |
// +--------------------+------------------+

// Colors
extern Color black;				// Black
extern Color red;				// Red
extern Color green;				// Green
extern Color yellow;			// Yellow
extern Color blue;				// Blue
extern Color magenta;			// Magenta
extern Color cyan;				// Cyan
extern Color white;				// White

// Bright Colors (High Intensity)
extern Color bright_black;      // Bright Black (Gray)
extern Color bright_red;        // Bright Red
extern Color bright_green;      // Bright Green
extern Color bright_yellow;     // Bright Yellow
extern Color bright_blue;       // Bright Blue
extern Color bright_magenta;    // Bright Magenta
extern Color bright_cyan;       // Bright Cyan
extern Color bright_white;      // Bright White

// Dark Colors (Low Intensity)
extern Color dark_yellow;		// Custom