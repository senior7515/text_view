// Copyright (c) 2016, Tom Honermann
//
// This file is distributed under the MIT License. See the accompanying file
// LICENSE.txt or http://www.opensource.org/licenses/mit-license.php for terms
// and conditions.

#if !defined(TEXT_VIEW_CODECS_UTF32BOM_CODEC_HPP) // {
#define TEXT_VIEW_CODECS_UTF32BOM_CODEC_HPP


#include <cassert>
#include <climits>
#include <text_view_detail/concepts.hpp>
#include <text_view_detail/codecs/utf32be_codec.hpp>
#include <text_view_detail/codecs/utf32le_codec.hpp>


namespace std {
namespace experimental {
inline namespace text {


/*
 *  to_initial                                             to_initial
 *  +-------------------------------v-------------------------------+
 *  |                               v                               |
 *  |                       +---------------+                       |
 *  ^----------------------<| initial state |                       |
 *  |                       +---------------+                       |
 *  |  to_assume_bom_written    v |   | v                           |
 *  |  to_assume_be_bom_written | |   | |to_assume_le_bom_written   |
 *  |     +---------------------+ |   | +---------------------+     |
 *  |     |      to_bom_written   |   |                       |     |
 *  |     |      to_be_bom_written|   |to_le_bom_written      |     |
 *  |     |          +------------+   +------------+          |     |
 *  |     |          v                             v          |     |
 *  |     |   /------------\                 /------------\   |     |
 *  |     |  | write BE BOM |               | write LE BOM |  |     |
 *  |     |   \------------/                 \------------/   |     |
 *  |     |          v                             v          |     |
 *  |     |         e|                             |e         |     |
 *  |     |          v-------------< >-------------v          |     |
 *  |     v          v             | |             v          v     |
 *  |  +------------------------+  | |  +------------------------+  |
 *  |  | BE BOM read or written |  | |  | LE BOM read or written |  |
 *  |  +------------------------+  | |  +------------------------+  |
 *  |     v        v      [1]v     | |     v[2]     v         v     |
 *  +-----+        |         +-----+ +-----+        |         +-----+
 *                 |                                |
 *                 |             /-----\            |
 *                 +----------->| ERROR |<----------+
 *             to_le_bom_written \-----/ to_be_bom_written
 *      to_assume_le_bom_written         to_assume_be_bom_written
 *
 * [1]: to_bom_written              [2]: to_bom_written
 *      to_be_bom_written                to_le_bom_written
 *      to_assume_bom_written            to_assume_bom_written
 *      to_assume_be_bom_written         to_assume_le_bom_written
 */
struct utf32bom_encoding_state_transition {
    enum {
        to_initial,
        to_bom_written,
        to_be_bom_written,
        to_le_bom_written,
        to_assume_bom_written,
        to_assume_be_bom_written,
        to_assume_le_bom_written
    } state_transition;

    static utf32bom_encoding_state_transition
    to_initial_state() noexcept {
        return { to_initial };
    }

    static utf32bom_encoding_state_transition
    to_bom_written_state() noexcept {
        return { to_bom_written };
    }

    static utf32bom_encoding_state_transition
    to_be_bom_written_state() noexcept {
        return { to_be_bom_written };
    }

    static utf32bom_encoding_state_transition
    to_le_bom_written_state() noexcept {
        return { to_le_bom_written };
    }

    static utf32bom_encoding_state_transition
    to_assume_bom_written_state() noexcept {
        return { to_assume_bom_written };
    }

    static utf32bom_encoding_state_transition
    to_assume_be_bom_written_state() noexcept {
        return { to_assume_be_bom_written };
    }

    static utf32bom_encoding_state_transition
    to_assume_le_bom_written_state() noexcept {
        return { to_assume_le_bom_written };
    }
};

struct utf32bom_encoding_state {
    enum endian_type {
        big_endian = 0,
        little_endian = 1
    };
    bool bom_read_or_written : 1;
    endian_type endian : 1;
};


namespace text_detail {

template<Character CT, CodeUnit CUT>
class utf32bom_codec {
public:
    using state_type = utf32bom_encoding_state;
    using state_transition_type = utf32bom_encoding_state_transition;
    using character_type = CT;
    using code_unit_type = CUT;
    static constexpr int min_code_units = 4;
    static constexpr int max_code_units = 4;

    static_assert(sizeof(code_unit_type) * CHAR_BIT >= 8);

    template<CodeUnitOutputIterator<code_unit_type> CUIT>
    static void encode_state_transition(
        state_type &state,
        CUIT &out,
        const state_transition_type &stt,
        int &encoded_code_units)
    {
        encoded_code_units = 0;

        using unsigned_code_unit_type =
            std::make_unsigned_t<code_unit_type>;

        if (stt.state_transition == state_transition_type::to_initial) {
            // Transition to initial state from any state.
            state.bom_read_or_written = false;
            state.endian = state_type::big_endian;
        } else if (! state.bom_read_or_written) {
            // In initial state.
            switch (stt.state_transition) {
                case state_transition_type::to_initial:
                    // Handled above.
                    break;
                case state_transition_type::to_bom_written:
                case state_transition_type::to_be_bom_written:
                    *out++ = unsigned_code_unit_type(0x00);
                    ++encoded_code_units;
                    *out++ = unsigned_code_unit_type(0x00);
                    ++encoded_code_units;
                    *out++ = unsigned_code_unit_type(0xFE);
                    ++encoded_code_units;
                    *out++ = unsigned_code_unit_type(0xFF);
                    ++encoded_code_units;
                    state.endian = state_type::big_endian;
                    break;
                case state_transition_type::to_le_bom_written:
                    *out++ = unsigned_code_unit_type(0xFF);
                    ++encoded_code_units;
                    *out++ = unsigned_code_unit_type(0xFE);
                    ++encoded_code_units;
                    *out++ = unsigned_code_unit_type(0x00);
                    ++encoded_code_units;
                    *out++ = unsigned_code_unit_type(0x00);
                    ++encoded_code_units;
                    state.endian = state_type::little_endian;
                    break;
                case state_transition_type::to_assume_bom_written:
                case state_transition_type::to_assume_be_bom_written:
                    state.endian = state_type::big_endian;
                    break;
                case state_transition_type::to_assume_le_bom_written:
                    state.endian = state_type::little_endian;
                    break;
            }
            state.bom_read_or_written = true;
        } else if (state.endian == state_type::big_endian) {
            // In BE BOM read or written state.
            if (stt.state_transition ==
                    state_transition_type::to_le_bom_written ||
                stt.state_transition ==
                    state_transition_type::to_assume_le_bom_written)
            {
                throw text_encode_error("Invalid LE state transition");
            } else {
                // to_initial handled above, the rest have no affect.
            }
        } else {
            // In BE BOM read or written state.
            if (stt.state_transition ==
                    state_transition_type::to_be_bom_written ||
                stt.state_transition ==
                    state_transition_type::to_assume_be_bom_written)
            {
                throw text_encode_error("Invalid BE state transition");
            } else {
                // to_initial handled above, the rest have no affect.
            }
        }
    }

    template<CodeUnitOutputIterator<code_unit_type> CUIT>
    static void encode(
        state_type &state,
        CUIT &out,
        character_type c,
        int &encoded_code_units)
    {
        encoded_code_units = 0;

        if (! state.bom_read_or_written) {
            encode_state_transition(
                state, out, state_transition_type::to_bom_written_state(),
                encoded_code_units);
        }

        if (state.endian == state_type::big_endian) {
            using utf32_codec = utf32be_codec<CT, CUT>;
            using utf32_state_type = typename utf32_codec::state_type;
            static_assert(std::is_empty<utf32_state_type>::value);

            utf32_state_type utf32_state;
            int utf32_encoded_code_units = 0;
            try {
                utf32_codec::encode(utf32_state, out, c,
                                    utf32_encoded_code_units);
            } catch(...) {
                encoded_code_units += utf32_encoded_code_units;
                throw;
            }
        } else {
            using utf32_codec = utf32le_codec<CT, CUT>;
            using utf32_state_type = typename utf32_codec::state_type;
            static_assert(std::is_empty<utf32_state_type>::value);

            utf32_state_type utf32_state;
            int utf32_encoded_code_units = 0;
            try {
                utf32_codec::encode(utf32_state, out, c,
                                    utf32_encoded_code_units);
            } catch(...) {
                encoded_code_units += utf32_encoded_code_units;
                throw;
            }
        }
    }

    template<CodeUnitIterator CUIT, typename CUST>
    requires ranges::InputIterator<CUIT>()
          && ranges::ConvertibleTo<ranges::value_type_t<CUIT>, code_unit_type>()
          && ranges::Sentinel<CUST, CUIT>()
    static bool decode(
        state_type &state,
        CUIT &in_next,
        CUST in_end,
        character_type &c,
        int &decoded_code_units)
    {
        decoded_code_units = 0;

        bool return_value;
        if (state.endian == state_type::big_endian) {
            using utf32_codec = utf32be_codec<CT, CUT>;
            using utf32_state_type = typename utf32_codec::state_type;
            static_assert(std::is_empty<utf32_state_type>::value);

            utf32_state_type utf32_state;
            int utf32_decoded_code_units = 0;
            try {
                return_value = utf32_codec::decode(utf32_state, in_next, in_end,
                                                   c, utf32_decoded_code_units);
            } catch(...) {
                decoded_code_units += utf32_decoded_code_units;
                throw;
            }
            decoded_code_units += utf32_decoded_code_units;
        } else {
            using utf32_codec = utf32le_codec<CT, CUT>;
            using utf32_state_type = typename utf32_codec::state_type;
            static_assert(std::is_empty<utf32_state_type>::value);

            utf32_state_type utf32_state;
            int utf32_decoded_code_units = 0;
            try {
                return_value = utf32_codec::decode(utf32_state, in_next, in_end,
                                                   c, utf32_decoded_code_units);
            } catch(...) {
                decoded_code_units += utf32_decoded_code_units;
                throw;
            }
            decoded_code_units += utf32_decoded_code_units;
        }

        assert(return_value);
        if (! state.bom_read_or_written
            && (c.get_code_point() == 0x0000FEFF || c.get_code_point() == 0xFFFE0000))
        {
            // A BOM has been read at the start of input.  Adjust the state
            // and return false to indicate that a code point has not been
            // decoded.
            if (state.endian == state_type::big_endian
                && c.get_code_point() == 0xFFFE0000)
            {
                state.endian = state_type::little_endian;
            }
            else if (state.endian == state_type::little_endian
                && c.get_code_point() == 0xFFFE0000)
            {
                state.endian = state_type::big_endian;
            }
            return_value = false;
        }
        state.bom_read_or_written = true;

        return return_value;
    }

    template<CodeUnitIterator CUIT, typename CUST>
    requires ranges::InputIterator<CUIT>()
          && ranges::ConvertibleTo<ranges::value_type_t<CUIT>, code_unit_type>()
          && ranges::Sentinel<CUST, CUIT>()
    static bool rdecode(
        state_type &state,
        CUIT &in_next,
        CUST in_end,
        character_type &c,
        int &decoded_code_units)
    {
        decoded_code_units = 0;

        bool return_value;
        if (state.endian == state_type::big_endian) {
            using utf32_codec = utf32be_codec<CT, CUT>;
            using utf32_state_type = typename utf32_codec::state_type;
            static_assert(std::is_empty<utf32_state_type>::value);

            utf32_state_type utf32_state;
            int utf32_decoded_code_units = 0;
            try {
                return_value = utf32_codec::rdecode(utf32_state, in_next, in_end,
                                                    c, utf32_decoded_code_units);
            } catch(...) {
                decoded_code_units += utf32_decoded_code_units;
                throw;
            }
            decoded_code_units += utf32_decoded_code_units;
        } else {
            using utf32_codec = utf32le_codec<CT, CUT>;
            using utf32_state_type = typename utf32_codec::state_type;
            static_assert(std::is_empty<utf32_state_type>::value);

            utf32_state_type utf32_state;
            int utf32_decoded_code_units = 0;
            try {
                return_value = utf32_codec::rdecode(utf32_state, in_next, in_end,
                                                    c, utf32_decoded_code_units);
            } catch(...) {
                decoded_code_units += utf32_decoded_code_units;
                throw;
            }
            decoded_code_units += utf32_decoded_code_units;
        }

        assert(return_value);
        if (in_next == in_end
            && c.get_code_point() == 0x0000FEFF)
        {
            // A BOM has been read at the start of input.  Return false to
            // indicate that a code point has not been decoded.
            return_value = false;
        }

        return return_value;
    }
};

} // namespace text_detail


} // inline namespace text
} // namespace experimental
} // namespace std


#endif // } TEXT_VIEW_CODECS_UTF32BOM_CODEC_HPP
