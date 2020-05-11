// Copyright (c) 2014-2019, The Monero Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <boost/math/special_functions/round.hpp>

#include "int-util.h"
#include "crypto/hash.h"
#include "cryptonote_config.h"
#include "difficulty.h"

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "difficulty"

namespace cryptonote {

  using std::size_t;
  using std::uint64_t;
  using std::vector;

#if defined(__x86_64__)
  static inline void mul(uint64_t a, uint64_t b, uint64_t &low, uint64_t &high) {
    low = mul128(a, b, &high);
  }

#else

  static inline void mul(uint64_t a, uint64_t b, uint64_t &low, uint64_t &high) {
    // __int128 isn't part of the standard, so the previous function wasn't portable. mul128() in Windows is fine,
    // but this portable function should be used elsewhere. Credit for this function goes to latexi95.

    uint64_t aLow = a & 0xFFFFFFFF;
    uint64_t aHigh = a >> 32;
    uint64_t bLow = b & 0xFFFFFFFF;
    uint64_t bHigh = b >> 32;

    uint64_t res = aLow * bLow;
    uint64_t lowRes1 = res & 0xFFFFFFFF;
    uint64_t carry = res >> 32;

    res = aHigh * bLow + carry;
    uint64_t highResHigh1 = res >> 32;
    uint64_t highResLow1 = res & 0xFFFFFFFF;

    res = aLow * bHigh;
    uint64_t lowRes2 = res & 0xFFFFFFFF;
    carry = res >> 32;

    res = aHigh * bHigh + carry;
    uint64_t highResHigh2 = res >> 32;
    uint64_t highResLow2 = res & 0xFFFFFFFF;

    //Addition

    uint64_t r = highResLow1 + lowRes2;
    carry = r >> 32;
    low = (r << 32) | lowRes1;
    r = highResHigh1 + highResLow2 + carry;
    uint64_t d3 = r & 0xFFFFFFFF;
    carry = r >> 32;
    r = highResHigh2 + carry;
    high = d3 | (r << 32);
  }

#endif

  static inline bool cadd(uint64_t a, uint64_t b) {
    return a + b < a;
  }

  static inline bool cadc(uint64_t a, uint64_t b, bool c) {
    return a + b < a || (c && a + b == (uint64_t) -1);
  }

  bool check_hash_64(const crypto::hash &hash, uint64_t difficulty) {
    uint64_t low, high, top, cur;
    // First check the highest word, this will most likely fail for a random hash.
    mul(swap64le(((const uint64_t *) &hash)[3]), difficulty, top, high);
    if (high != 0) {
      return false;
    }
    mul(swap64le(((const uint64_t *) &hash)[0]), difficulty, low, cur);
    mul(swap64le(((const uint64_t *) &hash)[1]), difficulty, low, high);
    bool carry = cadd(cur, low);
    cur = high;
    mul(swap64le(((const uint64_t *) &hash)[2]), difficulty, low, high);
    carry = cadc(cur, low, carry);
    carry = cadc(high, top, carry);
    return !carry;
  }

  uint64_t next_difficulty_64(std::vector<std::uint64_t> timestamps, std::vector<uint64_t> cumulative_difficulties, size_t target_seconds) {

    if(timestamps.size() > DIFFICULTY_WINDOW)
    {
      timestamps.resize(DIFFICULTY_WINDOW);
      cumulative_difficulties.resize(DIFFICULTY_WINDOW);
    }


    size_t length = timestamps.size();
    assert(length == cumulative_difficulties.size());
    if (length <= 1) {
      return 1;
    }
    static_assert(DIFFICULTY_WINDOW >= 2, "Window is too small");
    assert(length <= DIFFICULTY_WINDOW);
    sort(timestamps.begin(), timestamps.end());
    size_t cut_begin, cut_end;
    static_assert(2 * DIFFICULTY_CUT <= DIFFICULTY_WINDOW - 2, "Cut length is too large");
    if (length <= DIFFICULTY_WINDOW - 2 * DIFFICULTY_CUT) {
      cut_begin = 0;
      cut_end = length;
    } else {
      cut_begin = (length - (DIFFICULTY_WINDOW - 2 * DIFFICULTY_CUT) + 1) / 2;
      cut_end = cut_begin + (DIFFICULTY_WINDOW - 2 * DIFFICULTY_CUT);
    }
    assert(/*cut_begin >= 0 &&*/ cut_begin + 2 <= cut_end && cut_end <= length);
    uint64_t time_span = timestamps[cut_end - 1] - timestamps[cut_begin];
    if (time_span == 0) {
      time_span = 1;
    }
    uint64_t total_work = cumulative_difficulties[cut_end - 1] - cumulative_difficulties[cut_begin];
    assert(total_work > 0);
    uint64_t low, high;
    mul(total_work, target_seconds, low, high);
    // blockchain errors "difficulty overhead" if this function returns zero.
    // TODO: consider throwing an exception instead
    if (high != 0 || low + time_span - 1 < low) {
      return 0;
    }
    return (low + time_span - 1) / time_span;
  }

#if defined(_MSC_VER)
#ifdef max
#undef max
#endif
#endif

