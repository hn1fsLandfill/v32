/*
 * Just enough of explorer to manage the desktop
 *
 * Copyright 2006 Alexandre Julliard
 * Copyright 2013 Hans Leidekker for CodeWeavers
 * Copyright 2026 hn1f
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

#define COBJMACROS
#define OEMRESOURCE
#include <stdio.h>
#include <windows.h>
#include <winternl.h>
#include <rpc.h>
#include <wchar.h>
#include <ntuser.h>

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(v32m);

#define DESKTOP_CLASS_ATOM ((LPCWSTR)MAKEINTATOM(32769))
#define DESKTOP_ALL_ACCESS 0x01ff

static const char *debugstr_devmodew( const DEVMODEW *devmode )
{
    char position[32] = {0};

    if (devmode->dmFields & DM_POSITION)
    {
        snprintf( position, sizeof(position), " at (%d,%d)",
                 (int)devmode->dmPosition.x, (int)devmode->dmPosition.y );
    }

    return wine_dbg_sprintf( "%ux%u %ubits %uHz rotated %u degrees%s",
                             (unsigned int)devmode->dmPelsWidth,
                             (unsigned int)devmode->dmPelsHeight,
                             (unsigned int)devmode->dmBitsPerPel,
                             (unsigned int)devmode->dmDisplayFrequency,
                             (unsigned int)devmode->dmDisplayOrientation * 90,
                             position );
}


static const WCHAR default_driver[] = L"mac,x11,wayland";

static void load_graphics_driver( const WCHAR *driver, GUID *guid )
{
    static const WCHAR device_keyW[] = L"System\\CurrentControlSet\\Control\\Video\\{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}\\0000";

    WCHAR buffer[MAX_PATH], libname[32], *name, *next;
    WCHAR key[ARRAY_SIZE( device_keyW ) + 39];
    BOOL null_driver = FALSE;
    HMODULE module = 0;
    HKEY hkey;
    char error[80];

    if (!driver)
    {
        lstrcpyW( buffer, default_driver );

        /* @@ Wine registry key: HKCU\Software\Wine\Drivers */
        if (!RegOpenKeyW( HKEY_CURRENT_USER, L"Software\\Wine\\Drivers", &hkey ))
        {
            DWORD count = sizeof(buffer);
            RegQueryValueExW( hkey, L"Graphics", 0, NULL, (LPBYTE)buffer, &count );
            RegCloseKey( hkey );
        }
    }
    else lstrcpynW( buffer, driver, ARRAY_SIZE( buffer ));

    name = buffer;
    while (name)
    {
        next = wcschr( name, ',' );
        if (next) *next++ = 0;

        if (!wcscmp( name, L"null" ))
        {
            memset( guid, 0, sizeof(*guid) );
            TRACE( "display %s using null driver\n", debugstr_guid(guid) );
            wcscpy( libname, L"null" );
            null_driver = TRUE;
            break;
        }

        swprintf( libname, ARRAY_SIZE( libname ), L"wine%s.drv", name );
        if ((module = LoadLibraryW( libname )) != 0) break;
        switch (GetLastError())
        {
        case ERROR_MOD_NOT_FOUND:
            strcpy( error, "The graphics driver is missing. Check your build!" );
            break;
        case ERROR_DLL_INIT_FAILED:
            strcpy( error, "Make sure that your display server is running and that its variables are set." );
            break;
        default:
            sprintf( error, "Unknown error (%lu).", GetLastError() );
            break;
        }
        name = next;
    }

    TRACE( "display %s driver %s\n", debugstr_guid(guid), debugstr_w(libname) );

    swprintf( key, ARRAY_SIZE(key), device_keyW, guid->Data1, guid->Data2, guid->Data3,
              guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
              guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7] );

    if (!RegCreateKeyExW( HKEY_LOCAL_MACHINE, key, 0, NULL,
                          REG_OPTION_VOLATILE, KEY_SET_VALUE, NULL, &hkey, NULL  ))
    {
        if (module || null_driver)
            RegSetValueExW( hkey, L"GraphicsDriver", 0, REG_SZ,
                            (BYTE *)libname, (lstrlenW(libname) + 1) * sizeof(WCHAR) );
        else
            RegSetValueExA( hkey, "DriverError", 0, REG_SZ, (BYTE *)error, strlen(error) + 1 );
        RegCloseKey( hkey );
    }
}

static void initialize_display_settings( unsigned int width, unsigned int height )
{
    DISPLAY_DEVICEW device = {.cb = sizeof(DISPLAY_DEVICEW)};
    DWORD i = 0, flags = CDS_GLOBAL | CDS_UPDATEREGISTRY;
    HANDLE thread;

    /* Store current display mode in the registry */
    while (EnumDisplayDevicesW( NULL, i++, &device, 0 ))
    {
        DEVMODEW devmode = {.dmSize = sizeof(DEVMODEW)};

        if (!EnumDisplaySettingsExW( device.DeviceName, ENUM_CURRENT_SETTINGS, &devmode, 0))
        {
            ERR( "Failed to query current display settings for %s.\n", debugstr_w( device.DeviceName ) );
            continue;
        }

        TRACE( "Device %s current display mode %s.\n", debugstr_w( device.DeviceName ), debugstr_devmodew( &devmode ) );

        if (ChangeDisplaySettingsExW( device.DeviceName, &devmode, 0, flags | CDS_NORESET, 0 ))
            ERR( "Failed to initialize registry display settings for %s.\n", debugstr_w( device.DeviceName ) );
    }

    DEVMODEW devmode =
    {
        .dmSize = sizeof(DEVMODEW),
        .dmFields = DM_PELSWIDTH | DM_PELSHEIGHT,
        .dmPelsWidth = width,
        .dmPelsHeight = height,
    };
    
	/* in virtual desktop mode, set the primary display settings to match desktop size */
    if (ChangeDisplaySettingsExW( NULL, &devmode, 0, flags, NULL ))
        ERR( "Failed to set primary display settings.\n" );
}

