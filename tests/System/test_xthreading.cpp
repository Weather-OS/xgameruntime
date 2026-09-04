/*
 * Xbox Game runtime Library
 *  Tests
 *
 * Written by Weather
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

#include "../test_fixtures.h"

#include <windows.h>

#include <xasync.h>
#include <xasyncprovider.h>

TEST_F(XGameRuntimeTests, XThreading)
{
    /*
     * Test #1:
     *  Create a simple, asynchronous call
     */
    {
        struct testResultContext
        {
            INT result;
        };

        INT ctxWork = 2;
        XAsyncBlock testBlock{};
        testResultContext testResult{};

        testBlock.context = static_cast<PVOID>(&testResult);
        testBlock.callback = []( XAsyncBlock* asyncBlock )
        {
            // Completion routine
            auto ctx = static_cast<testResultContext*>(asyncBlock->context);
            SIZE_T written;

            EXPECT_HRESULT_SUCCEEDED( XAsyncGetResult( asyncBlock, nullptr, sizeof(INT), static_cast<PVOID>(&ctx->result), &written ) );
            EXPECT_EQ( written, sizeof(INT) );
        };

        EXPECT_HRESULT_SUCCEEDED( XAsyncBegin( &testBlock, static_cast<PVOID>(&ctxWork), nullptr, __FUNCTION__, []( XAsyncOp op, const XAsyncProviderData* data )
        {
            // Work routine
            INT *work;

            switch ( op )
            {
            case XAsyncOp::Begin:
                break;

            case XAsyncOp::Cancel:
                break;

            case XAsyncOp::Cleanup:
                break;

            case XAsyncOp::GetResult:
                RtlCopyMemory( data->buffer, data->context, sizeof(INT) );
                break;

            case XAsyncOp::DoWork:
                work = static_cast<INT *>(data->context);
                *work = *work * 2;
                XAsyncComplete( data->async, S_OK, sizeof(INT) );
                break;
            }
            return S_OK;
        } ) );

        EXPECT_HRESULT_SUCCEEDED( XAsyncSchedule( &testBlock, 0 ) );

        EXPECT_HRESULT_SUCCEEDED( XAsyncGetStatus( &testBlock, true ) );

        EXPECT_EQ( ctxWork, testResult.result );
        EXPECT_EQ( ctxWork, 4 );
    }
}
