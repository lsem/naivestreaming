#include <gtest/gtest.h>
#include <cstdlib>
#include <format>

#include "rtp.hpp"

TEST(rtp_tests, basic_serialize_test) {
  // The data for this test was captured from wireshark from Google Chrome.

  RTP_PacketHeader p;
  p.version = 2;
  p.padding_bit = false;
  p.extension_bit = true;
  p.marker_bit = true;
  p.payload_type = 45;
  p.sequence_num = 12927;
  p.timestamp = 1662400414;
  p.ssrc = 0xfe15124a;

  std::array<uint8_t, RTP_PacketHeader_Size> buff;
  auto ec = serialize_rtp_header_to(p, buff);
  ASSERT_FALSE(ec);

  EXPECT_EQ(buff[0], 0x90);
  EXPECT_EQ(buff[1], 0xad);
  EXPECT_EQ(buff[2], 0x32);
  EXPECT_EQ(buff[3], 0x7f);
  EXPECT_EQ(buff[4], 0x63);
  EXPECT_EQ(buff[5], 0x16);
  EXPECT_EQ(buff[6], 0x37);
  EXPECT_EQ(buff[7], 0x9e);
  EXPECT_EQ(buff[8], 0xfe);
  EXPECT_EQ(buff[9], 0x15);
  EXPECT_EQ(buff[10], 0x12);
  EXPECT_EQ(buff[11], 0x4a);
}

TEST(rtp_tests, basic_deserialize_test) {
  const std::array<uint8_t, RTP_PacketHeader_Size> data = {
      0x90, 0xad, 0x32, 0x7f, 0x63, 0x16, 0x37, 0x9e, 0xfe, 0x15, 0x12, 0x4a};

  RTP_PacketHeader p;
  p.version = 2;
  p.padding_bit = false;
  p.extension_bit = true;
  p.marker_bit = true;
  p.payload_type = 45;
  p.sequence_num = 12927;
  p.timestamp = 1662400414;
  p.ssrc = 0xfe15124a;

  auto maybe_deserialized_packet = deserialize_rtp_header_from(data);
  ASSERT_TRUE(maybe_deserialized_packet.has_value());
  ASSERT_EQ(maybe_deserialized_packet.value(), p);
}

TEST(rtp_tests, basic_routrip_test) {
  RTP_PacketHeader p;

  p.version = 2;
  p.padding_bit = 1;
  p.extension_bit = 0;
  p.marker_bit = 1;
  p.payload_type = 23;
  p.sequence_num = 100;
  p.timestamp = 200;
  p.ssrc = 300;

  std::array<uint8_t, RTP_PacketHeader_Size> buff;

  auto ec = serialize_rtp_header_to(p, buff);
  ASSERT_FALSE(ec);

  auto maybe_deserialized_packet = deserialize_rtp_header_from(buff);
  ASSERT_TRUE(maybe_deserialized_packet.has_value());
  ASSERT_EQ(maybe_deserialized_packet.value(), p);
}

TEST(rtp_tests, randomized_routrip_test) {
  const auto seed = time(nullptr);
  srand(seed);
  SCOPED_TRACE(std::format("Seed: {}", seed));

  for (int i = 0; i < 1000; ++i) {
    RTP_PacketHeader p;
    p.version = rand() % 3;
    p.padding_bit = static_cast<bool>(rand() % 2);
    p.extension_bit = static_cast<bool>(rand() % 2);
    p.marker_bit = static_cast<bool>(rand() % 2);
    p.payload_type = static_cast<unsigned>(rand() % 128);
    p.sequence_num = static_cast<uint16_t>(rand() % 0xFFFF);
    p.timestamp = static_cast<uint32_t>(rand() % 0xFFFFFFFF);
    p.ssrc = static_cast<uint32_t>(rand() % 0xFFFFFFFF);

    std::array<uint8_t, RTP_PacketHeader_Size> buff;

    auto ec = serialize_rtp_header_to(p, buff);
    ASSERT_FALSE(ec);

    auto maybe_deserialized_packet = deserialize_rtp_header_from(buff);
    ASSERT_TRUE(maybe_deserialized_packet.has_value());
    ASSERT_EQ(maybe_deserialized_packet.value(), p);
  }
}

TEST(rtp_tests, randomized_payload_header_routrip_test) {
  const auto seed = time(nullptr);
  srand(seed);
  SCOPED_TRACE(std::format("Seed: {}", seed));

  for (int i = 0; i < 1000; ++i) {
    RTP_PayloadHeader p;
    p.nal_type =
        static_cast<NAL_Type>((rand() % (static_cast<int>(NAL_Type::__end) -
                                         static_cast<int>(NAL_Type::__begin))));
    p.first_mb = rand() % 65535;
    p.last_mb = rand() % 65535;
    p.flags = rand() % 65535;

    std::array<uint8_t, RTP_PacketHeader_Size> buff;

    auto ec = serialize_payload_header(p, buff);
    ASSERT_FALSE(ec);

    auto maybe_deserialized_packet = deserialize_payload_header(buff);
    ASSERT_TRUE(maybe_deserialized_packet.has_value());
    ASSERT_EQ(maybe_deserialized_packet.value(), p);
  }
}
