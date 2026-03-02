// Copyright (c) 2026 M. A. Chatterjee, BSD-2-Clause.

/**
 * triepack — Native JavaScript implementation of the Triepack .trp binary format.
 */

'use strict';

const { encode } = require('./encoder');
const { decode } = require('./decoder');

module.exports = { encode, decode };
