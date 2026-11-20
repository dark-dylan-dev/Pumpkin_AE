#include <gtest/gtest.h>

#include <Common.hpp>
#include <TestUtils.hpp>

using Pumpkin::AE::Reader;
using Pumpkin::AE::Writer;

// ===========================================================================
// Add
// ===========================================================================

TEST(WriterTests, AddReturnsTrueForANewId) {
	Writer w;
	EXPECT_TRUE(w.Add(1, 0, Bytes{ 1, 2, 3 }));
	EXPECT_TRUE(w.Add(2, 0, Bytes{ 4 }));
}

TEST(WriterTests, AddRejectsADuplicateId) {
	Writer w;
	ASSERT_TRUE(w.Add(1, 0, Bytes{ 1 }));
	EXPECT_FALSE(w.Add(1, 0, Bytes{ 2 }));
}

TEST(WriterTests, AddRejectsADuplicateIdEvenWithAnotherType) {
	Writer w;
	ASSERT_TRUE(w.Add(1, 10, Bytes{ 1 }));
	EXPECT_FALSE(w.Add(1, 20, Bytes{ 1 }));
}

TEST(WriterTests, ADuplicateAddDoesNotChangeTheArchive) {
	TempFile f("WriterDuplicate.assets");
	Writer w;
	ASSERT_TRUE(w.Add(1, 0, Bytes{ 1, 1 }));
	ASSERT_FALSE(w.Add(1, 0, Bytes{ 2, 2, 2 }));
	ASSERT_TRUE(w.Save(f.path));

	Reader r;
	ASSERT_TRUE(r.Open(f.path));
	ASSERT_EQ(r.List().size(), 1u);
	EXPECT_EQ(*r.Load(1), (Bytes{ 1, 1 }));
}

TEST(WriterTests, AddAcceptsIdZeroAndMaxId) {
	Writer w;
	EXPECT_TRUE(w.Add(0, 0, Bytes{ 1 }));
	EXPECT_TRUE(w.Add(std::numeric_limits<std::uint64_t>::max(), 0, Bytes{ 2 }));
}

TEST(WriterTests, AddAcceptsAnEmptyPayload) {
	Writer w;
	EXPECT_TRUE(w.Add(1, 0, Bytes{}));
}

TEST(WriterTests, RawByteContainersAreStoredVerbatim) {
	TempFile f("WriterVerbatim.assets");
	Writer w;
	ASSERT_TRUE(w.Add(1, 0, Bytes{ 1, 2, 3 }));                          // vector<uint8_t>
	ASSERT_TRUE(w.Add(2, 0, std::array<std::uint8_t, 2>{ 9, 8 }));       // array
	const Bytes src{ 5, 6, 7, 8 };
	ASSERT_TRUE(w.Add(3, 0, std::span<const std::uint8_t>(src).subspan(1, 2))); // span
	ASSERT_TRUE(w.Save(f.path));

	Reader r;
	ASSERT_TRUE(r.Open(f.path));
	EXPECT_EQ(*r.Load(1), (Bytes{ 1, 2, 3 }));
	EXPECT_EQ(*r.Load(2), (Bytes{ 9, 8 }));
	EXPECT_EQ(*r.Load(3), (Bytes{ 6, 7 }));
}

TEST(WriterTests, TypedAddEncodesTheValue) {
	TempFile f("WriterTyped.assets");
	Writer w;
	ASSERT_TRUE(w.Add(1, 5, Position{ 1, 2 }));
	ASSERT_TRUE(w.Save(f.path));

	Reader r;
	ASSERT_TRUE(r.Open(f.path));
	EXPECT_EQ(*r.Load(1), (Bytes{ 1, 0, 0, 0, 2, 0, 0, 0 }));
	EXPECT_EQ(r.List()[0].type, 5u);
}

TEST(WriterTests, TypedAddRejectsADuplicateId) {
	Writer w;
	ASSERT_TRUE(w.Add(1, 0, Position{ 1, 2 }));
	EXPECT_FALSE(w.Add(1, 0, Position{ 3, 4 }));
}

