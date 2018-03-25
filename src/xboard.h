/*
  Name:         Zeta
  Description:  Experimental chess engine written in OpenCL.
  Author:       Srdja Matovic <s.matovic@app26.de>
  Created at:   2018-03-25
  Updated at:   2018
  License:      GPL >= v2

  Copyright (C) 2011-2018 Srdja Matovic

  Zeta is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  Zeta is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

#ifndef XBOARD_H_INCLUDED
#define XBOARD_H_INCLUDED

// xboard flags
extern bool xboard_mode;
extern bool xboard_force;
extern bool xboard_post;
extern bool xboard_san;
extern bool xboard_time;
extern bool xboard_debug;

void xboard();                 // xboard protocol command loop

#endif /* XBOARD_H_INCLUDED */

