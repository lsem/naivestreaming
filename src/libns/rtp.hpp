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
#include "types.hpp"

// https://datatracker.ietf.org/doc/html/rfc3550#section-5.1

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

// RFC does not define any extensions and leaves it open for implementations.
// For now, I don't need extensions but if I event need I would need to model it
// properly with C++. For now I just leave generic fields and raw data bytes
// just to be able to skip extension data. See 5.3.1 of RFC for details.
class RTP_HeaderExtension {
  std::byte header_bytes[2];
  uint16_t length{};
  std::vector<std::byte> data;
};

// This is the size that client needs to allocate to accomodate header. If we
// want to use extensions, they must be calculated separately for extensions
// that are used.
constexpr size_t RTP_PacketHeader_Size = 12;

// The total size of header with extension would be RTP_PacketHeader_Size +
// RTP_HeaderExtensionFixed_Size + profile-specific extension length (can be
// even variable length). See 3.5.1 for details.
constexpr size_t RTP_HeaderExtensionFixed_Size = 4;

// This is non-RTP header that from RTP point of view is hidden in payload.
struct RTP_PayloadHeader {
  NAL_Type nal_type{};
  uint16_t first_mb{};
  uint16_t last_mb{};
  uint16_t flags{};
};

constexpr size_t RTP_PayloadHeader_Size = 7;

std::ostream& operator<<(std::ostream& os, const RTP_PayloadHeader& h);

bool operator==(const RTP_PayloadHeader& lhs, const RTP_PayloadHeader& rhs);

std::error_code serialize_payload_header(const RTP_PayloadHeader& ph,
                                         std::span<uint8_t> buffer);
expected<RTP_PayloadHeader> deserialize_payload_header(
    std::span<const uint8_t>);
