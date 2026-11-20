#include <gtest/gtest.h>

#include <Common.hpp>
#include <TestUtils.hpp>

using Pumpkin::AE::Entry;
using Pumpkin::AE::Reader;
using Pumpkin::AE::Writer;

namespace {

	// Archive with three assets: {10, type 1, 3 bytes}, {20, type 2, 2 bytes}, {30, type 3, 1 byte}.
	Bytes BuildSample(const std::string& path) {
		Writer w;
		EXPECT_TRUE(w.Add(10, 1, Bytes{ 1, 2, 3 }));
		EXPECT_TRUE(w.Add(20, 2, Bytes{ 4, 5 }));
		EXPECT_TRUE(w.Add(30, 3, Bytes{ 6 }));
		EXPECT_TRUE(w.Save(path));
		return ReadFileBytes(path);
	}

	std::size_t DirOffset(const Bytes& file) { return static_cast<std::size_t>(GetLE(file, kOffDirOffset, 8)); }
	std::size_t EntryPos(const Bytes& file, std::size_t i) { return DirOffset(file) + i * kEntrySize; }

	// Writes the (possibly corrupted) bytes back and tries to open them.
	bool OpenBytes(Reader& r, const std::string& path, const Bytes& b) {
		WriteFileBytes(path, b);
		return r.Open(path);
	}

} // namespace

// ===========================================================================
// Open: valid archives
// ===========================================================================

TEST(ReaderTests, ANewReaderIsEmpty) {
	Reader r;
	EXPECT_TRUE(r.List().empty());
	EXPECT_TRUE(r.error.empty());
}

TEST(ReaderTests, OpensAValidArchive) {
	TempFile f("ReaderValid.assets");
	BuildSample(f.path);
	Reader r;
	EXPECT_TRUE(r.Open(f.path));
	EXPECT_TRUE(r.error.empty());
	EXPECT_EQ(r.List().size(), 3u);
}

TEST(ReaderTests, OpensAnEmptyArchive) {
	TempFile f("ReaderEmptyArchive.assets");
	ASSERT_TRUE(Writer{}.Save(f.path));
	Reader r;
	EXPECT_TRUE(r.Open(f.path));
	EXPECT_TRUE(r.List().empty());
}

TEST(ReaderTests, ListReturnsTheDirectoryInOrder) {
	TempFile f("ReaderList.assets");
	const Bytes file = BuildSample(f.path);
	Reader r;
	ASSERT_TRUE(r.Open(f.path));

	const auto& list = r.List();
	ASSERT_EQ(list.size(), 3u);
	const std::uint64_t ids[]   = { 10, 20, 30 };
	const std::uint32_t types[] = { 1, 2, 3 };
	const std::uint64_t sizes[] = { 3, 2, 1 };
	const std::uint64_t offs[]  = { kHeaderSize, kHeaderSize + 3, kHeaderSize + 5 };
	for (std::size_t i = 0; i < 3; ++i) {
		EXPECT_EQ(list[i].id, ids[i]);
		EXPECT_EQ(list[i].type, types[i]);
		EXPECT_EQ(list[i].size, sizes[i]);
		EXPECT_EQ(list[i].offset, offs[i]);
		EXPECT_EQ(list[i].crc, GetLE(file, EntryPos(file, i) + kEntCrc, 4));
	}
}

TEST(ReaderTests, ReaderCanBeReusedForAnotherArchive) {
	TempFile a("ReaderReuseA.assets"), b("ReaderReuseB.assets");
	BuildSample(a.path);
	Writer w;
	ASSERT_TRUE(w.Add(99, 0, Bytes{ 7 }));
	ASSERT_TRUE(w.Save(b.path));

	Reader r;
	ASSERT_TRUE(r.Open(a.path));
	ASSERT_TRUE(r.Open(b.path));
	ASSERT_EQ(r.List().size(), 1u);
	EXPECT_EQ(r.List()[0].id, 99u);
	EXPECT_EQ(*r.Load(99), (Bytes{ 7 }));
	EXPECT_FALSE(r.Load(10).has_value()); // belongs to the first archive
}

