#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include <Common.hpp>
#include <TestUtils.hpp>

using Pumpkin::AE::BinaryReader;
using Pumpkin::AE::BinaryWriter;
using Pumpkin::AE::Codec;
using Pumpkin::AE::Decode;
using Pumpkin::AE::Encode;

// ===========================================================================
// Encode / Decode
// ===========================================================================

TEST(CodecTests, EncodeProducesTheReflectLayout) {
	const auto bytes = Encode(Position{ 1, 2 });
	ASSERT_TRUE(bytes.has_value());
	EXPECT_EQ(*bytes, (Bytes{ 1, 0, 0, 0, 2, 0, 0, 0 }));
}

TEST(CodecTests, DecodeReadsTheReflectLayout) {
	const Bytes bytes{ 1, 0, 0, 0, 2, 0, 0, 0 };
	const auto p = Decode<Position>(bytes);
	ASSERT_TRUE(p.has_value());
	EXPECT_EQ(p->x, 1);
	EXPECT_EQ(p->y, 2);
}

TEST(CodecTests, DecodeRejectsTrailingBytes) {
	const Bytes bytes{ 1, 0, 0, 0, 2, 0, 0, 0, 0xFF };
	EXPECT_FALSE(Decode<Position>(bytes).has_value());
}

TEST(CodecTests, DecodeRejectsTruncatedPayload) {
	const Bytes bytes{ 1, 0, 0, 0, 2, 0, 0 };
	EXPECT_FALSE(Decode<Position>(bytes).has_value());
}

TEST(CodecTests, DecodeRejectsEmptyPayload) {
	EXPECT_FALSE(Decode<Position>(Bytes{}).has_value());
}

TEST(CodecTests, DecodeOfAnEmptyInventoryNeedsAnItemCountAndAPointerTag) {
	EXPECT_TRUE(Decode<Inventory>(Bytes{ 0, 0, 0, 0, 0 }).has_value()); // 0 items + null equipped
	EXPECT_FALSE(Decode<Inventory>(Bytes{}).has_value());
}

TEST(CodecTests, EncodeFailsWhenTheValueCannotBeEncoded) {
	SelfRef s;
	s.self = &s;
	EXPECT_FALSE(Encode(s).has_value());
}

TEST(CodecTests, EncodeIsDeterministic) {
	Item item;
	item.id = 42;
	item.name = "sword";
	item.tags = { "a", "b" };
	item.position = std::make_unique<Position>(Position{ 1, 2 });
	EXPECT_EQ(Encode(item), Encode(item));
}

// ===========================================================================
// Generated Codec
// ===========================================================================

TEST(CodecTests, GeneratedReadLeavesTheValueUntouchedOnFailure) {
	const Bytes truncated{ 9, 0, 0 };
	BinaryReader r(truncated);
	Position p{ 5, 6 };
	EXPECT_FALSE(Codec<Position>::Read(r, p));
	EXPECT_EQ(p.x, 5);
	EXPECT_EQ(p.y, 6);
}

TEST(CodecTests, GeneratedReadCommitsOnSuccess) {
	const Bytes bytes{ 9, 0, 0, 0, 8, 0, 0, 0 };
	BinaryReader r(bytes);
	Position p{ 5, 6 };
	EXPECT_TRUE(Codec<Position>::Read(r, p));
	EXPECT_EQ(p.x, 9);
	EXPECT_EQ(p.y, 8);
}

TEST(CodecTests, GeneratedWriteMatchesBinaryWriter) {
	BinaryWriter direct;
	direct.write(Material{ "metal", 0.25f });
	BinaryWriter viaCodec;
	Codec<Material>::Write(viaCodec, Material{ "metal", 0.25f });
	EXPECT_EQ(direct.take(), viaCodec.take());
}

// ===========================================================================
// Custom Codec specializations
// ===========================================================================

TEST(CodecTests, ASpecializationTakesPrecedenceOverReflect) {
	const auto bytes = Encode(Legacy{ 0x0102 });
	ASSERT_TRUE(bytes.has_value());
	EXPECT_EQ(*bytes, (Bytes{ 0x02, 0x01 })); // 2 bytes, not the 4 Reflect() would give
	const auto back = Decode<Legacy>(*bytes);
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(back->a, 0x0102);
}

TEST(CodecTests, NonReflectableTypeWorksThroughItsCodec) {
	const auto bytes = Encode(Opaque{ 200 });
	ASSERT_TRUE(bytes.has_value());
	EXPECT_EQ(*bytes, (Bytes{ 200 }));
	const auto back = Decode<Opaque>(*bytes);
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(back->v, 200);
}

TEST(CodecTests, CustomCodecStillRejectsTrailingBytes) {
	EXPECT_FALSE(Decode<Opaque>(Bytes{ 1, 2 }).has_value());
}

