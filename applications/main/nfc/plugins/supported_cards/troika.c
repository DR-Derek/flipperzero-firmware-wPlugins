#include "nfc_supported_card_plugin.h"
#include <core/check.h>

#include <flipper_application/flipper_application.h>

#include <nfc/nfc_device.h>
#include <core/string.h>
#include <nfc/helpers/nfc_util.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include "furi_hal_rtc.h"

#define TAG "Troika"

typedef struct {
    uint8_t s;
    uint64_t a;
    uint64_t b;
} MfClassicKeyPair;

typedef struct {
    const MfClassicKeyPair* keys;
    uint32_t data_sector;
} TroikaCardConfig;

typedef enum {
    TroikaLayoutUnknown = 0x0,
    TroikaLayout2 = 0x2,
    TroikaLayoutE = 0xE,
} TroikaLayout;

typedef enum {
    TroikaSublayoutUnknown = 0x0,
    TroikaSublayout3 = 0x3,
    TroikaSublayout5 = 0x5,
    TroikaSublayout6 = 0x6,
} TroikaSubLayout;

static const MfClassicKeyPair troika_1k_keys[] = {
    {.s = 0, .a = 0xa0a1a2a3a4a5, .b = 0xfbf225dc5d58},
    {.s = 1, .a = 0xa82607b01c0d, .b = 0x2910989b6880},
    {.s = 2, .a = 0x2aa05ed1856f, .b = 0xeaac88e5dc99},
    {.s = 3, .a = 0x2aa05ed1856f, .b = 0xeaac88e5dc99},
    {.s = 4, .a = 0x73068f118c13, .b = 0x2b7f3253fac5},
    {.s = 5, .a = 0xfbc2793d540b, .b = 0xd3a297dc2698},
    {.s = 6, .a = 0x2aa05ed1856f, .b = 0xeaac88e5dc99},
    {.s = 7, .a = 0xae3d65a3dad4, .b = 0x0f1c63013dba},
    {.s = 8, .a = 0xa73f5dc1d333, .b = 0xe35173494a81},
    {.s = 9, .a = 0x69a32f1c2f19, .b = 0x6b8bd9860763},
    {.s = 10, .a = 0x9becdf3d9273, .b = 0xf8493407799d},
    {.s = 11, .a = 0x08b386463229, .b = 0x5efbaecef46b},
    {.s = 12, .a = 0xcd4c61c26e3d, .b = 0x31c7610de3b0},
    {.s = 13, .a = 0xa82607b01c0d, .b = 0x2910989b6880},
    {.s = 14, .a = 0x0e8f64340ba4, .b = 0x4acec1205d75},
    {.s = 15, .a = 0x2aa05ed1856f, .b = 0xeaac88e5dc99},
};

static const MfClassicKeyPair troika_4k_keys[] = {
    {.s = 0, .a = 0xEC29806D9738, .b = 0xFBF225DC5D58},
    {.s = 1, .a = 0xA0A1A2A3A4A5, .b = 0x7DE02A7F6025},
    {.s = 2, .a = 0x2AA05ED1856F, .b = 0xEAAC88E5DC99},
    {.s = 3, .a = 0x2AA05ED1856F, .b = 0xEAAC88E5DC99},
    {.s = 4, .a = 0x73068F118C13, .b = 0x2B7F3253FAC5},
    {.s = 5, .a = 0xFBC2793D540B, .b = 0xD3A297DC2698},
    {.s = 6, .a = 0x2AA05ED1856F, .b = 0xEAAC88E5DC99},
    {.s = 7, .a = 0xAE3D65A3DAD4, .b = 0x0F1C63013DBA},
    {.s = 8, .a = 0xA73F5DC1D333, .b = 0xE35173494A81},
    {.s = 9, .a = 0x69A32F1C2F19, .b = 0x6B8BD9860763},
    {.s = 10, .a = 0x9BECDF3D9273, .b = 0xF8493407799D},
    {.s = 11, .a = 0x08B386463229, .b = 0x5EFBAECEF46B},
    {.s = 12, .a = 0xCD4C61C26E3D, .b = 0x31C7610DE3B0},
    {.s = 13, .a = 0xA82607B01C0D, .b = 0x2910989B6880},
    {.s = 14, .a = 0x0E8F64340BA4, .b = 0x4ACEC1205D75},
    {.s = 15, .a = 0x2AA05ED1856F, .b = 0xEAAC88E5DC99},
    {.s = 16, .a = 0x6B02733BB6EC, .b = 0x7038CD25C408},
    {.s = 17, .a = 0x403D706BA880, .b = 0xB39D19A280DF},
    {.s = 18, .a = 0xC11F4597EFB5, .b = 0x70D901648CB9},
    {.s = 19, .a = 0x0DB520C78C1C, .b = 0x73E5B9D9D3A4},
    {.s = 20, .a = 0x3EBCE0925B2F, .b = 0x372CC880F216},
    {.s = 21, .a = 0x16A27AF45407, .b = 0x9868925175BA},
    {.s = 22, .a = 0xABA208516740, .b = 0xCE26ECB95252},
    {.s = 23, .a = 0xCD64E567ABCD, .b = 0x8F79C4FD8A01},
    {.s = 24, .a = 0x764CD061F1E6, .b = 0xA74332F74994},
    {.s = 25, .a = 0x1CC219E9FEC1, .b = 0xB90DE525CEB6},
    {.s = 26, .a = 0x2FE3CB83EA43, .b = 0xFBA88F109B32},
    {.s = 27, .a = 0x07894FFEC1D6, .b = 0xEFCB0E689DB3},
    {.s = 28, .a = 0x04C297B91308, .b = 0xC8454C154CB5},
    {.s = 29, .a = 0x7A38E3511A38, .b = 0xAB16584C972A},
    {.s = 30, .a = 0x7545DF809202, .b = 0xECF751084A80},
    {.s = 31, .a = 0x5125974CD391, .b = 0xD3EAFB5DF46D},
    {.s = 32, .a = 0xFFFFFFFFFFFF, .b = 0xFFFFFFFFFFFF},
    {.s = 33, .a = 0xFFFFFFFFFFFF, .b = 0xFFFFFFFFFFFF},
    {.s = 34, .a = 0xFFFFFFFFFFFF, .b = 0xFFFFFFFFFFFF},
    {.s = 35, .a = 0xFFFFFFFFFFFF, .b = 0xFFFFFFFFFFFF},
    {.s = 36, .a = 0xFFFFFFFFFFFF, .b = 0xFFFFFFFFFFFF},
    {.s = 37, .a = 0xFFFFFFFFFFFF, .b = 0xFFFFFFFFFFFF},
    {.s = 38, .a = 0xFFFFFFFFFFFF, .b = 0xFFFFFFFFFFFF},
    {.s = 39, .a = 0xFFFFFFFFFFFF, .b = 0xFFFFFFFFFFFF},
};

#define TOPBIT(X) (1 << ((X)-1))

typedef enum {
    BitLibParityEven,
    BitLibParityOdd,
    BitLibParityAlways0,
    BitLibParityAlways1,
} BitLibParity;

typedef struct {
    const char mark;
    const size_t start;
    const size_t length;
} BitLibRegion;

void push_bit(uint8_t* data, size_t data_size, bool bit) {
    size_t last_index = data_size - 1;

    for(size_t i = 0; i < last_index; ++i) {
        data[i] = (data[i] << 1) | ((data[i + 1] >> 7) & 1);
    }
    data[last_index] = (data[last_index] << 1) | bit;
}

void set_bit(uint8_t* data, size_t position, bool bit) {
    if(bit) {
        data[position / 8] |= 1UL << (7 - (position % 8));
    } else {
        data[position / 8] &= ~(1UL << (7 - (position % 8)));
    }
}

void set_bits(uint8_t* data, size_t position, uint8_t byte, uint8_t length) {
    furi_check(length <= 8);
    furi_check(length > 0);

    for(uint8_t i = 0; i < length; ++i) {
        uint8_t shift = (length - 1) - i;
        set_bit(data, position + i, (byte >> shift) & 1); //-V610
    }
}

bool get_bit(const uint8_t* data, size_t position) {
    return (data[position / 8] >> (7 - (position % 8))) & 1;
}

uint8_t get_bits(const uint8_t* data, size_t position, uint8_t length) {
    uint8_t shift = position % 8;
    if(shift == 0) {
        return data[position / 8] >> (8 - length);
    } else {
        // TODO fix read out of bounds
        uint8_t value = (data[position / 8] << (shift));
        value |= data[position / 8 + 1] >> (8 - shift);
        value = value >> (8 - length);
        return value;
    }
}

uint16_t get_bits_16(const uint8_t* data, size_t position, uint8_t length) {
    uint16_t value = 0;
    if(length <= 8) {
        value = get_bits(data, position, length);
    } else {
        value = get_bits(data, position, 8) << (length - 8);
        value |= get_bits(data, position + 8, length - 8);
    }
    return value;
}

