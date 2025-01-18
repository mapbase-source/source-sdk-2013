//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 =================
//
// Purpose: Contains the standard color format for the tools.
//
// Author(s): Unusuario2
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "StandartColorFormat.h"

// Insted of using ansi values, we the valve implementation of the color format.
// So we dont have issues with standard hammer compile window/log format & linux.

// We will use the next color format in the tools.
//	- Green: process done.
//	- Blue: indicates a path.
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

// Ansi Color Codes in valve format.
// Format:	Color <name>(R, G, B, 255);
// Funtion: ColorSpewMessage( SPEW_MESSAGE, &<name>, "");

// Basic Colors
Color black(0, 0, 0, 255);				// Black
Color red(255, 0, 0, 255);				// Red,	Error
Color green(0, 255, 0, 255);			// Green, Done
Color yellow(255, 255, 0, 255);			// Yellow, Warning
Color blue(0, 0, 255, 255);				// Blue, Path
Color magenta(255, 0, 255, 255);		// Magenta, Number
Color cyan(0, 255, 255, 255);			// Cyan, Header
Color white(255, 255, 255, 255);		// White, Standard

//These seen to dont work so will we use the standard colors.
// Bright Colors (High Intensity)
Color bright_black(85, 85, 85, 255);    // Bright Black (Gray)
Color bright_red(255, 85, 85, 255);     // Bright Red
Color bright_green(85, 255, 85, 255);   // Bright Green
Color bright_yellow(255, 255, 85, 255); // Bright Yellow
Color bright_blue(85, 85, 255, 255);    // Bright Blue
Color bright_magenta(255, 85, 255, 255);// Bright Magenta
Color bright_cyan(85, 255, 255, 255);   // Bright Cyan
Color bright_white(255, 255, 255, 255); // Bright White}

// Dark Colors (Low Intensity)
Color dark_yellow(100, 100, 0, 255);	// Custom

//Common funtions:
// ColorSpewMessage( SPEW_MESSAGE, &green, "done (%d)");	