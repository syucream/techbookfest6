= Rust で ffi で FUSE ライブラリを自作する


== はじめに

Rust は FireFox などの開発元としてよく知られる Mozilla が開発しているモダンな言語です。
モダンな言語だけあって、最近台頭してきた言語によくあるような、パターンマッチや null 安全な型 Option 、エラーが起こるかもしれないことを表現する型 Result  などの機能が存在します。
また Rust の特徴的な機能として、デフォルトムーブな変数所有権の扱いや借用、コンパイル時のレースコンディションのチェック、変数のライフタイムの管理、シンプルな ffi による C, C++ 関数の呼び出し、unsafe ブロックによる柔軟で明示的な危険な操作(ポインタのデリファレンスや C, C++関数呼び出しなど)も存在します。
これらの機能により、 C, C++ などより安全にシステムプログラミングが可能であり、かつ既存の C, C++ の資産を利用可能なプログラミング言語であると言えます。

本記事では Rust でffi するコードを書いてみます。
ここでは FUSE(Filesystem in UserSpace) のライブラリである libfuse のハイレベル API のラッパーライブラリを作ってみます。


== FUSE(Filesystem in UserSpace)  について

FUSE はユーザスペースで安全かつ気軽に独自のファイルシステムを動作させるための仕組みです。

独自にファイルシステムを作る場合、従来はカーネルレベルのプログラミングが必要で、実装やデバッグが大変だったり導入障壁が高かったりなどの問題がありました。
FUSE ではユーザスペースで動作するプログラムを記述することになるので、カーネルプログラミングを意識せずデバッグの容易性も上がります。

Linux における FUSE の構成としては、 FUSE のカーネルモジュールとそれをユーザスペースから使いやすくするライブラリ @<fn>{libfuse} の 2 コンポーネントから構成されます。
FUSE を使ったアプリケーションの多くはこの libfuse のインタフェースを利用してファイルシステムを構築したものになります。
また Linux の他にも macOS 向けに libfuse を使えるようにした FUSE for macOS(osxfuse) @<fn>{osxfuse} も存在して、 macOS ユーザも気軽に FUSE アプリケーションを開発・利用できます。

libfuse にはハイレベル API とローレベル API が存在します。
どちらの API を利用する場合でも、ファイルシステムへの操作に対する各コールバック関数を実装していくことになるのですが、そのインタフェースにハイレベル API とローレベル API では差異が存在します。
ハイレベル API ではコールバック関数にアクセス対象ファイルパスが渡されるのですが、ローレベル API ではファイルパスでなく inode を扱うことになります。
またハイレベル API ではコールバック関数をリターンする際にその処理を終えるモデルになるのですが、ローレベル API ではそうではなく、明示的に処理を終えてレスポンスを返すモデルになります。
ちなみに Web に存在する FUSE アプリケーションの参考例は、実装の容易性などからハイレベル API を用いたものの方が多く見つかることでしょう。

//footnote[libfuse][libfuse: https://github.com/libfuse/libfuse]
//footnote[osxfuse][FUSE for macOS: https://osxfuse.github.io/]


== C++ ではじめる FUSE

さて、いきなり Rust で libfuse を ffi を使って利用するコードを実装し始めてもいいのですが、 ffi を使う上で C, C++ として利用する構造体や関数などがどのような構成になっているかを知ることは重要になります。
ここでは C++ で "Hello, World!" を表示するだけのシンプルな FUSE アプリケーションを作ってみましょう。
（C ではなく  C++ を用いる強い理由はありません。単純に C のサンプルは既に数多く Web 上にあるため参考として C++ で実装してみた次第です）

=== Hello, FUSE! in C++

C++ で実装した、最低限 "Hello, World!" をコンソール上で表示でき、ファイルシステムとして動作しているように見えるコードが以下のようになります。

//source[hello.cpp]{
#include <cerrno>
#include <fuse.h>
#include <string>

using std::string;

