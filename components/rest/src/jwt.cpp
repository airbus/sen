// === jwt.cpp =========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "jwt.h"

// std
#include <array>
#include <cstddef>
#include <ctime>
#include <string>
#include <string_view>
#include <tuple>

// json
#include "nlohmann/json.hpp"

using Json = nlohmann::json;

namespace sen::components::rest
{

//--------------------------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------------------------

constexpr std::string_view b64Table =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  "abcdefghijklmnopqrstuvwxyz"
  "0123456789+/";

constexpr auto b64RevTable = []()
{
  std::array<char, 256> table {};
  for (auto& i: table)
  {
    i = -1;
  }

  for (size_t i = 0; i < b64Table.size(); ++i)
  {
    table[static_cast<char>(b64Table[i])] = static_cast<char>(i);  // NOLINT
  }

  return table;
}();

constexpr std::string_view tokenHeader = R"({"typ":"JST"})";
constexpr int charBits = 8;
constexpr int sextetBits = 6;
constexpr unsigned int sextetMask = 0x3f;
constexpr unsigned int octetMask = 0xff;
constexpr char padding = '=';

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

std::string base64Encode(const std::string_view& input)
{
  std::string output;

  unsigned int accumulator = 0;
  int bitsInAccumulator = -sextetBits;  // Track how many bits we have (-6 means we need more)

  for (unsigned char c: input)
  {
    accumulator = (accumulator << charBits) + c;  // NOLINT (charBits always positive)
    bitsInAccumulator += charBits;

    // Encode 6-bits chunks
    while (bitsInAccumulator >= 0)
    {
      // Extract top 6 bits and convert to Base64 character
      unsigned int index = (accumulator >> bitsInAccumulator) & sextetMask;  // NOLINT (always positive)
      output.push_back(b64Table.at(index));

      bitsInAccumulator -= 6;
    }
  }

  // Padding case
  if (bitsInAccumulator > -sextetBits)
  {
    // Shift remaining bits to align to 6-bit
    unsigned int index =
      ((accumulator << charBits) >> (bitsInAccumulator + charBits)) & sextetMask;  // NOLINT (hicpp-signed-bitwise)
    output.push_back(b64Table.at(index));
  }

  // Add padding '=' characters to make it multiple of 4
  while (output.size() % 4 != 0)
  {
    output.push_back(padding);
  }

  return output;
}

// Base64 Decode
std::string base64Decode(const std::string_view& input)
{
  std::string output;

  unsigned int accumulatedBits = 0;   // Accumulated bits from Base64 characters
  int bitsInAccumulator = -charBits;  // Track how many bits we have (-8 means we need more)

  // Process each Base64 character
  for (char c: input)
  {
    // Check if character is valid (skip padding and invalid chars)
    if (b64RevTable.at(c) == -1)
    {
      break;
    }

    // Add 6 bits from current character to accumulator
    accumulatedBits = (accumulatedBits << sextetBits) + b64RevTable.at(c);  // NOLINT (hicpp-signed-bitwise)
    bitsInAccumulator += 6;

    // Extract complete bytes (8 bits) when we have enough
    if (bitsInAccumulator >= 0)
    {
      char byte = (accumulatedBits >> bitsInAccumulator) & octetMask;  // NOLINT (hicpp-signed-bitwise)
      output.push_back(byte);
      bitsInAccumulator -= 8;
    }
  }

  return output;
}

//--------------------------------------------------------------------------------------------------------------
// JWT
//--------------------------------------------------------------------------------------------------------------

std::string encodeJWT(const std::string& payload)
{
  std::string encodedHeader = base64Encode(tokenHeader);
  std::string encodedPayload = base64Encode(payload);

  return encodedHeader.append(".").append(encodedPayload);
}

JWT decodeJWT(const std::string& token)
{
  JWT jwt;

  size_t firstDot = token.find('.');
  if (firstDot == std::string::npos)
  {
    jwt.error = JWTError::invalidTokenFormat;
    return jwt;
  }

  std::string encodedHeader = token.substr(0, firstDot);
  std::string encodedPayload = token.substr(firstDot + 1);

  // Remove signature part if present
  size_t secondDot = encodedPayload.find('.');
  if (secondDot != std::string::npos)
  {
    encodedPayload = encodedPayload.substr(0, secondDot);
  }

  try
  {
    jwt.header = base64Decode(encodedHeader);
    jwt.payload = base64Decode(encodedPayload);
  }
  catch (...)
  {
    jwt.error = JWTError::tokenDecodingError;
    return jwt;
  }

  // Verify expiration time if present
  size_t expPos = jwt.payload.find("\"exp\":");
  if (expPos != std::string::npos)
  {
    size_t expStart = expPos + 6;
    size_t expEnd = jwt.payload.find_first_of(",}", expStart);
    std::string expStr = jwt.payload.substr(expStart, expEnd - expStart);

    try
    {
      time_t expTime = std::stoll(expStr);
      time_t now = time(nullptr);

      if (now > expTime)
      {
        jwt.error = JWTError::tokenExpired;
        return jwt;
      }
    }
    catch (...)
    {
      jwt.error = JWTError::invalidExpirationTime;
      return jwt;
    }
  }

  try
  {
    std::ignore = Json::parse(jwt.payload);
    jwt.valid = true;
    return jwt;
  }
  catch (Json::exception&)
  {
    jwt.error = JWTError::invalidPayload;
    jwt.valid = false;
    return jwt;
  }
}

std::string toString(const JWTError& jwtError)
{
  switch (jwtError)
  {
    case JWTError::noError:
      return "no error";
    case JWTError::tokenExpired:
      return "token expired";
    case JWTError::tokenDecodingError:
      return "token decoding error";
    case JWTError::invalidTokenFormat:
      return "invalid token format";
    case JWTError::invalidSignatureFormat:
      return "invalid signature format";
    case JWTError::invalidExpirationTime:
      return "invalid expiration time";
    case JWTError::invalidSignature:
      return "invalid signature";
    case JWTError::invalidPayload:
      return "invalid payload";
    default:
      return "unknown error";
  }
}

}  // namespace sen::components::rest