TEST(CodecTests, CustomCodecRejectsTruncatedPayload) {
	EXPECT_FALSE(Decode<Legacy>(Bytes{ 1 }).has_value());
}

// ===========================================================================
// Round trips through Encode / Decode
// ===========================================================================

TEST(CodecRoundTrip, EveryPrimitiveType) {
	Primitives p;
	p.b = true; p.c = 'z'; p.i8 = -128; p.u8 = 255;
	p.i16 = -32768; p.u16 = 65535;
	p.i32 = std::numeric_limits<std::int32_t>::min(); p.u32 = std::numeric_limits<std::uint32_t>::max();
	p.i64 = std::numeric_limits<std::int64_t>::min(); p.u64 = std::numeric_limits<std::uint64_t>::max();
	p.f = 3.14159f; p.d = -2.718281828459045;
	p.color = Color::Blue; p.sign = Signed::Low;

	const auto bytes = Encode(p);
	ASSERT_TRUE(bytes.has_value());
	const auto back = Decode<Primitives>(*bytes);
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(*back, p);
}

TEST(CodecRoundTrip, FloatingPointSpecialValues) {
	for (const double d : { 0.0, -0.0, std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
	                        std::numeric_limits<double>::denorm_min(), std::numeric_limits<double>::max() }) {
		Primitives p;
		p.d = d;
		p.f = static_cast<float>(d);
		const auto back = Decode<Primitives>(*Encode(p));
		ASSERT_TRUE(back.has_value());
		EXPECT_EQ(std::bit_cast<std::uint64_t>(back->d), std::bit_cast<std::uint64_t>(d)); // bit exact, keeps -0.0
		EXPECT_EQ(std::bit_cast<std::uint32_t>(back->f), std::bit_cast<std::uint32_t>(p.f));
	}
}

TEST(CodecRoundTrip, NaNStaysNaN) {
	Primitives p;
	p.f = std::numeric_limits<float>::quiet_NaN();
	p.d = std::numeric_limits<double>::quiet_NaN();
	const auto back = Decode<Primitives>(*Encode(p));
	ASSERT_TRUE(back.has_value());
	EXPECT_TRUE(std::isnan(back->f));
	EXPECT_TRUE(std::isnan(back->d));
}

TEST(CodecRoundTrip, ContainersPairsAndTuples) {
	Containers c;
	c.numbers = { 1, -2, 3 };
	c.words = { "", "hello", "wörld" };
	c.nested = { {}, { 1, 2, 3 }, { 255 } };
	c.pair = { 9, "nine" };
	c.tuple = { 513, 0.5f };
	const auto back = Decode<Containers>(*Encode(c));
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(*back, c);
}

TEST(CodecRoundTrip, EmptyContainers) {
	const Containers c;
	const auto back = Decode<Containers>(*Encode(c));
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(*back, c);
}

TEST(CodecRoundTrip, StringsWithNulAndUtf8) {
	Material m;
	m.name = std::string("a\0b\xC3\xA9", 5);
	const auto back = Decode<Material>(*Encode(m));
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(back->name, m.name);
	EXPECT_EQ(back->name.size(), 5u);
}

TEST(CodecRoundTrip, LargeVector) {
	Containers c;
	for (int i = 0; i < 100000; ++i) c.numbers.push_back(i * 3 - 7);
	const auto back = Decode<Containers>(*Encode(c));
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(back->numbers, c.numbers);
}

TEST(CodecRoundTrip, ListAndDequeSequences) {
	BinaryWriter w;
	w.write(std::list<std::string>{ "a", "bc" });
	w.write(std::deque<int>{ 1, 2, 3 });
	const Bytes bytes = w.take();
	BinaryReader r(bytes);
	std::list<std::string> l;
	std::deque<int> d;
	r.read(l);
	r.read(d);
	EXPECT_TRUE(r.ok());
	EXPECT_TRUE(r.atEnd());
	EXPECT_EQ(l, (std::list<std::string>{ "a", "bc" }));
	EXPECT_EQ(d, (std::deque<int>{ 1, 2, 3 }));
}

// ----- Items, pointers, sharing ---------------------------------------------

TEST(CodecRoundTrip, ItemWithEveryFieldSet) {
	Item item;
	item.id = 0x1122334455667788ull;
	item.name = "Excalibur";
	item.tags = { "weapon", "legendary" };
	item.weight = 12.5f;
	item.position = std::make_unique<Position>(Position{ -3, 4 });
	item.material = std::make_shared<Material>(Material{ "steel", 0.1f });

	const auto back = Decode<Item>(*Encode(item));
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(back->id, item.id);
	EXPECT_EQ(back->name, item.name);
	EXPECT_EQ(back->tags, item.tags);
	EXPECT_EQ(back->weight, item.weight);
	ASSERT_NE(back->position, nullptr);
	EXPECT_EQ(back->position->x, -3);
	EXPECT_EQ(back->position->y, 4);
	ASSERT_NE(back->material, nullptr);
	EXPECT_EQ(back->material->name, "steel");
	EXPECT_EQ(back->material->roughness, 0.1f);
}

