/*
 * Xbox Game runtime Library
 *  GDK Component: System API -> XUser
 *
 * Copyright 2026 Olivia Ryan
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

#include "private.h"
#include "userprovider.h"
#include "util.h"
#include <bcrypt.h>
#include <dbghelp.h>
#include <ntdef.h>
#include <wininet.h>

WINE_DEFAULT_DEBUG_CHANNEL(gdkc);

static const WCHAR *ACCEPT_JSON[] = { L"application/json", NULL };

static HRESULT parse_json( const char *json, SIZE_T jsonLen, IJsonObject **object )
{
    static const WCHAR *name = RuntimeClass_Windows_Data_Json_JsonValue;
    IJsonValueStatics *statics;
    HSTRING_HEADER header;
    IJsonValue *value;
    UINT32 wJsonLen;
    HSTRING string;
    WCHAR *wJson;
    HRESULT hr;

    TRACE( "json %s, object %p.\n", debugstr_an( json, jsonLen ), object );

    if (!(wJsonLen = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, json, jsonLen, NULL, 0 ))) return HRESULT_FROM_WIN32( GetLastError() );
    if (FAILED(hr = WindowsCreateStringReference( name, wcslen( name ), &header, &string ))) return hr;
    if (FAILED(hr = RoGetActivationFactory( string, &IID_IJsonValueStatics, (void **)&statics ))) return hr;
    if (!(wJson = calloc( wJsonLen + 1, sizeof(WCHAR) )))
    {
        IJsonValueStatics_Release( statics );
        return E_OUTOFMEMORY;
    }

    if (!MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, json, jsonLen, wJson, wJsonLen + 1 )) goto error;
    if (FAILED(hr = WindowsCreateStringReference( wJson, wJsonLen + 1, &header, &string ))) goto cleanup;
    if (FAILED(hr = IJsonValueStatics_Parse( statics, string, &value ))) goto cleanup;
    hr = IJsonValue_GetObject( value, object );
    IJsonValue_Release( value );
    goto cleanup;

error:
    hr = HRESULT_FROM_WIN32( GetLastError() );
cleanup:
    IJsonValueStatics_Release( statics );
    free( wJson );
    return hr;
}

struct policy
{
    UINT32 version;
    UINT32 maxBodyBytes;
};

struct endpoint
{
    char *protocol;
    char *host;
    char *path;
    char *relyingParty;
    char *tokenType;
    struct policy *policy;
    BOOL wildcard;
};

struct XUser
{
    IUser IUser_iface;
    LONG ref;

    BCRYPT_KEY_HANDLE key;
    char *proofKey;
    char *userToken;
    UINT64 xuid;
    UINT32 policiesLen;
    UINT32 endpointsLen;
    struct policy *policies;
    struct endpoint *endpoints;

    char gamertag[16];
    char modernGamertag[97];
    char modernGamertagSuffix[15];
    char uniqueModernGamertag[101];
};

static inline struct XUser *impl_from_IUser( IUser *iface )
{
    return CONTAINING_RECORD( iface, struct XUser, IUser_iface );
}

static ULONG WINAPI user_AddRef( IUser *iface )
{
    struct XUser *impl = impl_from_IUser( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI user_Release( IUser *iface )
{
    struct XUser *impl = impl_from_IUser( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p decreasing refcount to %lu.\n", iface, ref );
    if (!ref)
    {
        if (impl->key) BCryptDestroyKey( impl->key );
        if (impl->proofKey) free( impl->proofKey );
        if (impl->userToken) free( impl->userToken );
        if (impl->policies) free( impl->policies );
        if (impl->endpoints) free( impl->endpoints );
    }
    return ref;
}

static HRESULT get_rps_tickets( BOOLEAN allowUi, char **userTicket, char **deviceTicket )
{
    FIXME( "allowUi %d, userTicket %p, deviceTicket %p stub!\n", allowUi, userTicket, deviceTicket );
    return E_NOTIMPL;
}

static HRESULT device_auth( XUserHandle user, const char *deviceTicket, char **deviceToken )
{
    static const char template[] = "{\"RelyingParty\":\"http://auth.xboxlive.com\",\"TokenType\":\"JWT\",\"Properties\":{\"AuthMethod\":\"RPS\",\"SiteName\":\"user.auth.xboxlive.com\",\"ProofKey\":";
    SIZE_T size = ARRAY_SIZE( template ) + strlen( user->proofKey ) + strlen( ",\"RpsTicket\":\"\"}" ) + strlen( deviceTicket );
    WCHAR header[116] = { 'S', 'i', 'g', 'n', 'a', 't', 'u', 'r', 'e', ':', ' ' };
    IJsonObject *object = NULL;
    const WCHAR *tokenBuffer;
    HSTRING token = NULL;
    UCHAR *buffer = NULL;
    char signature[104];
    UINT32 tokenSize;
    HRESULT hr;
    char *body;

    TRACE( "user %p, deviceTicket %p, deviceToken %p.\n", user, deviceTicket, deviceToken );

    if (!(body = calloc( 1, size ))) return E_OUTOFMEMORY;
    strcpy( body, template );
    strcat( body, user->proofKey );
    strcat( body, ",\"RpsTicket\":\"" );
    strcat( body, deviceTicket );
    strcat( body, "\"}" );
    if (FAILED(hr = IUser_GetSignature( &user->IUser_iface, 1, "POST", "https://device.auth.xboxlive.com/device/authenticate", "", size, body, signature ))) goto cleanup;
    if (!MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, signature, 104, header + 11, 104 )) goto error;
    if (FAILED(hr = http_request( L"POST", L"https://device.auth.xboxlive.com/device/authenticate", body, header, ACCEPT_JSON, &buffer, &size ))) goto cleanup;
    if (FAILED(hr = parse_json( (char *)buffer, size, &object ))) goto cleanup;
    if (FAILED(hr = get_json_string( object, L"Token", &token ))) goto cleanup;
    tokenBuffer = WindowsGetStringRawBuffer( token, NULL );
    if (!(size = WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, tokenBuffer, -1, NULL, 0, NULL, NULL ))) goto error;
    if (!(*deviceToken = calloc( 1, size )))
    {
        hr = E_OUTOFMEMORY;
        goto cleanup;
    }
    if (!WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, tokenBuffer, -1, *deviceToken, tokenSize, NULL, NULL )) goto error;
    goto cleanup;

error:
    hr = HRESULT_FROM_WIN32( GetLastError() );
cleanup:
    if (FAILED(hr) && *deviceToken) free( *deviceToken );
    if (object) IJsonObject_Release( object );
    if (token) WindowsDeleteString( token );
    if (buffer) free( buffer );
    free( body );
    return hr;
}

static HRESULT sisu_auth( XUserHandle user, const char *userTicket, const char *deviceToken, WCHAR **auth )
{
    static const char template[] = "{\"Sandbox\":\"RETAIL\",\"UseModernGamertag\":true,\"DeviceToken\":\"";
    SIZE_T size = ARRAY_SIZE( template ) + strlen( deviceToken ) + strlen( "\",\"AccessToken\":\"\"}" ) + strlen( userTicket );
    HSTRING authToken = NULL, gtg = NULL, mgs = NULL, mgt = NULL, umg = NULL, userToken = NULL, xid = NULL;
    IJsonObject *authObject = NULL, *claims = NULL, *identity = NULL, *object = NULL, *userObject = NULL;
    WCHAR header[116] = { 'S', 'i', 'g', 'n', 'a', 't', 'u', 'r', 'e', ':', ' ' };
    char *body, signature[104];
    const WCHAR *stringBuffer;
    IJsonArray *xui = NULL;
    UCHAR *buffer = NULL;
    HRESULT hr;

    TRACE( "user %p, userTicket %p, deviceToken %p, auth %p.\n", user, userTicket, deviceToken, auth );

    if (!(body = calloc( 1, size ))) return E_OUTOFMEMORY;
    strcpy( body, template );
    strcat( body, deviceToken );
    strcat( body, "\",\"AccessToken\":\"" );
    strcat( body, userTicket );
    strcat( body, "\"}" );
    if (FAILED(hr = IUser_GetSignature( &user->IUser_iface, 1, "POST", "https://sisu.auth.xboxlive.com/authorize", "", size, body, signature ))) goto cleanup;
    if (!MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, signature, 104, header + 11, 104 )) goto error;
    if (FAILED(hr = http_request( L"POST", L"https://sisu.auth.xboxlive.com/authorize", body, header, ACCEPT_JSON, &buffer, &size ))) goto cleanup;
    if (FAILED(hr = parse_json( (char *)buffer, size, &object ))) goto cleanup;
    if (FAILED(hr = get_json_object( object, L"UserToken", &userObject ))) goto cleanup;
    if (FAILED(hr = get_json_string( userObject, L"Token", &userToken ))) goto cleanup;
    stringBuffer = WindowsGetStringRawBuffer( userToken, NULL );
    if (!(size = WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, stringBuffer, -1, NULL, 0, NULL, NULL ))) goto error;
    if (!(user->userToken = calloc( 1, size )))
    {
        hr = E_OUTOFMEMORY;
        goto cleanup;
    }
    if (!WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, stringBuffer, -1, user->userToken, size, NULL, NULL )) goto error;
    if (FAILED(hr = get_json_object( object, L"AuthorizationToken", &authObject ))) goto cleanup;
    if (FAILED(hr = get_json_object( authObject, L"DisplayClaims", &claims ))) goto cleanup;
    if (FAILED(hr = get_json_array( claims, L"xui", &xui ))) goto cleanup;
    if (FAILED(hr = IJsonArray_GetObjectAt( xui, 0, &identity ))) goto cleanup;
    if (FAILED(hr = get_json_string( identity, L"xid", &xid ))) goto cleanup;
    stringBuffer = WindowsGetStringRawBuffer( xid, NULL );
    user->xuid = wcstoull( stringBuffer, NULL, 10 );
    if (FAILED(hr = get_json_string( identity, L"gtg", &gtg ))) goto cleanup;
    stringBuffer = WindowsGetStringRawBuffer( gtg, NULL );
    if (!WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, stringBuffer, -1, user->gamertag, 16, NULL, NULL )) goto error;
    if (FAILED(hr = get_json_string( identity, L"mgt", &mgt ))) goto cleanup;
    stringBuffer = WindowsGetStringRawBuffer( mgt, NULL );
    if (!WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, stringBuffer, -1, user->modernGamertag, 97, NULL, NULL )) goto error;
    if (SUCCEEDED(hr = get_json_string( identity, L"mgs", &mgs )))
    {
        stringBuffer = WindowsGetStringRawBuffer( mgs, NULL );
        if (!WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, stringBuffer, -1, user->modernGamertagSuffix, 15, NULL, NULL )) goto error;
    }
    else if (hr != WEB_E_JSON_VALUE_NOT_FOUND) goto cleanup;
    if (FAILED(hr = get_json_string( identity, L"umg", &umg ))) goto cleanup;
    stringBuffer = WindowsGetStringRawBuffer( umg, NULL );
    if (!WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, stringBuffer, -1, user->uniqueModernGamertag, 101, NULL, NULL )) goto error;
    if (FAILED(hr = get_json_string( authObject, L"Token", &authToken ))) goto cleanup;
    stringBuffer = WindowsGetStringRawBuffer( authToken, NULL );
    if (!(*auth = calloc( wcslen( L"Authorization: XBL3.0 x=-;" ) + wcslen( stringBuffer ) + 1, sizeof(WCHAR) )))
    {
        hr = E_OUTOFMEMORY;
        goto cleanup;
    }
    wcscpy( *auth, L"Authorization: XBL3.0 x=-;" );
    wcscat( *auth, stringBuffer );
    goto cleanup;

error:
    hr = HRESULT_FROM_WIN32( GetLastError() );
cleanup:
    if (authObject) IJsonObject_Release( authObject );
    if (userObject) IJsonObject_Release( userObject );
    if (authToken) WindowsDeleteString( authToken );
    if (userToken) WindowsDeleteString( userToken );
    if (identity) IJsonObject_Release( identity );
    if (claims) IJsonObject_Release( claims );
    if (object) IJsonObject_Release( object );
    if (gtg) WindowsDeleteString( gtg );
    if (mgs) WindowsDeleteString( mgs );
    if (mgt) WindowsDeleteString( mgt );
    if (umg) WindowsDeleteString( umg );
    if (xid) WindowsDeleteString( xid );
    if (xui) IJsonArray_Release( xui );
    if (buffer) free( buffer );
    free( body );
    return hr;
}

static HRESULT load_endpoints( XUserHandle user, BYTE *buffer, SIZE_T size )
{
    FIXME( "user %p, buffer %p, size %Iu stub!\n", user, buffer, size );
    return E_NOTIMPL;
}

static HRESULT WINAPI user_Initialize( IUser *iface, const XUserAddOptions options )
{
    static char proofKeyTemplate[] = "{\"alg\":\"ES256\",\"crv\":\"P-256\",\"kty\":\"EC\",\"use\":\"sig\",\"x\":\"";
    static const SIZE_T proofKeySize = ARRAY_SIZE( proofKeyTemplate ) + strlen( "\",\"y\":\"\"}" ) + 86;
    BYTE blob[sizeof(BCRYPT_ECCKEY_BLOB) + 64], *currentBuffer = NULL, *defaultBuffer = NULL;
    char *deviceTicket = NULL, *deviceToken = NULL, *userTicket = NULL, *x, *y;
    struct XUser *impl = impl_from_IUser( iface );
    WCHAR *auth = NULL;
    NTSTATUS status;
    SIZE_T size;
    ULONG dummy;
    HRESULT hr;

    TRACE( "iface %p.\n", iface );

    /* generate signing key pair and convert public key to jwk */
    if (!NT_SUCCESS(status = BCryptGenerateKeyPair( BCRYPT_ECDSA_P256_ALG_HANDLE, &impl->key, 256, 0 ))) return HRESULT_FROM_NT( status );
    if (!NT_SUCCESS(status = BCryptFinalizeKeyPair( impl->key, 0 ))) return HRESULT_FROM_NT( status );
    if (!NT_SUCCESS(status = BCryptExportKey( impl->key, NULL, BCRYPT_ECCPUBLIC_BLOB, blob, sizeof(blob), &dummy, 0 ))) return HRESULT_FROM_NT( status );
    if (!(impl->proofKey = calloc( 1, proofKeySize ))) return E_OUTOFMEMORY;
    x = impl->proofKey + ARRAY_SIZE( proofKeyTemplate ) - 1;
    y = x + 43 + strlen( "\",\"y\":\"" );
    strcpy( impl->proofKey, proofKeyTemplate );
    if (FAILED(hr = encode_base64_url( 32, blob + sizeof(BCRYPT_ECCKEY_BLOB), 43, x, FALSE ))) return hr;
    strcat( impl->proofKey, "\",\"y\":\"" );
    if (FAILED(hr = encode_base64_url( 32, blob + sizeof(BCRYPT_ECCKEY_BLOB) + 32, 43, y, FALSE ))) return hr;
    strcat( impl->proofKey, "}" );

    if (FAILED(hr = http_request( L"GET", L"https://title.mgt.xboxlive.com/titles/default/endpoints?type=1", NULL, NULL, ACCEPT_JSON, &defaultBuffer, &size ))) return hr;
    if (FAILED(hr = load_endpoints( impl, defaultBuffer, size ))) goto cleanup;
    if (FAILED(hr = get_rps_tickets( options & XUserAddOptions_AddDefaultUserAllowingUI, &userTicket, &deviceTicket ))) goto cleanup;
    if (FAILED(hr = device_auth( impl, deviceTicket, &deviceToken ))) goto cleanup;
    if (FAILED(hr = sisu_auth( impl, userTicket, deviceToken, &auth ))) goto cleanup;
    if (FAILED(hr = http_request( L"GET", L"https://title.mgt.xboxlive.com/titles/current/endpoints", NULL, auth, ACCEPT_JSON, &currentBuffer, &size ))) goto cleanup;
    if (FAILED(hr = load_endpoints( impl, currentBuffer, size ))) goto cleanup;