TEST(ReaderTests, TwoReadersCanOpenTheSameFile) {
	TempFile f("ReaderShared.assets");
	BuildSample(f.path);
	Reader a, b;
	ASSERT_TRUE(a.Open(f.path));
	ASSERT_TRUE(b.Open(f.path));
	EXPECT_EQ(*a.Load(20), *b.Load(20));
}

// ===========================================================================
// Open: invalid archives
// ===========================================================================

TEST(ReaderTests, MissingFileFails) {
	Reader r;
	EXPECT_FALSE(r.Open("UnitTests/DoesNotExist.assets"));
	EXPECT_EQ(r.error, "cannot open file");
	EXPECT_TRUE(r.List().empty());
}

TEST(ReaderTests, EmptyFileFails) {
	TempFile f("ReaderZeroBytes.assets");
	Reader r;
	EXPECT_FALSE(OpenBytes(r, f.path, Bytes{}));
	EXPECT_EQ(r.error, "truncated header");
}

TEST(ReaderTests, FileShorterThanTheHeaderFails) {
	TempFile f("ReaderShortHeader.assets");
	const Bytes file = BuildSample(f.path);
	Reader r;
	EXPECT_FALSE(OpenBytes(r, f.path, Bytes(file.begin(), file.begin() + kHeaderSize - 1)));
	EXPECT_EQ(r.error, "truncated header");
}

TEST(ReaderTests, WrongMagicFails) {
	TempFile f("ReaderMagic.assets");
	Bytes file = BuildSample(f.path);
	file[0] ^= 0xFF;
	Reader r;
	EXPECT_FALSE(OpenBytes(r, f.path, file));
	EXPECT_EQ(r.error, "bad magic");
}

TEST(ReaderTests, ATextFileIsNotAnArchive) {
	TempFile f("ReaderText.assets");
	Reader r;
	EXPECT_FALSE(OpenBytes(r, f.path, ToBytes("this is definitely not a binary archive, just some text")));
	EXPECT_EQ(r.error, "bad magic");
}

TEST(ReaderTests, VersionZeroIsRejected) {
	TempFile f("ReaderVersion0.assets");
	Bytes file = BuildSample(f.path);
	PatchLE(file, kOffVersion, 0, 2);
	Reader r;
	EXPECT_FALSE(OpenBytes(r, f.path, file));
	EXPECT_EQ(r.error, "unsupported version");
}

TEST(ReaderTests, AVersionNewerThanTheLibraryIsRejected) {
	TempFile f("ReaderVersionNew.assets");
	Bytes file = BuildSample(f.path);
	PatchLE(file, kOffVersion, PUMPKIN_AE_VERSION + 1, 2);
	Reader r;
	EXPECT_FALSE(OpenBytes(r, f.path, file));
	EXPECT_EQ(r.error, "unsupported version");
}

TEST(ReaderTests, TheCurrentVersionIsAccepted) {
	TempFile f("ReaderVersionCurrent.assets");
	Bytes file = BuildSample(f.path);
	PatchLE(file, kOffVersion, PUMPKIN_AE_VERSION, 2);
	Reader r;
	EXPECT_TRUE(OpenBytes(r, f.path, file));
}

TEST(ReaderTests, DirectoryOffsetInsideTheHeaderFails) {
	TempFile f("ReaderDirInHeader.assets");
	Bytes file = BuildSample(f.path);
	PatchLE(file, kOffDirOffset, kHeaderSize - 1, 8);
	Reader r;
	EXPECT_FALSE(OpenBytes(r, f.path, file));
	EXPECT_EQ(r.error, "bad directory offset");
}