TEST(CodecRoundTrip, ItemWithNullPointers) {
	Item item;
	item.name = "bare";
	const auto back = Decode<Item>(*Encode(item));
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(back->name, "bare");
	EXPECT_EQ(back->position, nullptr);
	EXPECT_EQ(back->material, nullptr);
}

TEST(CodecRoundTrip, DefaultItemUsesTheDefaultFieldValues) {
	const auto back = Decode<Item>(*Encode(Item{}));
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(back->id, 0u);
	EXPECT_EQ(back->weight, 1.0f);
	EXPECT_TRUE(back->name.empty());
	EXPECT_TRUE(back->tags.empty());
}

TEST(CodecRoundTrip, SharedMaterialIsSharedAfterTheRoundTrip) {
	const auto steel = std::make_shared<Material>(Material{ "steel", 0.1f });
	Inventory inv;
	for (int i = 0; i < 3; ++i) {
		auto item = std::make_unique<Item>();
		item->id = i;
		item->material = steel;
		inv.items.push_back(std::move(item));
	}

	const auto back = Decode<Inventory>(*Encode(inv));
	ASSERT_TRUE(back.has_value());
	ASSERT_EQ(back->items.size(), 3u);
	EXPECT_EQ(back->items[0]->material, back->items[1]->material);
	EXPECT_EQ(back->items[1]->material, back->items[2]->material);
	EXPECT_EQ(back->items[0]->material->name, "steel");
	EXPECT_EQ(back->items[0]->material.use_count(), 3);
	EXPECT_NE(back->items[0]->material, steel); // a new object, not the original
}

TEST(CodecRoundTrip, EqualButDistinctMaterialsStayDistinct) {
	Inventory inv;
	for (int i = 0; i < 2; ++i) {
		auto item = std::make_unique<Item>();
		item->material = std::make_shared<Material>(Material{ "same", 0.5f });
		inv.items.push_back(std::move(item));
	}
	const auto back = Decode<Inventory>(*Encode(inv));
	ASSERT_TRUE(back.has_value());
	EXPECT_NE(back->items[0]->material, back->items[1]->material);
}

TEST(CodecRoundTrip, SharingIsPerEncodeNotGlobal) {
	// Two separate encodes of the same shared object are independent payloads.
	Item a, b;
	a.material = b.material = std::make_shared<Material>(Material{ "m", 1.0f });
	const auto backA = Decode<Item>(*Encode(a));
	const auto backB = Decode<Item>(*Encode(b));
	ASSERT_TRUE(backA && backB);
	EXPECT_NE(backA->material, backB->material);
}

TEST(CodecRoundTrip, EquippedRawPointerPointsIntoTheDecodedItems) {
	Inventory inv;
	for (int i = 0; i < 3; ++i) {
		auto item = std::make_unique<Item>();
		item->id = 100 + i;
		inv.items.push_back(std::move(item));
	}
	inv.equipped = inv.items[1].get();

	const auto back = Decode<Inventory>(*Encode(inv));
	ASSERT_TRUE(back.has_value());
	ASSERT_EQ(back->items.size(), 3u);
	EXPECT_EQ(back->equipped, back->items[1].get());
	EXPECT_EQ(back->equipped->id, 101u);
}

TEST(CodecRoundTrip, EquippedNullStaysNull) {
	Inventory inv;
	inv.items.push_back(std::make_unique<Item>());
	const auto back = Decode<Inventory>(*Encode(inv));
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(back->equipped, nullptr);
}

TEST(CodecRoundTrip, EmptyInventory) {
	const auto back = Decode<Inventory>(*Encode(Inventory{}));
	ASSERT_TRUE(back.has_value());
	EXPECT_TRUE(back->items.empty());
	EXPECT_EQ(back->equipped, nullptr);
}

TEST(CodecRoundTrip, NullEntriesInAPointerVector) {
	Inventory inv;
	inv.items.push_back(nullptr);
	inv.items.push_back(std::make_unique<Item>());
	inv.items.push_back(nullptr);
	const auto back = Decode<Inventory>(*Encode(inv));
	ASSERT_TRUE(back.has_value());
	ASSERT_EQ(back->items.size(), 3u);
	EXPECT_EQ(back->items[0], nullptr);
	EXPECT_NE(back->items[1], nullptr);
	EXPECT_EQ(back->items[2], nullptr);
}