TEST(WriterTests, TypedAddFailsWhenTheValueCannotBeEncoded) {
	Writer w;
	SelfRef s;
	s.self = &s;
	EXPECT_FALSE(w.Add(1, 0, s));
}

TEST(WriterTests, AFailedTypedAddDoesNotConsumeTheId) {
	Writer w;
	SelfRef s;
	s.self = &s;
	ASSERT_FALSE(w.Add(1, 0, s));
	EXPECT_TRUE(w.Add(1, 0, Position{ 1, 2 }));
}

TEST(WriterTests, CyclicDependencyDoesntSave) {
	// A cycle back to the root value can't be encoded: Add fails and nothing reaches the archive.
	TempFile f("Leaky.assets");
	Writer w;
	Leaky1 l1;
	l1.leaky = std::make_shared<Leaky2>();
	l1.leaky->leaky = std::shared_ptr<Leaky1>(std::shared_ptr<void>(), &l1); // non-owning alias of l1
	EXPECT_FALSE(w.Add(0, 0, l1));

	ASSERT_TRUE(w.Save(f.path));
	Reader r;
	ASSERT_TRUE(r.Open(f.path));
	EXPECT_TRUE(r.List().empty());
}

TEST(WriterTests, CycleBetweenSharedObjectsIsSaved) {
	TempFile f("LeakyShared.assets");
	Leaky1 root;
	auto a = std::make_shared<Leaky2>();
	auto b = std::make_shared<Leaky1>();
	a->leaky = b;
	b->leaky = a;
	root.leaky = a;

	Writer w;
	EXPECT_TRUE(w.Add(0, 0, root));
	EXPECT_TRUE(w.Save(f.path));

	a->leaky.reset(); // no leak
}

// ===========================================================================
// Save: on-disk format
// ===========================================================================

TEST(WriterTests, EmptyWriterSavesAHeaderOnlyArchive) {
	TempFile f("WriterEmpty.assets");
	Writer w;
	ASSERT_TRUE(w.Save(f.path));

	const Bytes file = ReadFileBytes(f.path);
	ASSERT_EQ(file.size(), kHeaderSize);
	EXPECT_EQ(GetLE(file, kOffCount, 4), 0u);
	EXPECT_EQ(GetLE(file, kOffDirOffset, 8), kHeaderSize);
}

TEST(WriterTests, HeaderLayout) {
	TempFile f("WriterHeader.assets");
	Writer w;
	ASSERT_TRUE(w.Add(1, 0, Bytes{ 1, 2, 3 }));
	ASSERT_TRUE(w.Add(2, 0, Bytes{ 4, 5 }));
	ASSERT_TRUE(w.Save(f.path));

	const Bytes file = ReadFileBytes(f.path);
	EXPECT_EQ(file[0], 'A'); EXPECT_EQ(file[1], 'S'); EXPECT_EQ(file[2], 'E'); EXPECT_EQ(file[3], 'T');
	EXPECT_EQ(GetLE(file, kOffMagic, 4), 0x54455341u);
	EXPECT_EQ(GetLE(file, kOffVersion, 2), static_cast<std::uint64_t>(PUMPKIN_AE_VERSION));
	EXPECT_EQ(GetLE(file, kOffReserved, 2), 0u);
	EXPECT_EQ(GetLE(file, kOffCount, 4), 2u);
	EXPECT_EQ(GetLE(file, kOffDirOffset, 8), kHeaderSize + 5u); // right after the 5 payload bytes
}

TEST(WriterTests, FileSizeIsHeaderPlusDataPlusDirectory) {
	TempFile f("WriterSize.assets");
	Writer w;
	ASSERT_TRUE(w.Add(1, 0, Bytes(10, 0xAA)));
	ASSERT_TRUE(w.Add(2, 0, Bytes(7, 0xBB)));
	ASSERT_TRUE(w.Save(f.path));
	EXPECT_EQ(ReadFileBytes(f.path).size(), kHeaderSize + 17 + 2 * kEntrySize);
}

