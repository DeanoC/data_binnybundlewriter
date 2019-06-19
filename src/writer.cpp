#include "al2o3_platform/platform.h"
#include "al2o3_memory/memory.h"
#include "al2o3_tinystl/string.hpp"
#include "al2o3_lz4/lz4.h"
#include "data_bufferutils/crc32c.h"
#include "data_binnybundle/bundle.h"
#include "data_binify/binify.h"
#include "data_binify/writehelper.hpp"

void InitHeader(Binify::WriteHelper &writer) {
	// write header
	writer.Comment("---------------------------------------------");
	writer.Comment("Bundle");
	writer.Comment("---------------------------------------------");
	writer.SetDefaultType<uint32_t>();
	writer.AddEnum("HeaderFlag");
	writer.AddEnumValue("HeaderFlag", "64Bit", DataBinnyBundle_HeaderFlag64Bit);

	writer.Comment("----------------------");
	writer.Comment("Bundle Header");

	writer.Write<uint32_t>(DATABINNYBUNDLE_ID("BUND"), "magic");
	writer.WriteFlags("HeaderFlag", DataBinnyBundle_HeaderFlag64Bit, "flags");
	writer.Write<uint16_t>(DataBinnyBundle_MajorVersion, DataBinnyBundle_MinorVersion, "Major, Minor Version");

	// micro offsets
	writer.WriteExpressionAs<uint16_t>("stringTable - beginEnd", "strings micro offset");
	writer.WriteExpressionAs<uint16_t>("chunks - stringTableEnd", "chunks micro offset");

	// 64 bit user data set anywhere last one used else 0
	writer.SetVariable("HeaderUserData", 0, true); // init variable to be set later
	writer.WriteExpressionAs<uint64_t>(writer.GetVariable("HeaderUserData"),
																		 "a 64 bit integer for whatever the user wants");

	// string table size
	writer.SizeOfBlock("stringTable");

	// directory entry count from the variable
	writer.SetVariable("DirEntryCount", 0, true); // init counter variable
	writer.WriteExpressionAs<uint32_t>(writer.GetVariable("DirEntryCount"), "Directory entry count");
}

void WriteChunkHeader(Binify::WriteHelper &writer, uint16_t majorVersion_, uint16_t minorVersion_) {
	writer.Comment("--------");
	writer.Comment("Chunk");
	writer.Comment("--------");
	writer.SetDefaultType<uint64_t>();

	writer.WriteLabel("chunk_begin", true);
	writer.SetStringTableBase("chunk_begin");
	writer.ReserveLabel("fixups");
	writer.ReserveLabel("data", true);
	writer.ReserveLabel("fixupsEnd");
	writer.ReserveLabel("dataEnd");

	writer.SizeOfBlock("fixups");
	writer.SizeOfBlock("data");
	writer.UseLabel("fixups", "chunk_begin", false, false);
	writer.UseLabel("data", "chunk_begin", false, false);
	writer.WriteAs<uint16_t>(majorVersion_, "Major Version");
	writer.WriteAs<uint16_t>(minorVersion_, "Minor Version");
	writer.Align();
	writer.SetDefaultType<uint32_t>();
	writer.SetStringTableBase("data");
}

bool AddChunkInternal(tinystl::string const &name_,
											uint32_t id_,
											uint32_t flags_,
											tinystl::vector<uint32_t> const &dependencies_,
											tinystl::vector<uint8_t> const &bin_) {

	uint32_t const uncompressedCrc32c = CRC32C_Calculate(0, bin_.data(), bin_.size());
	size_t const maxSize = LZ4_CompressionBoundFromInputSize((int) bin_.size());

	tinystl::vector<uint8_t> *compressedData = new tinystl::vector<uint8_t>(maxSize);
	{
		int okay = LZ4_Compress(bin_.data(),
														compressedData->data(),
														bin_.size(),
														compressedData->size());
		if (!okay)
			return false;
		compressedData->resize(okay);
	}

	// if compression made this block bigger, use the uncompressed data and mark it by
	// having uncompressed size == 0 in the file
	if (compressedData->size() >= bin_.size()) {
		*compressedData = bin_;
	}

	ASSERT(false);//TODO
/*	DirEntryWriter entry =
			{
					id_,
					flags_,
					name_,
					compressedData->size(),
					bin_.size(),
					CRC32C_Calculate(0, compressedData->data(), compressedData->size()),
					uncompressedCrc32c,
					compressedData,
					dependencies_
			};

	dirEntries.push_back(entry);
	o.reserve_label(name_ + "chunk"s);*/
	return true;
}

typedef bool (*ChunkWriter)(Binify::WriteHelper &writer);

bool AddChunk(tinystl::string const &name_,
							uint32_t id_,
							uint16_t majorVersion_,
							uint16_t minorVersion_,
							uint32_t flags_,
							tinystl::vector<uint32_t> const &dependencies_,
							ChunkWriter chunkWriter) {
	Binify::WriteHelper h;

	// add chunk header
	WriteChunkHeader(h, majorVersion_, minorVersion_);

	h.WriteLabel("data");
	chunkWriter(h);
	h.FinishStringTable();
	h.Align();
	h.WriteLabel("dataEnd");
	h.WriteLabel("fixups");
	uint32_t const numFixups = (uint32_t) h.GetFixupCount();
	for (auto i = 0u; i < numFixups; ++i) {
		h.UseLabel(h.GetFixup(i), "", false, false);
	}
	h.WriteLabel("fixupsEnd");

	/*
	std::string binifyText = h.ostr.str();

	std::string log;
	std::vector<uint8_t> data;
	std::stringstream binData;
	bool okay = BINIFY_StringToOStream(binifyText, &binData, log);
	if(!okay) return false;
	std::string tmp = binData.str();
	data.resize(tmp.size());
	std::memcpy(data.data(), tmp.data(), tmp.size());

	if (!log.empty() || logBinifyText)
	{
		LOG_S(INFO) << name_ << "_Chunk:" << log;
		LOG_S(INFO) << std::endl << binifyText;
	}
	return addChunkInternal(name_, id_, flags_, dependencies_, data);
	 */
	return true;
}