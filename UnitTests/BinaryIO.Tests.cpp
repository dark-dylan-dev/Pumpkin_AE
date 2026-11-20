#include <gtest/gtest.h>

#include <Common.hpp>
#include <TestUtils.hpp>

using Pumpkin::AE::BinaryReader;
using Pumpkin::AE::BinaryWriter;

// ===========================================================================
// BinaryWriter: byte layout
// ===========================================================================

TEST(BinaryWriterTests, FreshWriterIsOkAndEmpty) {
	BinaryWriter w;
	EXPECT_TRUE(w.ok());
	EXPECT_TRUE(w.take().empty());
}

TEST(BinaryWriterTests, IntegersAreLittleEndian) {
	BinaryWriter w;
	w.write(std::uint8_t{ 0xAB });
	w.write(std::uint16_t{ 0x0102 });
	w.write(std::uint32_t{ 0x01020304 });
	w.write(std::uint64_t{ 0x0102030405060708 });
	EXPECT_EQ(w.take(), (Bytes{ 0xAB,
	                            0x02, 0x01,
	                            0x04, 0x03, 0x02, 0x01,
	                            0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01 }));
}

TEST(BinaryWriterTests, NegativeIntegersAreTwosComplement) {
	BinaryWriter w;
	w.write(std::int32_t{ -2 });
	w.write(std::int8_t{ -1 });
	EXPECT_EQ(w.take(), (Bytes{ 0xFE, 0xFF, 0xFF, 0xFF, 0xFF }));
}

TEST(BinaryWriterTests, BoolIsOneByte) {
	BinaryWriter w;
	w.write(true);
	w.write(false);
	EXPECT_EQ(w.take(), (Bytes{ 1, 0 }));
}

TEST(BinaryWriterTests, FloatsAreIEEE754Bits) {
	BinaryWriter w;
	w.write(1.0f);
	w.write(1.0);
	EXPECT_EQ(w.take(), (Bytes{ 0x00, 0x00, 0x80, 0x3F,
	                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x3F }));
}

TEST(BinaryWriterTests, EnumUsesItsUnderlyingType) {
	BinaryWriter w;
	w.write(Color::Blue);   // uint16_t, 513 = 0x0201
	w.write(Signed::Low);   // int, -5
	EXPECT_EQ(w.take(), (Bytes{ 0x01, 0x02, 0xFB, 0xFF, 0xFF, 0xFF }));
}

TEST(BinaryWriterTests, StringHasLengthPrefixAndNoTerminator) {
	BinaryWriter w;
	w.write(std::string("AB"));
	EXPECT_EQ(w.take(), (Bytes{ 2, 0, 0, 0, 'A', 'B' }));
}

TEST(BinaryWriterTests, EmptyStringIsOnlyTheLength) {
	BinaryWriter w;
	w.write(std::string());
	EXPECT_EQ(w.take(), (Bytes{ 0, 0, 0, 0 }));
}

TEST(BinaryWriterTests, StringWithEmbeddedNulKeepsEveryByte) {
	BinaryWriter w;
	w.write(std::string("a\0b", 3));
	EXPECT_EQ(w.take(), (Bytes{ 3, 0, 0, 0, 'a', 0, 'b' }));
}

TEST(BinaryWriterTests, VectorHasCountPrefix) {
	BinaryWriter w;
	w.write(std::vector<std::uint16_t>{ 1, 2 });
	EXPECT_EQ(w.take(), (Bytes{ 2, 0, 0, 0, 1, 0, 2, 0 }));
}

TEST(BinaryWriterTests, EmptyVectorIsOnlyTheCount) {
	BinaryWriter w;
	w.write(std::vector<int>{});
	EXPECT_EQ(w.take(), (Bytes{ 0, 0, 0, 0 }));
}

