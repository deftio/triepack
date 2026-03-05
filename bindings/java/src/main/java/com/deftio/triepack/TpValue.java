/*
 * TpValue.java
 *
 * Value type wrapper for TriePack typed values.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

package com.deftio.triepack;

import java.util.Arrays;
import java.util.Objects;

/**
 * Immutable wrapper for TriePack typed values.
 */
public class TpValue {

    public enum Type {
        NULL, BOOL, INT, UINT, FLOAT64, STRING, BLOB
    }

    private final Type type;
    private final boolean boolVal;
    private final long longVal;
    private final double doubleVal;
    private final String stringVal;
    private final byte[] blobVal;

    private TpValue(Type type, boolean boolVal, long longVal, double doubleVal,
                    String stringVal, byte[] blobVal) {
        this.type = type;
        this.boolVal = boolVal;
        this.longVal = longVal;
        this.doubleVal = doubleVal;
        this.stringVal = stringVal;
        this.blobVal = blobVal;
    }

    public static TpValue ofNull() {
        return new TpValue(Type.NULL, false, 0, 0.0, null, null);
    }

    public static TpValue ofBool(boolean v) {
        return new TpValue(Type.BOOL, v, 0, 0.0, null, null);
    }

    public static TpValue ofInt(long v) {
        return new TpValue(Type.INT, false, v, 0.0, null, null);
    }

    public static TpValue ofUInt(long v) {
        return new TpValue(Type.UINT, false, v, 0.0, null, null);
    }

    public static TpValue ofFloat64(double v) {
        return new TpValue(Type.FLOAT64, false, 0, v, null, null);
    }

    public static TpValue ofString(String v) {
        return new TpValue(Type.STRING, false, 0, 0.0, v, null);
    }

    public static TpValue ofBlob(byte[] v) {
        return new TpValue(Type.BLOB, false, 0, 0.0, null,
                           v != null ? Arrays.copyOf(v, v.length) : null);
    }

    public Type getType() {
        return type;
    }

    public boolean boolValue() {
        return boolVal;
    }

    public long intValue() {
        return longVal;
    }

    public long uintValue() {
        return longVal;
    }

    public double float64Value() {
        return doubleVal;
    }

    public String stringValue() {
        return stringVal;
    }

    public byte[] blobValue() {
        return blobVal != null ? Arrays.copyOf(blobVal, blobVal.length) : null;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof TpValue)) return false;
        TpValue other = (TpValue) o;
        if (type != other.type) return false;
        switch (type) {
            case NULL:
                return true;
            case BOOL:
                return boolVal == other.boolVal;
            case INT:
            case UINT:
                return longVal == other.longVal;
            case FLOAT64:
                return Double.compare(doubleVal, other.doubleVal) == 0;
            case STRING:
                return Objects.equals(stringVal, other.stringVal);
            case BLOB:
                return Arrays.equals(blobVal, other.blobVal);
            default:
                return false;
        }
    }

    @Override
    public int hashCode() {
        switch (type) {
            case NULL:
                return 0;
            case BOOL:
                return Boolean.hashCode(boolVal);
            case INT:
            case UINT:
                return Long.hashCode(longVal);
            case FLOAT64:
                return Double.hashCode(doubleVal);
            case STRING:
                return stringVal != null ? stringVal.hashCode() : 0;
            case BLOB:
                return Arrays.hashCode(blobVal);
            default:
                return 0;
        }
    }

    @Override
    public String toString() {
        switch (type) {
            case NULL:
                return "TpValue(null)";
            case BOOL:
                return "TpValue(bool=" + boolVal + ")";
            case INT:
                return "TpValue(int=" + longVal + ")";
            case UINT:
                return "TpValue(uint=" + longVal + ")";
            case FLOAT64:
                return "TpValue(f64=" + doubleVal + ")";
            case STRING:
                return "TpValue(str=\"" + stringVal + "\")";
            case BLOB:
                return "TpValue(blob[" + (blobVal != null ? blobVal.length : 0) + "])";
            default:
                return "TpValue(?)";
        }
    }
}
