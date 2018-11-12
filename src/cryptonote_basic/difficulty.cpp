// Copyright (c) 2014-2018, The Monero Project
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

#include "common/int-util.h"
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

  bool check_hash(const crypto::hash &hash, difficulty_type difficulty) {
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

  difficulty_type next_difficulty(std::vector<std::uint64_t> timestamps, std::vector<difficulty_type> cumulative_difficulties, size_t target_seconds) {

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
    uint64_t low, high;
    mul(total_work, target_seconds, low, high);
    // blockchain errors "difficulty overhead" if this function returns zero.
    // TODO: consider throwing an exception instead
    if (high != 0 || low + time_span - 1 < low) {
      return 0;
    }
    return (low + time_span - 1) / time_span;
  }

// LWMA difficulty algorithm
// Background:  https://github.com/zawy12/difficulty-algorithms/issues/3
// Copyright (c) 2017-2018 Zawy (pseudocode)
// MIT license http://www.opensource.org/licenses/mit-license.php
// Copyright (c) 2018 Wownero Inc., a Monero Enterprise Alliance partner company
// Copyright (c) 2018 The Karbowanec developers (initial code)
// Copyright (c) 2018 Haven Protocol (refinements)
// Degnr8, Karbowanec, Masari, Bitcoin Gold, Bitcoin Candy, and Haven have contributed.

// This algorithm is: next_difficulty = harmonic_mean(Difficulties) * T / LWMA(Solvetimes)
// The harmonic_mean(Difficulties) = 1/average(Targets) so it is also:
// next_target = avg(Targets) * LWMA(Solvetimes) / T.
// This is "the best algorithm" because it has lowest root-mean-square error between 
// needed & actual difficulty during hash attacks while having the lowest standard 
// deviation during stable hashrate. That is, it's the fastest for a given stability and vice versa.
// Do not use "if solvetime < 1 then solvetime = 1" which allows a catastrophic exploit.
// Do not sort timestamps.  "Solvetimes" and "LWMA" variables must allow negatives.
// Do not use MTP as most recent block.  Do not use (POW)Limits, filtering, or tempering.
// Do not forget to set N (aka DIFFICULTY_WINDOW in Cryptonote) to recommendation below.
// The nodes' future time limit (FTL) aka CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT needs to
// be reduced from 60*60*2 to 500 seconds to prevent timestamp manipulation from miner's with 
//  > 50% hash power.  If this is too small, it can be increased to 1000 at a cost in protection.
  
// Cryptonote clones:  #define DIFFICULTY_BLOCKS_COUNT_V2 DIFFICULTY_WINDOW_V2 + 1


  difficulty_type next_difficulty_v2(std::vector<std::uint64_t> timestamps, std::vector<difficulty_type> cumulative_difficulties, size_t target_seconds) {

    const int64_t T = static_cast<int64_t>(target_seconds);
    
    size_t N = DIFFICULTY_WINDOW_V2;

    // Return a difficulty of 1 for first 3 blocks if it's the start of the chain.
    if (timestamps.size() < 4) { 
      return 1; 
    }
    // Otherwise, use a smaller N if the start of the chain is less than N+1.
    else if ( timestamps.size() < N+1 ) { 
      N = timestamps.size() - 1;
    }
    // Otherwise make sure timestamps and cumulative_difficulties are correct size.
    else {  
      timestamps.resize(N+1);  
      cumulative_difficulties.resize(N+1);
    }
    // To get an average solvetime to within +/- ~0.1%, use an adjustment factor.
    // adjust=0.999 for 80 < N < 120(?)
    const double adjust = 0.998;
    // The divisor k normalizes the LWMA sum to a standard LWMA.
    const double k = N * (N + 1) / 2;

    double LWMA(0), sum_inverse_D(0), harmonic_mean_D(0), nextDifficulty(0);
    int64_t solveTime(0);
    uint64_t difficulty(0), next_difficulty(0);

    // Loop through N most recent blocks. N is most recently solved block.
    for (size_t i = 1; i <= N; i++) {
      solveTime = static_cast<int64_t>(timestamps[i]) - static_cast<int64_t>(timestamps[i - 1]);
      solveTime = std::min<int64_t>((T * 7), std::max<int64_t>(solveTime, (-7 * T)));
      difficulty = cumulative_difficulties[i] - cumulative_difficulties[i - 1];
      LWMA += (int64_t)(solveTime * i) / k;
      sum_inverse_D += 1 / static_cast<double>(difficulty);
    }

    harmonic_mean_D = N / sum_inverse_D;

    // Keep LWMA sane in case something unforeseen occurs.
    if (static_cast<int64_t>(boost::math::round(LWMA)) < T / 20)
      LWMA = static_cast<double>(T / 20);

    nextDifficulty = harmonic_mean_D * T / LWMA * adjust;

    // No limits should be employed, but this is correct way to employ a 20% symmetrical limit:
    // nextDifficulty=max(previous_Difficulty*0.8,min(previous_Difficulty/0.8, next_Difficulty));
    next_difficulty = static_cast<uint64_t>(nextDifficulty);
    return next_difficulty;
  }

// LWMA-2 difficulty algorithm 
// Copyright (c) 2017-2018 Zawy, MIT License
// The code below has been tailored to Wownero.
// Don’t be an igit and copy and paste it into 
// to your shitcoin without reading the comments in 
// https://github.com/zawy12/difficulty-algorithms/issues/3

  difficulty_type next_difficulty_v3(std::vector<uint64_t> timestamps, std::vector<difficulty_type> cumulative_difficulties, size_t height) {

    int64_t  T = DIFFICULTY_TARGET_V2;
    int64_t  N = DIFFICULTY_WINDOW_V2;
    int64_t  L(0), ST, sum_3_ST(0), next_D, prev_D;

    assert(timestamps.size() == cumulative_difficulties.size() && timestamps.size() <= static_cast<uint64_t>(N+1) );

    /*if ( height <= DIFFICULTY_HEIGHT ){ 
      return static_cast<uint64_t>(DIFFICULTY_GUESS); 
    }*/

    for ( int64_t i = 1; i <= N; i++ ) {  
      ST = static_cast<int64_t>(timestamps[i]) - static_cast<int64_t>(timestamps[i-1]);
      ST = std::max(-4*T, std::min(ST, 6*T));
      L +=  ST * i ; 
      if ( i > N-3 ) { 
        sum_3_ST += ST; 
      } 
    }
    next_D = (static_cast<int64_t>(cumulative_difficulties[N] - cumulative_difficulties[0])*T*(N+1)*99)/(100*2*L);

    prev_D = cumulative_difficulties[N] - cumulative_difficulties[N-1];
    next_D = std::max((prev_D*67)/100, std::min(next_D, (prev_D*150)/100));

    if ( sum_3_ST < (8*T)/10) { 
      next_D = std::max(next_D,(prev_D*108)/100); 
    }

    if ( next_D < DIFFICULTY_MINIMUM ) { 
      return static_cast<uint64_t>(DIFFICULTY_MINIMUM); 
    }
    else { 
      return static_cast<uint64_t>(next_D); 
    }
  }

  // LWMA-4 difficulty algorithm 
  // Copyright (c) 2017-2018 Zawy, MIT License
  // https://github.com/zawy12/difficulty-algorithms/issues/3
  difficulty_type next_difficulty_v4(std::vector<uint64_t> timestamps, std::vector<difficulty_type> cumulative_difficulties, size_t height) {

   uint64_t  T = DIFFICULTY_TARGET_V2;
   uint64_t  N = DIFFICULTY_WINDOW_V2; // N=45, 60, and 90 for T=600, 120, 60.
   uint64_t  L(0), ST(0), next_D, prev_D, avg_D, i;
        
    assert(timestamps.size() == cumulative_difficulties.size() && timestamps.size() <= N+1 );

   if ( height >= DIFFICULTY_HEIGHT && height <= DIFFICULTY_HEIGHT + 1 ) { return DIFFICULTY_GUESS;  }
 
   // Safely convert out-of-sequence timestamps into > 0 solvetimes.
   std::vector<uint64_t>TS(N+1);
   TS[0] = timestamps[0];
   for ( i = 1; i <= N; i++) {        
      if ( timestamps[i]  > TS[i-1]  ) {   TS[i] = timestamps[i];  } 
      else {  TS[i] = TS[i-1];   }
   }

   for ( i = 1; i <= N; i++) {  
      // Temper long solvetime drops if they were preceded by 3 or 6 fast solves.
      if ( i > 4 && TS[i]-TS[i-1] > 5*T  && TS[i-1] - TS[i-4] < (14*T)/10 ) {   ST = 2*T; }
      else if ( i > 7 && TS[i]-TS[i-1] > 5*T  && TS[i-1] - TS[i-7] < 4*T ) {   ST = 2*T; }
      else { // Assume normal conditions, so get ST.
         // LWMA drops too much from long ST, so limit drops with a 5*T limit 
         ST = std::min(5*T ,TS[i] - TS[i-1]);
      }
      L +=  ST * i ; 
   } 
   if (L < N*N*T/20 ) { L =  N*N*T/20; } 
   avg_D = ( cumulative_difficulties[N] - cumulative_difficulties[0] )/ N;
   
   // Prevent round off error for small D and overflow for large D.
   if (avg_D > 2000000*N*N*T) { 
       next_D = (avg_D/(200*L))*(N*(N+1)*T*97);   
   }   
   else {    next_D = (avg_D*N*(N+1)*T*97)/(200*L);    }

   prev_D =  cumulative_difficulties[N] - cumulative_difficulties[N-1] ; 

   // Apply 10% jump rule.
   if (  ( TS[N] - TS[N-1] < (2*T)/10 ) || 
         ( TS[N] - TS[N-2] < (5*T)/10 ) ||  
         ( TS[N] - TS[N-3] < (8*T)/10 )    )
   {  
       next_D = std::max( next_D, std::min( (prev_D*110)/100, (105*avg_D)/100 ) ); 
   }
   // Make all insignificant digits zero for easy reading.
   i = 1000000000;
   while (i > 1) { 
     if ( next_D > i*100 ) { next_D = ((next_D+i/2)/i)*i; break; }
     else { i /= 10; }
   }
   // Make least 3 digits equal avg of past 10 solvetimes.
   if ( next_D > 100000 ) { 
    next_D = ((next_D+500)/1000)*1000 + std::min(static_cast<uint64_t>(999), (TS[N]-TS[N-10])/10); 
   }

   if ( next_D < DIFFICULTY_MINIMUM ) {
    return static_cast<uint64_t>(DIFFICULTY_MINIMUM);
   } 
   else { 
    return static_cast<uint64_t>(next_D); 
   }
  }
}
