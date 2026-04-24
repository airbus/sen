// === hash32.cpp ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/base/hash32.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"

// std
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>

#define _CRT_SECURE_NO_WARNINGS  1  // NOLINT NOSONAR
#define _CRT_SECURE_NO_DEPRECATE 1  // NOLINT NOSONAR

namespace sen
{

namespace
{

template <class ForwardIt, class Generator>
constexpr void generateHelper(ForwardIt first, ForwardIt last, Generator g)
{
  while (first != last)
  {
    *first++ = g();  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }
}

using uint = unsigned int;    // NOLINT
using uchar = unsigned char;  // NOLINT

constexpr std::size_t crc32LookupTableSize = 256;
constexpr int stbWindowConst = 0x40000;  // 256K
constexpr uint stbHashSizeConst = 32768;
constexpr uint64_t adlerMod = 65521;

// Compression
uchar* stbOutPtr;
FILE* stbOutFile;
uint stbOutBytes;
unsigned int stbRunningAdler;

#define STB_IN2(x)          ((i[x] << 8) + i[(x) + 1])                                      // NOLINT
#define STB_IN3(x)          ((i[x] << 16) + STB_IN2((x) + 1))                               // NOLINT
#define STB_IN4(x)          ((i[x] << 24) + STB_IN3((x) + 1))                               // NOLINT
#define STB_HC2(q, h, c, d) (((h) << 14) + ((h) >> 18) + (q[c] << 7) + q[d])                // NOLINT
#define STB_HC3(q, c, d, e) ((q[c] << 14) + (q[d] << 7) + q[e])                             // NOLINT
#define STB_SCRAMBLE(h)     (((h) + ((h) >> 16)) & mask)                                    // NOLINT
#define STB_NC(b, d)        ((d) <= window && ((b) > 9 || stbNotCrap((int)(b), (int)(d))))  // NOLINT

// NOLINTNEXTLINE
#define STB_TRY(t, p) /* avoid retrying a match we already tried */                                                    \
  if (p ? dist != (int)(q - t) : 1)                                                                                    \
    if ((m = stbMatchLen(t, q, matchMax)) > best)                                                                      \
      if (STB_NC(m, q - (t)))                                                                                          \
  best = m, dist = (int)(q - (t))

// NOLINTNEXTLINE
#define STB_OUT(v)                                                                                                     \
  do                                                                                                                   \
  {                                                                                                                    \
    if (stbOutPtr)                                                                                                     \
    {                                                                                                                  \
      *stbOutPtr++ = (uchar)(v);                                                                                       \
    }                                                                                                                  \
    else                                                                                                               \
    {                                                                                                                  \
      stbWriteFunc((uchar)(v));                                                                                        \
    }                                                                                                                  \
  } while (0)

unsigned int stbDecompressLength(const unsigned char* input)
{
  return (input[8] << 24) + (input[9] << 16) + (input[10] << 8) + input[11];  // NOLINT
}

unsigned int stbAdler32(unsigned int adler32, unsigned char* buffer, unsigned int buflen)  // NOLINT
{
  uint64_t s1 = adler32 & 0xffff;        // NOLINT
  uint64_t s2 = adler32 >> 16;           // NOLINT
  uint64_t blockLength = buflen % 5552;  // NOLINT

  uint64_t i;
  while (buflen)  // NOLINT
  {
    for (i = 0; i + 7 < blockLength; i += 8)  // NOLINT
    {
      s1 += buffer[0];  // NOLINT
      s2 += s1;         // NOLINT
      s1 += buffer[1];  // NOLINT
      s2 += s1;         // NOLINT
      s1 += buffer[2];  // NOLINT
      s2 += s1;         // NOLINT
      s1 += buffer[3];  // NOLINT
      s2 += s1;         // NOLINT
      s1 += buffer[4];  // NOLINT
      s2 += s1;         // NOLINT
      s1 += buffer[5];  // NOLINT
      s2 += s1;         // NOLINT
      s1 += buffer[6];  // NOLINT
      s2 += s1;         // NOLINT
      s1 += buffer[7];  // NOLINT
      s2 += s1;         // NOLINT

      buffer += 8;  // NOLINT
    }

    for (; i < blockLength; ++i)
    {
      s1 += *buffer++;  // NOLINT
      s2 += s1;         // NOLINT
    }

    s1 %= adlerMod, s2 %= adlerMod;
    buflen -= (unsigned int)blockLength;  // NOLINT
    blockLength = 5552;                   // NOLINT
  }
  return (unsigned int)(s2 << 16) + (unsigned int)s1;  // NOLINT
}

// Encapsulated Decompression State
struct StbDecompressor
{

private:
  unsigned char* stbBarrierOutE_ = nullptr;
  unsigned char* stbBarrierOutB_ = nullptr;
  const unsigned char* stbBarrierInB_ = nullptr;
  unsigned char* stbDOut_ = nullptr;

public:
  void stbMatch(const unsigned char* data, unsigned int length)
  {
    // INVERSE of memmove... write each byte before copying the next...
    assert(stbDOut_ + length <= stbBarrierOutE_);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    if (stbDOut_ + length > stbBarrierOutE_)  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    {
      stbDOut_ += length;  // NOLINT
      return;
    }
    if (data < stbBarrierOutB_)
    {
      stbDOut_ = stbBarrierOutE_ + 1;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      return;
    }
    while (length--)  // NOLINT(readability-implicit-bool-conversion)
    {
      *stbDOut_++ = *data++;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }
  }

