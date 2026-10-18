#!/usr/bin/env python3
"""
tests/clone.sh が使う、https から http へ転送するだけの小さなサーバー。

  python3 clone-redirect-server.py <証明書> <鍵> <ポートを書くファイル> <記録のファイル>

https と http のポートを 1 つずつ空いているところに開き、ポートを書くファイルに
`https=番号 http=番号` の 1 行で書く (書き終えたら準備ができた印)。

- https に来た要求は、同じホストの http のポートの同じパスへ 302 で転送する
- http に来た要求は、Authorization と Cookie を記録のファイルに 1 行ずつ足して 200 を返す

クローン元が https から http に落とすとき、ログインの情報を送らずに止まるかを
確かめるためのもの。127.0.0.1 だけで待ち受ける。止めるときはプロセスを kill する。
"""
import http.server
import os
import ssl
import sys
import threading

cert, key, port_file, log_file = sys.argv[1:5]
lock = threading.Lock()


class Redirect(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        host = self.headers.get('Host', 'localhost').rsplit(':', 1)[0]
        self.send_response(302)
        self.send_header('Location', 'http://%s:%d%s' % (host, plain.server_port, self.path))
        self.send_header('Content-Length', '0')
        self.end_headers()

    def log_message(self, *args):
        pass


class Record(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        with lock, open(log_file, 'a') as f:
            f.write('path=%s authorization=%s cookie=%s\n' % (
                self.path,
                'yes' if self.headers.get('Authorization') else 'no',
                self.headers.get('Cookie', '')))
        body = b'ok'
        self.send_response(200)
        self.send_header('Content-Type', 'text/plain')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


secure = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Redirect)
context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
context.load_cert_chain(cert, key)
secure.socket = context.wrap_socket(secure.socket, server_side=True)
plain = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Record)

threading.Thread(target=plain.serve_forever, daemon=True).start()
with open(port_file + '.tmp', 'w') as f:
    f.write('https=%d http=%d\n' % (secure.server_port, plain.server_port))
os.replace(port_file + '.tmp', port_file)
secure.serve_forever()