uint32_t get_bits_32(const uint8_t* data, size_t position, uint8_t length) {
    uint32_t value = 0;
    if(length <= 8) {
        value = get_bits(data, position, length);
    } else if(length <= 16) {
        value = get_bits(data, position, 8) << (length - 8);
        value |= get_bits(data, position + 8, length - 8);
    } else if(length <= 24) {
        value = get_bits(data, position, 8) << (length - 8);
        value |= get_bits(data, position + 8, 8) << (length - 16);
        value |= get_bits(data, position + 16, length - 16);
    } else {
        value = (uint32_t)get_bits(data, position, 8) << (length - 8);
        value |= (uint32_t)get_bits(data, position + 8, 8) << (length - 16);
        value |= (uint32_t)get_bits(data, position + 16, 8) << (length - 24);
        value |= get_bits(data, position + 24, length - 24);
    }

    return value;
}

uint64_t get_bits_64(const uint8_t* data, size_t position, uint8_t length) {
    uint64_t value = 0;
    if(length <= 8) {
        value = get_bits(data, position, length);
    } else if(length <= 16) {
        value = get_bits(data, position, 8) << (length - 8);
        value |= get_bits(data, position + 8, length - 8);
    } else if(length <= 24) {
        value = get_bits(data, position, 8) << (length - 8);
        value |= get_bits(data, position + 8, 8) << (length - 16);
        value |= get_bits(data, position + 16, length - 16);
    } else if(length <= 32) {
        value = (uint64_t)get_bits(data, position, 8) << (length - 8);
        value |= (uint64_t)get_bits(data, position + 8, 8) << (length - 16);
        value |= (uint64_t)get_bits(data, position + 16, 8) << (length - 24);
        value |= get_bits(data, position + 24, length - 24);
    } else {
        value = (uint64_t)get_bits(data, position, 8) << (length - 8);
        value |= (uint64_t)get_bits(data, position + 8, 8) << (length - 16);
        value |= (uint64_t)get_bits(data, position + 16, 8) << (length - 24);
        value |= (uint64_t)get_bits(data, position + 24, 8) << (length - 32);
        value |= (uint64_t)get_bits(data, position + 32, 8) << (length - 40);
        value |= (uint64_t)get_bits(data, position + 40, 8) << (length - 48);
        value |= (uint64_t)get_bits(data, position + 48, 8) << (length - 56);
        value |= (uint64_t)get_bits(data, position + 56, 8) << (length - 64);
        value |= get_bits(data, position + 64, length - 64);
    }

    return value;
}

bool test_parity_32(uint32_t bits, BitLibParity parity) {
#if !defined __GNUC__
#error Please, implement parity test for non-GCC compilers
#else
    switch(parity) {
    case BitLibParityEven:
        return __builtin_parity(bits);
    case BitLibParityOdd:
        return !__builtin_parity(bits);
    default:
        furi_crash("Unknown parity");
    }
#endif
}

bool bit_lib_test_parity(
    const uint8_t* bits,
    size_t position,
    uint8_t length,
    BitLibParity parity,
    uint8_t parity_length) {
    uint32_t parity_block;
    bool result = true;
    const size_t parity_blocks_count = length / parity_length;

    for(size_t i = 0; i < parity_blocks_count; ++i) {
        switch(parity) {
        case BitLibParityEven:
        case BitLibParityOdd:
            parity_block = get_bits_32(bits, position + i * parity_length, parity_length);
            if(!test_parity_32(parity_block, parity)) {
                result = false;
            }
            break;
        case BitLibParityAlways0:
            if(get_bit(bits, position + i * parity_length + parity_length - 1)) {
                result = false;
            }
            break;
        case BitLibParityAlways1:
            if(!get_bit(bits, position + i * parity_length + parity_length - 1)) {
                result = false;
            }
            break;
        }

        if(!result) break;
    }
    return result;
}

size_t bit_lib_add_parity(
    const uint8_t* data,
    size_t position,
    uint8_t* dest,
    size_t dest_position,
    uint8_t source_length,
    uint8_t parity_length,
    BitLibParity parity) {
    uint32_t parity_word = 0;
    size_t j = 0, bit_count = 0;
    for(int word = 0; word < source_length; word += parity_length - 1) {
        for(int bit = 0; bit < parity_length - 1; bit++) {
            parity_word = (parity_word << 1) | get_bit(data, position + word + bit);
            set_bit(dest, dest_position + j++, get_bit(data, position + word + bit));
        }
        // if parity fails then return 0
        switch(parity) {
        case BitLibParityAlways0:
            set_bit(dest, dest_position + j++, 0);
            break; // marker bit which should be a 0
        case BitLibParityAlways1:
            set_bit(dest, dest_position + j++, 1);
            break; // marker bit which should be a 1
        default:
            set_bit(
                dest,
                dest_position + j++,
                (test_parity_32(parity_word, BitLibParityOdd) ^ parity) ^ 1);
            break;
        }
        bit_count += parity_length;
        parity_word = 0;
    }
    // if we got here then all the parities passed
    // return bit count
    return bit_count;
}

size_t bit_lib_remove_bit_every_nth(uint8_t* data, size_t position, uint8_t length, uint8_t n) {
    size_t counter = 0;
    size_t result_counter = 0;
    uint8_t bit_buffer = 0;
    uint8_t bit_counter = 0;

    while(counter < length) {
        if((counter + 1) % n != 0) {
            bit_buffer = (bit_buffer << 1) | get_bit(data, position + counter);
            bit_counter++;
        }

        if(bit_counter == 8) {
            set_bits(data, position + result_counter, bit_buffer, 8);
            bit_counter = 0;
            bit_buffer = 0;
            result_counter += 8;
        }
        counter++;
    }

    if(bit_counter != 0) {
        set_bits(data, position + result_counter, bit_buffer, bit_counter);
        result_counter += bit_counter;
    }
    return result_counter;
}

void bit_lib_copy_bits(
    uint8_t* data,
    size_t position,
    size_t length,
    const uint8_t* source,
    size_t source_position) {
    for(size_t i = 0; i < length; ++i) {
        set_bit(data, position + i, get_bit(source, source_position + i));
    }
}

void bit_lib_reverse_bits(uint8_t* data, size_t position, uint8_t length) {
    size_t i = 0;
    size_t j = length - 1;

    while(i < j) {
        bool tmp = get_bit(data, position + i);
        set_bit(data, position + i, get_bit(data, position + j));
        set_bit(data, position + j, tmp);
        i++;
        j--;
    }
}

uint8_t get_bit_count(uint32_t data) {
#if defined __GNUC__
    return __builtin_popcountl(data);
#else
#error Please, implement popcount for non-GCC compilers
#endif
}

void bit_lib_print_bits(const uint8_t* data, size_t length) {
    for(size_t i = 0; i < length; ++i) {
        printf("%u", get_bit(data, i));
    }
}

void bit_lib_print_regions(
    const BitLibRegion* regions,
    size_t region_count,
    const uint8_t* data,
    size_t length) {
    // print data
    bit_lib_print_bits(data, length);
    printf("\r\n");

    // print regions
    for(size_t c = 0; c < length; ++c) {
        bool print = false;

        for(size_t i = 0; i < region_count; i++) {
            if(regions[i].start <= c && c < regions[i].start + regions[i].length) {
                print = true;
                printf("%c", regions[i].mark);
                break;
            }
        }

        if(!print) {
            printf(" ");
        }
    }
    printf("\r\n");

    // print regions data
    for(size_t c = 0; c < length; ++c) {
        bool print = false;

        for(size_t i = 0; i < region_count; i++) {
            if(regions[i].start <= c && c < regions[i].start + regions[i].length) {
                print = true;
                printf("%u", get_bit(data, c));
                break;
            }
        }

        if(!print) {
            printf(" ");
        }
    }
    printf("\r\n");
}

uint16_t bit_lib_reverse_16_fast(uint16_t data) {
    uint16_t result = 0;
    result |= (data & 0x8000) >> 15;
    result |= (data & 0x4000) >> 13;
    result |= (data & 0x2000) >> 11;
    result |= (data & 0x1000) >> 9;
    result |= (data & 0x0800) >> 7;
    result |= (data & 0x0400) >> 5;
    result |= (data & 0x0200) >> 3;
    result |= (data & 0x0100) >> 1;
    result |= (data & 0x0080) << 1;
    result |= (data & 0x0040) << 3;
    result |= (data & 0x0020) << 5;
    result |= (data & 0x0010) << 7;
    result |= (data & 0x0008) << 9;
    result |= (data & 0x0004) << 11;
    result |= (data & 0x0002) << 13;
    result |= (data & 0x0001) << 15;
    return result;
}

uint8_t bit_lib_reverse_8_fast(uint8_t byte) {
    byte = (byte & 0xF0) >> 4 | (byte & 0x0F) << 4;
    byte = (byte & 0xCC) >> 2 | (byte & 0x33) << 2;
    byte = (byte & 0xAA) >> 1 | (byte & 0x55) << 1;
    return byte;
}

uint16_t bit_lib_crc8(
    uint8_t const* data,
    size_t data_size,
    uint8_t polynom,
    uint8_t init,
    bool ref_in,
    bool ref_out,
    uint8_t xor_out) {
    uint8_t crc = init;

    for(size_t i = 0; i < data_size; ++i) {
        uint8_t byte = data[i];
        if(ref_in) bit_lib_reverse_bits(&byte, 0, 8);
        crc ^= byte;

        for(size_t j = 8; j > 0; --j) {
            if(crc & TOPBIT(8)) {
                crc = (crc << 1) ^ polynom;
            } else {
                crc = (crc << 1);
            }
        }
    }

    if(ref_out) bit_lib_reverse_bits(&crc, 0, 8);
    crc ^= xor_out;

    return crc;
}

