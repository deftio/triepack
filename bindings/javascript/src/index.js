// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

/**
 * triepack — Native JavaScript implementation of the Triepack .trp binary format.
 */

'use strict';

const { encode, MAX_ALPHABET_SIZE } = require('./encoder');
const { decode } = require('./decoder');

module.exports = { encode, decode, MAX_ALPHABET_SIZE };
