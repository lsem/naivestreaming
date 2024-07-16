////////////////////////////////////////////////////////////
// Generic RTP types and routines.
////////////////////////////////////////////////////////////
#pragma once
#include <cstdint>
#include <iosfwd>
#include <span>
#include <system_error>
#include <vector>
#include "defs.hpp"

// https://datatracker.ietf.org/doc/html/rfc3550#section-5.1

constexpr size_t RTP_PacketHeader_Size = 12;
struct RTP_PacketHeader {
  unsigned version{};
  bool padding_bit{};
  bool extension_bit{};
  bool marker_bit{};
  unsigned payload_type{};
  uint16_t sequence_num{};
  uint32_t timestamp{};
  uint32_t ssrc{};
  std::vector<uint32_t> csrc;
};

std::error_code serialize_rtp_header_to(const RTP_PacketHeader& ph,
                                        std::span<uint8_t> buffer);
expected<RTP_PacketHeader> deserialize_rtp_header_from(
    std::span<const uint8_t> data);

bool operator==(const RTP_PacketHeader& lhs, const RTP_PacketHeader& rhs);

std::ostream& operator<<(std::ostream& os, const RTP_PacketHeader&);