  void stbLit(const unsigned char* data, unsigned int length)
  {
    assert(stbDOut_ + length <= stbBarrierOutE_);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    if (stbDOut_ + length > stbBarrierOutE_)  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    {
      stbDOut_ += length;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      return;
    }
    if (data < stbBarrierInB_)
    {
      stbDOut_ = stbBarrierOutE_ + 1;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      return;
    }
    memcpy(stbDOut_, data, length);
    stbDOut_ += length;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }

  const unsigned char* stbDecompressToken(const unsigned char* i)
  {
    if (*i >= 0x20)
    {  // use fewer if's for cases that expand small
      if (*i >= 0x80)
      {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        stbMatch(stbDOut_ - i[1] - 1, i[0] - 0x80 + 1), i += 2;
      }
      else if (*i >= 0x40)
      {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        stbMatch(stbDOut_ - (STB_IN2(0) - 0x4000 + 1), i[2] + 1), i += 3;
      }
      else /* *i >= 0x20 */
      {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        stbLit(i + 1, i[0] - 0x20 + 1), i += 1 + (i[0] - 0x20 + 1);
      }
    }
    else
    {  // more ifs for cases that expand large, since overhead is amortized
      if (*i >= 0x18)
      {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        stbMatch(stbDOut_ - (STB_IN3(0) - 0x180000 + 1), i[3] + 1), i += 4;
      }
      else if (*i >= 0x10)
      {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        stbMatch(stbDOut_ - (STB_IN3(0) - 0x100000 + 1), STB_IN2(3) + 1), i += 5;
      }
      else if (*i >= 0x08)  // NOLINT
      {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        stbLit(i + 2, STB_IN2(0) - 0x0800 + 1), i += 2 + (STB_IN2(0) - 0x0800 + 1);
      }
      else if (*i == 0x07)
      {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        stbLit(i + 3, STB_IN2(1) + 1), i += 3 + (STB_IN2(1) + 1);
      }
      else if (*i == 0x06)
      {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        stbMatch(stbDOut_ - (STB_IN3(1) + 1), i[4] + 1), i += 5;
      }
      else if (*i == 0x04)
      {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        stbMatch(stbDOut_ - (STB_IN3(1) + 1), STB_IN2(4) + 1), i += 6;
      }
    }
    return i;
  }