TEST(BinaryWriterTests, NestedVectorsAreEncodedRecursively) {
	BinaryWriter w;
	w.write(std::vector<std::vector<std::uint8_t>>{ { 9 }, {} });
	EXPECT_EQ(w.take(), (Bytes{ 2, 0, 0, 0,
	                            1, 0, 0, 0, 9,
	                            0, 0, 0, 0 }));
}

TEST(BinaryWriterTests, PairAndTupleHaveNoPrefix) {
	BinaryWriter w;
	w.write(std::pair<std::uint8_t, std::uint8_t>{ 1, 2 });
	w.write(std::tuple<std::uint8_t, std::uint16_t>{ 3, 4 });
	EXPECT_EQ(w.take(), (Bytes{ 1, 2, 3, 4, 0 }));
}

TEST(BinaryWriterTests, ReflectableWritesFieldsInReflectOrder) {
	BinaryWriter w;
	w.write(Position{ 1, 2 });
	EXPECT_EQ(w.take(), (Bytes{ 1, 0, 0, 0, 2, 0, 0, 0 }));
}

TEST(BinaryWriterTests, ReflectableFieldsMayBeReflectable) {
	BinaryWriter w;
	w.write(Material{ "x", 1.0f });
	EXPECT_EQ(w.take(), (Bytes{ 1, 0, 0, 0, 'x', 0x00, 0x00, 0x80, 0x3F }));
}

// ===========================================================================
// BinaryWriter: pointers
// ===========================================================================

TEST(BinaryWriterTests, NullPointersAreASingleZeroTag) {
	BinaryWriter w;
	w.write(std::unique_ptr<Position>());
	w.write(std::shared_ptr<Position>());
	w.write(static_cast<Position*>(nullptr));
	EXPECT_TRUE(w.ok());
	EXPECT_EQ(w.take(), (Bytes{ 0, 0, 0 }));
}

TEST(BinaryWriterTests, NewPointerIsTagOneThenThePointeeInline) {
	BinaryWriter w;
	w.write(std::make_unique<Position>(Position{ 7, 8 }));
	EXPECT_EQ(w.take(), (Bytes{ 1, 7, 0, 0, 0, 8, 0, 0, 0 }));
}

TEST(BinaryWriterTests, SharedPointerSeenTwiceBecomesAReference) {
	const auto p = std::make_shared<Position>(Position{ 7, 8 });
	BinaryWriter w;
	w.write(std::vector<std::shared_ptr<Position>>{ p, p });
	EXPECT_TRUE(w.ok());
	EXPECT_EQ(w.take(), (Bytes{ 2, 0, 0, 0,                // count
	                            1, 7, 0, 0, 0, 8, 0, 0, 0, // new object, number 0
	                            2, 0, 0, 0, 0 }));         // reference to object 0
}

TEST(BinaryWriterTests, EachNewPointeeGetsTheNextNumber) {
	const auto p = std::make_shared<Position>(Position{ 1, 1 });
	const auto q = std::make_shared<Position>(Position{ 2, 2 });
	BinaryWriter w;
	w.write(std::vector<std::shared_ptr<Position>>{ p, q, q, p });
	EXPECT_TRUE(w.ok());
	const Bytes b = w.take();
	// count + (1 + 8) + (1 + 8) + (1 + 4) + (1 + 4)
	ASSERT_EQ(b.size(), 4u + 9u + 9u + 5u + 5u);
	EXPECT_EQ(b[22], 2); EXPECT_EQ(b[23], 1); // q again -> object 1
	EXPECT_EQ(b[27], 2); EXPECT_EQ(b[28], 0); // p again -> object 0
}

TEST(BinaryWriterTests, TwoEqualButDistinctObjectsAreNotMerged) {
	const auto p = std::make_shared<Position>(Position{ 1, 1 });
	const auto q = std::make_shared<Position>(Position{ 1, 1 });
	BinaryWriter w;
	w.write(std::vector<std::shared_ptr<Position>>{ p, q });
	EXPECT_EQ(w.take().size(), 4u + 9u + 9u); // two full copies, no reference
}