uint16_t crc16(
    uint8_t const* data,
    size_t data_size,
    uint16_t polynom,
    uint16_t init,
    bool ref_in,
    bool ref_out,
    uint16_t xor_out) {
    uint16_t crc = init;

    for(size_t i = 0; i < data_size; ++i) {
        uint8_t byte = data[i];
        if(ref_in) byte = bit_lib_reverse_16_fast(byte) >> 8;

        for(size_t j = 0; j < 8; ++j) {
            bool c15 = (crc >> 15 & 1);
            bool bit = (byte >> (7 - j) & 1);
            crc <<= 1;
            if(c15 ^ bit) crc ^= polynom;
        }
    }

    if(ref_out) crc = bit_lib_reverse_16_fast(crc);
    crc ^= xor_out;

    return crc;
}

void from_days_to_datetime(uint16_t days, FuriHalRtcDateTime* datetime, uint16_t start_year) {
    uint32_t timestamp = days * 24 * 60 * 60;
    FuriHalRtcDateTime start_datetime = {0};
    start_datetime.year = start_year - 1;
    start_datetime.month = 12;
    start_datetime.day = 31;
    timestamp += furi_hal_rtc_datetime_to_timestamp(&start_datetime);
    furi_hal_rtc_timestamp_to_datetime(timestamp, datetime);
}

void from_minutes_to_datetime(uint32_t minutes, FuriHalRtcDateTime* datetime, uint16_t start_year) {
    uint32_t timestamp = minutes * 60;
    FuriHalRtcDateTime start_datetime = {0};
    start_datetime.year = start_year - 1;
    start_datetime.month = 12;
    start_datetime.day = 31;
    timestamp += furi_hal_rtc_datetime_to_timestamp(&start_datetime);
    furi_hal_rtc_timestamp_to_datetime(timestamp, datetime);
}