  unsigned int stbDecompress(unsigned char* output, const unsigned char* i)
  {
    if (STB_IN4(0) != 0x57bC0000)
    {
      return 0;
    }
    if (STB_IN4(4) != 0)
    {
      return 0;  // error! stream is > 4GB
    }

    const unsigned int olen = stbDecompressLength(i);

    stbBarrierInB_ = i;
    stbBarrierOutE_ = output + olen;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    stbBarrierOutB_ = output;
    i += 16;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    stbDOut_ = output;
    for (;;)
    {
      const unsigned char* oldI = i;
      i = stbDecompressToken(i);
      if (i == oldI)
      {
        if (*i == 0x05 && i[1] == 0xfa)  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        {
          assert(stbDOut_ == output + olen);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          if (stbDOut_ != output + olen)      // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
          {
            return 0;
          }
          if (stbAdler32(1, output, olen) != static_cast<unsigned int>(STB_IN4(2)))
          {
            return 0;
          }
          return olen;
        }

        assert(0);
        SEN_UNREACHABLE();
      }
      assert(stbDOut_ <= output + olen);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      if (stbDOut_ > output + olen)       // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      {
        return 0;
      }
    }

    SEN_UNREACHABLE();
  }
};

//-----------------------------------------------------------------------------------------------//
// Compression
//-----------------------------------------------------------------------------------------------//

unsigned int stbMatchLen(uchar* m1, uchar* m2, uint maxlen)  // NOLINT
{
  uint i;
  for (i = 0; i < maxlen; ++i)
  {
    if (m1[i] != m2[i])  // NOLINT
    {
      return i;
    }
  }
  return i;
}

void stbWriteFunc(unsigned char v)
{
  fputc(v, stbOutFile);
  ++stbOutBytes;
}

void stbOut2(uint v)
{
  STB_OUT(v >> 8);  // NOLINT
  STB_OUT(v);       // NOLINT
}

void stbOut3(uint v)
{
  STB_OUT(v >> 16);  // NOLINT
  STB_OUT(v >> 8);   // NOLINT
  STB_OUT(v);        // NOLINT
}

void stbOut4(uint v)
{
  STB_OUT(v >> 24);  // NOLINT
  STB_OUT(v >> 16);  // NOLINT
  STB_OUT(v >> 8);   // NOLINT
  STB_OUT(v);        // NOLINT
}

void outliterals(uchar* in, int numlit)
{
  while (numlit > 65536)  // NOLINT
  {
    outliterals(in, 65536);  // NOLINT
    in += 65536;             // NOLINT
    numlit -= 65536;         // NOLINT
  }

  if (numlit == 0)
  {
    // no code needed
  }
  else if (numlit <= 32)             // NOLINT
  {                                  // NOLINT
    STB_OUT(0x000020 + numlit - 1);  // NOLINT
  }  // NOLINT
  else if (numlit <= 2048)           // NOLINT
  {                                  // NOLINT
    stbOut2(0x000800 + numlit - 1);  // NOLINT
  }  // NOLINT
  else                               // NOLINT
  {                                  // NOLINT
    stbOut3(0x070000 + numlit - 1);  // NOLINT
  }  // NOLINT
  if (stbOutPtr)                    // NOLINT
  {                                 // NOLINT
    memcpy(stbOutPtr, in, numlit);  // NOLINT
    stbOutPtr += numlit;            // NOLINT
  }  // NOLINT
  else                                  // NOLINT
  {                                     // NOLINT
    fwrite(in, 1, numlit, stbOutFile);  // NOLINT
  }  // NOLINT
}

int stbNotCrap(int best, int dist)
{
  // NOLINTNEXTLINE
  return ((best > 2 && dist <= 0x00100) || (best > 5 && dist <= 0x04000) || (best > 7 && dist <= 0x80000));
}

// NOLINTNEXTLINE
int stbCompressChunk(uchar* history,
                     uchar* start,
                     const uchar* end,
                     int length,
                     int* pendingLiterals,
                     uchar** chash,
                     uint mask)
{
  (void)history;

  int window = stbWindowConst;
  uint matchMax;
  uchar* litStart = start - *pendingLiterals;  // NOLINT
  uchar* q = start;

  // stop short of the end so we don't scan off the end doing
  // the hashing; this means we won't compress the last few bytes
  // unless they were part of something longer
  while (q < start + length && q + 12 < end)  // NOLINT
  {
    int m;
    uint h1;
    uint h2;
    uint h3;
    uint h4;
    uint h;
    uchar* t;
    int best = 2;
    int dist = 0;

    if (q + 65536 > end)  // NOLINT
    {
      matchMax = (uint)(end - q);  // NOLINT
    }
    else
    {
      matchMax = 65536;  // NOLINT
    }

    // rather than search for all matches, only try 4 candidate locations,
    // chosen based on 4 different hash functions of different lengths.
    // this strategy is inspired by LZO; hashing is unrolled here using the
    // 'hc' macro
    h = STB_HC3(q, 0, 1, 2);  // NOLINT
    h1 = STB_SCRAMBLE(h);
    t = chash[h1];  // NOLINT

    if (t)
    {
      STB_TRY(t, 0);
    }

    h = STB_HC2(q, h, 3, 4);  // NOLINT
    h2 = STB_SCRAMBLE(h);     // NOLINT
    h = STB_HC2(q, h, 5, 6);  // NOLINT
    t = chash[h2];            // NOLINT

    if (t)
    {
      STB_TRY(t, 1);
    }

    h = STB_HC2(q, h, 7, 8);   // NOLINT
    h3 = STB_SCRAMBLE(h);      // NOLINT
    h = STB_HC2(q, h, 9, 10);  // NOLINT
    t = chash[h3];             // NOLINT

    if (t)
    {
      STB_TRY(t, 1);
    }

    h = STB_HC2(q, h, 11, 12);  // NOLINT
    h4 = STB_SCRAMBLE(h);       // NOLINT
    t = chash[h4];              // NOLINT

    if (t)
    {
      STB_TRY(t, 1);
    }

    // because we use a shared hash table, can only update it
    // _after_ we've probed all of them
    chash[h1] = chash[h2] = chash[h3] = chash[h4] = q;  // NOLINT

    if (best > 2)
    {
      assert(dist > 0);
    }

    // see if our best match qualifies
    if (best < 3)
    {       // fast path literals
      ++q;  // NOLINT
    }
    else if (best > 2 && best <= 0x80 && dist <= 0x100)  // NOLINT
    {                                                    // NOLINT
      outliterals(litStart, (int)(q - litStart));        // NOLINT
      litStart = (q += best);                            // NOLINT
      STB_OUT(0x80 + best - 1);                          // NOLINT
      STB_OUT(dist - 1);                                 // NOLINT
    }  // NOLINT
    else if (best > 5 && best <= 0x100 && dist <= 0x4000)  // NOLINT
    {                                                      // NOLINT
      outliterals(litStart, (int)(q - litStart));          // NOLINT
      litStart = (q += best);                              // NOLINT
      stbOut2(0x4000 + dist - 1);                          // NOLINT
      STB_OUT(best - 1);                                   // NOLINT
    }  // NOLINT
    else if (best > 7 && best <= 0x100 && dist <= 0x80000)  // NOLINT
    {                                                       // NOLINT
      outliterals(litStart, (int)(q - litStart));           // NOLINT
      litStart = (q += best);                               // NOLINT
      stbOut3(0x180000 + dist - 1);                         // NOLINT
      STB_OUT(best - 1);                                    // NOLINT
    }  // NOLINT
    else if (best > 8 && best <= 0x10000 && dist <= 0x80000)  // NOLINT
    {                                                         // NOLINT
      outliterals(litStart, (int)(q - litStart));             // NOLINT
      litStart = (q += best);                                 // NOLINT
      stbOut3(0x100000 + dist - 1);                           // NOLINT
      stbOut2(best - 1);                                      // NOLINT
    }  // NOLINT
    else if (best > 9 && dist <= 0x1000000)  // NOLINT
    {                                        // NOLINT
      if (best > 65536)                      // NOLINT
      {                                      // NOLINT
        best = 65536;                        // NOLINT
      }  // NOLINT
      outliterals(litStart, (int)(q - litStart));  // NOLINT
      litStart = (q += best);                      // NOLINT
      if (best <= 0x100)                           // NOLINT
      {                                            // NOLINT
        STB_OUT(0x06);                             // NOLINT
        stbOut3(dist - 1);                         // NOLINT
        STB_OUT(best - 1);                         // NOLINT
      }  // NOLINT
      else                  // NOLINT
      {                     // NOLINT
        STB_OUT(0x04);      // NOLINT
        stbOut3(dist - 1);  // NOLINT
        stbOut2(best - 1);  // NOLINT
      }  // NOLINT
    }  // NOLINT
    else    // NOLINT
    {       // NOLINT
      ++q;  // NOLINT
    }  // NOLINT
  }

  // if we didn't get all the way, add the rest to literals
  if (q - start < length)
  {
    q = start + length;  // NOLINT
  }

  // the literals are everything from litStart to q
  *pendingLiterals = (int)(q - litStart);                                   // NOLINT
  stbRunningAdler = stbAdler32(stbRunningAdler, start, (uint)(q - start));  // NOLINT
  return (int)(q - start);                                                  // NOLINT
}

int stbCompressInner(uchar* input, uint length)
{
  int literals = 0;

  uchar** chash;
  chash = (uchar**)malloc(stbHashSizeConst * sizeof(uchar*));  // NOLINT
  if (chash == nullptr)
  {
    return 0;  // failure
  }
  for (uint i = 0; i < stbHashSizeConst; ++i)
  {
    chash[i] = nullptr;  // NOLINT
  }

  // stream signature
  STB_OUT(0x57);  // NOLINT
  STB_OUT(0xbc);  // NOLINT
  stbOut2(0);

  stbOut4(0);  // 64-bit length requires 32-bit leading 0
  stbOut4(length);
  stbOut4(stbWindowConst);

  stbRunningAdler = 1;

  std::ignore =
    stbCompressChunk(input, input, input + length, length, &literals, chash, stbHashSizeConst - 1);  // NOLINT

  outliterals(input + length - literals, literals);  // NOLINT

  free(chash);  // NOLINT

  stbOut2(0x05fa);  // end opcode NOLINT
  stbOut4(stbRunningAdler);

  return 1;  // success
}

uint stbCompress(uchar* out, uchar* input, uint length)
{
  stbOutPtr = out;
  stbOutFile = nullptr;

  stbCompressInner(input, length);

  return (uint)(stbOutPtr - out);  // NOLINT
}

}  // namespace

