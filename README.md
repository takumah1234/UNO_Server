# UNO_Server

C言語を用いて、サーバーサイドを実装した。
[こちら](https://github.com/takumah1234/uno_client/tree/master)のアプリケーションがClientになっている。

## 実行方法

1. UNO_Serverを実行するPCのIPアドレスを調べる。（DNSサーバの実装まではしていない）
2. UNO_Serverを実行する。
3. [uno_client](https://github.com/takumah1234/uno_client/tree/master)にIPアドレスを記入してビルドおよび実行する。

## 注意事項

ネットワーク障害時のプログラムがないため、一度ネットワーク障害が起きると復旧できないことがある。
その場合は、もう一度実行してください。
何度か実行する内に上手く実行できることがあります。
