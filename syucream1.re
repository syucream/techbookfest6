= Rust で ffi で FUSE ライブラリを自作する

== はじめに

Rust は C++ より安全にシステムプログラミングが書きやすいモダンな言語です。
今回は Rust を使って、 libfuse のラッパーライブラリを作ってみます。

== 既存の Rust FUSE ライブラリについて

libfuse にはハイレベル API とローレベル API が存在します。
また Rust においては https://github.com/zargony/rust-fuse が存在します。
しかしながら rust-fuse はローレベル API だけをターゲットにしており、より抽象化されているハイレベル API を Rust から使用する手段は今の所ありません。
（rust-fuse としては、 C 実装のハイレベル API を直接呼び出すよりはローレベル API を利用するハイレベル API を Rust で実装するほうが Rust の良さを活かせるため、やるならそうすべきとの構想があるようです。）

== 今回自作する Rust FUSE ライブラリ

一方で今回は libfuse で持っているハイレベル API の C 実装を素直に Rust から呼び出すことに注力します。
ウェブでよく散見される libfuse を用いたファイルシステムの実装はハイレベル API を前提にしているものが多く、 libfuse 公式のサンプルコードもハイレベル API を利用していることから、ハイレベル API を Rust 向けに提供することに一定の価値があるものと思われます。

今回の Rust FUSE ライブラリは rust-fuse と親しい機能を持ちながらハイレベル API に対する着想が異なることから、 yarf(Yet Another Rust Fuse) と名付けます。

== yarf の実装

yarf の実装は以下にあります。

https://github.com/syucream/yarf

src/ 以下にハイレベル API のバインディング実装が存在します。また、 examples/ 以下にサンプル実装をおいています。