TEST(WriterTests, DirectoryEntryLayout) {
	TempFile f("WriterEntry.assets");
	const Bytes payload = ToBytes("123456789");
	Writer w;
	ASSERT_TRUE(w.Add(0x0102030405060708ull, 0xDEADBEEF, payload));
	ASSERT_TRUE(w.Save(f.path));

	const Bytes file = ReadFileBytes(f.path);
	const std::size_t dir = static_cast<std::size_t>(GetLE(file, kOffDirOffset, 8));
	ASSERT_EQ(dir, kHeaderSize + 9);
	ASSERT_EQ(file.size(), dir + kEntrySize);
	EXPECT_EQ(GetLE(file, dir + kEntId, 8), 0x0102030405060708ull);
	EXPECT_EQ(GetLE(file, dir + kEntType, 4), 0xDEADBEEFu);
	EXPECT_EQ(GetLE(file, dir + kEntCrc, 4), 0xCBF43926u); // CRC-32 check value of "123456789"
	EXPECT_EQ(GetLE(file, dir + kEntOffset, 8), kHeaderSize);
	EXPECT_EQ(GetLE(file, dir + kEntSize, 8), 9u);
}

TEST(WriterTests, PayloadsAreStoredBackToBackAfterTheHeader) {
	TempFile f("WriterData.assets");
	Writer w;
	ASSERT_TRUE(w.Add(1, 0, Bytes{ 1, 2, 3 }));
	ASSERT_TRUE(w.Add(2, 0, Bytes{ 4, 5 }));
	ASSERT_TRUE(w.Add(3, 0, Bytes{ 6 }));
	ASSERT_TRUE(w.Save(f.path));

	const Bytes file = ReadFileBytes(f.path);
	EXPECT_EQ(Bytes(file.begin() + kHeaderSize, file.begin() + kHeaderSize + 6), (Bytes{ 1, 2, 3, 4, 5, 6 }));
}

TEST(WriterTests, DirectoryKeepsInsertionOrderWithContiguousOffsets) {
	TempFile f("WriterOrder.assets");
	Writer w;
	ASSERT_TRUE(w.Add(30, 1, Bytes(4, 1)));
	ASSERT_TRUE(w.Add(10, 2, Bytes(6, 2)));
	ASSERT_TRUE(w.Add(20, 3, Bytes(2, 3)));
	ASSERT_TRUE(w.Save(f.path));

	const Bytes file = ReadFileBytes(f.path);
	const std::size_t dir = static_cast<std::size_t>(GetLE(file, kOffDirOffset, 8));
	const std::uint64_t ids[]     = { 30, 10, 20 };
	const std::uint64_t offsets[] = { kHeaderSize, kHeaderSize + 4, kHeaderSize + 10 };
	const std::uint64_t sizes[]   = { 4, 6, 2 };
	for (std::size_t i = 0; i < 3; ++i) {
		EXPECT_EQ(GetLE(file, dir + i * kEntrySize + kEntId, 8), ids[i]);
		EXPECT_EQ(GetLE(file, dir + i * kEntrySize + kEntOffset, 8), offsets[i]);
		EXPECT_EQ(GetLE(file, dir + i * kEntrySize + kEntSize, 8), sizes[i]);
	}
}

TEST(WriterTests, EmptyPayloadHasSizeZeroAndTheEmptyCrc) {
	TempFile f("WriterEmptyPayload.assets");
	Writer w;
	ASSERT_TRUE(w.Add(1, 0, Bytes{}));
	ASSERT_TRUE(w.Save(f.path));

	const Bytes file = ReadFileBytes(f.path);
	const std::size_t dir = static_cast<std::size_t>(GetLE(file, kOffDirOffset, 8));
	EXPECT_EQ(GetLE(file, dir + kEntSize, 8), 0u);
	EXPECT_EQ(GetLE(file, dir + kEntCrc, 4), 0u); // CRC-32 of nothing
}

