/*
 * SDL window function definitions
 *
 * Copyright (c) 2026 hn1f
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */
#if 0
#pragma makedep unix
#endif
#include "config.h"
#include <SDL.h>
#include "unixlib.h"
#include "wine/gdi_driver.h"
#include <winbase.h>
#include "ntstatus.h"

#include <wine/debug.h>
WINE_DEFAULT_DEBUG_CHANNEL(sdl);

BOOL SDL_WindowPosChanging(HWND hwnd, UINT swp_flags, BOOL shaped, const struct window_rects *rects)
{
	FIXME("hwnd %p, swp_flags %04x, shaped %u, rects %s\n", hwnd, swp_flags, shaped, debugstr_window_rects(rects));
	return TRUE;
}

void SDL_WindowPosChanged(HWND hwnd, HWND insert_after, HWND owner_hint, UINT swp_flags,
                              const struct window_rects *new_rects, struct window_surface *surface)
{
    TRACE("hwnd %p new_rects %s after %p flags %08x\n", hwnd, debugstr_window_rects(new_rects), insert_after, swp_flags);
}

BOOL SDL_CreateWindowSurface( HWND hwnd, BOOL layered, const RECT *surface_rect, struct window_surface **surface )
{
	FIXME("hwnd %p, layered %u, surface_rect %s, surface %p\n", hwnd, layered, wine_dbgstr_rect(surface_rect), surface);
    return FALSE;
}

LRESULT SDL_WindowMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    	default:
        	FIXME("got window msg %x hwnd %p wp %lx lp %lx\n", msg, hwnd, (long)wp, lp);
        	return 0;
    }
}

LRESULT SDL_DesktopWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    	case WM_DISPLAYCHANGE:
			FIXME("got wm display change");
    	    break;
    }

    return NtUserMessageCall(hwnd, msg, wp, lp, 0, NtUserDefWindowProc, FALSE);
}