// ----- Cycles ---------------------------------------------------------------

TEST(CodecRoundTrip, CycleBetweenSharedObjectsSurvives) {
	// root -> a (Leaky2) -> b (Leaky1) -> a : the cycle a <-> b doesn't involve the root value.
	Leaky1 root;
	auto a = std::make_shared<Leaky2>();
	auto b = std::make_shared<Leaky1>();
	a->leaky = b;
	b->leaky = a;
	root.leaky = a;

	const auto bytes = Encode(root);
	ASSERT_TRUE(bytes.has_value());
	auto back = Decode<Leaky1>(*bytes);
	ASSERT_TRUE(back.has_value());
	ASSERT_NE(back->leaky, nullptr);
	ASSERT_NE(back->leaky->leaky, nullptr);
	EXPECT_EQ(back->leaky->leaky->leaky, back->leaky); // b -> a is the same a as root -> a

	a->leaky.reset(); // no leak
	back->leaky->leaky->leaky.reset();
}

TEST(CodecRoundTrip, CycleBackToTheRootValueCannotBeEncoded) {
	Leaky1 root;
	root.leaky = std::make_shared<Leaky2>();
	root.leaky->leaky = std::shared_ptr<Leaky1>(std::shared_ptr<void>(), &root); // non-owning alias of the root
	EXPECT_FALSE(Encode(root).has_value());
}

// ----- Pointer nesting ------------------------------------------------------

TEST(CodecRoundTrip, LongButAcceptableChain) {
	Node root;
	Node* tail = &root;
	for (int i = 0; i < 200; ++i) {
		tail->next = std::make_unique<Node>();
		tail = tail->next.get();
	}
	const auto back = Decode<Node>(*Encode(root));
	ASSERT_TRUE(back.has_value());
	int depth = 0;
	for (const Node* n = back->next.get(); n; n = n->next.get()) ++depth;
	EXPECT_EQ(depth, 200);
}

TEST(CodecRoundTrip, ChainBeyondTheLimitCannotBeEncodedNorDecoded) {
	Node root;
	Node* tail = &root;
	for (std::uint32_t i = 0; i <= Pumpkin::AE::kMaxPointerDepth; ++i) {
		tail->next = std::make_unique<Node>();
		tail = tail->next.get();
	}
	EXPECT_FALSE(Encode(root).has_value());

	Bytes crafted(Pumpkin::AE::kMaxPointerDepth + 1, 1);
	crafted.push_back(0);
	EXPECT_FALSE(Decode<Node>(crafted).has_value());
}

// ===========================================================================
// Hostile payloads
// ===========================================================================

TEST(CodecHostileInput, EveryTruncationOfAnItemFailsCleanly) {
	Item item;
	item.id = 7;
	item.name = "truncate me";
	item.tags = { "x", "yy" };
	item.position = std::make_unique<Position>(Position{ 1, 2 });
	item.material = std::make_shared<Material>(Material{ "m", 0.5f });
	const Bytes full = *Encode(item);
	ASSERT_TRUE(Decode<Item>(full).has_value());

	for (std::size_t n = 0; n < full.size(); ++n) {
		const Bytes cut(full.begin(), full.begin() + static_cast<std::ptrdiff_t>(n));
		EXPECT_FALSE(Decode<Item>(cut).has_value()) << "prefix of " << n << " bytes was accepted";
	}
}

TEST(CodecHostileInput, EverySingleByteFlipEitherDecodesOrFailsCleanly) {
	Inventory inv;
	for (int i = 0; i < 2; ++i) {
		auto item = std::make_unique<Item>();
		item->name = "n";
		item->material = std::make_shared<Material>(Material{ "m", 1.0f });
		inv.items.push_back(std::move(item));
	}
	inv.equipped = inv.items[0].get();
	const Bytes full = *Encode(inv);

	for (std::size_t i = 0; i < full.size(); ++i) {
		for (const std::uint8_t mask : { 0x01, 0x80, 0xFF }) {
			Bytes mutated = full;
			mutated[i] ^= mask;
			const auto r = Decode<Inventory>(mutated);
			(void)r;
		}
	}
	SUCCEED();
}

TEST(CodecHostileInput, HugeCountsDoNotAllocate) {
	const Bytes b{ 0xFF, 0xFF, 0xFF, 0xFF };
	EXPECT_FALSE(Decode<Containers>(b).has_value());
	EXPECT_FALSE(Decode<Inventory>(b).has_value());
}

TEST(CodecHostileInput, InvalidBoolIsRejected) {
	Primitives p;
	Bytes bytes = *Encode(p);
	bytes[0] = 2;
	EXPECT_FALSE(Decode<Primitives>(bytes).has_value());
}