TEST(BinaryWriterTests, RawPointerToAnEarlierUniqueBecomesAReference) {
	std::pair<std::unique_ptr<Position>, Position*> pr{ std::make_unique<Position>(Position{ 3, 4 }), nullptr };
	pr.second = pr.first.get();
	BinaryWriter w;
	w.write(pr);
	EXPECT_TRUE(w.ok());
	EXPECT_EQ(w.take(), (Bytes{ 1, 3, 0, 0, 0, 4, 0, 0, 0, 2, 0, 0, 0, 0 }));
}

TEST(BinaryWriterTests, SharedThenRawIsAllowed) {
	const auto sp = std::make_shared<Position>(Position{ 3, 4 });
	const std::pair<std::shared_ptr<Position>, Position*> pr{ sp, sp.get() };
	BinaryWriter w;
	w.write(pr);
	EXPECT_TRUE(w.ok());
}

TEST(BinaryWriterTests, RawThenSharedOnTheSameObjectFails) {
	// The raw pointer met the object first, so it is the "owner" and a shared_ptr can't share it.
	const auto sp = std::make_shared<Position>(Position{ 3, 4 });
	const std::pair<Position*, std::shared_ptr<Position>> pr{ sp.get(), sp };
	BinaryWriter w;
	w.write(pr);
	EXPECT_FALSE(w.ok());
}

TEST(BinaryWriterTests, RawThenUniqueOnTheSameObjectFails) {
	// A unique_ptr is never the second pointer to an object.
	auto* raw = new Position{ 1, 2 };
	const std::pair<Position*, std::unique_ptr<Position>> pr{ raw, std::unique_ptr<Position>(raw) };
	BinaryWriter w;
	w.write(pr);
	EXPECT_FALSE(w.ok());
}

TEST(BinaryWriterTests, RawPointerBackToTheRootFails) {
	SelfRef s;
	s.self = &s;
	BinaryWriter w;
	w.write(s);
	EXPECT_FALSE(w.ok());
}

TEST(BinaryWriterTests, RawPointerToAnotherObjectOfTheSameTypeIsFine) {
	SelfRef other;
	SelfRef s;
	s.self = &other;
	BinaryWriter w;
	w.write(s);
	EXPECT_TRUE(w.ok());
}

TEST(BinaryWriterTests, SharedCycleThroughASharedRootIsEncodable) {
	auto a = std::make_shared<Leaky1>();
	a->leaky = std::make_shared<Leaky2>();
	a->leaky->leaky = a; // a -> Leaky2 -> a
	BinaryWriter w;
	w.write(a);
	EXPECT_TRUE(w.ok());
	EXPECT_EQ(w.take(), (Bytes{ 1,          // a: new, object 0
	                            1,          // Leaky2: new, object 1
	                            2, 0, 0, 0, 0 })); // back to object 0
	a->leaky->leaky.reset(); // no leak
}

TEST(BinaryWriterTests, CycleBackToARootValueFails) {
	Leaky1 root;
	root.leaky = std::make_shared<Leaky2>();
	// Non-owning alias of the root: the root is not a shared_ptr, so it can't be shared.
	root.leaky->leaky = std::shared_ptr<Leaky1>(std::shared_ptr<void>(), &root);
	BinaryWriter w;
	w.write(root);
	EXPECT_FALSE(w.ok());
}

TEST(BinaryWriterTests, PointerChainAtTheLimitIsAccepted) {
	Node root;
	Node* tail = &root;
	for (std::uint32_t i = 0; i < Pumpkin::AE::kMaxPointerDepth; ++i) {
		tail->next = std::make_unique<Node>();
		tail = tail->next.get();
	}
	BinaryWriter w;
	w.write(root);
	EXPECT_TRUE(w.ok());
}

