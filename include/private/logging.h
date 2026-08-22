/*
 * Copyright (c) 2025 Weather
 *
 * Permission is hereby granted, CoTaskMemFree of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef XGAMERUNTIME_LOGGING_H
#define XGAMERUNTIME_LOGGING_H

#include <unistd.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#include <processthreadsapi.h>
#define gettid() GetCurrentThreadId()
#else
#include <sys/syscall.h>
#include <unistd.h>
#ifdef SYS_gettid
#define gettid() syscall(SYS_gettid)
#else
#error "SYS_gettid unavailable on this system"
#endif
#endif

typedef enum _Log_Category
{
    LOG_CATEGORY_INFO = 0,
    LOG_CATEGORY_ERROR = 1,
    LOG_CATEGORY_WARNING = 2,
    LOG_CATEGORY_FIXME = 3,
    LOG_CATEGORY_TRACE = 4,
} Log_Category;

static LPCSTR LogCategoryNames[] =
{
    /* LOG_CATEGORY_INFO    */ "info",
    /* LOG_CATEGORY_ERROR   */ "err",
    /* LOG_CATEGORY_WARNING */ "warn",
    /* LOG_CATEGORY_FIXME   */ "fixme",
    /* LOG_CATEGORY_TRACE   */ "trace",
};

#define DEFAULT_LOG_LEVEL LOG_CATEGORY_FIXME

typedef struct _Log_Token
{
    LPCSTR Module;
    Log_Category Category;
} Log_Token;

HRESULT InitializeLogging();

void XGameRuntime_DEBUG( IN Log_Category category, IN pid_t threadId, IN LPCSTR module, IN LPCSTR function, IN LPCSTR fmt, ... );

#define __FILENAME__ \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : \
    strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : \
    __FILE__)

#define INFO(message, ...) \
    XGameRuntime_DEBUG( LOG_CATEGORY_INFO, gettid(), __FILENAME__, __FUNCTION__, message, ##__VA_ARGS__)

#define WARN(message, ...) \
    XGameRuntime_DEBUG( LOG_CATEGORY_WARNING, gettid(), __FILENAME__, __FUNCTION__, message, ##__VA_ARGS__)

#define ERROR(message, ...) \
    XGameRuntime_DEBUG( LOG_CATEGORY_ERROR, gettid(), __FILENAME__, __FUNCTION__, message, ##__VA_ARGS__)

#define FIXME(message, ...) \
    XGameRuntime_DEBUG( LOG_CATEGORY_FIXME, gettid(), __FILENAME__, __FUNCTION__, message, ##__VA_ARGS__)

#define TRACE(message, ...) \
    XGameRuntime_DEBUG( LOG_CATEGORY_TRACE, gettid(), __FILENAME__, __FUNCTION__, message, ##__VA_ARGS__)

// Critical Exceptions
// TODO: Implement unified throw routine
#define throw_NullPtrException() \
    {                                                           \
        ERROR("A critical null pointer exception occured!\n");  \
        exit(125);                                              \
    }                                                           \

LPCSTR debugstr_uuid( IN REFIID uuid );

#ifdef __cplusplus
} // extern "C"

#include <stdexcept>
#include <string>

struct Exception final : std::runtime_error
{
    HRESULT status;
    std::string msg;

    explicit Exception( HRESULT s ): std::runtime_error( "Unhandled Exception: " + std::to_string(s) ), status(s)
    {
        ERROR( "Exception %d within C++ code.\n", status );
    }

    explicit Exception( HRESULT s, const std::string &message ): std::runtime_error( "Unhandled Exception: " + std::to_string(s) + " with message " + message ), status(s), msg(message)
    {
        ERROR( "Exception %d within C++ code with message %s.\n", status, message.c_str() );
    }
};

#define check_hr_( hr ){ HRESULT st = hr; if ( FAILED( hr ) ) throw Exception( st ); }

#endif

#endif //XGAMERUNTIME_LOGGING_H
