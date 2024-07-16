#include "rtp.hpp"
#include "log.hpp"

#include <arpa/inet.h>
#include <type_traits>

namespace {
template <class T>
T to_n(T v) {
  if constexpr (std::is_same_v<T, uint16_t>) {
    return htons(v);
  } else if constexpr (std::is_same_v<T, uint32_t>) {
    return htonl(v);
  } else {
    // Note, static_assert should depdent on T, is_same<T, int> is work around.
    static_assert(std::is_same_v<T, void>, "Unsupported type");
  }
}

template <class T>
T to_h(T v) {
  if constexpr (std::is_same_v<T, uint16_t>) {
    return ntohs(v);
  } else if constexpr (std::is_same_v<T, uint32_t>) {
    return ntohl(v);
  } else {
    // Note, static_assert should depdent on T, is_same<T, int> is work around.
    static_assert(std::is_same_v<T, void>, "Unsupported type");
  }
}
}  // namespace

std::error_code serialize_to(const RTP_PacketHeader& ph,
                             std::span<uint8_t> buffer) {
  // TODO: check size of buffer.
  if (buffer.size() < 12) {
    LOG_ERROR("Minimum buffer size for RTP header is 12, there is: {}",
              buffer.size());
    return make_error_code(std::errc::invalid_argument);
  }

  if (ph.version > 3) {
    LOG_ERROR("version cannot exceed 2 bits");
    return make_error_code(std::errc::invalid_argument);
  }

  buffer[0] = 0;

  buffer[0] |= static_cast<uint8_t>(ph.version) << 6;

  if (ph.padding_bit)
    buffer[0] |= (1 << 5);

  if (ph.extension_bit)
    buffer[0] |= (1 << 4);

  if (ph.csrc.size() > 15) {
    LOG_ERROR("CSRC count cannot exceed value of 15, actual: {}",
              ph.csrc.size());
    return make_error_code(std::errc::invalid_argument);
  }

  buffer[0] |= static_cast<uint8_t>(ph.csrc.size());

  buffer[1] = 0;

  if (ph.marker_bit)
    buffer[1] |= (1 << 7);

  if (ph.payload_type > 127) {
    LOG_ERROR("Payload type cannot exceed 7 bits: {}", ph.payload_type);
    return make_error_code(std::errc::invalid_argument);
  }

  buffer[1] |= static_cast<uint8_t>(ph.payload_type);

  {
    const uint16_t n_seq_num = to_n(ph.sequence_num);
    buffer[2] = n_seq_num & 0x00FF;
    buffer[3] = n_seq_num >> 8;
  }
  {
    const uint32_t n_timestamp = to_n(ph.timestamp);
    buffer[4] = (n_timestamp & 0x000000FF);
    buffer[5] = (n_timestamp & 0x0000FFFF) >> 8;
    buffer[6] = (n_timestamp & 0x00FFFFFF) >> 16;
    buffer[7] = n_timestamp >> 24;
  }
  {
    const uint32_t n_ssrc = to_n(ph.ssrc);
    buffer[8] = (n_ssrc & 0x000000FF);
    buffer[9] = (n_ssrc & 0x0000FFFF) >> 8;
    buffer[10] = (n_ssrc & 0x00FFFFFF) >> 16;
    buffer[11] = n_ssrc >> 24;
  }

  if (ph.csrc.size() != 0) {
    // TODO: we are not writing the code we are not going to use.
    LOG_ERROR("CSRC not supported yet");
    return make_error_code(std::errc::protocol_not_supported);
  }

  return {};
}
expected<RTP_PacketHeader> deserialize_from(std::span<const uint8_t> data) {
  if (data.size() < 12) {
    LOG_ERROR("rtp header cannot be smaller than 12 bytes, there is {}",
              data.size());
    return tl::unexpected(make_error_code(std::errc::invalid_argument));
  }

  RTP_PacketHeader new_header;

  new_header.version = data[0] >> 6;
  // TODO: decide if we want to validate version on this level or let client do
  // this.

  new_header.padding_bit = (data[0] & 0x20) >> 5;
  new_header.extension_bit = (data[0] & 0x10) >> 4;

  // Skipping crcs.size (4 bits).

  new_header.marker_bit = data[1] >> 7;
  new_header.payload_type = data[1] & 0x7F;

  {
    const uint16_t n_sequence_num =
        static_cast<uint16_t>(data[2]) | static_cast<uint16_t>(data[3]) << 8;
    new_header.sequence_num = to_h(n_sequence_num);
  }

  {
    const uint32_t n_timestamp = static_cast<uint32_t>(data[4]) |
                                 static_cast<uint32_t>(data[5]) << 8 |
                                 static_cast<uint32_t>(data[6]) << 16 |
                                 static_cast<uint32_t>(data[7]) << 24;
    new_header.timestamp = to_h(n_timestamp);
  }

  {
    const uint32_t n_ssrc = static_cast<uint32_t>(data[8]) |
                            static_cast<uint32_t>(data[9]) << 8 |
                            static_cast<uint32_t>(data[10]) << 16 |
                            static_cast<uint32_t>(data[11]) << 24;
    new_header.ssrc = to_h(n_ssrc);
  }
  // Ignore csrc list items..

  return new_header;
}

namespace {
auto make_tie(const RTP_PacketHeader& p) {
  return std::tie(p.version, p.padding_bit, p.extension_bit, p.marker_bit,
                  p.payload_type, p.sequence_num, p.timestamp, p.ssrc, p.csrc);
}
}  // namespace

bool operator==(const RTP_PacketHeader& lhs, const RTP_PacketHeader& rhs) {
  return make_tie(lhs) == make_tie(rhs);
}

std::ostream& operator<<(std::ostream& os, const RTP_PacketHeader& p) {
  os << "RTP_Packeteader{version: " << p.version
     << ", padding_bit: " << p.padding_bit
     << ", extension_bit: " << p.extension_bit
     << ", marker_bit: " << p.marker_bit << ", payload_type: " << p.payload_type
     << ", sequence_num: " << p.sequence_num << ", timestamp: " << p.timestamp
     << ", ssrc: " << p.ssrc << "}";
  return os;
}
