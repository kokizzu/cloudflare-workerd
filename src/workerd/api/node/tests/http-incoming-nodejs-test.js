import http from 'node:http';
import { deepStrictEqual, strictEqual } from 'node:assert';

// Test is taken from test/parallel/test-http-incoming-matchKnownFields.js
export const testHttpIncomingMatchKnownFields = {
  async test() {
    function checkDest(field, result, value) {
      const dest = {};

      const incomingMessage = new http.IncomingMessage(field);
      // Dest is changed by IncomingMessage._addHeaderLine
      if (value) incomingMessage._addHeaderLine(field, 'test', dest);
      incomingMessage._addHeaderLine(field, value, dest);
      deepStrictEqual(dest, result);
    }

    checkDest('', { '': undefined });
    checkDest('Content-Type', { 'content-type': undefined });
    checkDest('content-type', { 'content-type': 'test' }, 'value');
    checkDest('User-Agent', { 'user-agent': undefined });
    checkDest('user-agent', { 'user-agent': 'test' }, 'value');
    checkDest('Referer', { referer: undefined });
    checkDest('referer', { referer: 'test' }, 'value');
    checkDest('Host', { host: undefined });
    checkDest('host', { host: 'test' }, 'value');
    checkDest('Authorization', { authorization: undefined }, undefined);
    checkDest('authorization', { authorization: 'test' }, 'value');
    checkDest('Proxy-Authorization', { 'proxy-authorization': undefined });
    checkDest(
      'proxy-authorization',
      { 'proxy-authorization': 'test' },
      'value'
    );
    checkDest('If-Modified-Since', { 'if-modified-since': undefined });
    checkDest('if-modified-since', { 'if-modified-since': 'test' }, 'value');
    checkDest('If-Unmodified-Since', { 'if-unmodified-since': undefined });
    checkDest(
      'if-unmodified-since',
      { 'if-unmodified-since': 'test' },
      'value'
    );
    checkDest('Form', { form: undefined });
    checkDest('form', { form: 'test, value' }, 'value');
    checkDest('Location', { location: undefined });
    checkDest('location', { location: 'test' }, 'value');
    checkDest('Max-Forwards', { 'max-forwards': undefined });
    checkDest('max-forwards', { 'max-forwards': 'test' }, 'value');
    checkDest('Retry-After', { 'retry-after': undefined });
    checkDest('retry-after', { 'retry-after': 'test' }, 'value');
    checkDest('Etag', { etag: undefined });
    checkDest('etag', { etag: 'test' }, 'value');
    checkDest('Last-Modified', { 'last-modified': undefined });
    checkDest('last-modified', { 'last-modified': 'test' }, 'value');
    checkDest('Server', { server: undefined });
    checkDest('server', { server: 'test' }, 'value');
    checkDest('Age', { age: undefined });
    checkDest('age', { age: 'test' }, 'value');
    checkDest('Expires', { expires: undefined });
    checkDest('expires', { expires: 'test' }, 'value');
    checkDest('Set-Cookie', { 'set-cookie': [undefined] });
    checkDest('set-cookie', { 'set-cookie': ['test', 'value'] }, 'value');
    checkDest('Transfer-Encoding', { 'transfer-encoding': undefined });
    checkDest(
      'transfer-encoding',
      { 'transfer-encoding': 'test, value' },
      'value'
    );
    checkDest('Date', { date: undefined });
    checkDest('date', { date: 'test, value' }, 'value');
    checkDest('Connection', { connection: undefined });
    checkDest('connection', { connection: 'test, value' }, 'value');
    checkDest('Cache-Control', { 'cache-control': undefined });
    checkDest('cache-control', { 'cache-control': 'test, value' }, 'value');
    checkDest('Transfer-Encoding', { 'transfer-encoding': undefined });
    checkDest(
      'transfer-encoding',
      { 'transfer-encoding': 'test, value' },
      'value'
    );
    checkDest('Vary', { vary: undefined });
    checkDest('vary', { vary: 'test, value' }, 'value');
    checkDest('Content-Encoding', { 'content-encoding': undefined }, undefined);
    checkDest(
      'content-encoding',
      { 'content-encoding': 'test, value' },
      'value'
    );
    checkDest('Cookie', { cookie: undefined });
    checkDest('cookie', { cookie: 'test; value' }, 'value');
    checkDest('Origin', { origin: undefined });
    checkDest('origin', { origin: 'test, value' }, 'value');
    checkDest('Upgrade', { upgrade: undefined });
    checkDest('upgrade', { upgrade: 'test, value' }, 'value');
    checkDest('Expect', { expect: undefined });
    checkDest('expect', { expect: 'test, value' }, 'value');
    checkDest('If-Match', { 'if-match': undefined });
    checkDest('if-match', { 'if-match': 'test, value' }, 'value');
    checkDest('If-None-Match', { 'if-none-match': undefined });
    checkDest('if-none-match', { 'if-none-match': 'test, value' }, 'value');
    checkDest('Accept', { accept: undefined });
    checkDest('accept', { accept: 'test, value' }, 'value');
    checkDest('Accept-Encoding', { 'accept-encoding': undefined });
    checkDest('accept-encoding', { 'accept-encoding': 'test, value' }, 'value');
    checkDest('Accept-Language', { 'accept-language': undefined });
    checkDest('accept-language', { 'accept-language': 'test, value' }, 'value');
    checkDest('X-Forwarded-For', { 'x-forwarded-for': undefined });
    checkDest('x-forwarded-for', { 'x-forwarded-for': 'test, value' }, 'value');
    checkDest('X-Forwarded-Host', { 'x-forwarded-host': undefined });
    checkDest(
      'x-forwarded-host',
      { 'x-forwarded-host': 'test, value' },
      'value'
    );
    checkDest('X-Forwarded-Proto', { 'x-forwarded-proto': undefined });
    checkDest(
      'x-forwarded-proto',
      { 'x-forwarded-proto': 'test, value' },
      'value'
    );
    checkDest('X-Foo', { 'x-foo': undefined });
    checkDest('x-foo', { 'x-foo': 'test, value' }, 'value');
  },
};

// Test is taken from test/parallel/test-http-incoming-message-connection-setter.js
export const testHttpIncomingMessageConnectionSetter = {
  async test() {
    const incomingMessage = new http.IncomingMessage();

    strictEqual(incomingMessage.connection, undefined);
    strictEqual(incomingMessage.socket, undefined);

    incomingMessage.connection = 'fhqwhgads';

    strictEqual(incomingMessage.connection, 'fhqwhgads');
    strictEqual(incomingMessage.socket, 'fhqwhgads');
  },
};

// Test is taken from test/parallel/test-http-incoming-message-destroy.js
export const testHttpIncomingMessageOptions = {
  async test() {
    const incomingMessage = new http.IncomingMessage();

    strictEqual(incomingMessage.destroy(), incomingMessage);
  },
};

// Node.js tests
// - [x] test/parallel/test-http-incoming-matchKnownFields.js
// - [x] test/parallel/test-http-incoming-message-connection-setter.js
// - [x] test/parallel/test-http-incoming-message-destroy.js
// - [x] test/parallel/test-http-incoming-message-options.js
// - [ ] test/parallel/test-http-incoming-pipelined-socket-destroy.js
// - [ ] test/parallel/test-http-incoming-message-options.js
// - [ ] test/parallel/test-http-incoming-pipelined-socket-destroy.js
