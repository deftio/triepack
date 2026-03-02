'use strict';

/**
 * Arbitrary-width bit I/O — MSB-first, matching src/bitstream/.
 */

class BitWriter {
    constructor(initialCap) {
        this._cap = initialCap || 256;
        this._buf = new Uint8Array(this._cap);
        this._pos = 0; // bit position
    }

    get position() {
        return this._pos;
    }

    _ensure(nBits) {
        const needed = ((this._pos + nBits + 7) >>> 3);
        if (needed <= this._cap) return;
        let newCap = this._cap * 2;
        while (newCap < needed) newCap *= 2;
        const newBuf = new Uint8Array(newCap);
        newBuf.set(this._buf);
        this._buf = newBuf;
        this._cap = newCap;
    }

    writeBits(value, n) {
        if (n === 0 || n > 32) throw new RangeError('writeBits: n must be 1..32');
        this._ensure(n);
        for (let i = 0; i < n; i++) {
            const bit = (value >>> (n - 1 - i)) & 1;
            const byteIdx = this._pos >>> 3;
            const bitIdx = 7 - (this._pos & 7);
            if (bit) {
                this._buf[byteIdx] |= (1 << bitIdx);
            } else {
                this._buf[byteIdx] &= ~(1 << bitIdx);
            }
            this._pos++;
        }
    }

    writeBit(val) {
        this._ensure(1);
        const byteIdx = this._pos >>> 3;
        const bitIdx = 7 - (this._pos & 7);
        if (val) {
            this._buf[byteIdx] |= (1 << bitIdx);
        } else {
            this._buf[byteIdx] &= ~(1 << bitIdx);
        }
        this._pos++;
    }

    writeU8(val) {
        this.writeBits(val & 0xFF, 8);
    }

    writeU16(val) {
        this.writeBits((val >>> 0) & 0xFFFF, 16);
    }

    writeU32(val) {
        this.writeBits(val >>> 0, 32);
    }

    writeU64BE(hi32, lo32) {
        this.writeBits(hi32 >>> 0, 32);
        this.writeBits(lo32 >>> 0, 32);
    }

    writeBytes(bytes) {
        for (let i = 0; i < bytes.length; i++) {
            this.writeU8(bytes[i]);
        }
    }

    alignToByte() {
        const rem = this._pos & 7;
        if (rem !== 0) {
            const pad = 8 - rem;
            this._ensure(pad);
            this._pos += pad;
        }
    }

    toUint8Array() {
        const len = (this._pos + 7) >>> 3;
        return this._buf.slice(0, len);
    }

    getBuffer() {
        return this._buf;
    }
}

class BitReader {
    constructor(buf) {
        this._buf = buf;
        this._bitLen = buf.length * 8;
        this._pos = 0;
    }

    get position() {
        return this._pos;
    }

    get remaining() {
        return this._bitLen - this._pos;
    }

    seek(bitPos) {
        this._pos = bitPos;
    }

    advance(nBits) {
        this._pos += nBits;
    }

    readBits(n) {
        if (n === 0 || n > 32) throw new RangeError('readBits: n must be 1..32');
        if (this._pos + n > this._bitLen) throw new Error('EOF');
        let val = 0;
        for (let i = 0; i < n; i++) {
            const byteIdx = this._pos >>> 3;
            const bitIdx = 7 - (this._pos & 7);
            val = (val << 1) | ((this._buf[byteIdx] >>> bitIdx) & 1);
            this._pos++;
        }
        return val >>> 0;
    }

    readBit() {
        if (this._pos >= this._bitLen) throw new Error('EOF');
        const byteIdx = this._pos >>> 3;
        const bitIdx = 7 - (this._pos & 7);
        const bit = (this._buf[byteIdx] >>> bitIdx) & 1;
        this._pos++;
        return bit;
    }

    readU8() {
        return this.readBits(8);
    }

    readU16() {
        return this.readBits(16);
    }

    readU32() {
        return this.readBits(32);
    }

    readU64BE() {
        const hi = this.readU32();
        const lo = this.readU32();
        return hi * 0x100000000 + lo;
    }

    readBytes(n) {
        const out = new Uint8Array(n);
        for (let i = 0; i < n; i++) {
            out[i] = this.readU8();
        }
        return out;
    }

    peekBits(n) {
        const saved = this._pos;
        const val = this.readBits(n);
        this._pos = saved;
        return val;
    }

    alignToByte() {
        const rem = this._pos & 7;
        if (rem !== 0) {
            this._pos += 8 - rem;
        }
    }

    isAligned() {
        return (this._pos & 7) === 0;
    }
}

module.exports = { BitWriter, BitReader };