cleanup:
    if (currentBuffer) free( currentBuffer );
    if (defaultBuffer) free( defaultBuffer );
    if (deviceTicket) free( deviceTicket );
    if (deviceToken) free( deviceToken );
    if (userTicket) free( userTicket );
    if (auth) free( auth );
    return S_OK;
}

static HRESULT WINAPI user_GetEndpointInfo( IUser *iface, const char *url, struct endpoint *info )
{
    URL_COMPONENTSA uc = { .dwStructSize = sizeof(URL_COMPONENTSA), .dwSchemeLength = -1, .dwHostNameLength = -1, .dwUrlPathLength = -1 };
    struct XUser *impl = impl_from_IUser( iface );

    TRACE( "iface %p, url %s, info %p.\n", iface, debugstr_a( url ), info );

    if (!InternetCrackUrlA( url, 0, 0, &uc )) return HRESULT_FROM_WIN32( GetLastError() );
    TRACE( "scheme %s, hostName %s, path %s.\n", debugstr_an( uc.lpszScheme, uc.dwSchemeLength ), debugstr_an( uc.lpszHostName, uc.dwHostNameLength ), debugstr_an( uc.lpszUrlPath, uc.dwUrlPathLength ) );

    for (UINT32 i = 0; i < impl->endpointsLen; i++)
    {
        if ((impl->endpoints[i].wildcard ? SymMatchString( uc.lpszHostName, impl->endpoints[i].host, FALSE )
             : !strncmp( uc.lpszHostName, impl->endpoints[i].host, max( uc.dwHostNameLength, strlen( impl->endpoints[i].host ) ) ) ) &&
            !strncmp( uc.lpszScheme, impl->endpoints[i].protocol, max( uc.dwSchemeLength, strlen( impl->endpoints[i].protocol ) ) ) &&
            (impl->endpoints[i].path ? !strncmp( uc.lpszUrlPath, impl->endpoints[i].path, max( uc.dwUrlPathLength, strlen( impl->endpoints[i].path ) ) ) : 1 ))
        {
            *info = impl->endpoints[i];
            return S_OK;
        }
    }
    return E_FAIL;
}