bool parse_transport_block(const MfClassicBlock* block, FuriString* result) {
    uint16_t transport_departament = get_bits_16(block->data, 0, 10);

    FURI_LOG_I(TAG, "Transport departament: %x", transport_departament);

    uint16_t layout_type = get_bits_16(block->data, 52, 4);
    if(layout_type == 0xE) {
        layout_type = get_bits_16(block->data, 52, 9);
    } else if(layout_type == 0xF) {
        layout_type = get_bits_16(block->data, 52, 14);
    }

    FURI_LOG_I(TAG, "Layout type %x", layout_type);

    uint16_t card_view = 0;
    uint16_t card_type = 0;
    uint32_t card_number = 0;
    uint8_t card_layout = 0;
    uint8_t card_layout2 = 0;
    uint16_t card_use_before_date = 0;
    uint16_t card_blank_type = 0;
    uint32_t card_start_trip_minutes = 0;
    uint8_t card_minutes_pass = 0;
    uint32_t card_remaining_funds = 0;
    uint16_t card_validator = 0;
    uint8_t card_blocked = 0;
    uint32_t card_hash = 0;

    switch(layout_type) {
    case 0x02: {
        card_view = get_bits_16(block->data, 0, 10); //101
        card_type = get_bits_16(block->data, 10, 10); //102
        card_number = get_bits_32(block->data, 20, 32); //201
        card_layout = get_bits(block->data, 52, 4); //111
        card_use_before_date = get_bits_16(block->data, 56, 16); //202
        uint8_t card_benefit_code = get_bits(block->data, 72, 8); //124
        uint32_t card_rfu1 = get_bits_32(block->data, 80, 32); //rfu1
        uint16_t card_crc16 = get_bits_16(block->data, 112, 16); //501.1
        card_blocked = get_bits(block->data, 128, 1); //303
        uint16_t card_start_trip_time = get_bits_16(block->data, 177, 12); //403
        uint16_t card_start_trip_date = get_bits_16(block->data, 189, 16); //402
        uint16_t card_valid_from_date = get_bits_16(block->data, 157, 16); //311
        uint16_t card_valid_by_date = get_bits_16(block->data, 173, 16); //312
        uint8_t card_start_trip_seconds = get_bits(block->data, 189, 6); //406
        uint8_t card_transport_type1 = get_bits(block->data, 180, 2); //421.1
        uint8_t card_transport_type2 = get_bits(block->data, 182, 2); //421.2
        uint8_t card_transport_type3 = get_bits(block->data, 184, 2); //421.3
        uint8_t card_transport_type4 = get_bits(block->data, 186, 2); //421.4
        uint16_t card_use_with_date = get_bits_16(block->data, 189, 16); //205
        uint8_t card_route = get_bits(block->data, 205, 1); //424
        uint16_t card_validator1 = get_bits_16(block->data, 206, 15); //422.1
        card_validator = get_bits_16(block->data, 205, 16); //422
        uint16_t card_total_trips = get_bits_16(block->data, 221, 16); //331
        uint8_t card_write_enabled = get_bits(block->data, 237, 1); //write_enabled
        uint8_t card_rfu2 = get_bits(block->data, 238, 2); //rfu2
        uint16_t card_crc16_2 = get_bits_16(block->data, 240, 16); //501.2

        FURI_LOG_D(
            TAG,
            "%x %x %lx %x %x %lx %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",
            card_view,
            card_type,
            card_number,
            card_use_before_date,
            card_benefit_code,
            card_rfu1,
            card_crc16,
            card_blocked,
            card_start_trip_time,
            card_start_trip_date,
            card_valid_from_date,
            card_valid_by_date,
            card_start_trip_seconds,
            card_transport_type1,
            card_transport_type2,
            card_transport_type3,
            card_transport_type4,
            card_use_with_date,
            card_route,
            card_validator1,
            card_validator,
            card_total_trips,
            card_write_enabled,
            card_rfu2,
            card_crc16_2);
        if(card_valid_by_date == 0) {
            return false;
        }
        FuriHalRtcDateTime card_use_before_date_s = {0};
        from_days_to_datetime(card_valid_by_date, &card_use_before_date_s, 1992);

        FuriHalRtcDateTime card_start_trip_minutes_s = {0};
        from_minutes_to_datetime(
            (card_start_trip_date)*24 * 60 + card_start_trip_time,
            &card_start_trip_minutes_s,
            1992);
        furi_string_printf(
            result,
            "Number: %010lu\nValid for: %02d.%02d.%04d\nTrips: %d\nTrip from: %02d.%02d.%04d %02d:%02d\nValidator: %05d",
            card_number,
            card_use_before_date_s.day,
            card_use_before_date_s.month,
            card_use_before_date_s.year,
            card_total_trips,
            card_start_trip_minutes_s.day,
            card_start_trip_minutes_s.month,
            card_start_trip_minutes_s.year,
            card_start_trip_minutes_s.hour,
            card_start_trip_minutes_s.minute,
            card_validator);
        break;
    }
    case 0x06: {
        card_view = get_bits_16(block->data, 0, 10); //101
        card_type = get_bits_16(block->data, 10, 10); //102
        card_number = get_bits_32(block->data, 20, 32); //201
        card_layout = get_bits(block->data, 52, 4); //111
        card_use_before_date = get_bits_16(block->data, 56, 16); //202
        uint8_t card_geozone_a = get_bits(block->data, 72, 4); //GeoZoneA
        uint8_t card_geozone_b = get_bits(block->data, 76, 4); //GeoZoneB
        card_blank_type = get_bits_16(block->data, 80, 10); //121.
        uint16_t card_type_of_extended = get_bits_16(block->data, 90, 10); //122
        uint32_t card_rfu1 = get_bits_16(block->data, 100, 12); //rfu1
        uint16_t card_crc16 = get_bits_16(block->data, 112, 16); //501.1
        card_blocked = get_bits(block->data, 128, 1); //303
        uint16_t card_start_trip_time = get_bits_16(block->data, 129, 12); //403
        uint16_t card_start_trip_date = get_bits_16(block->data, 141, 16); //402
        uint16_t card_valid_from_date = get_bits_16(block->data, 157, 16); //311
        uint16_t card_valid_by_date = get_bits_16(block->data, 173, 16); //312
        uint16_t card_company = get_bits(block->data, 189, 4); //Company
        uint8_t card_validator1 = get_bits(block->data, 193, 4); //422.1
        uint16_t card_remaining_trips = get_bits_16(block->data, 197, 10); //321
        uint8_t card_units = get_bits(block->data, 207, 6); //Units
        uint16_t card_validator2 = get_bits_16(block->data, 213, 10); //422.2
        uint16_t card_total_trips = get_bits_16(block->data, 223, 16); //331
        uint8_t card_extended = get_bits(block->data, 239, 1); //123
        uint16_t card_crc16_2 = get_bits_16(block->data, 240, 16); //501.2

        FURI_LOG_D(
            TAG,
            "%x %x %lx %x %x %x %x %x %lx %x %x %x %x %x %x %x %x %x %x %x %x %x %x",
            card_view,
            card_type,
            card_number,
            card_use_before_date,
            card_geozone_a,
            card_geozone_b,
            card_blank_type,
            card_type_of_extended,
            card_rfu1,
            card_crc16,
            card_blocked,
            card_start_trip_time,
            card_start_trip_date,
            card_valid_from_date,
            card_valid_by_date,
            card_company,
            card_validator1,
            card_remaining_trips,
            card_units,
            card_validator2,
            card_total_trips,
            card_extended,
            card_crc16_2);
        card_validator = card_validator1 * 1024 + card_validator2;
        FuriHalRtcDateTime card_use_before_date_s = {0};
        from_days_to_datetime(card_valid_by_date, &card_use_before_date_s, 1992);

        FuriHalRtcDateTime card_start_trip_minutes_s = {0};
        from_minutes_to_datetime(
            (card_start_trip_date)*24 * 60 + card_start_trip_time,
            &card_start_trip_minutes_s,
            1992);
        furi_string_printf(
            result,
            "Number: %010lu\nValid for: %02d.%02d.%04d\nTrips left: %d of %d\nTrip from: %02d.%02d.%04d %02d:%02d\nValidator: %05d",
            card_number,
            card_use_before_date_s.day,
            card_use_before_date_s.month,
            card_use_before_date_s.year,
            card_remaining_trips,
            card_total_trips,
            card_start_trip_minutes_s.day,
            card_start_trip_minutes_s.month,
            card_start_trip_minutes_s.year,
            card_start_trip_minutes_s.hour,
            card_start_trip_minutes_s.minute,
            card_validator);
        break;
    }
    case 0x08: {
        card_view = get_bits_16(block->data, 0, 10); //101
        card_type = get_bits_16(block->data, 10, 10); //102
        card_number = get_bits_32(block->data, 20, 32); //201
        card_layout = get_bits(block->data, 52, 4); //111
        card_use_before_date = get_bits_16(block->data, 56, 16); //202
        uint64_t card_rfu1 = get_bits_64(block->data, 72, 56); //rfu1
        uint16_t card_valid_from_date = get_bits_16(block->data, 128, 16); //311
        uint8_t card_valid_for_days = get_bits(block->data, 144, 8); //313
        uint8_t card_requires_activation = get_bits(block->data, 152, 1); //301
        uint8_t card_rfu2 = get_bits(block->data, 153, 7); //rfu2
        uint8_t card_remaining_trips1 = get_bits(block->data, 160, 8); //321.1
        uint8_t card_remaining_trips = get_bits(block->data, 168, 8); //321
        uint8_t card_validator1 = get_bits(block->data, 193, 2); //422.1
        uint16_t card_validator = get_bits_16(block->data, 177, 15); //422
        card_hash = get_bits_32(block->data, 192, 32); //502
        uint32_t card_rfu3 = get_bits_32(block->data, 224, 32); //rfu3

        FURI_LOG_D(
            TAG,
            "%x %x %lx %x %llx %x %x %x %x %x %x %x %x %lx %x %lx",
            card_view,
            card_type,
            card_number,
            card_use_before_date,
            card_rfu1,
            card_valid_from_date,
            card_valid_for_days,
            card_requires_activation,
            card_rfu2,
            card_remaining_trips1,
            card_remaining_trips,
            card_validator1,
            card_validator,
            card_hash,
            card_valid_from_date,
            card_rfu3);
        FuriHalRtcDateTime card_use_before_date_s = {0};
        from_days_to_datetime(card_use_before_date, &card_use_before_date_s, 1992);

        furi_string_printf(
            result,
            "Number: %010lu\nValid for: %02d.%02d.%04d\nTrips left: %d\nValidator: %05d",
            card_number,
            card_use_before_date_s.day,
            card_use_before_date_s.month,
            card_use_before_date_s.year,
            card_remaining_trips,
            card_validator);
        break;
    }
    case 0x0A: {
        card_view = get_bits_16(block->data, 0, 10); //101
        card_type = get_bits_16(block->data, 10, 10); //102
        card_number = get_bits_32(block->data, 20, 32); //201
        card_layout = get_bits(block->data, 52, 4); //111
        uint16_t card_valid_from_date = get_bits_16(block->data, 64, 12); //311
        uint32_t card_valid_for_minutes = get_bits_32(block->data, 76, 19); //314
        uint8_t card_requires_activation = get_bits(block->data, 95, 1); //301
        card_start_trip_minutes = get_bits_32(block->data, 96, 19); //405
        card_minutes_pass = get_bits(block->data, 119, 7); //412
        uint8_t card_transport_type_flag = get_bits(block->data, 126, 2); //421.0
        uint8_t card_remaining_trips = get_bits(block->data, 128, 8); //321
        uint16_t card_validator = get_bits_16(block->data, 136, 16); //422
        uint8_t card_transport_type1 = get_bits(block->data, 152, 2); //421.1
        uint8_t card_transport_type2 = get_bits(block->data, 154, 2); //421.2
        uint8_t card_transport_type3 = get_bits(block->data, 156, 2); //421.3
        uint8_t card_transport_type4 = get_bits(block->data, 158, 2); //421.4
        card_hash = get_bits_32(block->data, 192, 32); //502

        FURI_LOG_D(
            TAG,
            "%x %x %lx %x %x %lx %x %lx %x %x %x %x %x %x %x %x %lx",
            card_view,
            card_type,
            card_number,
            card_use_before_date,
            card_valid_from_date,
            card_valid_for_minutes,
            card_requires_activation,
            card_start_trip_minutes,
            card_minutes_pass,
            card_transport_type_flag,
            card_remaining_trips,
            card_validator,
            card_transport_type1,
            card_transport_type2,
            card_transport_type3,
            card_transport_type4,
            card_hash);
        FuriHalRtcDateTime card_use_before_date_s = {0};
        from_days_to_datetime(card_use_before_date, &card_use_before_date_s, 2016);

        FuriHalRtcDateTime card_start_trip_minutes_s = {0};
        from_minutes_to_datetime(card_start_trip_minutes, &card_start_trip_minutes_s, 2016);
        furi_string_printf(
            result,
            "Number: %010lu\nValid for: %02d.%02d.%04d\nTrip from: %02d.%02d.%04d %02d:%02d\nTrips left: %d\nValidator: %05d",
            card_number,
            card_use_before_date_s.day,
            card_use_before_date_s.month,
            card_use_before_date_s.year,
            card_start_trip_minutes_s.day,
            card_start_trip_minutes_s.month,
            card_start_trip_minutes_s.year,
            card_start_trip_minutes_s.hour,
            card_start_trip_minutes_s.minute,
            card_remaining_trips,
            card_validator);
        break;
    }
    case 0x0C: {
        card_view = get_bits_16(block->data, 0, 10); //101
        card_type = get_bits_16(block->data, 10, 10); //102
        card_number = get_bits_32(block->data, 20, 32); //201
        card_layout = get_bits(block->data, 52, 4); //111
        card_use_before_date = get_bits_16(block->data, 56, 16); //202
        uint64_t card_rfu1 = get_bits_64(block->data, 72, 56); //rfu1
        uint16_t card_valid_from_date = get_bits_16(block->data, 128, 16); //311
        uint8_t card_valid_for_days = get_bits(block->data, 144, 8); //313
        uint8_t card_requires_activation = get_bits(block->data, 152, 1); //301
        uint16_t card_rfu2 = get_bits_16(block->data, 153, 13); //rfu2
        uint16_t card_remaining_trips = get_bits_16(block->data, 166, 10); //321
        uint16_t card_validator = get_bits_16(block->data, 176, 16); //422
        card_hash = get_bits_32(block->data, 192, 32); //502
        uint16_t card_start_trip_date = get_bits_16(block->data, 224, 16); //402
        uint16_t card_start_trip_time = get_bits_16(block->data, 240, 11); //403
        uint8_t card_transport_type = get_bits(block->data, 251, 2); //421
        uint8_t card_rfu3 = get_bits(block->data, 253, 2); //rfu3
        uint8_t card_transfer_in_metro = get_bits(block->data, 255, 1); //432

        FURI_LOG_D(
            TAG,
            "%x %x %lx %x %llx %x %x %x %x %x %x %x %x %x %x %x",
            card_view,
            card_type,
            card_number,
            card_use_before_date,
            card_rfu1,
            card_valid_from_date,
            card_valid_for_days,
            card_requires_activation,
            card_rfu2,
            card_remaining_trips,
            card_validator,
            card_start_trip_date,
            card_start_trip_time,
            card_transport_type,
            card_rfu3,
            card_transfer_in_metro);
        FuriHalRtcDateTime card_use_before_date_s = {0};
        from_days_to_datetime(card_use_before_date, &card_use_before_date_s, 1992);
        FuriHalRtcDateTime card_start_trip_minutes_s = {0};
        from_minutes_to_datetime(
            (card_start_trip_date)*24 * 60 + card_start_trip_time,
            &card_start_trip_minutes_s,
            1992);
        furi_string_printf(
            result,
            "Number: %010lu\nValid for: %02d.%02d.%04d\nTrip from: %02d.%02d.%04d %02d:%02d\nTrips left: %d\nValidator: %05d",
            card_number,
            card_use_before_date_s.day,
            card_use_before_date_s.month,
            card_use_before_date_s.year,
            card_start_trip_minutes_s.day,
            card_start_trip_minutes_s.month,
            card_start_trip_minutes_s.year,
            card_start_trip_minutes_s.hour,
            card_start_trip_minutes_s.minute,
            card_remaining_trips,
            card_validator);
        break;
    }
    case 0x0D: {
        card_view = get_bits_16(block->data, 0, 10); //101
        card_type = get_bits_16(block->data, 10, 10); //102
        card_number = get_bits_32(block->data, 20, 32); //201
        card_layout = get_bits(block->data, 52, 4); //111
        uint8_t card_rfu1 = get_bits(block->data, 56, 8); //rfu1
        card_use_before_date = get_bits_16(block->data, 64, 16); //202
        uint16_t card_valid_for_time = get_bits_16(block->data, 80, 11); //316
        uint8_t card_rfu2 = get_bits(block->data, 91, 5); //rfu2
        uint16_t card_use_before_date2 = get_bits_16(block->data, 96, 16); //202.2
        uint16_t card_valid_for_time2 = get_bits_16(block->data, 123, 11); //316.2
        uint8_t card_rfu3 = get_bits(block->data, 123, 5); //rfu3
        uint16_t card_valid_from_date = get_bits_16(block->data, 128, 16); //311
        uint8_t card_valid_for_days = get_bits(block->data, 144, 8); //313
        uint8_t card_requires_activation = get_bits(block->data, 152, 1); //301
        uint8_t card_rfu4 = get_bits(block->data, 153, 2); //rfu4
        uint8_t card_passage_5_minutes = get_bits(block->data, 155, 5); //413
        uint8_t card_transport_type1 = get_bits(block->data, 160, 2); //421.1
        uint8_t card_passage_in_metro = get_bits(block->data, 162, 1); //431
        uint8_t card_passages_ground_transport = get_bits(block->data, 163, 3); //433
        uint16_t card_remaining_trips = get_bits_16(block->data, 166, 10); //321
        uint16_t card_validator = get_bits_16(block->data, 176, 16); //422
        card_hash = get_bits_32(block->data, 192, 32); //502
        uint16_t card_start_trip_date = get_bits_16(block->data, 224, 16); //402
        uint16_t card_start_trip_time = get_bits_16(block->data, 240, 11); //403
        uint8_t card_transport_type2 = get_bits(block->data, 251, 2); //421.2
        uint8_t card_rfu5 = get_bits(block->data, 253, 2); //rfu5
        uint8_t card_transfer_in_metro = get_bits(block->data, 255, 1); //432

        FURI_LOG_D(
            TAG,
            "%x %x %lx %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",
            card_view,
            card_type,
            card_number,
            card_layout,
            card_rfu1,
            card_use_before_date,
            card_valid_for_time,
            card_rfu2,
            card_use_before_date2,
            card_valid_for_time2,
            card_rfu3,
            card_valid_from_date,
            card_valid_for_days,
            card_requires_activation,
            card_rfu4,
            card_passage_5_minutes,
            card_transport_type1,
            card_passage_in_metro,
            card_passages_ground_transport,
            card_remaining_trips,
            card_validator,
            card_start_trip_date,
            card_start_trip_time,
            card_transport_type2,
            card_rfu5,
            card_transfer_in_metro);
        FuriHalRtcDateTime card_use_before_date_s = {0};
        from_days_to_datetime(card_use_before_date, &card_use_before_date_s, 1992);
        FuriHalRtcDateTime card_start_trip_minutes_s = {0};
        from_minutes_to_datetime(
            (card_start_trip_date)*24 * 60 + card_start_trip_time,
            &card_start_trip_minutes_s,
            1992);
        furi_string_printf(
            result,
            "Number: %010lu\nValid for: %02d.%02d.%04d\nTrip from: %02d.%02d.%04d %02d:%02d\nTrips left: %d\nValidator: %05d",
            card_number,
            card_use_before_date_s.day,
            card_use_before_date_s.month,
            card_use_before_date_s.year,
            card_start_trip_minutes_s.day,
            card_start_trip_minutes_s.month,
            card_start_trip_minutes_s.year,
            card_start_trip_minutes_s.hour,
            card_start_trip_minutes_s.minute,
            card_remaining_trips,
            card_validator);
        break;
    }
    case 0x1C1: {
        card_view = get_bits_16(block->data, 0, 10); //101
        card_type = get_bits_16(block->data, 10, 10); //102
        card_number = get_bits_32(block->data, 20, 32); //201
        card_layout = get_bits(block->data, 52, 4); //111
        card_layout2 = get_bits(block->data, 56, 5); //112
        card_use_before_date = get_bits_16(block->data, 61, 16); //202.
        card_blank_type = get_bits_16(block->data, 77, 10); //121.
        card_validator = get_bits_16(block->data, 128, 16); //422
        uint16_t card_start_trip_date = get_bits_16(block->data, 144, 16); //402
        uint16_t card_start_trip_time = get_bits_16(block->data, 160, 11); //403
        uint8_t card_transport_type1 = get_bits(block->data, 171, 2); //421.1
        uint8_t card_transport_type2 = get_bits(block->data, 173, 2); //421.2
        uint8_t card_transfer_in_metro = get_bits(block->data, 177, 1); //432
        uint8_t card_passage_in_metro = get_bits(block->data, 178, 1); //431
        uint8_t card_passages_ground_transport = get_bits(block->data, 179, 3); //433
        card_minutes_pass = get_bits(block->data, 185, 8); //412.
        card_remaining_funds = get_bits_32(block->data, 196, 19) / 100; //322
        uint8_t card_fare_trip = get_bits(block->data, 215, 2); //441
        card_blocked = get_bits(block->data, 202, 1); //303
        uint8_t card_zoo = get_bits(block->data, 218, 1); //zoo
        card_hash = get_bits_32(block->data, 224, 32); //502

        FURI_LOG_D(
            TAG,
            "%x %x %lx %x %x %x %x %x %x %x %x %x %x %x %x %x %lx %x %x %x %lx",
            card_view,
            card_type,
            card_number,
            card_layout,
            card_layout2,
            card_use_before_date,
            card_blank_type,
            card_validator,
            card_start_trip_date,
            card_start_trip_time,
            card_transport_type1,
            card_transport_type2,
            card_transfer_in_metro,
            card_passage_in_metro,
            card_passages_ground_transport,
            card_minutes_pass,
            card_remaining_funds,
            card_fare_trip,
            card_blocked,
            card_zoo,
            card_hash);
        FuriHalRtcDateTime card_use_before_date_s = {0};
        from_days_to_datetime(card_use_before_date, &card_use_before_date_s, 1992);

        FuriHalRtcDateTime card_start_trip_minutes_s = {0};
        from_minutes_to_datetime(card_start_trip_minutes, &card_start_trip_minutes_s, 1992);
        furi_string_printf(
            result,
            "Number: %010lu\nValid for: %02d.%02d.%04d\nTrip from: %02d.%02d.%04d %02d:%02d\nValidator: %05d",
            card_number,
            card_use_before_date_s.day,
            card_use_before_date_s.month,
            card_use_before_date_s.year,
            card_start_trip_minutes_s.day,
            card_start_trip_minutes_s.month,
            card_start_trip_minutes_s.year,
            card_start_trip_minutes_s.hour,
            card_start_trip_minutes_s.minute,
            card_validator);
        break;
    }
    case 0x1C2: {
        card_view = get_bits_16(block->data, 0, 10); //101
        card_type = get_bits_16(block->data, 10, 10); //102
        card_number = get_bits_32(block->data, 20, 32); //201
        card_layout = get_bits(block->data, 52, 4); //111
        card_layout2 = get_bits(block->data, 56, 5); //112
        uint16_t card_type_of_extended = get_bits_16(block->data, 61, 10); //122
        card_use_before_date = get_bits_16(block->data, 71, 16); //202.
        card_blank_type = get_bits_16(block->data, 87, 10); //121.
        uint16_t card_valid_to_date = get_bits_16(block->data, 97, 16); //311
        uint16_t card_activate_during = get_bits_16(block->data, 113, 9); //302
        uint32_t card_valid_for_minutes = get_bits_32(block->data, 131, 20); //314
        card_minutes_pass = get_bits(block->data, 154, 8); //412.
        uint8_t card_transport_type = get_bits(block->data, 163, 2); //421
        uint8_t card_passage_in_metro = get_bits(block->data, 165, 1); //431
        uint8_t card_transfer_in_metro = get_bits(block->data, 166, 1); //432
        uint16_t card_remaining_trips = get_bits_16(block->data, 167, 10); //321
        card_validator = get_bits_16(block->data, 177, 16); //422
        uint32_t card_start_trip_neg_minutes = get_bits_32(block->data, 196, 20); //404
        uint8_t card_requires_activation = get_bits(block->data, 216, 1); //301
        card_blocked = get_bits(block->data, 217, 1); //303
        uint8_t card_extended = get_bits(block->data, 218, 1); //123
        card_hash = get_bits_32(block->data, 224, 32); //502

        FURI_LOG_D(
            TAG,
            "%x %x %lx %x %x %x %x %x %x %x %lx %x %x %x %x %x %x %lx %x %x %x %lx",
            card_view,
            card_type,
            card_number,
            card_layout,
            card_layout2,
            card_type_of_extended,
            card_use_before_date,
            card_blank_type,
            card_valid_to_date,
            card_activate_during,
            card_valid_for_minutes,
            card_minutes_pass,
            card_transport_type,
            card_passage_in_metro,
            card_transfer_in_metro,
            card_remaining_trips,
            card_validator,
            card_start_trip_neg_minutes,
            card_requires_activation,
            card_blocked,
            card_extended,
            card_hash);
        FuriHalRtcDateTime card_use_before_date_s = {0};
        from_days_to_datetime(card_use_before_date, &card_use_before_date_s, 2016);

        FuriHalRtcDateTime card_start_trip_minutes_s = {0};
        from_minutes_to_datetime(
            (card_valid_to_date)*24 * 60 + card_valid_for_minutes - card_start_trip_neg_minutes,
            &card_start_trip_minutes_s,
            2016); //-time
        furi_string_printf(
            result,
            "Number: %010lu\nValid for: %02d.%02d.%04d\nTrip from: %02d.%02d.%04d %02d:%02d\nValidator: %05d",
            card_number,
            card_use_before_date_s.day,
            card_use_before_date_s.month,
            card_use_before_date_s.year,
            card_start_trip_minutes_s.day,
            card_start_trip_minutes_s.month,
            card_start_trip_minutes_s.year,
            card_start_trip_minutes_s.hour,
            card_start_trip_minutes_s.minute,
            card_validator);
        break;
    }
    case 0x1C3: {
        card_view = get_bits_16(block->data, 0, 10); //101
        card_type = get_bits_16(block->data, 10, 10); //102
        card_number = get_bits_32(block->data, 20, 32); //201
        card_layout = get_bits(block->data, 52, 4); //111
        card_layout2 = get_bits(block->data, 56, 5); //112
        card_use_before_date = get_bits_16(block->data, 61, 16); //202
        card_blank_type = get_bits_16(block->data, 77, 10); //121
        card_remaining_funds = get_bits_32(block->data, 188, 22) / 100; //322
        card_hash = get_bits_32(block->data, 224, 32); //502
        card_validator = get_bits_16(block->data, 128, 16); //422
        card_start_trip_minutes = get_bits_32(block->data, 144, 23); //405
        uint8_t card_fare_trip = get_bits(block->data, 210, 2); //441
        card_minutes_pass = get_bits(block->data, 171, 7); //412
        uint8_t card_transport_type_flag = get_bits(block->data, 178, 2); //421.0
        uint8_t card_transport_type1 = get_bits(block->data, 180, 2); //421.1
        uint8_t card_transport_type2 = get_bits(block->data, 182, 2); //421.2
        uint8_t card_transport_type3 = get_bits(block->data, 184, 2); //421.3
        uint8_t card_transport_type4 = get_bits(block->data, 186, 2); //421.4
        card_blocked = get_bits(block->data, 212, 1); //303
        FURI_LOG_D(
            TAG,
            "Card view: %x, type: %x, number: %lx, layout: %x, layout2: %x, use before date: %x, blank type: %x, remaining funds: %lx, hash: %lx, validator: %x, start trip minutes: %lx, fare trip: %x, minutes pass: %x, transport type flag: %x, transport type1: %x, transport type2: %x, transport type3: %x, transport type4: %x, blocked: %x",
            card_view,
            card_type,
            card_number,
            card_layout,
            card_layout2,
            card_use_before_date,
            card_blank_type,
            card_remaining_funds,
            card_hash,
            card_validator,
            card_start_trip_minutes,
            card_fare_trip,
            card_minutes_pass,
            card_transport_type_flag,
            card_transport_type1,
            card_transport_type2,
            card_transport_type3,
            card_transport_type4,
            card_blocked);
        FuriHalRtcDateTime card_use_before_date_s = {0};
        from_days_to_datetime(card_use_before_date, &card_use_before_date_s, 1992);

        FuriHalRtcDateTime card_start_trip_minutes_s = {0};
        from_minutes_to_datetime(card_start_trip_minutes, &card_start_trip_minutes_s, 2016);
        furi_string_printf(
            result,
            "Number: %010lu\nValid for: %02d.%02d.%04d\nBalance: %ld rub\nTrip from: %02d.%02d.%04d %02d:%02d\nValidator: %05d",
            card_number,
            card_use_before_date_s.day,
            card_use_before_date_s.month,
            card_use_before_date_s.year,
            card_remaining_funds,
            card_start_trip_minutes_s.day,
            card_start_trip_minutes_s.month,
            card_start_trip_minutes_s.year,
            card_start_trip_minutes_s.hour,
            card_start_trip_minutes_s.minute,
            card_validator);
        break;
    }
    case 0x1C4: {
        card_view = get_bits_16(block->data, 0, 10); //101
        card_type = get_bits_16(block->data, 10, 10); //102
        card_number = get_bits_32(block->data, 20, 32); //201
        card_layout = get_bits(block->data, 52, 4); //111
        card_layout2 = get_bits(block->data, 56, 5); //112
        uint16_t card_type_of_extended = get_bits_16(block->data, 61, 10); //122
        card_use_before_date = get_bits_16(block->data, 71, 13); //202.
        card_blank_type = get_bits_16(block->data, 84, 10); //121.
        uint16_t card_valid_to_date = get_bits_16(block->data, 94, 13); //311
        uint16_t card_activate_during = get_bits_16(block->data, 107, 9); //302
        uint16_t card_extension_counter = get_bits_16(block->data, 116, 10); //304
        uint32_t card_valid_for_minutes = get_bits_32(block->data, 128, 20); //314
        card_minutes_pass = get_bits(block->data, 158, 7); //412.
        uint8_t card_transport_type_flag = get_bits(block->data, 178, 2); //421.0
        uint8_t card_transport_type1 = get_bits(block->data, 180, 2); //421.1
        uint8_t card_transport_type2 = get_bits(block->data, 182, 2); //421.2
        uint8_t card_transport_type3 = get_bits(block->data, 184, 2); //421.3
        uint8_t card_transport_type4 = get_bits(block->data, 186, 2); //421.4
        uint16_t card_remaining_trips = get_bits_16(block->data, 169, 10); //321
        card_validator = get_bits_16(block->data, 179, 16); //422
        uint32_t card_start_trip_neg_minutes = get_bits_32(block->data, 195, 20); //404
        uint8_t card_requires_activation = get_bits(block->data, 215, 1); //301
        card_blocked = get_bits(block->data, 216, 1); //303
        uint8_t card_extended = get_bits(block->data, 217, 1); //123
        card_hash = get_bits_32(block->data, 224, 32); //502

        FURI_LOG_D(
            TAG,
            "%x %x %lx %x %x %x %x %x %x %x %x %lx %x %x %x %x %x %x %x %x %lx %x %x %x %lx",
            card_view,
            card_type,
            card_number,
            card_layout,
            card_layout2,
            card_type_of_extended,
            card_use_before_date,
            card_blank_type,
            card_valid_to_date,
            card_activate_during,
            card_extension_counter,
            card_valid_for_minutes,
            card_minutes_pass,
            card_transport_type_flag,
            card_transport_type1,
            card_transport_type2,
            card_transport_type3,
            card_transport_type4,
            card_remaining_trips,
            card_validator,
            card_start_trip_neg_minutes,
            card_requires_activation,
            card_blocked,
            card_extended,
            card_hash);
        FuriHalRtcDateTime card_use_before_date_s = {0};
        from_days_to_datetime(card_use_before_date, &card_use_before_date_s, 2016);

        FuriHalRtcDateTime card_start_trip_minutes_s = {0};
        from_minutes_to_datetime(
            (card_use_before_date + 1) * 24 * 60 + card_valid_for_minutes -
                card_start_trip_neg_minutes,
            &card_start_trip_minutes_s,
            2011); //-time
        furi_string_printf(
            result,
            "Number: %010lu\nValid for: %02d.%02d.%04d\nTrip from: %02d.%02d.%04d %02d:%02d\nValidator: %05d",
            card_number,
            card_use_before_date_s.day,
            card_use_before_date_s.month,
            card_use_before_date_s.year,
            card_start_trip_minutes_s.day,
            card_start_trip_minutes_s.month,
            card_start_trip_minutes_s.year,
            card_start_trip_minutes_s.hour,
            card_start_trip_minutes_s.minute,
            card_validator);
        break;
    }
    case 0x1C5: {
        card_view = get_bits_16(block->data, 0, 10); //101
        card_type = get_bits_16(block->data, 10, 10); //102
        card_number = get_bits_32(block->data, 20, 32); //201
        card_layout = get_bits(block->data, 52, 4); //111
        card_layout2 = get_bits(block->data, 56, 5); //112
        card_use_before_date = get_bits_16(block->data, 61, 13); //202.
        card_blank_type = get_bits_16(block->data, 74, 10); //121.
        uint32_t card_valid_to_time = get_bits_32(block->data, 84, 23); //317
        uint16_t card_extension_counter = get_bits_16(block->data, 107, 10); //304
        card_start_trip_minutes = get_bits_32(block->data, 128, 23); //405
        uint8_t card_metro_ride_with = get_bits(block->data, 151, 7); //414
        card_minutes_pass = get_bits(block->data, 158, 7); //412.
        card_remaining_funds = get_bits_32(block->data, 167, 19) / 100; //322
        card_validator = get_bits_16(block->data, 186, 16); //422
        card_blocked = get_bits(block->data, 202, 1); //303
        uint16_t card_route = get_bits_16(block->data, 204, 12); //424
        uint8_t card_passages_ground_transport = get_bits(block->data, 216, 7); //433
        card_hash = get_bits_32(block->data, 224, 32); //502

        FURI_LOG_D(
            TAG,
            "%x %x %lx %x %x %x %x %lx %x %lx %x %x %lx %x %x %x %x %lx",
            card_view,
            card_type,
            card_number,
            card_layout,
            card_layout2,
            card_use_before_date,
            card_blank_type,
            card_valid_to_time,
            card_extension_counter,
            card_start_trip_minutes,
            card_metro_ride_with,
            card_minutes_pass,
            card_remaining_funds,
            card_validator,
            card_blocked,
            card_route,
            card_passages_ground_transport,
            card_hash);
        FuriHalRtcDateTime card_use_before_date_s = {0};

        from_days_to_datetime(card_use_before_date, &card_use_before_date_s, 2019);

        FuriHalRtcDateTime card_start_trip_minutes_s = {0};
        from_minutes_to_datetime(card_start_trip_minutes, &card_start_trip_minutes_s, 2019);
        furi_string_printf(
            result,
            "Number: %010lu\nValid for: %02d.%02d.%04d\nBalance: %ld rub\nTrip from: %02d.%02d.%04d %02d:%02d\nValidator: %05d",
            card_number,
            card_use_before_date_s.day,
            card_use_before_date_s.month,
            card_use_before_date_s.year,
            card_remaining_funds,
            card_start_trip_minutes_s.day,
            card_start_trip_minutes_s.month,
            card_start_trip_minutes_s.year,
            card_start_trip_minutes_s.hour,
            card_start_trip_minutes_s.minute,
            card_validator);
        break;
    }
    case 0x1C6: {
        card_view = get_bits_16(block->data, 0, 10); //101
        card_type = get_bits_16(block->data, 10, 10); //102
        card_number = get_bits_32(block->data, 20, 32); //201
        card_layout = get_bits(block->data, 52, 4); //111
        card_layout2 = get_bits(block->data, 56, 5); //112
        uint16_t card_type_of_extended = get_bits_16(block->data, 61, 10); //122
        card_use_before_date = get_bits_16(block->data, 71, 13); //202.
        card_blank_type = get_bits_16(block->data, 84, 10); //121.
        uint32_t card_valid_from_date = get_bits_32(block->data, 94, 23); //311
        uint16_t card_extension_counter = get_bits_16(block->data, 117, 10); //304
        uint32_t card_valid_for_minutes = get_bits_32(block->data, 128, 20); //314
        uint32_t card_start_trip_neg_minutes = get_bits_32(block->data, 148, 20); //404
        uint8_t card_metro_ride_with = get_bits(block->data, 168, 7); //414
        card_minutes_pass = get_bits(block->data, 175, 7); //412.
        uint16_t card_remaining_trips = get_bits_16(block->data, 182, 7); //321
        card_validator = get_bits_16(block->data, 189, 16); //422
        card_blocked = get_bits(block->data, 205, 1); //303
        uint8_t card_extended = get_bits(block->data, 206, 1); //123
        uint16_t card_route = get_bits_16(block->data, 212, 12); //424
        card_hash = get_bits_32(block->data, 224, 32); //502

        FURI_LOG_D(
            TAG,
            "%x %x %lx %x %x %x %x %x %lx %x %lx %lx %x %x %x %x %x %x %x %lx",
            card_view,
            card_type,
            card_number,
            card_layout,
            card_layout2,
            card_type_of_extended,
            card_use_before_date,
            card_blank_type,
            card_valid_from_date,
            card_extension_counter,
            card_valid_for_minutes,
            card_start_trip_neg_minutes,
            card_metro_ride_with,
            card_minutes_pass,
            card_remaining_trips,
            card_validator,
            card_blocked,
            card_extended,
            card_route,
            card_hash);
        FuriHalRtcDateTime card_use_before_date_s = {0};
        from_days_to_datetime(card_use_before_date, &card_use_before_date_s, 2019);

        FuriHalRtcDateTime card_start_trip_minutes_s = {0};
        from_minutes_to_datetime(
            card_valid_from_date + card_valid_for_minutes - card_start_trip_neg_minutes,
            &card_start_trip_minutes_s,
            2019); //-time
        furi_string_printf(
            result,
            "Number: %010lu\nValid for: %02d.%02d.%04d\nTrip from: %02d.%02d.%04d %02d:%02d\nValidator: %05d",
            card_number,
            card_use_before_date_s.day,
            card_use_before_date_s.month,
            card_use_before_date_s.year,
            card_start_trip_minutes_s.day,
            card_start_trip_minutes_s.month,
            card_start_trip_minutes_s.year,
            card_start_trip_minutes_s.hour,
            card_start_trip_minutes_s.minute,
            card_validator);
        break;
    }
    case 0x3CCB: {
        card_view = get_bits_16(block->data, 0, 10); //101
        card_type = get_bits_16(block->data, 10, 10); //102
        card_number = get_bits_32(block->data, 20, 32); //201
        card_layout = get_bits(block->data, 52, 4); //111
        uint16_t card_tech_code = get_bits_32(block->data, 56, 10); //tech_code
        uint16_t card_valid_to_minutes = get_bits_16(block->data, 66, 16); //311
        uint16_t card_valid_by_date = get_bits_16(block->data, 82, 16); //312
        uint8_t card_interval = get_bits(block->data, 98, 4); //interval
        uint16_t card_app_code1 = get_bits_16(block->data, 102, 16); //app_code1
        uint16_t card_hash1 = get_bits_16(block->data, 112, 16); //502.1
        uint16_t card_type1 = get_bits_16(block->data, 128, 10); //type1
        uint16_t card_app_code2 = get_bits_16(block->data, 138, 10); //app_code2
        uint16_t card_type2 = get_bits_16(block->data, 148, 10); //type2
        uint16_t card_app_code3 = get_bits_16(block->data, 158, 10); //app_code3
        uint16_t card_type3 = get_bits_16(block->data, 148, 10); //type3
        uint16_t card_app_code4 = get_bits_16(block->data, 168, 10); //app_code4
        uint16_t card_type4 = get_bits_16(block->data, 178, 10); //type4
        card_hash = get_bits_32(block->data, 224, 32); //502.2

        FURI_LOG_D(
            TAG,
            "%x %x %lx %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %lx",
            card_view,
            card_type,
            card_number,
            card_layout,
            card_tech_code,
            card_use_before_date,
            card_blank_type,
            card_valid_to_minutes,
            card_valid_by_date,
            card_interval,
            card_app_code1,
            card_hash1,
            card_type1,
            card_app_code2,
            card_type2,
            card_app_code3,
            card_type3,
            card_app_code4,
            card_type4,
            card_hash);
        FuriHalRtcDateTime card_use_before_date_s = {0};
        from_days_to_datetime(card_valid_by_date, &card_use_before_date_s, 1992);

        furi_string_printf(
            result,
            "Number: %010lu\nValid for: %02d.%02d.%04d\nValidator: %05d",
            card_number,
            card_use_before_date_s.day,
            card_use_before_date_s.month,
            card_use_before_date_s.year,
            card_validator);
        break;
    }
    case 0x3C0B: {
        card_view = get_bits_16(block->data, 0, 10); //101
        card_type = get_bits_16(block->data, 10, 10); //102
        card_number = get_bits_32(block->data, 20, 32); //201
        card_layout = get_bits(block->data, 52, 4); //111
        uint16_t card_tech_code = get_bits_32(block->data, 56, 10); //tech_code
        uint16_t card_valid_to_minutes = get_bits_16(block->data, 66, 16); //311
        uint16_t card_valid_by_date = get_bits_16(block->data, 82, 16); //312
        uint16_t card_hash = get_bits_16(block->data, 112, 16); //502.1

        FURI_LOG_D(
            TAG,
            "%x %x %lx %x %x %x %x %x %x %x",
            card_view,
            card_type,
            card_number,
            card_layout,
            card_tech_code,
            card_use_before_date,
            card_blank_type,
            card_valid_to_minutes,
            card_valid_by_date,
            card_hash);
        FuriHalRtcDateTime card_use_before_date_s = {0};
        from_days_to_datetime(card_valid_by_date, &card_use_before_date_s, 1992);

        furi_string_printf(
            result,
            "Number: %010lu\nValid for: %02d.%02d.%04d\nValidator: %05d",
            card_number,
            card_use_before_date_s.day,
            card_use_before_date_s.month,
            card_use_before_date_s.year,
            card_validator);
        break;
    }
    default:
        return false;
    }

    return true;
}