static void set_desktop_window_title( HWND hwnd, const WCHAR *name )
{
    SetWindowTextW( hwnd, L"v32" );
    return;
}

/* parse the desktop size specification */
static BOOL parse_size( const WCHAR *size, unsigned int *width, unsigned int *height )
{
    WCHAR *end;

    *width = wcstoul( size, &end, 10 );
    if (end == size) return FALSE;
    if (*end != 'x') return FALSE;
    size = end + 1;
    *height = wcstoul( size, &end, 10 );
    return !*end;
}

/* retrieve the default desktop size from the registry */
static BOOL get_default_desktop_size( const WCHAR *name, unsigned int *width, unsigned int *height )
{
    HKEY hkey;
    WCHAR buffer[64];
    DWORD size = sizeof(buffer);
    BOOL found = FALSE;

    *width = 800;
    *height = 600;

    /* @@ Wine registry key: HKCU\Software\Wine\Explorer\Desktops */
    if (!RegOpenKeyW( HKEY_CURRENT_USER, L"Software\\Wine\\Explorer\\Desktops", &hkey ))
    {
        if (!RegQueryValueExW( hkey, name, 0, NULL, (LPBYTE)buffer, &size ))
        {
            found = TRUE;
            if (!parse_size( buffer, width, height )) *width = *height = 0;
        }
        RegCloseKey( hkey );
    }
    return found;
}

LRESULT WINAPI desktop_windowproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg) {
		case WM_SYSCOMMAND:
        	switch(wParam & 0xfff0)
        	{
        		case SC_CLOSE:
        		    ExitWindows( 0, 0 );
        		    return 0;
        	}
        	break;

    	case WM_CLOSE:
    	    PostQuitMessage(0);
    	    return 0;

    	case WM_SETCURSOR:
    	    return (LRESULT)SetCursor( LoadCursorA( 0, (LPSTR)IDC_ARROW ) );

    	case WM_NCHITTEST:
    	    return HTCLIENT;

    	case WM_ERASEBKGND:
    	    PaintDesktop( (HDC)wParam );
    	    return TRUE;

    	case WM_SETTINGCHANGE:
    	    if (wParam == SPI_SETDESKWALLPAPER)
    	        SystemParametersInfoW( SPI_SETDESKWALLPAPER, 0, NULL, FALSE );
    	    return 0;

		case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint( hwnd, &ps );
            PaintDesktop( ps.hdc );
            EndPaint( hwnd, &ps );
        }
	}
	return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

/* main desktop management function */
static void manage_desktop(void)
{
    HDESK desktop = 0;
    GUID guid;
    MSG msg;
    HWND hwnd;
    unsigned int width, height;
    WCHAR *driver = NULL;
    const WCHAR *name = NULL;
    HMODULE shell32;
    HANDLE thread;
    DWORD id;
    NTSTATUS status;

	// ALWAYS create a desktop no matter what
    // The root desktop shouldn't be existent as v32 is a compositor
    name = L"Default";
    if (!get_default_desktop_size( name, &width, &height )) {
		width = 800;
    	height = 600;
	}    

    UuidCreate( &guid );
    TRACE( "display guid %s\n", debugstr_guid(&guid) );
    load_graphics_driver( driver, &guid );

    if (name && width && height)
    {
        DEVMODEW devmode = {.dmPelsWidth = width, .dmPelsHeight = height};
        desktop = CreateDesktopW( name, NULL, &devmode, DF_WINE_VIRTUAL_DESKTOP, DESKTOP_ALL_ACCESS, NULL );
        if (!desktop)
        {
            ERR( "failed to create desktop %s error %ld\n", debugstr_w(name), GetLastError() );
            ExitProcess( 1 );
        }
        SetThreadDesktop( desktop );
    }

    /* the desktop process should always have an admin token */
    status = NtSetInformationProcess( GetCurrentProcess(), ProcessWineGrantAdminToken, NULL, 0 );
    if (status) WARN( "couldn't set admin token for desktop, error %08lx\n", status );

    /* create the desktop window */
    hwnd = CreateWindowExW( 0, DESKTOP_CLASS_ATOM, NULL,
                            WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0, 0, 0, 0, 0, 0, 0, &guid );

    if (hwnd)
    {
        /* create the HWND_MESSAGE parent */
        CreateWindowExW( 0, L"Message", NULL, WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                         0, 0, 100, 100, 0, 0, 0, NULL );

		SetWindowLongPtrW( hwnd, GWLP_WNDPROC,
            (LONG_PTR)desktop_windowproc );

        SetWindowPos( hwnd, 0, GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN),
                      GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN),
                      SWP_SHOWWINDOW );

        SystemParametersInfoW( SPI_SETDESKWALLPAPER, 0, NULL, FALSE );
        ClipCursor( NULL );
        initialize_display_settings( width, height );
    }

    /* run the desktop message loop */
    if (hwnd)
    {
        TRACE( "desktop message loop starting on hwnd %p\n", hwnd );
        while (GetMessageW( &msg, 0, 0, 0 )) DispatchMessageW( &msg );
        TRACE( "desktop message loop exiting for hwnd %p\n", hwnd );
    }

}

int WINAPI wWinMain(HINSTANCE hinstance,
                    HINSTANCE previnstance,
                    LPWSTR cmdline,
                    int cmdshow)
{

    manage_desktop();
    return 0;
}