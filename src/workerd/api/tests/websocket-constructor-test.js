// Copyright (c) 2017-2022 Cloudflare, Inc.
// Licensed under the Apache 2.0 license found in the LICENSE file or at:
//     https://opensource.org/licenses/Apache-2.0

import { doesNotThrow, strictEqual, throws } from 'node:assert';

// Test that WebSocket constructor handles empty protocols array correctly.
// Per the WebSocket spec, an empty protocols array should be valid and equivalent
// to not specifying any protocols.
// See: https://github.com/cloudflare/workerd/issues/5822
export const emptyProtocolsArray = {
  async test() {
    const ws = new WebSocket('wss://example.com/', []);
    strictEqual(ws.url, 'wss://example.com/');
    strictEqual(ws.protocol, '');
    strictEqual(ws.readyState, WebSocket.CONNECTING);
    ws.close();
  },
};

// Test that a single protocol string works
export const singleProtocolString = {
  async test() {
    const ws = new WebSocket('wss://example.com/', 'chat');
    strictEqual(ws.url, 'wss://example.com/');
    ws.close();
  },
};

// Test that invalid protocol tokens still throw SyntaxError
export const invalidProtocolToken = {
  async test() {
    throws(
      () => new WebSocket('wss://example.com/', 'invalid protocol with spaces'),
      {
        name: 'SyntaxError',
      }
    );
  },
};

// Test that duplicate valid protocols throw SyntaxError
export const duplicateProtocols = {
  async test() {
    throws(() => new WebSocket('wss://example.com/', ['chat', 'chat']), {
      name: 'SyntaxError',
    });
  },
};

// Test that close() throws SyntaxError when reason exceeds 123 bytes (UTF-8).
// Per WHATWG WebSocket spec and RFC 6455 Section 5.5, the close frame body
// must not exceed 125 bytes (2-byte code + up to 123 bytes of reason).
export const closeReasonTooLong = {
  async test() {
    const ws = new WebSocket('wss://example.com/');
    // 124 ASCII bytes => 124 UTF-8 bytes, which exceeds the 123-byte limit.
    const longReason = 'a'.repeat(124);
    throws(() => ws.close(1000, longReason), {
      name: 'SyntaxError',
    });
    ws.close();
  },
};

// Test that close() accepts a reason of exactly 123 bytes.
export const closeReasonExact123Bytes = {
  async test() {
    const ws = new WebSocket('wss://example.com/');
    const reason123 = 'a'.repeat(123);
    doesNotThrow(() => ws.close(1000, reason123));
  },
};

// Test that close() counts UTF-8 bytes, not characters.
// U+00E9 (é) is 2 bytes in UTF-8. 62 of them = 124 bytes > 123 limit.
export const closeReasonMultibyteExceeds = {
  async test() {
    const ws = new WebSocket('wss://example.com/');
    const multibyteReason = '\u00e9'.repeat(62); // 62 chars × 2 bytes = 124 bytes
    throws(() => ws.close(1000, multibyteReason), {
      name: 'SyntaxError',
    });
    ws.close();
  },
};

// Test that close() replaces lone surrogates with U+FFFD per the USVString spec.
// This is tested end-to-end by the WPT Close-reason-unpaired-surrogates.any.js test.
// Here we verify the CloseEvent constructor also applies USVString conversion.
export const closeReasonUSVString = {
  async test() {
    // CloseEvent.reason is typed as USVString per the HTML spec,
    // so lone surrogates must be replaced with U+FFFD.
    const evt = new CloseEvent('close', { code: 1000, reason: '\uD807' });
    strictEqual(
      evt.reason,
      '\uFFFD',
      'CloseEvent reason should replace lone surrogate with U+FFFD'
    );

    // Multiple surrogates in a string.
    const evt2 = new CloseEvent('close', {
      code: 1000,
      reason: 'hello\uD800world\uDC00end',
    });
    strictEqual(
      evt2.reason,
      'hello\uFFFDworld\uFFFDend',
      'CloseEvent reason should replace each lone surrogate with U+FFFD'
    );

    // Properly paired surrogates should pass through unchanged.
    const evt3 = new CloseEvent('close', {
      code: 1000,
      reason: '\uD83D\uDE00',
    });
    strictEqual(
      evt3.reason,
      '\uD83D\uDE00',
      'Properly paired surrogates should not be replaced'
    );
  },
};