TEST(ReaderTests, DirectoryOffsetPastTheEndFails) {
	TempFile f("ReaderDirPastEnd.assets");
	Bytes file = BuildSample(f.path);
	PatchLE(file, kOffDirOffset, file.size() + 1, 8);
	Reader r;
	EXPECT_FALSE(OpenBytes(r, f.path, file));
	EXPECT_EQ(r.error, "bad directory offset");
}

TEST(ReaderTests, HugeDirectoryOffsetFails) {
	TempFile f("ReaderDirHuge.assets");
	Bytes file = BuildSample(f.path);
	PatchLE(file, kOffDirOffset, std::numeric_limits<std::uint64_t>::max(), 8);
	Reader r;
	EXPECT_FALSE(OpenBytes(r, f.path, file));
	EXPECT_EQ(r.error, "bad directory offset");
}

TEST(ReaderTests, CountLargerThanTheFileFails) {
	TempFile f("ReaderCountBig.assets");
	Bytes file = BuildSample(f.path);
	PatchLE(file, kOffCount, 4, 4);
	Reader r;
	EXPECT_FALSE(OpenBytes(r, f.path, file));
	EXPECT_EQ(r.error, "bad directory size");
}

TEST(ReaderTests, HugeCountFailsWithoutAllocating) {
	TempFile f("ReaderCountHuge.assets");
	Bytes file = BuildSample(f.path);
	PatchLE(file, kOffCount, 0xFFFFFFFFull, 4);
	Reader r;
	EXPECT_FALSE(OpenBytes(r, f.path, file));
	EXPECT_EQ(r.error, "bad directory size");
}

TEST(ReaderTests, CountSmallerThanTheDirectoryFails) {
	// The directory must end exactly at the end of the file.
	TempFile f("ReaderCountSmall.assets");
	Bytes file = BuildSample(f.path);
	PatchLE(file, kOffCount, 2, 4);
	Reader r;
	EXPECT_FALSE(OpenBytes(r, f.path, file));
	EXPECT_EQ(r.error, "bad directory size");
}

TEST(ReaderTests, TrailingBytesAfterTheDirectoryFail) {
	TempFile f("ReaderTrailing.assets");
	Bytes file = BuildSample(f.path);
	file.push_back(0);
	Reader r;
	EXPECT_FALSE(OpenBytes(r, f.path, file));
	EXPECT_EQ(r.error, "bad directory size");
}

TEST(ReaderTests, TruncatedDirectoryFails) {
	TempFile f("ReaderTruncDir.assets");
	Bytes file = BuildSample(f.path);
	file.pop_back();
	Reader r;
	EXPECT_FALSE(OpenBytes(r, f.path, file));
	EXPECT_EQ(r.error, "bad directory size");
}

TEST(ReaderTests, EveryTruncationOfTheFileIsRejected) {
	TempFile f("ReaderEveryTrunc.assets");
	const Bytes full = BuildSample(f.path);
	for (std::size_t n = 0; n < full.size(); ++n) {
		Reader r;
		EXPECT_FALSE(OpenBytes(r, f.path, Bytes(full.begin(), full.begin() + static_cast<std::ptrdiff_t>(n))))
			<< "a file cut to " << n << " bytes was accepted";
		EXPECT_TRUE(r.List().empty());
	}
}

TEST(ReaderTests, EntryOffsetInsideTheHeaderFails) {
	TempFile f("ReaderEntryOffLow.assets");
	Bytes file = BuildSample(f.path);
	PatchLE(file, EntryPos(file, 1) + kEntOffset, 0, 8);
	Reader r;
	EXPECT_FALSE(OpenBytes(r, f.path, file));
	EXPECT_EQ(r.error, "entry out of bounds");
}

TEST(ReaderTests, EntryOffsetPastTheDirectoryFails) {
	TempFile f("ReaderEntryOffHigh.assets");
	Bytes file = BuildSample(f.path);
	PatchLE(file, EntryPos(file, 0) + kEntOffset, DirOffset(file) + 1, 8);
	Reader r;
	EXPECT_FALSE(OpenBytes(r, f.path, file));
	EXPECT_EQ(r.error, "entry out of bounds");
}

