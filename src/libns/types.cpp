#include "types.hpp"

std::string to_string(NAL_Type nal_unit) {
  switch (nal_unit) {
    case NAL_Type::unknown:
      return "unknown";
    case NAL_Type::slice:
      return "slice";
    case NAL_Type::slice_dpa:
      return "slice_dpa";
    case NAL_Type::slice_dpb:
      return "slice_dpb";
    case NAL_Type::slice_dpc:
      return "slice_dpc";
    case NAL_Type::slice_idr:
      return "slice_idr";
    case NAL_Type::sei:
      return "sei";
    case NAL_Type::sps:
      return "sps";
    case NAL_Type::pps:
      return "pps";
    case NAL_Type::aud:
      return "aud";
    case NAL_Type::filler:
      return "filler";
    default:
      return "unknown";
  }
}
std::ostream& operator<<(std::ostream& os, NAL_Type v) {
  os << to_string(v);
  return os;
}
