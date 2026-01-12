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

#include <wine/debug.h>
WINE_DEFAULT_DEBUG_CHANNEL(sdl);

UINT SDL_UpdateDisplayDevices(const struct gdi_device_manager *device_manager, void *param)
{
	UINT dpi = NtUserGetSystemDpiForProcess( NULL );
	struct gdi_monitor fake_monitor = {
		.rc_monitor = {
			.left = 0,
			.right = 1024,
			.top = 0,
			.bottom = 0
		},
		.rc_work = {
			.left = 0,
			.right = 1024,
			.top = 0,
			.bottom = 0
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
    .pUpdateDisplayDevices = SDL_UpdateDisplayDevices,
};

SDL_Window* window = NULL;

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

	FIXME("sdl: god bless to google balls\n");
    return 0;
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    sdl_unix_init,
};

C_ASSERT(ARRAYSIZE(__wine_unix_call_funcs) == sdl_unix_func_count);

#ifdef _WIN64
const unixlib_entry_t __wine_unix_call_wow64_funcs[] =
{
    sdl_unix_init,
};
C_ASSERT(ARRAYSIZE(__wine_unix_call_wow64_funcs) == sdl_unix_func_count);
#endif /* _WIN64 */