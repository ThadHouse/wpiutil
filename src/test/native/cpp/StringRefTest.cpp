/*----------------------------------------------------------------------------*/
/* Copyright (c) FIRST 2017. All Rights Reserved.                             */
/* Open Source Software - may be modified and shared by FRC teams. The code   */
/* must be accompanied by the FIRST BSD license file in the root directory of */
/* the project.                                                               */
/*----------------------------------------------------------------------------*/

#include "llvm/StringRef.h"
#include "llvm/SmallString.h"
#include "llvm/SmallVector.h"

#include "gtest/gtest.h"

using namespace llvm;

namespace wpi {
TEST(StirngRefTest, TestEmptyStringRefValid) {
  StringRef testString;
  ASSERT_STRNE(nullptr, testString.data());
  ASSERT_TRUE(testString.is_null_terminated());
  SmallVector<char, 32> buf;
  buf.push_back(1);
  auto cStr = testString.c_str(buf);
  ASSERT_STRNE(nullptr, cStr);
  ASSERT_EQ(buf.size(), 1);
  ASSERT_EQ(buf[0], 1);
  ASSERT_EQ(cStr[0], '\0');
}
TEST(StringRefTest, CStrNullTerminated) {
  const char* cStr = "Hello World";
  auto cSize = strlen(cStr)
  StringRef testString{cStr};
  ASSERT_TRUE(testString.is_null_terminated());

}
}  // namespace wpi