TEST(BinaryWriterTests, PointerChainBeyondTheLimitFails) {
	Node root;
	Node* tail = &root;
	for (std::uint32_t i = 0; i <= Pumpkin::AE::kMaxPointerDepth; ++i) {
		tail->next = std::make_unique<Node>();
		tail = tail->next.get();
	}
	BinaryWriter w;
	w.write(root);
	EXPECT_FALSE(w.ok());
}

TEST(BinaryWriterTests, FailureIsSticky) {
	SelfRef s;
	s.self = &s;
	BinaryWriter w;
	w.write(s);
	ASSERT_FALSE(w.ok());
	w.write(std::uint32_t{ 1 }); // a perfectly valid value afterwards
	EXPECT_FALSE(w.ok());
}

// ===========================================================================
// BinaryReader: primitives
// ===========================================================================

TEST(BinaryReaderTests, EmptyInputIsOkAndAtEnd) {
	const Bytes b;
	BinaryReader r(b);
	EXPECT_TRUE(r.ok());
	EXPECT_TRUE(r.atEnd());
}

TEST(BinaryReaderTests, ReadsLittleEndianIntegers) {
	const Bytes b{ 0xAB, 0x02, 0x01, 0x04, 0x03, 0x02, 0x01, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01 };
	BinaryReader r(b);
	EXPECT_EQ(r.u8(), 0xAB);
	EXPECT_EQ(r.u16(), 0x0102);
	EXPECT_EQ(r.u32(), 0x01020304u);
	EXPECT_EQ(r.u64(), 0x0102030405060708ull);
	EXPECT_TRUE(r.ok());
	EXPECT_TRUE(r.atEnd());
}

TEST(BinaryReaderTests, AtEndIsFalseWhileBytesRemain) {
	const Bytes b{ 1, 2 };
	BinaryReader r(b);
	r.u8();
	EXPECT_FALSE(r.atEnd());
	r.u8();
	EXPECT_TRUE(r.atEnd());
}

TEST(BinaryReaderTests, ReadingPastTheEndFailsAndReturnsZero) {
	const Bytes b{ 1, 2, 3 };
	BinaryReader r(b);
	EXPECT_EQ(r.u32(), 0u);
	EXPECT_FALSE(r.ok());
}

TEST(BinaryReaderTests, ReadingExactlyTheRemainingBytesSucceeds) {
	const Bytes b{ 1, 2, 3, 4 };
	BinaryReader r(b);
	EXPECT_EQ(r.u32(), 0x04030201u);
	EXPECT_TRUE(r.ok());
}

TEST(BinaryReaderTests, FailureIsSticky) {
	const Bytes b{ 1, 2, 3, 4, 5 };
	BinaryReader r(b);
	r.u64();                 // fails: only 5 bytes
	EXPECT_EQ(r.u8(), 0);    // there are bytes left, but the reader stays failed
	EXPECT_FALSE(r.ok());
}

TEST(BinaryReaderTests, BooleanAcceptsZeroAndOne) {
	const Bytes b{ 0, 1 };
	BinaryReader r(b);
	EXPECT_FALSE(r.boolean());
	EXPECT_TRUE(r.boolean());
	EXPECT_TRUE(r.ok());
}

TEST(BinaryReaderTests, BooleanRejectsOtherValues) {
	const Bytes b{ 2 };
	BinaryReader r(b);
	r.boolean();
	EXPECT_FALSE(r.ok());
}

TEST(BinaryReaderTests, ReadsFloatsFromIEEE754Bits) {
	const Bytes b{ 0x00, 0x00, 0x80, 0x3F,
	               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x3F };
	BinaryReader r(b);
	EXPECT_EQ(r.f32(), 1.0f);
	EXPECT_EQ(r.f64(), 1.0);
	EXPECT_TRUE(r.ok());
}

// ===========================================================================
// BinaryReader: strings and counts
// ===========================================================================

TEST(BinaryReaderTests, ReadsAString) {
	const Bytes b{ 2, 0, 0, 0, 'A', 'B' };
	BinaryReader r(b);
	EXPECT_EQ(r.str(), "AB");
	EXPECT_TRUE(r.ok());
	EXPECT_TRUE(r.atEnd());
}