TEST(WriterTests, CrcMatchesAnIndependentImplementation) {
	TempFile f("WriterCrc.assets");
	Bytes payload(1000);
	for (std::size_t i = 0; i < payload.size(); ++i) payload[i] = static_cast<std::uint8_t>(i * 31 + 7);
	Writer w;
	ASSERT_TRUE(w.Add(1, 0, payload));
	ASSERT_TRUE(w.Save(f.path));

	const Bytes file = ReadFileBytes(f.path);
	const std::size_t dir = static_cast<std::size_t>(GetLE(file, kOffDirOffset, 8));
	EXPECT_EQ(GetLE(file, dir + kEntCrc, 4), Crc32(payload));
}

TEST(WriterTests, EveryByteValueSurvivesBinaryMode) {
	TempFile f("WriterBytes.assets");
	Bytes all(256);
	for (int i = 0; i < 256; ++i) all[i] = static_cast<std::uint8_t>(i); // includes 0x00, 0x0A, 0x0D, 0x1A
	Writer w;
	ASSERT_TRUE(w.Add(1, 0, all));
	ASSERT_TRUE(w.Save(f.path));

	Reader r;
	ASSERT_TRUE(r.Open(f.path));
	EXPECT_EQ(*r.Load(1), all);
}

// ===========================================================================
// Save: behavior
// ===========================================================================

TEST(WriterTests, SaveFailsWhenTheDirectoryDoesNotExist) {
	Writer w;
	ASSERT_TRUE(w.Add(1, 0, Bytes{ 1 }));
	EXPECT_FALSE(w.Save("UnitTests/NoSuchDirectory/Archive.assets"));
}

TEST(WriterTests, SaveFailsOnAnEmptyPath) {
	Writer w;
	EXPECT_FALSE(w.Save(""));
}

TEST(WriterTests, SaveTruncatesAnExistingFile) {
	TempFile f("WriterTruncate.assets");
	Writer big;
	ASSERT_TRUE(big.Add(1, 0, Bytes(5000, 1)));
	ASSERT_TRUE(big.Save(f.path));

	Writer small;
	ASSERT_TRUE(small.Add(2, 0, Bytes{ 9 }));
	ASSERT_TRUE(small.Save(f.path));

	EXPECT_EQ(ReadFileBytes(f.path).size(), kHeaderSize + 1 + kEntrySize);
	Reader r;
	ASSERT_TRUE(r.Open(f.path)); // no leftover bytes from the old file
	EXPECT_EQ(r.List().size(), 1u);
}

TEST(WriterTests, SavingTwiceGivesIdenticalFiles) {
	TempFile a("WriterTwiceA.assets"), b("WriterTwiceB.assets");
	Writer w;
	ASSERT_TRUE(w.Add(1, 2, Bytes{ 1, 2, 3 }));
	ASSERT_TRUE(w.Save(a.path));
	ASSERT_TRUE(w.Save(b.path));
	EXPECT_EQ(ReadFileBytes(a.path), ReadFileBytes(b.path));
}

TEST(WriterTests, TheWriterCanKeepGrowingAfterASave) {
	TempFile f("WriterGrow.assets");
	Writer w;
	ASSERT_TRUE(w.Add(1, 0, Bytes{ 1 }));
	ASSERT_TRUE(w.Save(f.path));
	EXPECT_FALSE(w.Add(1, 0, Bytes{ 2 })); // the id is still taken
	ASSERT_TRUE(w.Add(2, 0, Bytes{ 2 }));
	ASSERT_TRUE(w.Save(f.path));

	Reader r;
	ASSERT_TRUE(r.Open(f.path));
	EXPECT_EQ(r.List().size(), 2u);
}

TEST(WriterTests, ManyAssets) {
	TempFile f("WriterMany.assets");
	Writer w;
	for (std::uint64_t i = 0; i < 2000; ++i) ASSERT_TRUE(w.Add(i, static_cast<std::uint32_t>(i % 7), Bytes(i % 50, static_cast<std::uint8_t>(i))));
	ASSERT_TRUE(w.Save(f.path));

	Reader r;
	ASSERT_TRUE(r.Open(f.path));
	ASSERT_EQ(r.List().size(), 2000u);
	for (std::uint64_t i : { 0ull, 1ull, 999ull, 1999ull })
		EXPECT_EQ(*r.Load(i), Bytes(i % 50, static_cast<std::uint8_t>(i)));
}
