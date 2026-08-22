/*
 * StaticArray Implementation
 *  From https://github.com/microsoft/libHttpClient
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

#ifndef _STATICARRAY_H_
#define _STATICARRAY_H_

#include "../../private.h"

template <typename TData, UINT32 size>
class StaticArray
{
public:

    StaticArray()
    {
    }

    StaticArray(const StaticArray& other)
    {
        m_count = other.m_count;
        for(UINT32 idx = 0; idx < m_count; idx++)
        {
            m_array[idx] = other.m_array[idx];
        }
    }

    StaticArray& operator=(const StaticArray& other)
    {
        m_count = other.m_count;
        for(UINT32 idx = 0; idx < m_count; idx++)
        {
            m_array[idx] = other.m_array[idx];
        }
        return *this;
    }

    UINT32 count() { return m_count; }
    UINT32 capacity() { return ARRAYSIZE(m_array) - m_count; }
    void clear() { m_count = 0; }
    TData* data() { return m_array; }
    TData& operator[](size_t idx) { return m_array[idx]; }

    void append(const TData& data)
    {
        assert(m_count != ARRAYSIZE(m_array));
        m_array[m_count++] = data;
    }

    void removeAt(UINT32 idx)
    {
        if (idx == m_count - 1)
        {
            m_count --;
        }
        else
        {
            for (UINT32 i = idx + 1; i < m_count; i++)
            {
                m_array[i - 1] = m_array[i];
            }
            m_count--;
        }
    }

private:

    UINT32 m_count = 0;
    TData m_array[size];

};

#endif