constexpr auto STAT_SIZE = sizeof(struct stat);
const auto HELLO_CONTENT = string("Hello, World!");
const auto HELLO_CONTENT_LEN = HELLO_CONTENT.length();
const auto HELLO_PATH = string("/hello");

static int hello_getattr(const char *path, struct stat *stbuf) {
  const auto path_str = string(path);

  memset(stbuf, 0, STAT_SIZE);

  if (path_str == "/") {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
    return 0;
  } else if (path_str == HELLO_PATH) {
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = HELLO_CONTENT_LEN;
    return 0;
  } else {
    return -ENOENT;
  }
}

static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi) {
  const auto path_str = string(path);

  if (path_str != "/") {
    return -ENOENT;
  }

  filler(buf, ".", nullptr, 0);
  filler(buf, "..", nullptr, 0);
  filler(buf, HELLO_PATH.c_str() + 1, nullptr, 0);

  return 0;
}

static int hello_open(const char *path, struct fuse_file_info *fi) {
  const auto path_str = string(path);

  if (path_str != HELLO_PATH) {
    return -ENOENT;
  }

  return 0;
}

static int hello_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
  const auto path_str = string(path);

  if (path_str != HELLO_PATH) {
    return -ENOENT;
  }

  if (offset < HELLO_CONTENT_LEN) {
    if (offset + size > HELLO_CONTENT_LEN) {
      size = HELLO_CONTENT_LEN - offset;
    }
    std::memcpy(buf, HELLO_CONTENT.c_str() + offset, size);
  } else {
    size = 0;
  }

  return size;
}

static struct fuse_operations hello_oper = {.getattr = hello_getattr,
                                            .readdir = hello_readdir,
                                            .open = hello_open,
                                            .read = hello_read};

int main(int argc, char *argv[]) {
  fuse_main(argc, argv, &hello_oper, nullptr);
}
//}

実装した FUSE のコールバック関数としては getattr, readdir, open, read の 4 種類だけです。これ以外のコールバック関数の実装が必要な操作、例えば書き込みを行うと未実装である旨のエラーが返却されます。

=== FUSE アプリケーションの動作確認方法

このサンプルをコンパイルする時には例えば以下のようにします。
このサンプルでは constexpr や auto など C++11 以降の機能を使っているため、 --std=c++0x のように C++11 以降を使うことを明示します。
また FUSE のバージョンは今回は筆者の環境では 29 を指定しました。
FUSE の共有ライブラリとヘッダのパスですが、 pkg-config を介して指定します。
pkg-config を用いることは必須では無いのですが、特に macOS を用いる場合は osxfuse のパスを指定する必要があるなど手動指定だとハマりどころが多いため、 pkg-config を用いてしまうのがシンプルで済むと思われます。
余談ですが libfuse と osxfuse はシグネチャとしてはほぼ同じになり、内容によっては osxfuse にリンクする想定のはずが libfuse にリンクしてしまえる場合があります。
筆者はそれに気づかず多少の時間を無駄に費やしました。。。

//cmd{
$ g++ -Wall hello.cpp --std=c++0x -DFUSE_USE_VERSION=29 `pkg-config fuse --cflags --libs` -o hello
//}

次にこの hello ファイルシステムを実際にマウントしてみます。
ここでは適当に tmp ディレクトリを作成し、それにマウントしてみました。
ここでデバッグを容易にするため -d オプションも付与しています。
-d を付与することで、ファイルシステムへ発行されているオペレーションとそれに対する結果を出力してもらえます。

//cmd{
$ mkdir tmp
$ ./hello -d tmp
//}

最後に想定されるファイルパス "/hello" に cat コマンドでアクセスしてみます。
上手く行っていれば "Hello, World!" と表示されると思われます。

//cmd{
$ cat tmp/hello
Hello, World!
//}

== RUST でやるFUSE

C++ での FUSE アプリケーションプログラミングの流れもあらかたつかめたところで、お待ちかねの Rust でのコーディングに入ってみましょう。

=== コンセプト

==== 既存の Rust FUSE ライブラリについて