TEST(ReaderTests, EntryReachingIntoTheDirectoryFails) {
	TempFile f("ReaderEntryOverlap.assets");
	Bytes file = BuildSample(f.path);
	PatchLE(file, EntryPos(file, 2) + kEntSize, 2, 8); // last asset: 1 byte -> 2 bytes
	Reader r;
	EXPECT_FALSE(OpenBytes(r, f.path, file));
	EXPECT_EQ(r.error, "entry out of bounds");
}

TEST(ReaderTests, EntryEndingExactlyAtTheDirectoryIsAccepted) {
	TempFile f("ReaderEntryExact.assets");
	Bytes file = BuildSample(f.path);
	Reader r;
	EXPECT_TRUE(OpenBytes(r, f.path, file)); // the last asset already ends where the directory starts
	EXPECT_EQ(r.List().back().offset + r.List().back().size, DirOffset(file));
}

TEST(ReaderTests, HugeEntrySizeFailsWithoutOverflowing) {
	TempFile f("ReaderEntrySizeHuge.assets");
	Bytes file = BuildSample(f.path);
	PatchLE(file, EntryPos(file, 0) + kEntSize, std::numeric_limits<std::uint64_t>::max(), 8);
	Reader r;
	EXPECT_FALSE(OpenBytes(r, f.path, file));
	EXPECT_EQ(r.error, "entry out of bounds");
}

TEST(ReaderTests, OffsetPlusSizeOverflowingUint64Fails) {
	TempFile f("ReaderEntryWrap.assets");
	Bytes file = BuildSample(f.path);
	PatchLE(file, EntryPos(file, 0) + kEntOffset, std::numeric_limits<std::uint64_t>::max() - 1, 8);
	PatchLE(file, EntryPos(file, 0) + kEntSize, 10, 8); // offset + size wraps around to a small number
	Reader r;
	EXPECT_FALSE(OpenBytes(r, f.path, file));
	EXPECT_EQ(r.error, "entry out of bounds");
}

TEST(ReaderTests, DuplicateIdsFail) {
	TempFile f("ReaderDuplicateIds.assets");
	Bytes file = BuildSample(f.path);
	PatchLE(file, EntryPos(file, 2) + kEntId, 10, 8); // same id as entry 0
	Reader r;
	EXPECT_FALSE(OpenBytes(r, f.path, file));
	EXPECT_EQ(r.error, "duplicate ID");
}

TEST(ReaderTests, AFailedOpenLeavesTheReaderEmpty) {
	TempFile good("ReaderAfterFailGood.assets"), bad("ReaderAfterFailBad.assets");
	BuildSample(good.path);
	Bytes file = ReadFileBytes(good.path);
	file[0] ^= 0xFF;

	Reader r;
	ASSERT_TRUE(r.Open(good.path));
	ASSERT_FALSE(OpenBytes(r, bad.path, file));
	EXPECT_TRUE(r.List().empty());
	EXPECT_FALSE(r.Load(10).has_value());
	EXPECT_FALSE(r.error.empty());
}

TEST(ReaderTests, ASuccessfulOpenClearsThePreviousError) {
	TempFile f("ReaderClearsError.assets");
	BuildSample(f.path);
	Reader r;
	ASSERT_FALSE(r.Open("UnitTests/DoesNotExist.assets"));
	ASSERT_FALSE(r.error.empty());
	ASSERT_TRUE(r.Open(f.path));
	EXPECT_TRUE(r.error.empty());
}

// ===========================================================================
// Load
// ===========================================================================

TEST(ReaderTests, LoadReturnsThePayload) {
	TempFile f("ReaderLoad.assets");
	BuildSample(f.path);
	Reader r;
	ASSERT_TRUE(r.Open(f.path));
	EXPECT_EQ(*r.Load(10), (Bytes{ 1, 2, 3 }));
	EXPECT_EQ(*r.Load(20), (Bytes{ 4, 5 }));
	EXPECT_EQ(*r.Load(30), (Bytes{ 6 }));
	EXPECT_TRUE(r.error.empty());
}

