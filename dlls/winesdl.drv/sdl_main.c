/*
 * SDL driver DLL definitions
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

static const struct user_driver_funcs sdl_funcs;

BOOL SDL_WindowPosChanging(HWND hwnd, UINT swp_flags, BOOL shaped, const struct window_rects *rects);
void SDL_WindowPosChanged(HWND hwnd, HWND insert_after, HWND owner_hint, UINT swp_flags,
                              const struct window_rects *new_rects, struct window_surface *surface);
BOOL SDL_CreateWindowSurface( HWND hwnd, BOOL layered, const RECT *surface_rect, struct window_surface **surface );
LRESULT SDL_WindowMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
LRESULT SDL_DesktopWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

UINT SDL_UpdateDisplayDevices(const struct gdi_device_manager *device_manager, void *param)
{
	UINT dpi = NtUserGetSystemDpiForProcess( NULL );
	struct gdi_monitor fake_monitor = {
		.rc_monitor = {
			.left = 0,
			.right = 1024,
			.top = 0,
			.bottom = 768
		},
		.rc_work = {
			.left = 0,
			.right = 1024,
			.top = 0,
			.bottom = 768
		},
		.edid = "SDL",
		.edid_len = 4,
		.hdr_enabled = FALSE
	};

	device_manager->add_gpu("SDL Graphics Device", NULL, NULL, param);
	device_manager->add_source("SDL Video Device", 0, dpi, param );
	device_manager->add_monitor(&fake_monitor, param );

	return 0;
}

static const struct user_driver_funcs sdl_funcs =
{
	//.dc_funcs = {
	//	//.pCreateCompatibleDC = SDL_CreateCompatibleDC,
	//	//.pCreateDC = SDL_CreateDC,
	//	//.pDeleteDC = SDL_DeleteDC,
	//	.priority = GDI_PRIORITY_GRAPHICS_DRV
	//},
	.pCreateWindowSurface = SDL_CreateWindowSurface,
	.pWindowPosChanging = SDL_WindowPosChanging,
	.pWindowPosChanged = SDL_WindowPosChanged,
	.pDesktopWindowProc = SDL_DesktopWindowProc,
	.pWindowMessage = SDL_WindowMessage,
    .pUpdateDisplayDevices = SDL_UpdateDisplayDevices
};

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;

static NTSTATUS sdl_unix_init(void *arg)
{
	__wine_set_user_driver(&sdl_funcs, WINE_GDI_DRIVER_VERSION);
	window = SDL_CreateWindow("v32",
    	SDL_WINDOWPOS_UNDEFINED,
    	SDL_WINDOWPOS_UNDEFINED,
    	1024,
    	768,
    	SDL_WINDOW_SHOWN
    );
	if(window == NULL) {
		FIXME("can't create SDL window\n");
		__wine_set_user_driver(NULL, WINE_GDI_DRIVER_VERSION);
		return STATUS_UNSUCCESSFUL;
	}
	renderer =  SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	SDL_RenderClear(renderer);
	SDL_RenderPresent(renderer);
	FIXME("god bless to google balls\n");
    return STATUS_SUCCESS;
}

static NTSTATUS sdl_unix_poll_events(void *arg)
{
	SDL_Event e;
	while(TRUE) {
		SDL_PollEvent(&e);
    	if (e.type == SDL_QUIT){
    	  TRACE("SDL Quit requested\n");
    	  return 0;
    	}
	}
	return 0;
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    sdl_unix_init,
	sdl_unix_poll_events,
};

C_ASSERT(ARRAYSIZE(__wine_unix_call_funcs) == sdl_unix_func_count);

#ifdef _WIN64
const unixlib_entry_t __wine_unix_call_wow64_funcs[] =
{
    sdl_unix_init,
	sdl_unix_poll_events,
};
C_ASSERT(ARRAYSIZE(__wine_unix_call_wow64_funcs) == sdl_unix_func_count);
#endif /* _WIN64 */