TEST(BinaryReaderTests, ReadsAnEmptyString) {
	const Bytes b{ 0, 0, 0, 0 };
	BinaryReader r(b);
	EXPECT_EQ(r.str(), "");
	EXPECT_TRUE(r.ok());
}

TEST(BinaryReaderTests, StringLongerThanTheRemainingBytesFails) {
	const Bytes b{ 5, 0, 0, 0, 'A', 'B' };
	BinaryReader r(b);
	EXPECT_EQ(r.str(), "");
	EXPECT_FALSE(r.ok());
}

TEST(BinaryReaderTests, HugeStringLengthFailsWithoutAllocating) {
	const Bytes b{ 0xFF, 0xFF, 0xFF, 0xFF, 'A' };
	BinaryReader r(b);
	EXPECT_EQ(r.str(), "");
	EXPECT_FALSE(r.ok());
}

TEST(BinaryReaderTests, StringWithTruncatedLengthFails) {
	const Bytes b{ 1, 0 };
	BinaryReader r(b);
	r.str();
	EXPECT_FALSE(r.ok());
}

TEST(BinaryReaderTests, CountReturnsTheValueWhenItFits) {
	const Bytes b{ 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	BinaryReader r(b);
	EXPECT_EQ(r.count(4), 2u); // 2 * 4 bytes remain after the count itself
	EXPECT_TRUE(r.ok());
}

TEST(BinaryReaderTests, CountFailsWhenElementsCannotFit) {
	const Bytes b{ 3, 0, 0, 0, 0, 0, 0, 0 };
	BinaryReader r(b);
	EXPECT_EQ(r.count(4), 0u); // 3 * 4 > 4 bytes remaining
	EXPECT_FALSE(r.ok());
}

TEST(BinaryReaderTests, CountDoesNotOverflowOnHugeValues) {
	const Bytes b{ 0xFF, 0xFF, 0xFF, 0xFF };
	BinaryReader r(b);
	EXPECT_EQ(r.count(1u << 20), 0u);
	EXPECT_FALSE(r.ok());
}

// ===========================================================================
// BinaryReader: generic read()
// ===========================================================================

TEST(BinaryReaderTests, ReadsAnEnumFromItsUnderlyingType) {
	const Bytes b{ 0x01, 0x02, 0xFB, 0xFF, 0xFF, 0xFF };
	BinaryReader r(b);
	Color c = Color::Red;
	Signed s = Signed::High;
	r.read(c);
	r.read(s);
	EXPECT_TRUE(r.ok());
	EXPECT_EQ(c, Color::Blue);
	EXPECT_EQ(s, Signed::Low);
}

TEST(BinaryReaderTests, ReadsAReflectableType) {
	const Bytes b{ 1, 0, 0, 0, 2, 0, 0, 0 };
	BinaryReader r(b);
	Position p;
	r.read(p);
	EXPECT_TRUE(r.ok());
	EXPECT_EQ(p.x, 1);
	EXPECT_EQ(p.y, 2);
}

TEST(BinaryReaderTests, ReadsAVector) {
	const Bytes b{ 2, 0, 0, 0, 1, 0, 2, 0 };
	BinaryReader r(b);
	std::vector<std::uint16_t> v;
	r.read(v);
	EXPECT_TRUE(r.ok());
	EXPECT_EQ(v, (std::vector<std::uint16_t>{ 1, 2 }));
}

TEST(BinaryReaderTests, ReadingAVectorReplacesItsPreviousContent) {
	const Bytes b{ 1, 0, 0, 0, 9, 0, 0, 0 };
	BinaryReader r(b);
	std::vector<int> v{ 1, 2, 3, 4 };
	r.read(v);
	EXPECT_EQ(v, (std::vector<int>{ 9 }));
}

TEST(BinaryReaderTests, VectorWithAnImpossibleCountFails) {
	const Bytes b{ 0xFF, 0xFF, 0xFF, 0x7F, 1, 0, 0, 0 };
	BinaryReader r(b);
	std::vector<int> v;
	r.read(v);
	EXPECT_FALSE(r.ok());
	EXPECT_TRUE(v.empty());
}

TEST(BinaryReaderTests, VectorWithTruncatedElementsFails) {
	const Bytes b{ 2, 0, 0, 0, 1, 0, 0, 0 }; // announces 2 ints, only 1 present
	BinaryReader r(b);
	std::vector<int> v;
	r.read(v);
	EXPECT_FALSE(r.ok());
}

TEST(BinaryReaderTests, ReadsAPairAndATuple) {
	const Bytes b{ 1, 2, 3, 4, 0 };
	BinaryReader r(b);
	std::pair<std::uint8_t, std::uint8_t> p;
	std::tuple<std::uint8_t, std::uint16_t> t;
	r.read(p);
	r.read(t);
	EXPECT_TRUE(r.ok());
	EXPECT_EQ(p, (std::pair<std::uint8_t, std::uint8_t>{ 1, 2 }));
	EXPECT_EQ(std::get<0>(t), 3);
	EXPECT_EQ(std::get<1>(t), 4);
}

TEST(BinaryReaderTests, ReadsAStdList) {
	const Bytes b{ 2, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0 };
	BinaryReader r(b);
	std::list<int> l;
	r.read(l);
	EXPECT_TRUE(r.ok());
	EXPECT_EQ(l, (std::list<int>{ 1, 2 }));
}

// ===========================================================================
// BinaryReader: pointers
// ===========================================================================

TEST(BinaryReaderTests, NullTagGivesANullPointer) {
	const Bytes b{ 0 };
	BinaryReader r(b);
	std::unique_ptr<Position> p = std::make_unique<Position>();
	r.read(p);
	EXPECT_TRUE(r.ok());
	EXPECT_EQ(p, nullptr);
}

TEST(BinaryReaderTests, NewTagCreatesThePointee) {
	const Bytes b{ 1, 7, 0, 0, 0, 8, 0, 0, 0 };
	BinaryReader r(b);
	std::unique_ptr<Position> p;
	r.read(p);
	ASSERT_TRUE(r.ok());
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(p->x, 7);
	EXPECT_EQ(p->y, 8);
}

TEST(BinaryReaderTests, RawPointerReadsAnOwnedPointee) {
	const Bytes b{ 1, 7, 0, 0, 0, 8, 0, 0, 0 };
	BinaryReader r(b);
	Position* p = nullptr;
	r.read(p);
	ASSERT_TRUE(r.ok());
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(p->x, 7);
	delete p; // a raw pointer read as "new" is heap allocated: the caller owns it
}

TEST(BinaryReaderTests, UnknownPointerTagFails) {
	const Bytes b{ 3 };
	BinaryReader r(b);
	std::unique_ptr<Position> p;
	r.read(p);
	EXPECT_FALSE(r.ok());
	EXPECT_EQ(p, nullptr);
}

TEST(BinaryReaderTests, TruncatedPointeeFailsAndLeavesNoPointer) {
	const Bytes b{ 1, 7, 0, 0, 0 }; // Position needs 8 bytes
	BinaryReader r(b);
	std::unique_ptr<Position> p;
	r.read(p);
	EXPECT_FALSE(r.ok());
	EXPECT_EQ(p, nullptr);
}

TEST(BinaryReaderTests, SharedReferenceGivesTheSamePointer) {
	const Bytes b{ 1, 7, 0, 0, 0, 8, 0, 0, 0, 2, 0, 0, 0, 0 };
	BinaryReader r(b);
	std::pair<std::shared_ptr<Position>, std::shared_ptr<Position>> pr;
	r.read(pr);
	ASSERT_TRUE(r.ok());
	ASSERT_NE(pr.first, nullptr);
	EXPECT_EQ(pr.first, pr.second);
	EXPECT_GE(pr.first.use_count(), 2);
}

TEST(BinaryReaderTests, RawReferenceResolvesToAnEarlierUniquePointee) {
	const Bytes b{ 1, 7, 0, 0, 0, 8, 0, 0, 0, 2, 0, 0, 0, 0 };
	BinaryReader r(b);
	std::pair<std::unique_ptr<Position>, Position*> pr;
	r.read(pr);
	ASSERT_TRUE(r.ok());
	EXPECT_EQ(pr.second, pr.first.get());
}

TEST(BinaryReaderTests, ReferenceToAnObjectThatDoesNotExistFails) {
	const Bytes b{ 2, 5, 0, 0, 0 }; // nothing has been created yet
	BinaryReader r(b);
	std::shared_ptr<Position> p;
	r.read(p);
	EXPECT_FALSE(r.ok());
	EXPECT_EQ(p, nullptr);
}

TEST(BinaryReaderTests, UniquePointerCannotBeAReference) {
	const Bytes b{ 1, 7, 0, 0, 0, 8, 0, 0, 0, 2, 0, 0, 0, 0 };
	BinaryReader r(b);
	std::pair<std::shared_ptr<Position>, std::unique_ptr<Position>> pr;
	r.read(pr);
	EXPECT_FALSE(r.ok());
	EXPECT_EQ(pr.second, nullptr);
}

TEST(BinaryReaderTests, SharedPointerCannotReferenceAnObjectOwnedByAUniquePointer) {
	const Bytes b{ 1, 7, 0, 0, 0, 8, 0, 0, 0, 2, 0, 0, 0, 0 };
	BinaryReader r(b);
	std::pair<std::unique_ptr<Position>, std::shared_ptr<Position>> pr;
	r.read(pr);
	EXPECT_FALSE(r.ok());
	EXPECT_EQ(pr.second, nullptr);
}

TEST(BinaryReaderTests, ReferenceToAnObjectOfAnotherTypeFails) {
	const Bytes b{ 1, 7, 0, 0, 0, 8, 0, 0, 0, 2, 0, 0, 0, 0 };
	BinaryReader r(b);
	std::pair<std::shared_ptr<Position>, std::shared_ptr<Material>> pr;
	r.read(pr);
	EXPECT_FALSE(r.ok());
	EXPECT_EQ(pr.second, nullptr);
}

TEST(BinaryReaderTests, PointerChainAtTheLimitIsAccepted) {
	Bytes b(Pumpkin::AE::kMaxPointerDepth, 1); // N "new" tags...
	b.push_back(0);                            // ...ended by a null
	BinaryReader r(b);
	Node n;
	r.read(n);
	EXPECT_TRUE(r.ok());
}

TEST(BinaryReaderTests, PointerChainBeyondTheLimitFails) {
	Bytes b(Pumpkin::AE::kMaxPointerDepth + 1, 1);
	b.push_back(0);
	BinaryReader r(b);
	Node n;
	r.read(n);
	EXPECT_FALSE(r.ok());
}

// ===========================================================================
// BinaryWriter -> BinaryReader
// ===========================================================================

TEST(BinaryIORoundTrip, SharedCycleKeepsItsShape) {
	auto a = std::make_shared<Leaky1>();
	a->leaky = std::make_shared<Leaky2>();
	a->leaky->leaky = a;

	BinaryWriter w;
	w.write(a);
	ASSERT_TRUE(w.ok());
	const Bytes bytes = w.take();

	BinaryReader r(bytes);
	std::shared_ptr<Leaky1> b;
	r.read(b);
	ASSERT_TRUE(r.ok());
	ASSERT_NE(b, nullptr);
	ASSERT_NE(b->leaky, nullptr);
	EXPECT_EQ(b->leaky->leaky, b); // the cycle comes back as a cycle
	EXPECT_TRUE(r.atEnd());

	a->leaky->leaky.reset(); // no leak
	b->leaky->leaky.reset();
}