TEST(ReaderTests, LoadWorksInAnyOrderAndRepeatedly) {
	TempFile f("ReaderLoadOrder.assets");
	BuildSample(f.path);
	Reader r;
	ASSERT_TRUE(r.Open(f.path));
	EXPECT_EQ(*r.Load(30), (Bytes{ 6 }));
	EXPECT_EQ(*r.Load(10), (Bytes{ 1, 2, 3 }));
	EXPECT_EQ(*r.Load(30), (Bytes{ 6 }));
	EXPECT_EQ(*r.Load(20), (Bytes{ 4, 5 }));
}

TEST(ReaderTests, LoadOfAnUnknownIdFails) {
	TempFile f("ReaderLoadUnknown.assets");
	BuildSample(f.path);
	Reader r;
	ASSERT_TRUE(r.Open(f.path));
	EXPECT_FALSE(r.Load(11).has_value());
	EXPECT_EQ(r.error, "unknown ID");
}

TEST(ReaderTests, LoadBeforeOpenFails) {
	Reader r;
	EXPECT_FALSE(r.Load(1).has_value());
	EXPECT_EQ(r.error, "unknown ID");
}

TEST(ReaderTests, ALoadClearsThePreviousError) {
	TempFile f("ReaderLoadClears.assets");
	BuildSample(f.path);
	Reader r;
	ASSERT_TRUE(r.Open(f.path));
	ASSERT_FALSE(r.Load(11).has_value());
	ASSERT_FALSE(r.error.empty());
	ASSERT_TRUE(r.Load(10).has_value());
	EXPECT_TRUE(r.error.empty());
}

TEST(ReaderTests, LoadOfAnEmptyPayloadReturnsAnEmptyVector) {
	TempFile f("ReaderLoadEmpty.assets");
	Writer w;
	ASSERT_TRUE(w.Add(1, 0, Bytes{}));
	ASSERT_TRUE(w.Save(f.path));
	Reader r;
	ASSERT_TRUE(r.Open(f.path));
	const auto data = r.Load(1);
	ASSERT_TRUE(data.has_value());
	EXPECT_TRUE(data->empty());
}

TEST(ReaderTests, ACorruptedPayloadFailsItsCrcCheck) {
	TempFile f("ReaderCrc.assets");
	Bytes file = BuildSample(f.path);
	file[kHeaderSize + 3] ^= 0x01; // first byte of asset 20
	Reader r;
	ASSERT_TRUE(OpenBytes(r, f.path, file));
	EXPECT_FALSE(r.Load(20).has_value());
	EXPECT_EQ(r.error, "CRC mismatch");
}

TEST(ReaderTests, ACorruptedPayloadDoesNotAffectTheOthers) {
	TempFile f("ReaderCrcIsolated.assets");
	Bytes file = BuildSample(f.path);
	file[kHeaderSize + 3] ^= 0xFF;
	Reader r;
	ASSERT_TRUE(OpenBytes(r, f.path, file));
	EXPECT_FALSE(r.Load(20).has_value());
	EXPECT_EQ(*r.Load(10), (Bytes{ 1, 2, 3 }));
	EXPECT_EQ(*r.Load(30), (Bytes{ 6 }));
}

TEST(ReaderTests, ACorruptedCrcFieldFailsTheCheck) {
	TempFile f("ReaderCrcField.assets");
	Bytes file = BuildSample(f.path);
	file[EntryPos(file, 0) + kEntCrc] ^= 0x01;
	Reader r;
	ASSERT_TRUE(OpenBytes(r, f.path, file));
	EXPECT_FALSE(r.Load(10).has_value());
	EXPECT_EQ(r.error, "CRC mismatch");
}

