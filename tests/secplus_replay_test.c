#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../components/secplus_gdo/gdolib/secplus.h"

static int expect_round_trip(uint32_t rolling, uint64_t fixed, uint32_t data) {
  uint8_t packet[19] = {0};
  uint32_t decoded_rolling = 0;
  uint64_t decoded_fixed = 0;
  uint32_t decoded_data = 0;

  if (encode_wireline(rolling, fixed, data, packet) != 0) {
    fprintf(stderr, "encode_wireline failed\n");
    return 1;
  }
  if (decode_wireline(packet, &decoded_rolling, &decoded_fixed, &decoded_data) != 0) {
    fprintf(stderr, "decode_wireline failed\n");
    return 1;
  }

  if (decoded_rolling != rolling) {
    fprintf(stderr, "rolling mismatch: expected=%u got=%u\n", rolling, decoded_rolling);
    return 1;
  }
  if (decoded_fixed != fixed) {
    fprintf(stderr, "fixed mismatch: expected=%llu got=%llu\n",
            (unsigned long long) fixed, (unsigned long long) decoded_fixed);
    return 1;
  }
  if ((decoded_data & ~0xF000) != (data & ~0xF000)) {
    fprintf(stderr, "data mismatch: expected=0x%08x got=0x%08x\n", data, decoded_data);
    return 1;
  }
  return 0;
}

static int expect_decode_failure(void) {
  uint8_t packet[19] = {0};
  uint32_t rolling = 0;
  uint64_t fixed = 0;
  uint32_t data = 0;

  if (encode_wireline(0x1234567, 0x00A1B2C3D4ULL, 0x00112233, packet) != 0) {
    fprintf(stderr, "encode_wireline failed\n");
    return 1;
  }

  packet[0] = 0x00;  // break framing signature
  if (decode_wireline(packet, &rolling, &fixed, &data) == 0) {
    fprintf(stderr, "decode_wireline unexpectedly accepted invalid framing\n");
    return 1;
  }

  if (encode_wireline(0x1234567, 0x00A1B2C3D4ULL, 0x00112233, packet) != 0) {
    fprintf(stderr, "encode_wireline failed\n");
    return 1;
  }
  packet[10] ^= 0x01;  // parity-sensitive payload mutation
  if (decode_wireline(packet, &rolling, &fixed, &data) == 0) {
    fprintf(stderr, "decode_wireline unexpectedly accepted corrupted payload\n");
    return 1;
  }

  return 0;
}

int main(void) {
  if (expect_round_trip(0x0000001, 0x0000002908ULL, 0x00008100) != 0) {
    return 1;
  }
  if (expect_round_trip(0x0ABCDEF, 0x00A1B2C3D4ULL, 0x00FEDCBA) != 0) {
    return 1;
  }
  if (expect_round_trip(0x0000000, 0x0000000001ULL, 0x00000000) != 0) {
    return 1;
  }
  if (expect_decode_failure() != 0) {
    return 1;
  }

  printf("secplus replay tests passed\n");
  return 0;
}
