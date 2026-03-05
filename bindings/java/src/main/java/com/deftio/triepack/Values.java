/*
 * Values.java
 *
 * Value encoder/decoder for 8 type tags -- matches src/core/value.c.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

package com.deftio.triepack;

import java.nio.charset.StandardCharsets;

/**
 * Encode and decode typed values to/from a bitstream.
 */
public class Values {

    // Type tags (4-bit, matches tp_value_type enum)
    static final int TP_NULL = 0;
    static final int TP_BOOL = 1;
    static final int TP_INT = 2;
    static final int TP_UINT = 3;
    static final int TP_FLOAT32 = 4;
    static final int TP_FLOAT64 = 5;
    static final int TP_STRING = 6;
    static final int TP_BLOB = 7;

    /**
     * Encode a TpValue into the bitstream.
     */
    public static void encodeValue(BitWriter writer, TpValue val) {
        if (val == null || val.getType() == TpValue.Type.NULL) {
            writer.writeBits(TP_NULL, 4);
            return;
        }

        switch (val.getType()) {
            case BOOL:
                writer.writeBits(TP_BOOL, 4);
                writer.writeBit(val.boolValue() ? 1 : 0);
                break;

            case INT:
                writer.writeBits(TP_INT, 4);
                VarInt.writeVarInt(writer, val.intValue());
                break;

            case UINT:
                writer.writeBits(TP_UINT, 4);
                VarInt.writeVarUint(writer, val.uintValue());
                break;

            case FLOAT64: {
                writer.writeBits(TP_FLOAT64, 4);
                long bits = Double.doubleToRawLongBits(val.float64Value());
                long hi = (bits >>> 32) & 0xFFFFFFFFL;
                long lo = bits & 0xFFFFFFFFL;
                writer.writeU64Parts(hi, lo);
                break;
            }

            case STRING: {
                byte[] encoded = val.stringValue().getBytes(StandardCharsets.UTF_8);
                writer.writeBits(TP_STRING, 4);
                VarInt.writeVarUint(writer, encoded.length);
                writer.alignToByte();
                writer.writeBytes(encoded);
                break;
            }

            case BLOB: {
                byte[] blob = val.blobValue();
                writer.writeBits(TP_BLOB, 4);
                VarInt.writeVarUint(writer, blob.length);
                writer.alignToByte();
                writer.writeBytes(blob);
                break;
            }

            default:
                throw new IllegalArgumentException("Unsupported value type: " + val.getType());
        }
    }

    /**
     * Decode a TpValue from the bitstream.
     */
    public static TpValue decodeValue(BitReader reader) {
        int tag = (int) reader.readBits(4);

        switch (tag) {
            case TP_NULL:
                return TpValue.ofNull();

            case TP_BOOL:
                return TpValue.ofBool(reader.readBit() != 0);

            case TP_INT:
                return TpValue.ofInt(VarInt.readVarInt(reader));

            case TP_UINT:
                return TpValue.ofUInt(VarInt.readVarUint(reader));

            case TP_FLOAT32: {
                long bits = reader.readU32();
                float f = Float.intBitsToFloat((int) bits);
                return TpValue.ofFloat64(f);
            }

            case TP_FLOAT64: {
                long hi = reader.readU32();
                long lo = reader.readU32();
                long bits = (hi << 32) | lo;
                return TpValue.ofFloat64(Double.longBitsToDouble(bits));
            }

            case TP_STRING: {
                int slen = (int) VarInt.readVarUint(reader);
                reader.alignToByte();
                byte[] raw = reader.readBytes(slen);
                return TpValue.ofString(new String(raw, StandardCharsets.UTF_8));
            }

            case TP_BLOB: {
                int blen = (int) VarInt.readVarUint(reader);
                reader.alignToByte();
                byte[] raw = reader.readBytes(blen);
                return TpValue.ofBlob(raw);
            }

            default:
                throw new IllegalArgumentException("Unknown value type tag: " + tag);
        }
    }
}