TEST(ReaderTests, ALoadStillWorksAfterAFailedOne) {
	TempFile f("ReaderLoadAfterFail.assets");
	Bytes file = BuildSample(f.path);
	file[kHeaderSize] ^= 0x01;
	Reader r;
	ASSERT_TRUE(OpenBytes(r, f.path, file));
	ASSERT_FALSE(r.Load(10).has_value());
	EXPECT_EQ(*r.Load(30), (Bytes{ 6 }));
}

TEST(ReaderTests, LargePayloadRoundTrips) {
	TempFile f("ReaderLarge.assets");
	Bytes big(1 << 20);
	for (std::size_t i = 0; i < big.size(); ++i) big[i] = static_cast<std::uint8_t>((i * 2654435761u) >> 24);
	Writer w;
	ASSERT_TRUE(w.Add(1, 0, big));
	ASSERT_TRUE(w.Save(f.path));
	Reader r;
	ASSERT_TRUE(r.Open(f.path));
	EXPECT_EQ(*r.Load(1), big);
}

TEST(ReaderTests, EntryTypeAndIdArePreservedExactly) {
	TempFile f("ReaderTypes.assets");
	Writer w;
	ASSERT_TRUE(w.Add(std::numeric_limits<std::uint64_t>::max(), 0xFFFFFFFFu, Bytes{ 1 }));
	ASSERT_TRUE(w.Add(0, 0, Bytes{ 2 }));
	ASSERT_TRUE(w.Save(f.path));
	Reader r;
	ASSERT_TRUE(r.Open(f.path));
	EXPECT_EQ(r.List()[0].id, std::numeric_limits<std::uint64_t>::max());
	EXPECT_EQ(r.List()[0].type, 0xFFFFFFFFu);
	EXPECT_EQ(*r.Load(std::numeric_limits<std::uint64_t>::max()), (Bytes{ 1 }));
	EXPECT_EQ(*r.Load(0), (Bytes{ 2 }));
}

// ===========================================================================
// Typed Load
// ===========================================================================

TEST(ReaderTests, TypedLoadDecodesTheValue) {
	TempFile f("ReaderTyped.assets");
	Writer w;
	ASSERT_TRUE(w.Add(1, 0, Position{ 3, 4 }));
	ASSERT_TRUE(w.Save(f.path));
	Reader r;
	ASSERT_TRUE(r.Open(f.path));
	const auto p = r.Load<Position>(1);
	ASSERT_TRUE(p.has_value());
	EXPECT_EQ(p->x, 3);
	EXPECT_EQ(p->y, 4);
	EXPECT_TRUE(r.error.empty());
}

TEST(ReaderTests, TypedLoadOfAnUnknownIdFails) {
	TempFile f("ReaderTypedUnknown.assets");
	BuildSample(f.path);
	Reader r;
	ASSERT_TRUE(r.Open(f.path));
	EXPECT_FALSE(r.Load<Position>(999).has_value());
	EXPECT_EQ(r.error, "unknown ID");
}

TEST(ReaderTests, TypedLoadOfAPayloadThatIsTooLongFails) {
	TempFile f("ReaderTypedLong.assets");
	Writer w;
	ASSERT_TRUE(w.Add(1, 0, Material{ "abc", 0.5f })); // 11 bytes: Position only needs 8
	ASSERT_TRUE(w.Save(f.path));
	Reader r;
	ASSERT_TRUE(r.Open(f.path));
	EXPECT_FALSE(r.Load<Position>(1).has_value());
	EXPECT_EQ(r.error, "payload does not match the layout of the requested type");
}

TEST(ReaderTests, TypedLoadOfAPayloadThatIsTooShortFails) {
	TempFile f("ReaderTypedShort.assets");
	Writer w;
	ASSERT_TRUE(w.Add(1, 0, Bytes{ 1, 2, 3 }));
	ASSERT_TRUE(w.Save(f.path));
	Reader r;
	ASSERT_TRUE(r.Open(f.path));
	EXPECT_FALSE(r.Load<Position>(1).has_value());
	EXPECT_EQ(r.error, "payload does not match the layout of the requested type");
}