static bool troika_get_card_config(TroikaCardConfig* config, MfClassicType type) {
    bool success = true;

    if(type == MfClassicType1k) {
        config->data_sector = 11;
        config->keys = troika_1k_keys;
    } else if(type == MfClassicType4k) {
        config->data_sector = 11;
        config->keys = troika_4k_keys;
    } else {
        success = false;
    }

    return success;
}

static TroikaLayout troika_get_layout(const MfClassicData* data, uint8_t start_block_num) {
    furi_assert(data);

    // Layout is stored in byte 6 of block, length 4 bits (bits 52 - 55), second nibble.
    const uint8_t* layout_ptr = &data->block[start_block_num].data[6];
    const uint8_t layout = (*layout_ptr & 0x0F);

    TroikaLayout result = TroikaLayoutUnknown;
    switch(layout) {
    case 0x2:
        result = TroikaLayout2;
        break;
    case 0xE:
        result = TroikaLayoutE;
        break;
    default:
        // If debug is enabled - pass the actual layout value for the debug text
        if(furi_hal_rtc_is_flag_set(FuriHalRtcFlagDebug)) {
            return layout;
        } else {
            return TroikaLayoutUnknown;
        }
    }

    return result;
}

static TroikaSubLayout troika_get_sub_layout(const MfClassicData* data, uint8_t start_block_num) {
    furi_assert(data);

    // Sublayout is stored in byte 7 (bits 56 - 60) of block, length 5 bits (first nibble and one bit from second nibble)
    const uint8_t* sub_layout_ptr = &data->block[start_block_num].data[7];
    const uint8_t sub_layout = (*sub_layout_ptr & 0x3F) >> 3;

    TroikaSubLayout result = TroikaSublayoutUnknown;
    switch(sub_layout) {
    case 3:
        result = TroikaSublayout3;
        break;
    case 5:
        result = TroikaSublayout5;
        break;
    case 6:
        result = TroikaSublayout6;
        break;
    default:
        // If debug is enabled - pass the actual sublayout value for the debug text
        if(furi_hal_rtc_is_flag_set(FuriHalRtcFlagDebug)) {
            return sub_layout;
        } else {
            return TroikaSublayoutUnknown;
        }
    }

    return result;
}

