/*
* Xbox Game runtime Library
 *
 * Written by Weather.
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

#ifndef __XGAMERUNTIME_PRIVATE_H__
#define __XGAMERUNTIME_PRIVATE_H__

#define COBJMACROS
#include <private/logging.h>
#include <windows.h>
#include <winstring.h>

#include <version.h>
#include <xgameruntimeinit.h>
#include <xgameerr.h>


/**
 *   Logging initialization is separate from xgameruntime.dll
 *  They both would share the same environment variables.
 */
inline volatile LONG loggerInitialized = FALSE;
inline void ensureLoggerInitialized()
{
    if ( !loggerInitialized )
    {
        InitializeLogging();
        loggerInitialized = TRUE;
    }
}

namespace GlobalState
{
    inline std::mutex g_lock{};
    inline HMODULE xgameruntime = nullptr;
    inline BOOLEAN initialized = FALSE;

    inline HRESULT (WINAPI *InitializeApiImplEx2)( ULONG gdkVer, ULONG gsVer, char mode, const INITIALIZE_OPTIONS *options ) = nullptr;
    inline HRESULT (WINAPI *InitializeApiImplEx)( ULONG gdkVer, ULONG gsVer, char mode ) = nullptr;
    inline HRESULT (WINAPI *InitializeApiImpl)( ULONG gdkVer, ULONG gsVer ) = nullptr;
    inline HRESULT (WINAPI *QueryApiImpl)( REFCLSID clsid, REFIID iid, void **out ) = nullptr;
    inline HRESULT (WINAPI *UninitializeApiImpl)() = nullptr;
    inline HRESULT (WINAPI *XErrorReport)( HRESULT status, LPCSTR message ) = nullptr;
}

HRESULT CheckXGameRuntimeInitialized();

#endif
