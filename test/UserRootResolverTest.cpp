/*
 * UserRootResolverTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 */

#include "../lib/VCMIDirsOSXInternal.h"

#include <gtest/gtest.h>

namespace
{
using namespace VCMIDirs::detail;

TEST(UserRootResolverTest, absentValueUsesDefault)
{
	const auto result = resolveUserRoot(std::nullopt);
	EXPECT_TRUE(std::holds_alternative<UseDefault>(result));
	EXPECT_EQ(result.index(), 0);
}

TEST(UserRootResolverTest, emptyValueIsRejected)
{
	const auto result = resolveUserRoot(std::string_view{});
	ASSERT_TRUE(std::holds_alternative<Invalid>(result));
	EXPECT_EQ(std::get<Invalid>(result).reason, UserRootInvalidReason::Empty);
}

TEST(UserRootResolverTest, relativeValueIsRejected)
{
	const auto result = resolveUserRoot("state-root");
	ASSERT_TRUE(std::holds_alternative<Invalid>(result));
	EXPECT_EQ(std::get<Invalid>(result).reason, UserRootInvalidReason::NotAbsolute);
}

TEST(UserRootResolverTest, embeddedNulIsRejectedAsPathSyntax)
{
	constexpr std::string_view value("/state\0root", 11);
	const auto result = resolveUserRoot(value);
	ASSERT_TRUE(std::holds_alternative<Invalid>(result));
	EXPECT_EQ(std::get<Invalid>(result).reason, UserRootInvalidReason::PathSyntax);
}

TEST(UserRootResolverTest, absoluteValueProducesExactWritableLayout)
{
	const auto result = resolveUserRoot("/isolated/state");
	ASSERT_TRUE(std::holds_alternative<UseOverride>(result));
	const auto & layout = std::get<UseOverride>(result).layout;
	EXPECT_EQ(layout.root, "/isolated/state");
	EXPECT_EQ(layout.data, "/isolated/state/data");
	EXPECT_EQ(layout.config, "/isolated/state/config");
	EXPECT_EQ(layout.cache, "/isolated/state/cache");
	EXPECT_EQ(layout.logs, "/isolated/state/logs");
	EXPECT_EQ(layout.saves, "/isolated/state/data/Saves");
	EXPECT_EQ(layout.extracted, "/isolated/state/cache/extracted");
	EXPECT_EQ(result.index(), 1);
}

TEST(UserRootResolverTest, absoluteValueIsLexicallyNormalizedBeforeLayoutDerivation)
{
	const auto result = resolveUserRoot("/isolated//state/./session/../run");
	ASSERT_TRUE(std::holds_alternative<UseOverride>(result));
	const auto & layout = std::get<UseOverride>(result).layout;
	EXPECT_EQ(layout.root, "/isolated/state/run");
	EXPECT_EQ(layout.data, "/isolated/state/run/data");
	EXPECT_EQ(layout.extracted, "/isolated/state/run/cache/extracted");
}

TEST(UserRootResolverTest, resultTagsAreMutuallyExclusive)
{
	const auto defaultResult = resolveUserRoot(std::nullopt);
	const auto overrideResult = resolveUserRoot("/isolated/state");
	const auto invalidResult = resolveUserRoot("relative");

	EXPECT_TRUE(std::holds_alternative<UseDefault>(defaultResult));
	EXPECT_FALSE(std::holds_alternative<UseOverride>(defaultResult));
	EXPECT_FALSE(std::holds_alternative<Invalid>(defaultResult));
	EXPECT_TRUE(std::holds_alternative<UseOverride>(overrideResult));
	EXPECT_TRUE(std::holds_alternative<Invalid>(invalidResult));
}
}

int main(int argc, char * argv[])
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
