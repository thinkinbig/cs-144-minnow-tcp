#include "wrapping_integers.hh"
#include "debug.hh"

using namespace std;

// 定义32位轮次常量
static constexpr uint64_t UINT32_ROUND = 1ULL << 32;
static constexpr uint64_t UINT32_MAX_MASK = UINT32_ROUND - 1;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + static_cast<uint32_t>(n);
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
    uint32_t offset = raw_value_ - zero_point.raw_value_;
    uint64_t base = checkpoint & ~UINT32_MAX_MASK;
    uint64_t candidate = base | offset;

    uint64_t next_round = candidate + UINT32_ROUND;
    uint64_t prev_round = (candidate >= UINT32_ROUND) ? candidate - UINT32_ROUND : candidate;
    
    int64_t dist_curr = abs(int64_t(candidate - checkpoint));
    int64_t dist_next = abs(int64_t(next_round - checkpoint));
    int64_t dist_prev = abs(int64_t(prev_round - checkpoint));
    
    if (dist_next < dist_curr && dist_next < dist_prev) {
        return next_round;
    }
    if (dist_prev < dist_curr && dist_prev < dist_next) {
        return prev_round;
    }
    return candidate;
}
