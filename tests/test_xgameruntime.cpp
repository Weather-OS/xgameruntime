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

#include <gtest/gtest.h>

int add(int a, int b)
{
    return a + b;
}

TEST(AddTest, AddsPositiveNumbers)
{
    EXPECT_EQ(add(2, 3), 5);
}

TEST(AddTest, AddsNegativeNumbers)
{
    EXPECT_EQ(add(-2, -3), -5);
}

TEST(AddTest, AddsZero)
{
    EXPECT_EQ(add(42, 0), 44); // Intentional failure
}

TEST(AddTest, IntentionalFailure)
{
    FAIL() << "This test is supposed to fail";
}