static bool troika_has_balance(TroikaLayout layout, TroikaSubLayout sub_layout) {
    UNUSED(sub_layout);
    // Layout 0x2 has no balance

    if(layout == TroikaLayout2) {
        return false;
    }

    return true;
}

static uint16_t troika_get_balance(
    const MfClassicData* data,
    uint8_t start_block_num,
    TroikaLayout layout,
    TroikaSubLayout sub_layout) {
    furi_assert(data);

    // In layout 0x3 balance in bits 188:209 ( from sector start, length 22).
    // In layout 0x5 balance in bits 165:185 ( from sector start, length 20).

    if(layout == TroikaLayoutE && sub_layout == TroikaSublayout3) {
        const uint8_t* temp_ptr = &data->block[start_block_num + 1].data[7];
        uint32_t balance = 0;
        balance |= (temp_ptr[0] & 0x3) << 18;
        balance |= temp_ptr[1] << 10;
        balance |= temp_ptr[2] << 2;
        balance |= (temp_ptr[3] & 0xC0) >> 6;

        return balance / 100;
    } else if(layout == TroikaLayoutE && sub_layout == TroikaSublayout5) {
        const uint8_t* temp_ptr = &data->block[start_block_num + 1].data[4];
        uint32_t balance = 0;
        balance |= (temp_ptr[0] & 0x3) << 18;
        balance |= temp_ptr[1] << 10;
        balance |= temp_ptr[2] << 2;
        balance |= (temp_ptr[3] & 0xC0) >> 6;

        return balance / 100;
    } else {
        return 0;
    }
}

