#pragma once
// IMA ADPCM encoder/decoder for low-latency audio streaming
// Each stereo frame (2×16bit = 4 bytes) → 1 byte (2×4bit nibbles)
// Compression ratio: 4:1, CPU cost: negligible, added latency: 0

#include <cstdint>
#include <cstring>

namespace ima_adpcm {

struct State {
  int16_t predictor;
  uint8_t stepIndex;
};

// --- Standard IMA ADPCM tables ---

static const int16_t stepTable[89] = {
    7,     8,     9,     10,    11,    12,    13,    14,    16,    17,
    19,    21,    23,    25,    28,    31,    34,    37,    41,    45,
    50,    55,    60,    66,    73,    80,    88,    97,    107,   118,
    130,   143,   157,   173,   190,   209,   230,   253,   279,   307,
    337,   371,   408,   449,   494,   544,   598,   658,   724,   796,
    876,   963,   1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,
    2272,  2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,
    5894,  6484,  7132,  7845,  8630,  9493,  10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};

static const int8_t indexTable[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8};

// --- Core encode/decode ---

inline uint8_t encodeSample(State &s, int16_t sample) {
  int32_t diff = sample - s.predictor;
  uint8_t nibble = 0;
  if (diff < 0) {
    nibble = 8;
    diff = -diff;
  }
  int32_t step = stepTable[s.stepIndex];
  int32_t diffq = step >> 3;
  if (diff >= step) { nibble |= 4; diff -= step; diffq += step; }
  step >>= 1;
  if (diff >= step) { nibble |= 2; diff -= step; diffq += step; }
  step >>= 1;
  if (diff >= step) { nibble |= 1; diffq += step; }

  int32_t pred = s.predictor;
  pred += (nibble & 8) ? -(int32_t)diffq : (int32_t)diffq;
  if (pred > 32767) pred = 32767;
  if (pred < -32768) pred = -32768;
  s.predictor = (int16_t)pred;

  int idx = s.stepIndex + indexTable[nibble];
  if (idx < 0) idx = 0;
  if (idx > 88) idx = 88;
  s.stepIndex = (uint8_t)idx;

  return nibble & 0x0F;
}

inline int16_t decodeSample(State &s, uint8_t nibble) {
  int32_t step = stepTable[s.stepIndex];
  int32_t diffq = step >> 3;
  if (nibble & 4) diffq += step;
  if (nibble & 2) diffq += step >> 1;
  if (nibble & 1) diffq += step >> 2;

  int32_t pred = s.predictor;
  pred += (nibble & 8) ? -(int32_t)diffq : (int32_t)diffq;
  if (pred > 32767) pred = 32767;
  if (pred < -32768) pred = -32768;
  s.predictor = (int16_t)pred;

  int idx = s.stepIndex + indexTable[nibble];
  if (idx < 0) idx = 0;
  if (idx > 88) idx = 88;
  s.stepIndex = (uint8_t)idx;

  return s.predictor;
}

// --- Stereo batch encode/decode ---
// Each byte: low nibble = L, high nibble = R

inline void encodeStereo(State &stateL, State &stateR,
                         const int16_t *pcm, uint16_t numFrames,
                         uint8_t *adpcmOut) {
  for (uint16_t i = 0; i < numFrames; i++) {
    uint8_t nL = encodeSample(stateL, pcm[i * 2]);
    uint8_t nR = encodeSample(stateR, pcm[i * 2 + 1]);
    adpcmOut[i] = nL | (nR << 4);
  }
}

inline void decodeStereo(State &stateL, State &stateR,
                         const uint8_t *adpcmIn, uint16_t numFrames,
                         int16_t *pcmOut) {
  for (uint16_t i = 0; i < numFrames; i++) {
    uint8_t byte = adpcmIn[i];
    pcmOut[i * 2]     = decodeSample(stateL, byte & 0x0F);
    pcmOut[i * 2 + 1] = decodeSample(stateR, (byte >> 4) & 0x0F);
  }
}

// --- Packet header helpers ---
// ADPCM packet layout (with piggyback):
//   [0]    type (0xAA)
//   [1]    seqNum
//   [2-3]  numFrames (LE)
//   [4-5]  predictorL (int16_t LE)
//   [6]    stepIndexL
//   [7-8]  predictorR (int16_t LE)
//   [9]    stepIndexR
//   [10..10+N-1] ADPCM data (N = numFrames bytes)
//   --- piggyback section (optional, present if data_len > 10+N) ---
//   [10+N]       prevSeqNum
//   [10+N+1,+2]  prevPredictorL
//   [10+N+3]     prevStepIndexL
//   [10+N+4,+5]  prevPredictorR
//   [10+N+6]     prevStepIndexR
//   [10+N+7..]   prev ADPCM data (N bytes)
// Total without piggyback: 10 + N
// Total with piggyback:    10 + N + 7 + N = 17 + 2N

static constexpr uint8_t ADPCM_HEADER_SIZE = 10;

inline void writeStateToPacket(uint8_t *pkt, const State &stateL,
                               const State &stateR) {
  memcpy(&pkt[4], &stateL.predictor, 2);
  pkt[6] = stateL.stepIndex;
  memcpy(&pkt[7], &stateR.predictor, 2);
  pkt[9] = stateR.stepIndex;
}

inline void readStateFromPacket(const uint8_t *pkt, State &stateL,
                                State &stateR) {
  memcpy(&stateL.predictor, &pkt[4], 2);
  stateL.stepIndex = pkt[6];
  memcpy(&stateR.predictor, &pkt[7], 2);
  stateR.stepIndex = pkt[9];
}

// --- Piggyback helpers ---
static constexpr uint8_t PIGGYBACK_HEADER_SIZE = 7;  // 1 seqNum + 6 state

inline void writePiggybackHeader(uint8_t *dst, uint8_t prevSeq,
                                 const State &stL, const State &stR) {
  dst[0] = prevSeq;
  memcpy(&dst[1], &stL.predictor, 2);
  dst[3] = stL.stepIndex;
  memcpy(&dst[4], &stR.predictor, 2);
  dst[6] = stR.stepIndex;
}

inline void readPiggybackHeader(const uint8_t *src, uint8_t &prevSeq,
                                State &stL, State &stR) {
  prevSeq = src[0];
  memcpy(&stL.predictor, &src[1], 2);
  stL.stepIndex = src[3];
  memcpy(&stR.predictor, &src[4], 2);
  stR.stepIndex = src[6];
}

}  // namespace ima_adpcm