static HRESULT WINAPI user_GetAuthorization( IUser *iface, const char *relyingParty, WCHAR **auth )
{
    static const char template[] = "{\"TokenType\":\"JWT\",\"Properties\":{\"SandboxId\":\"RETAIL\",\"ProofKey\":";
    IJsonObject *identity = NULL, *claims = NULL, *object = NULL;
    struct XUser *impl = impl_from_IUser( iface );
    const WCHAR *tokenBuffer, *uhsBuffer;
    HSTRING token = NULL, uhs = NULL;
    IJsonArray *xui = NULL;
    BYTE *buffer = NULL;
    SIZE_T bufferSize;
    char *body = NULL;
    HRESULT hr;

    TRACE( "iface %p, relyingParty %s, auth %p.\n", iface, debugstr_a( relyingParty ), auth );

    /* request xsts token */
    if (!(body = calloc( 1, ARRAY_SIZE( template ) + strlen( impl->proofKey ) + strlen( ",\"UserTokens\":[\"\"]},\"RelyingParty\":\"\"}" ) + strlen( impl->userToken ) + strlen( relyingParty ) )))
        return E_OUTOFMEMORY;
    strcpy( body, template );
    strcat( body, impl->proofKey );
    strcat( body, ",\"UserTokens\":[\"" );
    strcat( body, impl->userToken );
    strcat( body, "\"]},\"RelyingParty\":\"" );
    strcat( body, relyingParty );
    strcat( body, "\"}" );
    hr = http_request( L"POST", L"https://xsts.auth.xboxlive.com/xsts/authorize", body, NULL, ACCEPT_JSON, &buffer, &bufferSize );
    if (FAILED(hr)) return hr;

    /* construct auth header from user hash and token */
    hr = parse_json( (char *)buffer, bufferSize, &object );
    if (FAILED(hr)) return hr;
    if (FAILED(hr = get_json_string( object, L"Token", &token ))) goto cleanup;
    tokenBuffer = WindowsGetStringRawBuffer( token, NULL );
    if (FAILED(hr = get_json_object( object, L"DisplayClaims", &claims ))) goto cleanup;
    if (FAILED(hr = get_json_array( claims, L"xui", &xui ))) goto cleanup;
    if (FAILED(hr = IJsonArray_GetObjectAt( xui, 0, &identity ))) goto cleanup;
    if (FAILED(hr = get_json_string( identity, L"uhs", &uhs ))) goto cleanup;
    uhsBuffer = WindowsGetStringRawBuffer( uhs, NULL );
    if (!(*auth = calloc( wcslen( L"XBL3.0 x=;" ) + wcslen( uhsBuffer ) + wcslen( tokenBuffer ) + 1, sizeof(WCHAR) )))
    {
        hr = E_OUTOFMEMORY;
        goto cleanup;
    }
    wcscpy( *auth, L"XBL3.0 x=" );
    wcscat( *auth, uhsBuffer );
    wcscat( *auth, L";" );
    wcscat( *auth, tokenBuffer );

cleanup:
    if (identity) IJsonObject_Release( identity );
    if (claims) IJsonObject_Release( claims );
    if (object) IJsonObject_Release( object );
    if (token) WindowsDeleteString( token );
    if (uhs) WindowsDeleteString( uhs );
    if (xui) IJsonArray_Release( xui );
    if (buffer) free( buffer );
    if (body) free( body );
    return hr;
}