static uint32_t troika_get_number(
    const MfClassicData* data,
    uint8_t start_block_num,
    TroikaLayout layout,
    TroikaSubLayout sub_layout) {
    furi_assert(data);
    UNUSED(sub_layout);

    if(layout == TroikaLayoutE || layout == TroikaLayout2) {
        const uint8_t* temp_ptr = &data->block[start_block_num].data[2];

        uint32_t number = 0;
        for(size_t i = 1; i < 5; i++) {
            number <<= 8;
            number |= temp_ptr[i];
        }
        number >>= 4;
        number |= (temp_ptr[0] & 0xf) << 28;

        return number;
    } else {
        return 0;
    }
}

static bool troika_verify_type(Nfc* nfc, MfClassicType type) {
    bool verified = false;

    do {
        TroikaCardConfig cfg = {};
        if(!troika_get_card_config(&cfg, type)) break;

        const uint8_t block_num = mf_classic_get_first_block_num_of_sector(cfg.data_sector);
        FURI_LOG_D(TAG, "Verifying sector %lu", cfg.data_sector);

        MfClassicKey key = {0};
        nfc_util_num2bytes(cfg.keys[cfg.data_sector].a, COUNT_OF(key.data), key.data);

        MfClassicAuthContext auth_context;
        MfClassicError error =
            mf_classic_poller_sync_auth(nfc, block_num, &key, MfClassicKeyTypeA, &auth_context);
        if(error != MfClassicErrorNone) {
            FURI_LOG_D(TAG, "Failed to read block %u: %d", block_num, error);
            break;
        }
        FURI_LOG_D(TAG, "Verify success!");
        verified = true;
    } while(false);

    return verified;
}