  const difficulty_type max64bit(std::numeric_limits<std::uint64_t>::max());
  const boost::multiprecision::uint256_t max128bit(std::numeric_limits<boost::multiprecision::uint128_t>::max());
  const boost::multiprecision::uint512_t max256bit(std::numeric_limits<boost::multiprecision::uint256_t>::max());

#define FORCE_FULL_128_BITS

  bool check_hash_128(const crypto::hash &hash, difficulty_type difficulty) {
#ifndef FORCE_FULL_128_BITS
    // fast check
    if (difficulty >= max64bit && ((const uint64_t *) &hash)[3] > 0)
      return false;
#endif
    // usual slow check
    boost::multiprecision::uint512_t hashVal = 0;
#ifdef FORCE_FULL_128_BITS
    for(int i = 0; i < 4; i++) { // highest word is zero
#else
    for(int i = 1; i < 4; i++) { // highest word is zero
#endif
      hashVal <<= 64;
      hashVal |= swap64le(((const uint64_t *) &hash)[3 - i]);
    }
    return hashVal * difficulty <= max256bit;
  }

  bool check_hash(const crypto::hash &hash, difficulty_type difficulty) {
    if (difficulty <= max64bit) // if can convert to small difficulty - do it
      return check_hash_64(hash, difficulty.convert_to<std::uint64_t>());
    else
      return check_hash_128(hash, difficulty);
  }

  difficulty_type next_difficulty(std::vector<uint64_t> timestamps, std::vector<difficulty_type> cumulative_difficulties, size_t target_seconds) {
    //cutoff DIFFICULTY_LAG
    if(timestamps.size() > DIFFICULTY_WINDOW)
    {
      timestamps.resize(DIFFICULTY_WINDOW);
      cumulative_difficulties.resize(DIFFICULTY_WINDOW);
    }


    size_t length = timestamps.size();
    assert(length == cumulative_difficulties.size());
    if (length <= 1) {
      return 1;
    }
    static_assert(DIFFICULTY_WINDOW >= 2, "Window is too small");
    assert(length <= DIFFICULTY_WINDOW);
    sort(timestamps.begin(), timestamps.end());
    size_t cut_begin, cut_end;
    static_assert(2 * DIFFICULTY_CUT <= DIFFICULTY_WINDOW - 2, "Cut length is too large");
    if (length <= DIFFICULTY_WINDOW - 2 * DIFFICULTY_CUT) {
      cut_begin = 0;
      cut_end = length;
    } else {
      cut_begin = (length - (DIFFICULTY_WINDOW - 2 * DIFFICULTY_CUT) + 1) / 2;
      cut_end = cut_begin + (DIFFICULTY_WINDOW - 2 * DIFFICULTY_CUT);
    }
    assert(/*cut_begin >= 0 &&*/ cut_begin + 2 <= cut_end && cut_end <= length);
    uint64_t time_span = timestamps[cut_end - 1] - timestamps[cut_begin];
    if (time_span == 0) {
      time_span = 1;
    }
    difficulty_type total_work = cumulative_difficulties[cut_end - 1] - cumulative_difficulties[cut_begin];
    assert(total_work > 0);
    boost::multiprecision::uint256_t res =  (boost::multiprecision::uint256_t(total_work) * target_seconds + time_span - 1) / time_span;
    if(res > max128bit)
      return 0; // to behave like previous implementation, may be better return max128bit?
    return res.convert_to<difficulty_type>();
  }

  std::string hex(difficulty_type v)
  {
    static const char chars[] = "0123456789abcdef";
    std::string s;
    while (v > 0)
    {
      s.push_back(chars[(v & 0xf).convert_to<unsigned>()]);
      v >>= 4;
    }
    if (s.empty())
      s += "0";
    std::reverse(s.begin(), s.end());
    return "0x" + s;
  }

  // LWMA difficulty algorithm
  // Background:  https://github.com/zawy12/difficulty-algorithms/issues/3
  // Copyright (c) 2017-2018 Zawy
  difficulty_type next_difficulty_v2(std::vector<std::uint64_t> timestamps, std::vector<difficulty_type> cumulative_difficulties, size_t target_seconds) {
  
    const int64_t T = static_cast<int64_t>(target_seconds);
    size_t N = DIFFICULTY_WINDOW_V2;
    if (timestamps.size() < 4) { 
      return 1; 
    } else if ( timestamps.size() < N+1 ) { 
      N = timestamps.size() - 1;
    } else {  
      timestamps.resize(N+1);  
      cumulative_difficulties.resize(N+1);
    }
    const double adjust = 0.998;
    const double k = N * (N + 1) / 2;
    double LWMA(0), sum_inverse_D(0), harmonic_mean_D(0), nextDifficulty(0);
    int64_t solveTime(0);
    uint64_t difficulty(0), next_difficulty(0);
    for (size_t i = 1; i <= N; i++) {
      solveTime = static_cast<int64_t>(timestamps[i]) - static_cast<int64_t>(timestamps[i - 1]);
      solveTime = std::min<int64_t>((T * 7), std::max<int64_t>(solveTime, (-7 * T)));
      difficulty = static_cast<uint64_t>(cumulative_difficulties[i] - cumulative_difficulties[i - 1]);
      LWMA += (int64_t)(solveTime * i) / k;
      sum_inverse_D += 1 / static_cast<double>(difficulty);
    }
    harmonic_mean_D = N / sum_inverse_D;
    if (static_cast<int64_t>(boost::math::round(LWMA)) < T / 20)
      LWMA = static_cast<double>(T / 20);

    nextDifficulty = harmonic_mean_D * T / LWMA * adjust;
    next_difficulty = static_cast<uint64_t>(nextDifficulty);
    return next_difficulty;
  }

  // LWMA-2
  difficulty_type next_difficulty_v3(std::vector<uint64_t> timestamps, std::vector<difficulty_type> cumulative_difficulties) {
  
    int64_t  T = DIFFICULTY_TARGET_V2;
    int64_t  N = DIFFICULTY_WINDOW_V2;
    int64_t  L(0), ST, sum_3_ST(0), next_D, prev_D;
    assert(timestamps.size() == cumulative_difficulties.size() && timestamps.size() <= static_cast<uint64_t>(N+1) );
    for ( int64_t i = 1; i <= N; i++ ) {
      ST = static_cast<int64_t>(timestamps[i]) - static_cast<int64_t>(timestamps[i-1]);
      ST = std::max(-4*T, std::min(ST, 6*T));
      L +=  ST * i ; 
      if ( i > N-3 ) { 
        sum_3_ST += ST; 
      } 
    }
    next_D = (static_cast<int64_t>(cumulative_difficulties[N] - cumulative_difficulties[0])*T*(N+1)*99)/(100*2*L);
    prev_D = static_cast<int64_t>(cumulative_difficulties[N] - cumulative_difficulties[N-1]);
    next_D = std::max((prev_D*67)/100, std::min(next_D, (prev_D*150)/100));
    if ( sum_3_ST < (8*T)/10) { 
      next_D = std::max(next_D,(prev_D*108)/100); 
    }
    return static_cast<uint64_t>(next_D);
  }

  // LWMA-4
  difficulty_type next_difficulty_v4(std::vector<uint64_t> timestamps, std::vector<difficulty_type> cumulative_difficulties, size_t height) {
  
    uint64_t  T = DIFFICULTY_TARGET_V2;
    uint64_t  N = DIFFICULTY_WINDOW_V2;
    uint64_t  L(0), ST(0), next_D, prev_D, avg_D, i;
    assert(timestamps.size() == cumulative_difficulties.size() && timestamps.size() <= N+1 );
    if ( height <= 63469 + 1 ) { return 100000069; }
    std::vector<uint64_t>TS(N+1);
    TS[0] = timestamps[0];
    for ( i = 1; i <= N; i++) {
      if ( timestamps[i]  > TS[i-1]  ) {   TS[i] = timestamps[i];  } 
      else {  TS[i] = TS[i-1];   }
    }
    for ( i = 1; i <= N; i++) {
      if ( i > 4 && TS[i]-TS[i-1] > 5*T  && TS[i-1] - TS[i-4] < (14*T)/10 ) {   ST = 2*T; }
      else if ( i > 7 && TS[i]-TS[i-1] > 5*T  && TS[i-1] - TS[i-7] < 4*T ) {   ST = 2*T; }
      else { 
        ST = std::min(5*T ,TS[i] - TS[i-1]);
      }
      L +=  ST * i ; 
    } 
    if (L < N*N*T/20 ) { L =  N*N*T/20; } 
    avg_D = static_cast<uint64_t>(( cumulative_difficulties[N] - cumulative_difficulties[0] )/ N);
    if (avg_D > 2000000*N*N*T) {
      next_D = (avg_D/(200*L))*(N*(N+1)*T*97);   
    }   
    else {    next_D = (avg_D*N*(N+1)*T*97)/(200*L);    }
    prev_D = static_cast<uint64_t>(cumulative_difficulties[N] - cumulative_difficulties[N-1]); 
    if (  ( TS[N] - TS[N-1] < (2*T)/10 ) || 
         ( TS[N] - TS[N-2] < (5*T)/10 ) ||  
         ( TS[N] - TS[N-3] < (8*T)/10 )    )
    {  
      next_D = std::max( next_D, std::min( (prev_D*110)/100, (105*avg_D)/100 ) ); 
    }
    i = 1000000000;
    while (i > 1) { 
      if ( next_D > i*100 ) { next_D = ((next_D+i/2)/i)*i; break; }
     else { i /= 10; }
   }
   if ( next_D > 100000 ) { 
    next_D = ((next_D+500)/1000)*1000 + std::min(static_cast<uint64_t>(999), (TS[N]-TS[N-10])/10); 
   }
   return static_cast<uint64_t>(next_D);
  }

  // LWMA-1 difficulty algorithm 
  // Copyright (c) 2017-2019 Zawy, MIT License
  // https://github.com/zawy12/difficulty-algorithms/issues/3
  difficulty_type next_difficulty_v5(std::vector<std::uint64_t> timestamps, std::vector<difficulty_type> cumulative_difficulties, uint64_t T, uint64_t N, uint64_t HEIGHT) {
    assert(timestamps.size() == cumulative_difficulties.size() && timestamps.size() <= N+1 );

    if (HEIGHT >= 81769 && HEIGHT < 81769 + N) { return 10000000; }
    assert(timestamps.size() == N+1);

    uint64_t  L(0), next_D, i, this_timestamp(0), previous_timestamp(0), avg_D;

    previous_timestamp = timestamps[0]-T;
    for ( i = 1; i <= N; i++) {
    // Safely prevent out-of-sequence timestamps
      if ( timestamps[i]  > previous_timestamp ) {   this_timestamp = timestamps[i];  }
      else {  this_timestamp = previous_timestamp+1;   }
      L +=  i*std::min(6*T ,this_timestamp - previous_timestamp);
      previous_timestamp = this_timestamp;
    }
    if (L < N*N*T/20 ) { L =  N*N*T/20; }
    avg_D = static_cast<uint64_t>(( cumulative_difficulties[N] - cumulative_difficulties[0] )/ N);

    // Prevent round off error for small D and overflow for large D.
    if (avg_D > 2000000*N*N*T) {
      next_D = (avg_D/(200*L))*(N*(N+1)*T*99);
    }
    else {    next_D = (avg_D*N*(N+1)*T*99)/(200*L);    }

    // Make all insignificant digits zero for easy reading.
    i = 1000000000;
    while (i > 1) {
      if ( next_D > i*100 ) { next_D = ((next_D+i/2)/i)*i; break; }
      else { i /= 10; }
    }
    return  next_D;
  }

  difficulty_type next_difficulty_test(std::vector<std::uint64_t> timestamps, std::vector<difficulty_type> cumulative_difficulties, uint64_t T, uint64_t N, uint64_t HEIGHT) {
    assert(timestamps.size() == cumulative_difficulties.size() && timestamps.size() <= N+1 );

    if (HEIGHT < N) { return 1337; }
    assert(timestamps.size() == N+1);

    uint64_t  L(0), next_D, i, this_timestamp(0), previous_timestamp(0), avg_D;

    previous_timestamp = timestamps[0]-T;
    for ( i = 1; i <= N; i++) {
    // Safely prevent out-of-sequence timestamps
      if ( timestamps[i]  > previous_timestamp ) {   this_timestamp = timestamps[i];  }
      else {  this_timestamp = previous_timestamp+1;   }
      L +=  i*std::min(6*T ,this_timestamp - previous_timestamp);
      previous_timestamp = this_timestamp;
    }
    if (L < N*N*T/20 ) { L =  N*N*T/20; }
    avg_D = static_cast<uint64_t>(( cumulative_difficulties[N] - cumulative_difficulties[0] )/ N);

    // Prevent round off error for small D and overflow for large D.
    if (avg_D > 2000000*N*N*T) {
      next_D = (avg_D/(200*L))*(N*(N+1)*T*99);
    }
    else {    next_D = (avg_D*N*(N+1)*T*99)/(200*L);    }

    // Make all insignificant digits zero for easy reading.
    i = 1000000000;
    while (i > 1) {
      if ( next_D > i*100 ) { next_D = ((next_D+i/2)/i)*i; break; }
      else { i /= 10; }
    }
    return  next_D;
  }
}