static HRESULT WINAPI user_GetSignature( IUser *iface, UINT32 version, const char *method, const char *url, const char *auth, UINT32 bodySize, const void *body, char signature[104] )
{
    FIXME( "iface %p, version %u, method %s, url %s, auth %s, bodySize %u, body %p, signature %p stub!\n", iface, version, debugstr_a( method ), debugstr_a( url ), debugstr_a( auth ), bodySize, body, signature );
    return E_NOTIMPL;
}

static const struct IUserVtbl user_vtbl =
{
    NULL,
    user_AddRef,
    user_Release,
    /* IUser methods */
    user_Initialize,
    user_GetEndpointInfo,
    user_GetAuthorization,
    user_GetSignature,
};

struct x_user
{
    IXUserImpl6 IXUserImpl6_iface;
    IXUserGamertagImpl IXUserGamertagImpl_iface;
    IXUserDeviceImpl2 IXUserDeviceImpl2_iface;
    LONG ref;
};

static inline struct x_user *impl_from_IXUserImpl6( IXUserImpl6 *iface )
{
    return CONTAINING_RECORD( iface, struct x_user, IXUserImpl6_iface );
}

static HRESULT WINAPI x_user_QueryInterface( IXUserImpl6 *iface, REFIID iid, void **out )
{
    struct x_user *impl = impl_from_IXUserImpl6( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown    ) ||
        IsEqualGUID( iid, &IID_IXUserImpl  ) ||
        IsEqualGUID( iid, &IID_IXUserImpl2 ) ||
        IsEqualGUID( iid, &IID_IXUserImpl3 ) ||
        IsEqualGUID( iid, &IID_IXUserImpl4 ) ||
        IsEqualGUID( iid, &IID_IXUserImpl5 ) ||
        IsEqualGUID( iid, &IID_IXUserImpl6 ))
    {
        IXUserImpl6_AddRef( *out = &impl->IXUserImpl6_iface );
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_IXUserGamertagImpl ))
    {
        IXUserGamertagImpl_AddRef( *out = &impl->IXUserGamertagImpl_iface );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI x_user_AddRef( IXUserImpl6 *iface )
{
    struct x_user *impl = impl_from_IXUserImpl6( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI x_user_Release( IXUserImpl6 *iface )
{
    struct x_user *impl = impl_from_IXUserImpl6( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p decreasing refcount to %lu.\n", iface, ref );
    return ref;
}

static HRESULT WINAPI x_user_XUserDuplicateHandle( IXUserImpl6 *iface, XUserHandle handle, XUserHandle *duplicatedHandle )
{
    TRACE( "iface %p, handle %p, duplicatedHandle %p.\n", iface, handle, duplicatedHandle );
    IUser_AddRef( &handle->IUser_iface );
    *duplicatedHandle = handle;
    return S_OK;
}

static void WINAPI x_user_XUserCloseHandle( IXUserImpl6 *iface, XUserHandle user )
{
    TRACE( "iface %p, user %p.\n", iface, user );
    IUser_Release( &user->IUser_iface );
}

static INT32 WINAPI x_user_XUserCompare( IXUserImpl6 *iface, XUserHandle user1, XUserHandle user2 )
{
    FIXME( "iface %p, user1 %p, user2 %p stub!\n", iface, user1, user2 );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetMaxUsers( IXUserImpl6 *iface, UINT32 *maxUsers )
{
    TRACE( "iface %p, maxUsers %p.\n", iface, maxUsers );
    *maxUsers = 1;
    return S_OK;
}

struct XUserAddContext
{
    XUserAddOptions options;
    XUserHandle user;
};

static HRESULT WINAPI XUserAddProvider( XAsyncOp op, const XAsyncProviderData *data )
{
    struct XUserAddContext *context;
    IXThreadingImpl *xthreading;
    HRESULT hr;

    TRACE( "op %d, data %p.\n", op, data );

    if (FAILED(hr = QueryApiImpl( &CLSID_XThreadingImpl, &IID_IXThreadingImpl, (void **)&xthreading ))) return hr;
    context = (struct XUserAddContext *)data->context;

    switch (op)
    {
        case XAsyncOp_Begin:
            hr = IXThreadingImpl_XAsyncSchedule( xthreading, data->async, 0 );
            break;

        case XAsyncOp_GetResult:
            memcpy( data->buffer, &context->user, sizeof(XUserHandle) );
            break;

        case XAsyncOp_DoWork:
            if (!(context->user = calloc( 1, sizeof(*context->user) )))
            {
                hr = E_OUTOFMEMORY;
                goto complete;
            }
            context->user->IUser_iface.lpVtbl = &user_vtbl;
            context->user->ref = 1;
            hr = IUser_Initialize( &context->user->IUser_iface, context->options );

        complete:
            IXThreadingImpl_XAsyncComplete( xthreading, data->async, hr, SUCCEEDED(hr) ? sizeof(XUserHandle) : 0 );
            if (FAILED(hr) && context->user) IUser_Release( &context->user->IUser_iface );
            hr = S_OK;
            break;

        case XAsyncOp_Cleanup:
            free( context );
            break;

        case XAsyncOp_Cancel:
            break;
    }

    IXThreadingImpl_Release( xthreading );
    return hr;
}

static HRESULT WINAPI x_user_XUserAddAsync( IXUserImpl6 *iface, XUserAddOptions options, XAsyncBlock *async )
{
    struct XUserAddContext *context;
    IXThreadingImpl *xthreading;
    HRESULT hr;

    TRACE( "iface %p, options %d, async %p.\n", iface, options, async );

    if (!async) return E_POINTER;
    if (FAILED(hr = QueryApiImpl( &CLSID_XThreadingImpl, &IID_IXThreadingImpl, (void **)&xthreading ))) return hr;
    if (!(context = calloc( 1, sizeof(*context) )))
    {
        IXThreadingImpl_Release( xthreading );
        return E_OUTOFMEMORY;
    }

    context->options = options;
    hr = IXThreadingImpl_XAsyncBegin( xthreading, async, context, NULL, "XUserAddAsync", XUserAddProvider );
    IXThreadingImpl_Release( xthreading );
    if (FAILED(hr)) free( context );
    return hr;
}

static HRESULT WINAPI x_user_XUserAddResult( IXUserImpl6 *iface, XAsyncBlock *async, XUserHandle *newUser )
{
    IXThreadingImpl *xthreading;
    HRESULT hr;

    TRACE( "iface %p, async %p, newUser %p.\n", iface, async, newUser );

    if (!async || !newUser) return E_POINTER;
    if (FAILED(hr = QueryApiImpl( &CLSID_XThreadingImpl, &IID_IXThreadingImpl, (void **)&xthreading ))) return hr;
    hr = IXThreadingImpl_XAsyncGetResult( xthreading, async, NULL, sizeof(*newUser), newUser, NULL );
    IXThreadingImpl_Release( xthreading );
    return hr;
}

static HRESULT WINAPI x_user_XUserGetLocalId( IXUserImpl6 *iface, XUserHandle user, XUserLocalId *userLocalId )
{
    FIXME( "iface %p, user %p, userLocalId %p stub!\n", iface, user, userLocalId );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserFindUserByLocalId( IXUserImpl6 *iface, XUserLocalId userLocalId, XUserHandle *handle )
{
    FIXME( "iface %p, userLocalId %p, handle %p stub!\n", iface, &userLocalId, handle );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetId( IXUserImpl6 *iface, XUserHandle user, UINT64 *userId )
{
    TRACE( "iface %p, user %p, userId %p.\n", iface, user, userId );
    *userId = user->xuid;
    return S_OK;
}

static HRESULT WINAPI x_user_XUserFindUserById( IXUserImpl6 *iface, UINT64 userId, XUserHandle *handle )
{
    FIXME( "iface %p, userId %llu, handle %p stub!\n", iface, userId, handle );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetIsGuest( IXUserImpl6 *iface, XUserHandle user, BOOLEAN *isGuest )
{
    FIXME( "iface %p, user %p, isGuest %p stub!\n", iface, user, isGuest );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetState( IXUserImpl6 *iface, XUserHandle user, XUserState *state )
{
    FIXME( "iface %p, user %p, state %p stub!\n", iface, user, state );
    return E_NOTIMPL;
}

static HRESULT WINAPI __PADDING__( IXUserImpl6 *iface )
{
    WARN( "iface %p padding function called! It's unknown what this function does.\n", iface );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetGamerPictureAsync( IXUserImpl6 *iface, XUserHandle user, XUserGamerPictureSize pictureSize, XAsyncBlock *async )
{
    FIXME( "iface %p, user %p, pictureSize %d, async %p stub!\n", iface, user, pictureSize, async );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetGamerPictureResultSize( IXUserImpl6 *iface, XAsyncBlock *async, SIZE_T *bufferSize )
{
    FIXME( "iface %p, async %p, bufferSize %p stub!\n", iface, async, bufferSize );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetGamerPictureResult( IXUserImpl6 *iface, XAsyncBlock *async, SIZE_T bufferSize, void *buffer, SIZE_T *bufferUsed )
{
    FIXME( "iface %p, async %p, bufferSize %Iu, buffer %p, bufferUsed %p stub!\n", iface, async, bufferSize, buffer, bufferUsed );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetAgeGroup( IXUserImpl6 *iface, XUserHandle user, XUserAgeGroup *ageGroup )
{
    FIXME( "iface %p, user %p, ageGroup %p stub!\n", iface, user, ageGroup );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserCheckPrivilege( IXUserImpl6 *iface, XUserHandle user, XUserPrivilegeOptions options, XUserPrivilege privilege, BOOLEAN *hasPrivilege, XUserPrivilegeDenyReason *reason )
{
    FIXME( "iface %p, user %p, options %d, privilege %d, hasPrivilege %p, reason %p stub!\n", iface, user, options, privilege, hasPrivilege, reason );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserResolvePrivilegeWithUiAsync( IXUserImpl6 *iface, XUserHandle user, XUserPrivilegeOptions options, XUserPrivilege privilege, XAsyncBlock *async )
{
    FIXME( "iface %p, user %p, options %d, privilege %d, async %p stub!\n", iface, user, options, privilege, async );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserResolvePrivilegeWithUiResult( IXUserImpl6 *iface, XAsyncBlock *async )
{
    FIXME( "iface %p, async %p stub!\n", iface, async );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetTokenAndSignatureAsync( IXUserImpl6 *iface, XUserHandle user, XUserGetTokenAndSignatureOptions options, const char *method, const char *url, SIZE_T headerCount, const XUserGetTokenAndSignatureHttpHeader *headers, SIZE_T bodySize, const void *bodyBuffer, XAsyncBlock *async )
{
    FIXME( "iface %p, user %p, options %d, method %s, url %s, headerCount %Iu, headers %p, bodySize %Iu, bodyBuffer %p, async %p stub!\n", iface, user, options, debugstr_a( method ), debugstr_a( url ), headerCount, headers, bodySize, bodyBuffer, async );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetTokenAndSignatureResultSize( IXUserImpl6 *iface, XAsyncBlock *async, SIZE_T *bufferSize )
{
    FIXME( "iface %p, async %p, bufferSize %p stub!\n", iface, async, bufferSize );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetTokenAndSignatureResult( IXUserImpl6 *iface, XAsyncBlock *async, SIZE_T bufferSize, void *buffer, XUserGetTokenAndSignatureData **ptrToBuffer, SIZE_T *bufferUsed )
{
    FIXME( "iface %p, async %p, bufferSize %Iu, buffer %p, ptrToBuffer %p, bufferUsed %p stub!\n", iface, async, bufferSize, buffer, ptrToBuffer, bufferUsed );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetTokenAndSignatureUtf16Async( IXUserImpl6 *iface, XUserHandle user, XUserGetTokenAndSignatureOptions options, const WCHAR *method, const WCHAR *url, SIZE_T headerCount, const XUserGetTokenAndSignatureUtf16HttpHeader *headers, SIZE_T bodySize, const void *bodyBuffer, XAsyncBlock *async )
{
    FIXME( "iface %p, user %p, options %d, method %s, url %s, headerCount %Iu, headers %p, bodySize %Iu, bodyBuffer %p, async %p stub!\n", iface, user, options, debugstr_w( method ), debugstr_w( url ), headerCount, headers, bodySize, bodyBuffer, async );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetTokenAndSignatureUtf16ResultSize( IXUserImpl6 *iface, XAsyncBlock *async, SIZE_T *bufferSize )
{
    FIXME( "iface %p, async %p, bufferSize %p stub!\n", iface, async, bufferSize );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetTokenAndSignatureUtf16Result( IXUserImpl6 *iface, XAsyncBlock *async, SIZE_T bufferSize, void *buffer, XUserGetTokenAndSignatureUtf16Data **ptrToBuffer, SIZE_T *bufferUsed )
{
    FIXME( "iface %p, async %p, bufferSize %Iu, buffer %p, ptrToBuffer %p, bufferUsed %p stub!\n", iface, async, bufferSize, buffer, ptrToBuffer, bufferUsed );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserResolveIssueWithUiAsync( IXUserImpl6 *iface, XUserHandle user, const char *url, XAsyncBlock *async )
{
    FIXME( "iface %p, user %p, url %s, async %p stub!\n", iface, user, debugstr_a( url ), async );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserResolveIssueWithUiResult( IXUserImpl6 *iface, XAsyncBlock *async )
{
    FIXME( "iface %p, async %p stub!\n", iface, async );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserResolveIssueWithUiUtf16Async( IXUserImpl6 *iface, XUserHandle user, const WCHAR *url, XAsyncBlock *async )
{
    FIXME( "iface %p, user %p, url %s, async %p stub!\n", iface, user, debugstr_w( url ), async );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserResolveIssueWithUiUtf16Result( IXUserImpl6 *iface, XAsyncBlock *async )
{
    FIXME( "iface %p, async %p stub!\n", iface, async );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserRegisterForChangeEvent( IXUserImpl6 *iface, XTaskQueueHandle queue, void *context, XUserChangeEventCallback *callback, XTaskQueueRegistrationToken *token )
{
    FIXME( "iface %p, queue %p, context %p, callback %p, token %p stub!\n", iface, queue, context, callback, token );
    return E_NOTIMPL;
}

static BOOLEAN WINAPI x_user_XUserUnregisterForChangeEvent( IXUserImpl6 *iface, XTaskQueueRegistrationToken token, BOOLEAN wait )
{
    FIXME( "iface %p, token %p, wait %d stub!\n", iface, &token, wait );
    return FALSE;
}

static HRESULT WINAPI x_user_XUserGetSignOutDeferral( IXUserImpl6 *iface, XUserSignOutDeferralHandle *deferral )
{
    TRACE( "iface %p, deferral %p.\n", iface, deferral );
    *deferral = NULL;
    return E_GAMEUSER_DEFERRAL_NOT_AVAILABLE;
}

static void WINAPI x_user_XUserCloseSignOutDeferralHandle( IXUserImpl6 *iface, XUserSignOutDeferralHandle deferral )
{
    TRACE( "iface %p, deferral %p.\n", iface, deferral );
}

static HRESULT WINAPI x_user_XUserAddByIdWithUiAsync( IXUserImpl6 *iface, UINT64 userId, XAsyncBlock *async )
{
    FIXME( "iface %p, userId %llu, async %p stub!\n", iface, userId, async );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserAddByIdWithUiResult( IXUserImpl6 *iface, XAsyncBlock *async, XUserHandle *newUser )
{
    FIXME( "iface %p, async %p, newUser %p stub!\n", iface, async, newUser );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetMsaTokenSilentlyAsync( IXUserImpl6 *iface, XUserHandle user, XUserGetMsaTokenSilentlyOptions options, const char *scope, XAsyncBlock *async )
{
    FIXME( "iface %p, user %p, options %u, scope %s, async %p stub!\n", iface, user, options, debugstr_a( scope ), async );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetMsaTokenSilentlyResult( IXUserImpl6 *iface, XAsyncBlock *async, SIZE_T resultTokenSize, char *resultToken, SIZE_T *resultTokenUsed )
{
    FIXME( "iface %p, async %p, resultTokenSize %Iu, resultToken %p, resultTokenUsed %p stub!\n", iface, async, resultTokenSize, resultToken, resultTokenUsed );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserGetMsaTokenSilentlyResultSize( IXUserImpl6 *iface, XAsyncBlock *async, SIZE_T *tokenSize )
{
    FIXME( "iface %p, async %p, tokenSize %p stub!\n", iface, async, tokenSize );
    return E_NOTIMPL;
}

static BOOLEAN WINAPI x_user_XUserIsStoreUser( IXUserImpl6 *iface, XUserHandle user )
{
    FIXME( "iface %p, user %p stub!\n", iface, user );
    return TRUE;
}

static HRESULT WINAPI x_user_XUserPlatformRemoteConnectSetEventHandlers( IXUserImpl6 *iface, XTaskQueueHandle queue, XUserPlatformRemoteConnectEventHandlers *handlers )
{
    FIXME( "iface %p, queue %p, handlers %p stub!\n", iface, queue, handlers );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserPlatformRemoteConnectCancelPrompt( IXUserImpl6 *iface, XUserPlatformOperation operation )
{
    FIXME( "iface %p, operation %p stub!\n", iface, operation );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserPlatformSpopPromptSetEventHandlers( IXUserImpl6 *iface, XTaskQueueHandle queue, XUserPlatformSpopPromptEventHandler *handler, void *context )
{
    FIXME( "iface %p, queue %p, handler %p, context %p stub!\n", iface, queue, handler, context );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserPlatformSpopPromptComplete( IXUserImpl6 *iface, XUserPlatformOperation operation, XUserPlatformOperationResult result )
{
    FIXME( "iface %p, operation %p, result %d stub!\n", iface, operation, result );
    return E_NOTIMPL;
}

static BOOLEAN WINAPI x_user_XUserIsSignOutPresent( IXUserImpl6 *iface )
{
    TRACE( "iface %p.\n", iface );
    return FALSE;
}

static HRESULT WINAPI x_user_XUserSignOutAsync( IXUserImpl6 *iface, XUserHandle user, XAsyncBlock *async )
{
    FIXME( "iface %p, user %p, async %p stub!\n", iface, user, async );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_XUserSignOutResult( IXUserImpl6 *iface, XAsyncBlock *async )
{
    FIXME( "iface %p, async %p stub!\n", iface, async );
    return E_NOTIMPL;
}

static const struct IXUserImpl6Vtbl x_user_vtbl =
{
    x_user_QueryInterface,
    x_user_AddRef,
    x_user_Release,
    /* IXUserImpl methods */
    x_user_XUserDuplicateHandle,
    x_user_XUserCloseHandle,
    x_user_XUserCompare,
    x_user_XUserGetMaxUsers,
    x_user_XUserAddAsync,
    x_user_XUserAddResult,
    x_user_XUserGetLocalId,
    x_user_XUserFindUserByLocalId,
    x_user_XUserGetId,
    x_user_XUserFindUserById,
    x_user_XUserGetIsGuest,
    x_user_XUserGetState,
    __PADDING__,
    x_user_XUserGetGamerPictureAsync,
    x_user_XUserGetGamerPictureResultSize,
    x_user_XUserGetGamerPictureResult,
    x_user_XUserGetAgeGroup,
    x_user_XUserCheckPrivilege,
    x_user_XUserResolvePrivilegeWithUiAsync,
    x_user_XUserResolvePrivilegeWithUiResult,
    x_user_XUserGetTokenAndSignatureAsync,
    x_user_XUserGetTokenAndSignatureResultSize,
    x_user_XUserGetTokenAndSignatureResult,
    x_user_XUserGetTokenAndSignatureUtf16Async,
    x_user_XUserGetTokenAndSignatureUtf16ResultSize,
    x_user_XUserGetTokenAndSignatureUtf16Result,
    x_user_XUserResolveIssueWithUiAsync,
    x_user_XUserResolveIssueWithUiResult,
    x_user_XUserResolveIssueWithUiUtf16Async,
    x_user_XUserResolveIssueWithUiUtf16Result,
    x_user_XUserRegisterForChangeEvent,
    x_user_XUserUnregisterForChangeEvent,
    x_user_XUserGetSignOutDeferral,
    x_user_XUserCloseSignOutDeferralHandle,
    /* IXUserImpl2 methods */
    x_user_XUserAddByIdWithUiAsync,
    x_user_XUserAddByIdWithUiResult,
    /* IXUserImpl3 methods */
    x_user_XUserGetMsaTokenSilentlyAsync,
    x_user_XUserGetMsaTokenSilentlyResult,
    x_user_XUserGetMsaTokenSilentlyResultSize,
    /* IXUserImpl4 methods */
    x_user_XUserIsStoreUser,
    /* IXUserImpl5 methods */
    x_user_XUserPlatformRemoteConnectSetEventHandlers,
    x_user_XUserPlatformRemoteConnectCancelPrompt,
    x_user_XUserPlatformSpopPromptSetEventHandlers,
    x_user_XUserPlatformSpopPromptComplete,
    /* IXUserImpl6 methods */
    x_user_XUserIsSignOutPresent,
    x_user_XUserSignOutAsync,
    x_user_XUserSignOutResult,
};

static inline struct x_user *impl_from_IXUserGamertagImpl( IXUserGamertagImpl *iface )
{
    return CONTAINING_RECORD( iface, struct x_user, IXUserGamertagImpl_iface );
}

static HRESULT WINAPI x_user_gamertag_QueryInterface( IXUserGamertagImpl *iface, REFIID riid, void **out )
{
    struct x_user *impl = impl_from_IXUserGamertagImpl( iface );
    return IXUserImpl6_QueryInterface( &impl->IXUserImpl6_iface, riid, out );
}

static ULONG WINAPI x_user_gamertag_AddRef( IXUserGamertagImpl *iface )
{
    struct x_user *impl = impl_from_IXUserGamertagImpl( iface );
    return IXUserImpl6_AddRef( &impl->IXUserImpl6_iface );
}

static ULONG WINAPI x_user_gamertag_Release( IXUserGamertagImpl *iface )
{
    struct x_user *impl = impl_from_IXUserGamertagImpl( iface );
    return IXUserImpl6_Release( &impl->IXUserImpl6_iface );
}

static HRESULT WINAPI x_user_gamertag_XUserGetGamertag( IXUserGamertagImpl *iface, XUserHandle user, XUserGamertagComponent gamertagComponent, SIZE_T gamertagSize, char *gamertag, SIZE_T *gamertagUsed )
{
    FIXME( "iface %p, user %p, gamertagComponent %d, gamertagSize %Iu, gamertag %p, gamertagUsed %p stub!\n", iface, user, gamertagComponent, gamertagSize, gamertag, gamertagUsed );
    return E_NOTIMPL;
}

static const struct IXUserGamertagImplVtbl x_user_gamertag_vtbl =
{
    x_user_gamertag_QueryInterface,
    x_user_gamertag_AddRef,
    x_user_gamertag_Release,
    /* IXUserGamertag methods */
    x_user_gamertag_XUserGetGamertag,
};

static inline struct x_user *impl_from_IXUserDeviceImpl2( IXUserDeviceImpl2 *iface )
{
    return CONTAINING_RECORD( iface, struct x_user, IXUserDeviceImpl2_iface );
}

static HRESULT WINAPI x_user_device_QueryInterface( IXUserDeviceImpl2 *iface, REFIID iid, void **out )
{
    struct x_user *impl = impl_from_IXUserDeviceImpl2( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown          ) ||
        IsEqualGUID( iid, &IID_IXUserDeviceImpl  ) ||
        IsEqualGUID( iid, &IID_IXUserDeviceImpl2 ))
    {
        IXUserDeviceImpl2_AddRef( *out = &impl->IXUserDeviceImpl2_iface );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI x_user_device_AddRef( IXUserDeviceImpl2 *iface )
{
    struct x_user *impl = impl_from_IXUserDeviceImpl2( iface );
    return IXUserImpl6_AddRef( &impl->IXUserImpl6_iface );
}

static ULONG WINAPI x_user_device_Release( IXUserDeviceImpl2 *iface )
{
    struct x_user *impl = impl_from_IXUserDeviceImpl2( iface );
    return IXUserImpl6_Release( &impl->IXUserImpl6_iface );
}

static HRESULT WINAPI x_user_device_XUserFindForDevice( IXUserDeviceImpl2 *iface, const APP_LOCAL_DEVICE_ID *deviceId, XUserHandle *handle )
{
    FIXME( "iface %p, deviceId %p, handle %p stub!\n", iface, deviceId, handle );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_device_XUserRegisterForDeviceAssociationChanged( IXUserDeviceImpl2 *iface, XTaskQueueHandle queue, void *context, XUserDeviceAssociationChangedCallback *callback, XTaskQueueRegistrationToken *token )
{
    FIXME( "iface %p, queue %p, context %p, callback %p, token %p stub!\n", iface, queue, context, callback, token );
    return E_NOTIMPL;
}

static BOOLEAN WINAPI x_user_device_XUserUnregisterForDeviceAssociationChanged( IXUserDeviceImpl2 *iface, XTaskQueueRegistrationToken token, BOOLEAN wait )
{
    FIXME( "iface %p, token %p, wait %d stub!\n", iface, &token, wait );
    return FALSE;
}

static HRESULT WINAPI x_user_device_XUserGetDefaultAudioEndpointUtf16( IXUserDeviceImpl2 *iface, XUserLocalId user, XUserDefaultAudioEndpointKind defaultAudioEndpointKind, SIZE_T endpointIdUtf16Count, WCHAR *endpointIdUtf16, SIZE_T *endpointIdUtf16Used )
{
    FIXME( "iface %p, user %p, defaultAudioEndpointKind %d, endpointIdUtf16Count %Iu, endpointIdUtf16 %p, endpointIdUtf16Used %p stub!\n", iface, &user, defaultAudioEndpointKind, endpointIdUtf16Count, endpointIdUtf16, endpointIdUtf16Used );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_device_XUserRegisterForDefaultAudioEndpointUtf16Changed( IXUserDeviceImpl2 *iface, XTaskQueueHandle queue, void *context, XUserDefaultAudioEndpointUtf16ChangedCallback *callback, XTaskQueueRegistrationToken *token )
{
    FIXME( "iface %p, queue %p, context %p, callback %p, token %p stub!\n", iface, queue, context, callback, token );
    return E_NOTIMPL;
}

static BOOLEAN WINAPI x_user_device_XUserUnregisterForDefaultAudioEndpointUtf16Changed( IXUserDeviceImpl2 *iface, XTaskQueueRegistrationToken token, BOOLEAN wait )
{
    FIXME( "iface %p, token %p, wait %d stub!\n", iface, &token, wait );
    return FALSE;
}

static HRESULT WINAPI x_user_device_XUserFindControllerForUserWithUiAsync( IXUserDeviceImpl2 *iface, XUserHandle user, XAsyncBlock *async )
{
    FIXME( "iface %p, user %p, async %p stub!\n", iface, user, async );
    return E_NOTIMPL;
}

static HRESULT WINAPI x_user_device_XUserFindControllerForUserWithUiResult( IXUserDeviceImpl2 *iface, XAsyncBlock *async, APP_LOCAL_DEVICE_ID *deviceId )
{
    FIXME( "iface %p, async %p, deviceId %p stub!\n", iface, async, deviceId );
    return E_NOTIMPL;
}

static const struct IXUserDeviceImpl2Vtbl x_user_device_vtbl =
{
    x_user_device_QueryInterface,
    x_user_device_AddRef,
    x_user_device_Release,
    /* IXUserDeviceImpl/IXUserDeviceImpl2 methods */
    x_user_device_XUserFindForDevice,
    x_user_device_XUserRegisterForDeviceAssociationChanged,
    x_user_device_XUserUnregisterForDeviceAssociationChanged,
    x_user_device_XUserGetDefaultAudioEndpointUtf16,
    x_user_device_XUserRegisterForDefaultAudioEndpointUtf16Changed,
    x_user_device_XUserUnregisterForDefaultAudioEndpointUtf16Changed,
    x_user_device_XUserFindControllerForUserWithUiAsync,
    x_user_device_XUserFindControllerForUserWithUiResult,
};

static struct x_user x_user =
{
    {&x_user_vtbl},
    {&x_user_gamertag_vtbl},
    {&x_user_device_vtbl},
    0,
};

IXUserImpl *x_user_impl = (IXUserImpl *)&x_user.IXUserImpl6_iface;
IXUserDeviceImpl *x_user_device_impl = (IXUserDeviceImpl *)&x_user.IXUserDeviceImpl2_iface;