static bool troika_verify(Nfc* nfc) {
    return troika_verify_type(nfc, MfClassicType1k) || troika_verify_type(nfc, MfClassicType4k);
}

static bool troika_read(Nfc* nfc, NfcDevice* device) {
    furi_assert(nfc);
    furi_assert(device);

    bool is_read = false;

    MfClassicData* data = mf_classic_alloc();
    nfc_device_copy_data(device, NfcProtocolMfClassic, data);

    do {
        MfClassicType type = MfClassicTypeMini;
        MfClassicError error = mf_classic_poller_sync_detect_type(nfc, &type);
        if(error != MfClassicErrorNone) break;

        data->type = type;
        TroikaCardConfig cfg = {};
        if(!troika_get_card_config(&cfg, data->type)) break;

        MfClassicDeviceKeys keys = {
            .key_a_mask = 0,
            .key_b_mask = 0,
        };
        for(size_t i = 0; i < mf_classic_get_total_sectors_num(data->type); i++) {
            nfc_util_num2bytes(cfg.keys[i].a, sizeof(MfClassicKey), keys.key_a[i].data);
            FURI_BIT_SET(keys.key_a_mask, i);
            nfc_util_num2bytes(cfg.keys[i].b, sizeof(MfClassicKey), keys.key_b[i].data);
            FURI_BIT_SET(keys.key_b_mask, i);
        }

        error = mf_classic_poller_sync_read(nfc, &keys, data);
        if(error == MfClassicErrorNotPresent) {
            FURI_LOG_W(TAG, "Failed to read data");
            break;
        }

        nfc_device_set_data(device, NfcProtocolMfClassic, data);

        is_read = (error == MfClassicErrorNone);
    } while(false);

    mf_classic_free(data);

    return is_read;
}

static bool troika_parse(const NfcDevice* device, FuriString* parsed_data) {
    furi_assert(device);

    const MfClassicData* data = nfc_device_get_data(device, NfcProtocolMfClassic);

    bool parsed = false;

    do {
        // Verify card type
        TroikaCardConfig cfg = {};
        if(!troika_get_card_config(&cfg, data->type)) break;

        // Verify key
        const MfClassicSectorTrailer* sec_tr =
            mf_classic_get_sector_trailer_by_sector(data, cfg.data_sector);

        const uint64_t key = nfc_util_bytes2num(sec_tr->key_a.data, COUNT_OF(sec_tr->key_a.data));
        if(key != cfg.keys[cfg.data_sector].a) break;

        // Get the block number of the block that contains the data
        const uint8_t start_block_num = mf_classic_get_first_block_num_of_sector(cfg.data_sector);

        // Get layout, sublayout, balance and number
        TroikaLayout layout = troika_get_layout(data, start_block_num);
        TroikaSubLayout sub_layout = troika_get_sub_layout(data, start_block_num);

        if(!furi_hal_rtc_is_flag_set(FuriHalRtcFlagDebug)) {
            // If debug is enabled - proceed even if layout or sublayout is unknown, that will make collecting data easier
            if(layout == TroikaLayoutUnknown || sub_layout == TroikaSublayoutUnknown) break;
        }

        uint32_t number = troika_get_number(data, start_block_num, layout, sub_layout);

        furi_string_printf(parsed_data, "\e#Troika\nNum: %lu", number);

        if(troika_has_balance(layout, sub_layout) ||
           furi_hal_rtc_is_flag_set(FuriHalRtcFlagDebug)) {
            uint16_t balance = troika_get_balance(data, start_block_num, layout, sub_layout);
            furi_string_cat_printf(parsed_data, "\nBalance: %u RUR", balance);
        } else {
            furi_string_cat_printf(parsed_data, "\nBalance: Not available");
        }

        if(furi_hal_rtc_is_flag_set(FuriHalRtcFlagDebug)) {
            furi_string_cat_printf(
                parsed_data,
                "\nLayout: %02x\nSublayout: %02x\nData Block: %u",
                layout,
                sub_layout,
                start_block_num);
        }

        parsed = true;
    } while(false);

    return parsed;
}

/* Actual implementation of app<>plugin interface */
static const NfcSupportedCardsPlugin troika_plugin = {
    .protocol = NfcProtocolMfClassic,
    .verify = troika_verify,
    .read = troika_read,
    .parse = troika_parse,
};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor troika_plugin_descriptor = {
    .appid = NFC_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = NFC_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &troika_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* troika_plugin_ep() {
    return &troika_plugin_descriptor;
}