そういえば Rust で FUSE を扱うための既存ライブラリは無いのでしょうか？
実はちょうどそれに当たる @<fn>{rustfuse} が存在します。
しかしながら rust-fuse はローレベル API だけをターゲットにしており、より抽象化されているハイレベル API を Rust から使用する手段は今の所ありません。
rust-fuse としては、 C 実装のハイレベル API を直接呼び出すよりはローレベル API を利用するハイレベル API を Rust で実装するほうが Rust の良さを活かせるため、やるならそうすべきとの構想があるようです。

//footnote[rustfuse][rust-fuse: https://github.com/zargony/rust-fuse]

==== 今回自作する Rust FUSE ライブラリ

一方で今回は libfuse で持っているハイレベル API の C 実装を素直に Rust から呼び出すことに注力します。
ウェブでよく散見される libfuse を用いたファイルシステムの実装はハイレベル API を前提にしているものが多く、 libfuse 公式のサンプルコードもハイレベル API を利用していることから、ハイレベル API を Rust 向けに提供することに一定の価値があるものと思われます。

今回の Rust FUSE ライブラリは rust-fuse と親しい機能を持ちながらハイレベル API に対する着想が異なることから、 yarf(Yet Another Rust Fuse) と名付けます。

=== Rust でやる ffi

さて、 Rust でそもそもどのように C あるいは C++ のライブラリを扱うことができるのでしょうか？

Rust の ffi で C のライブラリを利用するのは非常に簡単です。以下のように分類して考えます。

 * Rust と C どちらでも利用可能な型を用意する
 * C の関数をリンク可能にする
 * Rust の関数を C から呼び出し可能にする

まずは Rust と C の間で型を相互利用可能にすることから初めてみます。
ある程度は Rust では標準で C でも利用可能な型が用意されています。
例えば libc crate 以下に size_t などの型が定義されていたり、 std::os::raw モジュールで c_int, c_void などの型が定義されています。
それで満たされない、各々のライブラリで独自に定義している構造体に関しては、自前で定義しておく必要があります。
これは以下のように #[repr(C)] アトリビュートを構造体の定義に付与することで可能です。

//source[repr_c.rs]{
#[repr(C)]
struct MyStruct {
  id:   c_int,
  name: *const c_char,
  ...
}
//}

C の関数をリンクするには、 C のヘッダのように関数の宣言を Rust で行い、 #[link(name = "mylib")] アトリビュートを付与するだけです。

//source[link_c.rs]{
extern {
  fn anyfunc(c_int, *const c_char) -> c_void
}
//}

また C のライブラリ内から Rust の関数をコールバック呼び出すする際には、 Rust の関数側に extern "C" 指定が必要になります。

//source[callback_c.rs]{
extern "C" fn anycallback(id: c_int, name: *const c_char) -> c_void
  ...
}
//}

このような多少の準備は必要なものの、 Rust と C の相互利用はこれで可能になります。

=== yarf の実装の流れ

次に、 Rust で ffi で libfuse を利用する流れを考えます。

Rust の ffi を使ったライブラリの実装パターンとして、よく C, C++ の ffi のための記述だけ分離した sys crate とそれを用いて Rust らしいなるべく安全なインタフェースを提供する crate という二段構えがとられます。
yarf でもそれに従い、 FUSE ハイレベル API を扱うことに注力する yarf-sys crate とそれを用いて FUSE の各コールバック関数を trait として取り扱ったりなるべく unsafe ブロックを記述しなくて済むようにした yarf crate を作る事にします。

==== yarf-sys はじめの一歩

==== bindgen の利用

==== サンプルコードの記述

ここまで来れば最低限 Rust から FUSE ハイレベル API が扱えるようになっているはずです。
yarf crate の開発に着手する前に折角ですので、 yarf-sys crate で hello サンプルコードを実装してみます。
記述する処理は基本的に C のサンプルと同様になります。

==== yarf crate の実装

==== サンプルコードの記述

== 反省点や知見など

== まとめ

