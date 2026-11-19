#include <gtest/gtest.h>

#include <Common.hpp>

TEST(WriterTests, CyclicDependencyDoesntSave) {
	Pumpkin::AE::Writer w;
	Leaky1 l1{ std::make_shared<Leaky2>() };
	Leaky2 l2{ std::make_shared<Leaky1>(l1) };
	w.Add(0, 0, l1);
	w.Add(1, 1, l2);
	EXPECT_FALSE(w.Save("UnitTests/Leaky.assets"));
}