TEST(ReaderTests, TypedLoadOfTheWrongTypeFails) {
	TempFile f("ReaderTypedWrong.assets");
	Writer w;
	ASSERT_TRUE(w.Add(1, 0, Position{ 1000, 2 })); // as a Material: a 1000 byte string that isn't there
	ASSERT_TRUE(w.Save(f.path));
	Reader r;
	ASSERT_TRUE(r.Open(f.path));
	EXPECT_FALSE(r.Load<Material>(1).has_value());
	EXPECT_FALSE(r.error.empty());
}

TEST(ReaderTests, TypedLoadReportsACrcMismatch) {
	TempFile f("ReaderTypedCrc.assets");
	Writer w;
	ASSERT_TRUE(w.Add(1, 0, Position{ 3, 4 }));
	ASSERT_TRUE(w.Save(f.path));
	Bytes file = ReadFileBytes(f.path);
	file[kHeaderSize] ^= 0x01;
	Reader r;
	ASSERT_TRUE(OpenBytes(r, f.path, file));
	EXPECT_FALSE(r.Load<Position>(1).has_value());
	EXPECT_EQ(r.error, "CRC mismatch");
}

TEST(ReaderTests, TypedLoadWorksWithCustomCodecs) {
	TempFile f("ReaderTypedCodec.assets");
	Writer w;
	ASSERT_TRUE(w.Add(1, 0, Legacy{ 300 }));
	ASSERT_TRUE(w.Add(2, 0, Opaque{ 77 }));
	ASSERT_TRUE(w.Save(f.path));
	Reader r;
	ASSERT_TRUE(r.Open(f.path));
	EXPECT_EQ(r.Load<Legacy>(1)->a, 300);
	EXPECT_EQ(r.Load<Opaque>(2)->v, 77);
}

TEST(ReaderTests, TypedLoadOfAFullInventory) {
	TempFile f("ReaderTypedInventory.assets");
	const auto steel = std::make_shared<Material>(Material{ "steel", 0.2f });
	Inventory inv;
	for (std::uint64_t i = 0; i < 3; ++i) {
		auto item = std::make_unique<Item>();
		item->id = i;
		item->name = "item" + std::to_string(i);
		item->tags = { "t" };
		item->position = std::make_unique<Position>(Position{ static_cast<int>(i), 0 });
		item->material = steel;
		inv.items.push_back(std::move(item));
	}
	inv.equipped = inv.items[2].get();

	Writer w;
	ASSERT_TRUE(w.Add(1, 0, inv));
	ASSERT_TRUE(w.Save(f.path));

	Reader r;
	ASSERT_TRUE(r.Open(f.path));
	const auto back = r.Load<Inventory>(1);
	ASSERT_TRUE(back.has_value());
	ASSERT_EQ(back->items.size(), 3u);
	EXPECT_EQ(back->equipped, back->items[2].get());
	EXPECT_EQ(back->items[1]->name, "item1");
	EXPECT_EQ(back->items[1]->position->x, 1);
	EXPECT_EQ(back->items[0]->material, back->items[2]->material);
	EXPECT_EQ(back->items[0]->material->name, "steel");
}

TEST(ReaderTests, SeveralTypedAssetsInOneArchive) {
	TempFile f("ReaderMixed.assets");
	Writer w;
	ASSERT_TRUE(w.Add(1, 1, Position{ 1, 2 }));
	ASSERT_TRUE(w.Add(2, 2, Material{ "wood", 0.9f }));
	ASSERT_TRUE(w.Add(3, 3, Bytes{ 0xCA, 0xFE }));
	ASSERT_TRUE(w.Save(f.path));

	Reader r;
	ASSERT_TRUE(r.Open(f.path));
	EXPECT_EQ(r.Load<Position>(1)->y, 2);
	EXPECT_EQ(r.Load<Material>(2)->name, "wood");
	EXPECT_EQ(*r.Load(3), (Bytes{ 0xCA, 0xFE }));
}
