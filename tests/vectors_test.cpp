#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "crsf/crsf.hpp"

namespace {

struct VectorCase {
  std::string name;
  crsf::RcChannels channels{};
  std::vector<std::uint8_t> frame;
};

std::vector<std::uint8_t> parse_hex(const std::string& hex)
{
  std::vector<std::uint8_t> bytes;
  for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
    bytes.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
  }
  return bytes;
}

std::vector<VectorCase> load_cases()
{
  std::ifstream file(std::string{CRSF_TEST_VECTOR_DIR} + "/rc_channels.csv");
  EXPECT_TRUE(file.is_open()) << "missing generated vectors; run tests/vectors/generate_vectors.py";

  std::vector<VectorCase> cases;
  std::string line;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    std::istringstream fields(line);
    std::string name;
    std::string channels;
    std::string frame;
    std::getline(fields, name, ';');
    std::getline(fields, channels, ';');
    std::getline(fields, frame, ';');

    VectorCase entry;
    entry.name = name;
    std::istringstream channel_stream(channels);
    std::string value;
    std::size_t index = 0;
    while (std::getline(channel_stream, value, ',') && index < entry.channels.size()) {
      entry.channels[index++] = static_cast<std::uint16_t>(std::stoul(value));
    }
    entry.frame = parse_hex(frame);
    cases.push_back(std::move(entry));
  }
  return cases;
}

TEST(Vectors, FileIsPresentAndPopulated)
{
  const auto cases = load_cases();
  ASSERT_FALSE(cases.empty());
  for (const auto& entry : cases) {
    EXPECT_EQ(entry.frame.size(), 26U) << entry.name;
  }
}

// Our parser must decode frames produced by the independent Python generator.
TEST(Vectors, ParsesAndDecodesGeneratedFrames)
{
  for (const auto& entry : load_cases()) {
    crsf::Parser<> parser;
    auto status = crsf::ParseStatus::kIncomplete;
    for (const std::uint8_t byte : entry.frame) {
      status = parser.feed(byte);
    }
    ASSERT_EQ(status, crsf::ParseStatus::kFrameReady) << entry.name;
    EXPECT_EQ(parser.frame().type, static_cast<std::uint8_t>(crsf::FrameType::kRcChannelsPacked)) << entry.name;

    crsf::RcChannels decoded{};
    ASSERT_TRUE(crsf::decode_rc_channels(parser.frame().payload, decoded)) << entry.name;
    EXPECT_EQ(decoded, entry.channels) << entry.name;
  }
}

// Our builder must produce byte-identical frames to the independent generator, CRC included.
TEST(Vectors, BuilderReproducesGeneratedFramesByteForByte)
{
  for (const auto& entry : load_cases()) {
    std::array<std::uint8_t, crsf::kMaxFrameSize> built{};
    const std::size_t size = crsf::build_rc_channels_frame(crsf::kSyncByte, entry.channels, built);
    ASSERT_EQ(size, entry.frame.size()) << entry.name;
    EXPECT_TRUE(std::equal(entry.frame.begin(), entry.frame.end(), built.begin())) << entry.name;
  }
}

}  // namespace