std::unique_ptr<unsigned char[]> decompressSymbol(const void* compressedData)
{
  const unsigned int decompressedSize = stbDecompressLength(static_cast<const unsigned char*>(compressedData));
  if (decompressedSize == 0)
  {
    return nullptr;
  }

  auto decompressedData = std::make_unique<unsigned char[]>(decompressedSize);
  StbDecompressor decompressor;

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast, readability-implicit-bool-conversion)
  if (!decompressor.stbDecompress(decompressedData.get(), reinterpret_cast<const unsigned char*>(compressedData)))
  {
    return nullptr;
  }

  return decompressedData;
}

std::string decompressSymbolToString(const void* compressedData, unsigned originalSize)
{
  auto decompressedData = decompressSymbol(compressedData);
  if (!decompressedData)
  {
    return {};
  }

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  return {reinterpret_cast<const char*>(decompressedData.get()), originalSize};
}

bool fileToCompressedArrayFile(const std::filesystem::path& inputFile,
                               std::string_view symbolName,
                               const std::filesystem::path& outputFile)
{
  // Read file
  FILE* inputStream = fopen(inputFile.generic_string().c_str(), "rb");  // NOLINT
  if (!inputStream)
  {
    return false;
  }

  int dataSize;
  if (fseek(inputStream, 0, SEEK_END) || (dataSize = (int)ftell(inputStream)) == -1 ||  // NOLINT
      fseek(inputStream, 0, SEEK_SET))                                                  // NOLINT
  {
    fclose(inputStream);  // NOLINT
    return false;
  }

  auto* data = new char[dataSize + 4];                            // NOLINT
  if (fread(data, 1, dataSize, inputStream) != (size_t)dataSize)  // NOLINT
  {
    fclose(inputStream);  // NOLINT
    delete[] data;        // NOLINT
    return false;
  }
  memset((void*)(data + dataSize), 0, 4);  // NOLINT
  fclose(inputStream);                     // NOLINT

  // Compress
  int maxlen = dataSize + 512 + (dataSize >> 2) + sizeof(int);                   // NOLINT
  auto* compressed = new char[maxlen];                                           // NOLINT
  int compressedSize = stbCompress((uchar*)compressed, (uchar*)data, dataSize);  // NOLINT
  memset(compressed + compressedSize, 0, maxlen - compressedSize);               // NOLINT

  FILE* outputStream = fopen(outputFile.generic_string().c_str(), "w");  // NOLINT
  if (!outputStream)
  {
    delete[] data;        // NOLINT
    delete[] compressed;  // NOLINT
    return false;
  }

  fprintf(outputStream, "// NOLINTNEXTLINE\n");                                                 // NOLINT
  fprintf(outputStream, "constexpr unsigned int %sSize = %d;\n", symbolName.data(), dataSize);  // NOLINT
  fprintf(outputStream, "\n");                                                                  // NOLINT
  fprintf(outputStream, "// NOLINTNEXTLINE\n");                                                 // NOLINT
  fprintf(outputStream,
          "constexpr unsigned int %s[%d/4] =\n{",
          symbolName.data(),
          ((compressedSize + 3) / 4) * 4);  // NOLINT
  int column = 0;
  for (int i = 0; i < compressedSize; i += 4)
  {
    auto d = *reinterpret_cast<unsigned int*>(compressed + i);  // NOLINT NOSONAR
    if ((column++ % 9) == 0)                                    // NOLINT NOSONAR
    {
      fprintf(outputStream, "\n  0x%08x, ", d);  // NOLINT
    }
    else
    {
      fprintf(outputStream, "0x%08x, ", d);  // NOLINT
    }
  }
  fprintf(outputStream, "\n};\n\n");  // NOLINT

  // Cleanup
  delete[] data;         // NOLINT
  delete[] compressed;   // NOLINT
  fclose(outputStream);  // NOLINT
  return true;
}

namespace impl
{

std::array<u32, crc32LookupTableSize> generateCrc32LookupTable() noexcept
{
  constexpr u32 reversedPoly {0xedb88320UL};

  // This is a function object that calculates the checksum for a value,
  // then increments the value, starting from zero.
  struct ByteChecksum
  {
    u32 operator()() noexcept
    {
      auto checksum = n_++;
      for (auto i = 0U; i < 8U; ++i)  // NOLINT(readability-magic-numbers)
      {
        checksum = (checksum >> 1U) ^ ((checksum & 0x1U) != 0 ? reversedPoly : 0U);
      }
      return checksum;
    }

  private:
    u32 n_ = 0U;
  };

  auto table = std::array<u32, 256U> {};  // NOLINT(readability-magic-numbers)
  generateHelper(table.begin(), table.end(), ByteChecksum {});
  return table;
}

}  // namespace impl

}  // namespace sen
