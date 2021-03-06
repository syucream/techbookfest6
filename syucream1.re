= Rust で ffi を使って FUSE ライブラリを自作する

こんにちは、はじめまして、 @syu_cream です。
好きなスイーツはおはぎです。

この記事では Rust で ffi(Foreign Function Interface) を使ってライブラリを書いてみた知見について書きます。
今回は libfuse のオレオレライブラリを実装した上で得たノウハウをテーマにします。
また、読者がある程度 Rust でコーディングした事がある前提で執筆しております。
あらかじめご了承ください。

この記事が、あなたの Rust ハックライフのお役に立てれば光栄です。


== はじめに

Rust @<fn>{rust} は Firefox の開発元としてよく知られる Mozilla が開発しているモダンなプログラミング言語です。
新しめのプログラミング言語なだけあって、最近台頭してきた言語によくあるような以下のような機能を持っています。

 * パターンマッチ
 * null 安全な型 Option
 * エラーが起こる可能性があることを表現する型 Result

また Rust の特徴的な機能として以下も挙げられます。

 * デフォルトで所有権が移動するムーブセマンティクスベースの変数所有権
 * 借用とミュータビリティの管理によるコンパイル時のデータ競合チェック
 * 変数のライフタイムの管理
 * シンプルな ffi による C, C++ 関数の呼び出し
 * unsafe ブロックによる柔軟で明示的な危険な操作(ポインタのデリファレンスや C, C++関数呼び出しなど)

これらの機能により C, C++ などより安全にシステムプログラミングが可能であり、かつ既存の C, C++ の資産を利用可能です。

本記事では Rust で ffi で C, C++ の資産を利用するコードを書いてみます。
ここでは FUSE(Filesystem in UserSpace) のライブラリである libfuse のハイレベル API のラッパーライブラリを作ってみます。

//footnote[rust][Rust: https://www.rust-lang.org/]

== FUSE  について

FUSE(Filesystem in UserSpace) はユーザスペースで安全かつ簡単に独自のファイルシステムを動作させるための仕組みです。

独自にファイルシステムを作る場合、従来はカーネルレベルのプログラミングが必要で、実装やデバッグが大変だったり導入障壁が高かったりなどの問題がありました。
FUSE ではユーザスペースで動作するプログラムを記述することになるので、カーネルプログラミングをせずデバッグの容易性も上がります。

FUSE を使って実装されたプロダクトも世の中に幾つか存在します。
有名なものだと、例えば以下のようなものが挙げられます。

 * sshfs @<fn>{sshfs}
 * s3fs @<fn>{s3fs}
 * gcsfuse @<fn>{gcsfuse}
 * google-drive-ocamlfuse @<fn>{google_drive_ocamlfuse}

Linux における FUSE の構成としては、 FUSE のカーネルモジュールとそれをユーザスペースから使いやすくするライブラリ @<fn>{libfuse} の 2 コンポーネントから構成されます。
FUSE を使ったアプリケーションの多くはこの libfuse のインタフェースを利用してファイルシステムを構築したものになります。
また Linux の他にも macOS 向けに libfuse を使えるようにした FUSE for macOS(osxfuse) @<fn>{osxfuse} も存在して、 macOS ユーザも気軽に FUSE アプリケーションを開発・利用できます。

libfuse にはハイレベル API とローレベル API が存在します。
どちらの API を利用する場合でもファイルシステムへの操作に対する各コールバック関数を実装していくことになります。
しかしながらそのインタフェースにハイレベル API とローレベル API では差異が存在します。

ハイレベル API ではコールバック関数にアクセス対象ファイルパスが渡されるのですが、ローレベル API ではファイルパスでなく inode を扱うことになります。
またハイレベル API ではコールバック関数をリターンする際にその処理を終えるモデルになるのに対して、ローレベル API ではそうではなく明示的に処理を終えてレスポンスを返すモデルになります。
ちなみに Web に存在する FUSE アプリケーションの参考例は、実装の容易性などからハイレベル API を用いたものの方が多く見つかることでしょう。

//footnote[sshfs][sshfs: https://github.com/libfuse/sshfs]
//footnote[s3fs][s3fs: https://github.com/s3fs-fuse/s3fs-fuse]
//footnote[gcsfuse][gcsfuse: https://github.com/GoogleCloudPlatform/gcsfuse/]
//footnote[google_drive_ocamlfuse][google-drive-ocamlfuse: https://github.com/astrada/google-drive-ocamlfuse]
//footnote[libfuse][libfuse: https://github.com/libfuse/libfuse]
//footnote[osxfuse][FUSE for macOS: https://osxfuse.github.io/]


== C++ ではじめる FUSE

Rust で ffi を使う上で、 C, C++ として利用する構造体や関数などがどのような構成になっているかを知ることは重要になります。
まずは Rust でコードは書かずに、 C++ で "Hello, World!" を表示するだけのシンプルな FUSE アプリケーションを作ってみましょう。
ここで C++ を用いる強い理由はありません。
C のサンプルは既に Web 上に数多く存在するので、参考として C++ で実装してみました！

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

これらのコールバック関数を fuse_operations 構造体に詰めて、 fuse_main() という FUSE のエントリポイントとなる関数に渡して実行します。

=== FUSE アプリケーションの動作確認方法

このサンプルをコンパイルする時には例えば以下のようにします。

//cmd{
$ g++ -Wall hello.cpp --std=c++0x -DFUSE_USE_VERSION=29 `pkg-config fuse --cflags --libs` -o hello
//}

constexpr や auto など C++11 以降の機能を使っているため、 --std=c++0x のように C++11 以降を使うことを明示します。
また FUSE のバージョンは今回は筆者の環境では 29 を指定しました。
FUSE の共有ライブラリとヘッダのパスには pkg-config を介して指定します。
pkg-config を用いることは必須ではありません。
しかし、特に macOS を用いる場合は osxfuse のパスを指定する必要があるなど手動指定だとハマりどころが多いため、 pkg-config を用いてしまうのがシンプルで済むと思われます。
余談ですが libfuse と osxfuse はシグネチャとしてはほぼ同じになり、内容によっては osxfuse にリンクする想定のはずが libfuse にリンクしてしまえる場合があります。
筆者はそれに気づかず多少の時間を無駄に費やしました。

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

== RUST ではじめる FUSE

C++ での FUSE アプリケーションプログラミングの流れもあらかたつかめたところで、お待ちかねの Rust でのコーディングに入ってみましょう。

=== コンセプト

==== 既存の Rust FUSE ライブラリについて

そういえば Rust で FUSE を扱うための既存ライブラリは無いのでしょうか？
実はちょうどそれに当たる rust-fuse @<fn>{rustfuse} が存在します。
しかしながら rust-fuse はローレベル API だけをターゲットにしており、より抽象化されているハイレベル API を Rust から使用する手段は今の所ありません。
rust-fuse としては、 C 実装のハイレベル API を直接呼び出すよりは Rust で実装しなおすほうが Rust の良さを活かせるため、やるならそうすべきとの構想があるようです。@<fn>{rustfuse_todo} 

//footnote[rustfuse][rust-fuse: https://github.com/zargony/rust-fuse]
//footnote[rustfuse_todo][rust-fuse TODO: https://github.com/zargony/rust-fuse#to-do]

==== 今回自作する Rust FUSE ライブラリ

一方で今回は libfuse で持っているハイレベル API の C 実装を素直に Rust から呼び出すことに注力します。
ウェブでよく散見される libfuse を用いたファイルシステムの実装はハイレベル API を前提にしているものが多く、 libfuse 公式のサンプルコードもハイレベル API を利用していることから、ハイレベル API を Rust 向けに提供することに一定の価値があるものと思われます。

今回の Rust FUSE ライブラリは rust-fuse と親しい機能を持ちながら思想がやや異なることから、 yarf(Yet Another Rust Fuse) と名付けます。

== Rust ではじめる ffi

さて、 Rust でそもそもどのように C あるいは C++ のライブラリを扱うことができるのでしょうか？

Rust の ffi で C のライブラリを利用するのは非常に簡単です。以下のように分類して考えます。

 * Rust と C どちらでも利用可能な型を用意する
 * C の関数をリンク可能にする
 * Rust の関数を C から呼び出し可能にする

まずは Rust と C の間で型を相互利用可能にすることから初めてみます。
ある程度は Rust では標準で C でも利用可能な型が用意されています。
例えば libc クレート以下に size_t などの型が定義されていたり、 std::os::raw モジュールで c_int, c_void などの型が定義されています。
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
extern "C" {
  fn anyfunc(c_int, *const c_char) -> c_void
}
//}

また C のライブラリ内から Rust の関数をコールバック呼び出す際には、 Rust の関数側に extern "C" 指定が必要になります。

//source[callback_c.rs]{
extern "C" fn anycallback(id: c_int, name: *const c_char) -> c_void
  ...
}
//}

このような多少の準備は必要なものの、 Rust と C の相互利用はこれで可能になります。

== yarf の実装の流れ

次に、 Rust で ffi で libfuse を利用する流れを考えます。

Rust の ffi を使ったライブラリの実装パターンとして、よく以下のような 2 つのクレートで構成されます。 @<fn>{rust_sys_crate}

 * C, C++ の ffi のための記述だけ分離した sys クレート
 * sys クレートを用いて Rust らしいなるべく安全なインタフェースを提供するクレート

yarf でもそれに従い、@<img>{syucream1_yarf_design} に示すような構成で作っていきます。

//image[syucream1_yarf_design][yarf の設計]

 * FUSE ハイレベル API を扱うことに注力する yarf-sys クレート
 * yarf-sys を用いて Rust のコーディングが容易になる yarf クレート
 
yarf クレートではFUSE の各コールバック関数を trait として取り扱かいます。
また、なるべく unsafe ブロックを記述しなくて済むようにすることを目指します。

//footnote[rust_sys_crate][rust-sys-crate: https://kornel.ski/rust-sys-crate]

== yarf-sys クレートの実装

それでは yarf-sys を実装していきます。
この作業は前述の ffi の Rust のコードを、 C++ のサンプルで使ったような fuse_operations 構造体や, fuse_main() 関数など必要なパートを実装していくことで進めます。

まず libfuse のヘッダを見ながら必要な構造体を Rust で再実装していきます。
例えば fuse_operations や関連する構造体を以下のように定義していきます。
各構造体には #[repr(C)] アトリビュートを付与します。
また libfuse 由来で snake case の名前にしていますが、 Rust としては構造体は camel case での命名が想定されるため、このままでは警告が出てしまいます。これを抑制するため #[allow(non_camel_case_types)] アトリビュートも付与しています。

//source[yarf_sys_struct.rs]{
#[repr(C)]
#[allow(non_camel_case_types)]
pub struct fuse_file_info {
    pub flags: ::std::os::raw::c_int,
    pub fh_old: ::std::os::raw::c_ulong,
    ...
}

...

pub struct fuse_operations {
    pub getattr: ::std::option::Option<
        unsafe extern "C" fn(
            path: *const ::std::os::raw::c_char,
            stbuf: *mut ::libc::stat,
        ) -> ::std::os::raw::c_int,
    >,

    pub readlink: ::std::option::Option<
        unsafe extern "C" fn(
            arg1: *const ::std::os::raw::c_char,
            arg2: *mut ::std::os::raw::c_char,
            arg3: usize,
        ) -> ::std::os::raw::c_int,
    >,

    ...
}

...
//}

次に fuse_main() を Rust から呼べるようにしたいので Rust でこれを宣言します。
と言いたいところですが、実は fuse_main() は #define で定義されているマクロになっていて Rust としてはこれを直接使うことができません。
ここでは fuse_main() マクロで置換される fuse_main_real() 関数を宣言することにします。
fuse_main() とは異なり引数が 5 個になり第四引数が第三引数のサイズになることに注意しましょう。

//source[yarf_sys_func.rs]{
extern "C" {
    pub fn fuse_main_real(
        argc: ::std::os::raw::c_int,
        argv: *mut *mut ::std::os::raw::c_char,
        op: *const fuse_operations,
        op_size: usize,
        user_data: *mut ::std::os::raw::c_void,
    ) -> ::std::os::raw::c_int;
}
//}

ここで #[link(name = "fuse")] アトリビュートを与えることも可能です。
しかし macOS の場合は #[link(name = "osxfuse")] に置き換えたいのが悩みどころです。
Rust のアトリビュートでは target_os による分岐も可能なのでここにその条件分岐を記述してもいいのですが、今回はそれを使わず、 Rust のビルドスクリプトと pkg-config クレートを使ってリンクすることにしてみます。
これで条件付きで共有ライブラリにリンクする準備が整いました。

//source[build.rs]{
extern crate pkg_config;

#[cfg(not(target_os = "macos"))]
static LIBFUSE_NAME: &str = "fuse";

#[cfg(target_os = "macos")]
static LIBFUSE_NAME: &str = "osxfuse";

fn main() {
    pkg_config::Config::new()
        .atleast_version("2.6.0")
        .probe(LIBFUSE_NAME)
        .unwrap();
}

//}

=== bindgen の利用

Rust で ffi を行うのはこのような作業で単純に実行できるのですが、中身としては純粋に "作業" と言えそうな内容です。
それゆえ手動で ffi をする準備のコードを記述するのは面倒に思えたり、シグネチャの書き間違いや定義漏れなどを生む可能性もあります。

Rust では実はこの作業を簡略化してくれる bindgen @<fn>{bindgen} というツールが存在します。
bindgen に C, C++ のヘッダファイルを食わせることで ffi 用 Rust コードを自動生成してもらえます。

//cmd{
$ bindgen /usr/local/include/fuse.h -o fuse.rs -- -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=29
$ head fuse.rs
/* automatically generated by rust-bindgen */

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct __BindgenBitfieldUnit<Storage, Align>
where
    Storage: AsRef<[u8]> + AsMut<[u8]>,
{
    storage: Storage,
    align: [Align; 0],
//}

bindgen が出力したコードは、一部 link_name アトリビュートが微妙な値になったり、 libc クレートなどに定義されている型を再定義する部分が生じたりします。
しかしながらテストコードも生成してくれる、複雑な表現の型も自動生成してくれるなど多くのメリットがあるのでぜひ利用することをおすすめします。

//footnote[bindgen][bindgen: https://github.com/rust-lang/rust-bindgen]

=== サンプルコードの記述

ここまで来れば最低限 Rust から FUSE ハイレベル API が扱えるようになっているはずです。
yarf クレートの開発に着手する前に折角ですので、 yarf-sys クレートで hello サンプルコードを実装してみます。
記述する処理は基本的に C++ のサンプルと同様に、 getattr, readdir, open, read コールバックを実装して fuse_main() に渡すことになります。
yarf-sys では libfuse からコールバックされる関数を定義する必要があります。
そのため各 コールバック関数を extern "C" する必要があります。

//source[hello_sys.rs]{
extern crate libc;
extern crate yarf_sys;

use libc::{off_t, stat};
use std::ffi::{CStr, CString};
use std::mem;
use std::os::raw::{c_char, c_int, c_void};
use std::ptr;
use yarf_sys::{fuse_file_info, fuse_fill_dir_t, fuse_operations};

const HELLO_PATH: &str = "/hello";
const HELLO_CONTENT: &str = "hello, fuse!\n";

extern "C" fn yarf_getattr(path: *const c_char, stbuf: *mut stat) -> c_int {
    let path_str = unsafe { CStr::from_ptr(path) };
    let path_slice = path_str.to_str().unwrap();

    match path_slice {
        "/" => {
            unsafe {
                (*stbuf).st_mode = libc::S_IFDIR | 0o755;
                (*stbuf).st_nlink = 2;
            }
            0
        }
        HELLO_PATH => {
            unsafe {
                (*stbuf).st_mode = libc::S_IFREG | 0o444;
                (*stbuf).st_nlink = 1;
                (*stbuf).st_size = HELLO_CONTENT.len() as i64;
            }
            0
        }
        _ => -libc::ENOENT,
    }
}

extern "C" fn yarf_readdir(
    path: *const c_char,
    buf: *mut c_void,
    filler: fuse_fill_dir_t,
    _offset: off_t,
    _fi: *mut fuse_file_info,
) -> c_int {
    let path_str = unsafe { CStr::from_ptr(path) };
    let path_slice = path_str.to_str().unwrap();

    match path_slice {
        "/" => {
            match filler {
                Some(filler_func) => unsafe {
                    let current_dir = CString::new(".").unwrap();
                    let parent_dir = CString::new("..").unwrap();
                    let hello_file = CString::new("hello").unwrap();

                    filler_func(buf, current_dir.as_ptr(), ptr::null_mut(), 0);
                    filler_func(buf, parent_dir.as_ptr(), ptr::null_mut(), 0);
                    filler_func(buf, hello_file.as_ptr(), ptr::null_mut(), 0);
                },
                _ => {}
            }
            0
        }

        _ => -libc::ENOENT,
    }
}

extern "C" fn yarf_open(path: *const c_char, _fi: *mut fuse_file_info) -> c_int {
    let path_str = unsafe { CStr::from_ptr(path) };
    let path_slice = path_str.to_str().unwrap();

    match path_slice {
        HELLO_PATH => 0,
        _ => -libc::ENOENT,
    }
}

extern "C" fn yarf_read(
    path: *const c_char,
    buf: *mut c_char,
    _size: usize,
    _offset: off_t,
    _fi: *mut fuse_file_info,
) -> c_int {
    let path_str = unsafe { CStr::from_ptr(path) };
    let path_slice = path_str.to_str().unwrap();

    match path_slice {
        HELLO_PATH => {
            let content = CString::new(HELLO_CONTENT).unwrap();
            let content_len = HELLO_CONTENT.len();
            unsafe {
                ptr::copy_nonoverlapping(content.as_ptr(), buf, content_len);
            }
            content_len as c_int
        }
        _ => -libc::ENOENT,
    }
}

fn main() {
    let ops = fuse_operations {
        getattr: Some(yarf_getattr),
        readlink: None,
        getdir: None,
        mknod: None,
        mkdir: None,
        unlink: None,
        rmdir: None,
        symlink: None,
        rename: None,
        link: None,
        chmod: None,
        chown: None,
        truncate: None,
        utime: None,
        open: Some(yarf_open),
        read: Some(yarf_read),
        write: None,
        statfs: None,
        flush: None,
        release: None,
        fsync: None,
        setxattr: None,
        getxattr: None,
        listxattr: None,
        removexattr: None,
        opendir: None,
        readdir: Some(yarf_readdir),
        releasedir: None,
        fsyncdir: None,
        init: None,
        destroy: None,
        access: None,
        create: None,
        ftruncate: None,
        fgetattr: None,
        lock: None,
        utimens: None,
        bmap: None,
        reserved00: None,
        reserved01: None,
        reserved02: None,
        reserved03: None,
        reserved04: None,
        reserved05: None,
        reserved06: None,
        reserved07: None,
        reserved08: None,
        reserved09: None,
        reserved10: None,
        setvolname: None,
        exchange: None,
        getxtimes: None,
        setbkuptime: None,
        setchgtime: None,
        setcrtime: None,
        chflags: None,
        setattr_x: None,
        fsetattr_x: None,
    };

    // args
    let args = std::env::args()
        .map(|arg| CString::new(arg).unwrap())
        .collect::<Vec<CString>>();
    let c_args = args
        .iter()
        .map(|arg| arg.as_ptr())
        .collect::<Vec<*const c_char>>();

    let pdata: *mut c_void = ptr::null_mut();
    let opsize = mem::size_of::<fuse_operations>();
    unsafe {
        yarf_sys::fuse_main_real(
            c_args.len() as c_int,
            c_args.as_ptr() as *mut *mut c_char,
            &ops,
            opsize,
            pdata,
        )
    };
}
//}

このコードをビルド可能にする Cargo.toml を記述しておいて。

//source[Cargo.toml]{
[package]
name = "yarf-sys"
...

[build-dependencies]
pkg-config = "0.3"

[dependencies]
libc = "0.2.46"

[[bin]]
name = "hello_sys"
path = "examples/hello_sys.rs"
//}

cargo build でビルドした後は C++ のサンプルコードと同様に実行できます。

//cmd{
$ cargo build
   Compiling yarf-sys v0.0.1 (/path/to/yarf/yarf-sys)
    Finished dev [unoptimized + debuginfo] target(s) in 3.55s
$ ./target/debug/hello_sys -d tmp
...
$ cat tmp/hello
Hello, World!
//}

== yarf クレートの実装

無事にサンプルが動作した yarf-sys クレートですが、ポインタの取り回しやそれ起因で unsafe ブロックが多用されている状態です。
またコールバック関数をひたすら並べるという C++ で libfuse を直接使っていた時と状況が変わっていないのがいまいちイケていません。

yarf クレートではこれの使いやすさを向上するため更に抽象化して、またある程度利用者が unsafe ブロックを書かずに済むようにしましょう。

ここでは rust-fuse を参考にして、 libfuse のコールバック関数を trait として宣言しておいて、その各メソッドを実装することでファイルシステムが実装可能な状態を目指してみます。
またこの trait が受け付ける引数にはなるべくポインタをそのまま渡さないことを考えます。
これにより以下のようなコードを書ける状態を目標に掲げます。

//source[fs_trait.rs]{
// ファイルシステムを表現する trait
pub trait FileSystem {

  // 各コールバック関数は trait のメソッドとして実装していく
  fn getattr(&self, _path: String, _stbuf: Option<&mut stat>) -> c_int {
      -libc::ENOSYS
  }

  ...
}

...

// ファイルシステム trait を実装する構造体
struct HelloFS;

// 各コールバック関数を実装していく
impl FileSystem for HelloFS {
    fn getattr(&self, path: String, stbuf: Option<&mut stat>) -> c_int {
        match path.as_str() {
            "/" => {
            ...
//}

=== yarf でのコールバック関数

まず yarf-sys を使って libfuse からのコールバックを受け付ける関数を定義していきます。
ここでは単純に <FUSEの操作名>_proxy という関数をひたすら地道に生やしていくことにします。
これらのコールバック関数が、前述した trait のメソッドを呼び出してくれるように作ります。

ここは特に Hello, World! サンプルと大した差分も無く、ハマる事がありません。

//source[yarf_callbacks.rs]{
extern "C" fn getattr_proxy(path: *const c_char, stbuf: *mut stat) -> c_int {
  ...
}

extern "C" fn readlink_proxy(
    path: *const ::std::os::raw::c_char,
    buf: *mut ::std::os::raw::c_char,
    size: usize,
) -> ::std::os::raw::c_int {
  ...
}

...
//}

このコールバック関数を fuse_operations 構造体に詰めて、 yarf-sys クレートの fuse_main_real() に渡すようにします。

//source[yarf_entrypoint.rs]{
let ops = fuse_operations {
    getattr: Some(getattr_proxy),
    readlink: Some(readlink_proxy),
    ...
}
...

// この後は Hello, World! サンプルとほぼ一緒
//}

=== FileSystem trait を実装する構造体の引き回し

コールバック関数が呼び出された後に悩ましいのが、どのように FileSystem trait を実装する構造体を取り出すかです。
コールバック関数は C で実装された libfuse を介して呼び出されますし、引数に任意のパラメータを取る仕組みも無さそうです。
yarf クレートのモジュールのどこかでグローバル変数として格納してもいいですが、もう少し綺麗な取り回しを考えたいものです。

今回は libfuse で定義される構造体のうち fuse_context と、それを操作する関数を使用してみます。
fuse_context は以下のように void* の private_data という任意のデータをメンバとして持ちます。

//source[fuse_context.c]{
struct fuse_context {
  struct fuse* fuse;
  uid_t uid;
  gid_t gid;
  pid_t pid;
  void* private_data;
};
//}

private_data は fuse_main_real() の第五引数として渡した void* の値がそのままセットされます。
なお、もし init コールバックを実装した場合は、この関数の戻り値が上書きされることになります。
private_data を参照する際は fuse_get_context() を呼び出せば OK です。

これらを利用して Rust で FileSystem trait を実装する構造体を引き回すコードを以下のようにします。

//source[fuse_context_fs.rs]{
// fuse_context をセットする側
...
let fstmp = Box::new(fs);
let fsptr = Box::into_raw(fstmp) as *mut Box<FileSystem> as *mut c_void;

// call fuse_main
unsafe {
    yarf_sys::fuse_main_real(
        c_args.len() as c_int,
        c_args.as_ptr() as *mut *mut c_char,
        &ops,
        opsize,
        fsptr,
    )
};
...

// fuse_context を参照する側
fn get_filesystem() -> Option<Box<FileSystem>> {
    let ctx = unsafe { yarf_sys::fuse_get_context() };
    if ctx.is_null() {
        return None;
    }

    let fsbox = unsafe { (*ctx).private_data as *mut Box<FileSystem> };
    if fsbox.is_null() {
        return None;
    }

    let actual = unsafe { fsbox.read() };
    Some(actual)
}
//}

これで Rust で記述された FileSystem trait の実装を取ることができるようになりました。

=== unsafe な処理の隠蔽

取り出した FileSystem trait のメソッドには、なるべく Rust フレンドリーで安全な値を渡してあげたいものです。
ここでは以下の 4 パターンに分けて、 yarf クレート側で unsafe な処理をしてあげることにします。

 * *const c_char な値の扱い
 * *mut な値の扱い
 * バッファの扱い
 * 関数ポインタの扱い

*const c_char 型の値はかなり多く、 yarf-sys というか libfuse ハイレベル API の第一引数のほとんどがこれです。
実体は NULL 終端されたファイルパスやファイル名の文字列です。
今回はこれらを Rust の String 型として扱います。
この操作は危険ではあるものの、 libfuse がぶっ壊れたポインタを渡してこないことを信じて unsafe で囲んで操作して変換しておきます。

//source[to_string.rs]{
fn to_rust_str(cpath: *const c_char) -> String {
    let path_cstr = unsafe { CStr::from_ptr(cpath) };
    let path_str = path_cstr.to_str().unwrap();

    return String::from(path_str);
}
//}

*mut な型もそこそこ頻出します。
既出の例だと getattr() の第二引数 *mut stat です。
この型は素直に Rust 風に解釈して、値が無い可能性がある型（Option）でありその実体は mutable な参照、と捉えてみます。
*mut stat の場合は Option<&mut stat> にして扱うイメージです。
この変換は as_mut() で簡単に実現可能です。

//source[as_ref.rs]{
let stbuf_ref = unsafe { stbuf.as_mut() };
//}

バッファは read(), write() などで登場します。
特に read() の場合は読み込んだデータを書き出す必要があり mutable な値として扱うことに気をつけます。
今回は Rust としてのバッファの型は、扱いやすさと libfuse に割り当てられた領域を使い回すことから &[u8] 、 mutable な場合は &mut [u8] にします。
ここで代わりに Vec を使うとサイズを変更する操作をしてしまった際や Vec 型変数の寿命が尽きる際によくない振る舞いになることでしょう。
この一見複雑そうな変換ですが、 std::slice::from_raw_parts(), std::slice::from_raw_parts_mut() で簡単に実現可能です。

//source[from_raw_parts_mut.rs]{
let sbuf = unsafe { slice::from_raw_parts_mut(buf as *mut u8, size) };
//}

最後が関数ポインタを引数に取るような例です。
これは厄介であまり libfuse の中でも頻出する訳では無いのですが、先述の例でいうと readdir() の第三引数 fuse_fill_dir_t 型が該当するので向き合わなくてはなりません。
悩ましいですが、今回は fuse_fill_dir_t 型変数と関連する値を Rust の構造体にラップして、 Rust のメソッドとして取り扱えるようにします。
やや面倒ですが、逆に考えると readdir() を実装する側で面倒な処理を記述するコストが減りそうです。

//source[readdir_filler.rs]{
pub struct ReadDirFiller {
    buf: *mut ::std::os::raw::c_void,
    raw_filler: ::yarf_sys::fuse_fill_dir_t,
}

impl ReadDirFiller {
    pub fn new(
        buf: *mut ::std::os::raw::c_void,
        raw_filler: ::yarf_sys::fuse_fill_dir_t,
    ) -> ReadDirFiller {
        ReadDirFiller { buf, raw_filler }
    }

    pub fn fill(
        &self,
        name: &str,
        stbuf: *const stat,
        offset: ::libc::off_t,
    ) -> ::std::os::raw::c_int {
        if let Some(func) = self.raw_filler {
            if let Ok(cname) = CString::new(name) {
                return unsafe { func(self.buf, cname.as_ptr(), stbuf, offset) };
            }
        }
        -libc::EIO
    }
}
//}

これらにより、おおむね unsafe な操作を除外できました。

=== サンプルコードの記述

改めて yarf クレートを利用して Hello, World! サンプルを実装してみます。
以下のように unsafe を書かず、記述量も減らしつつサンプルの実装が行えています！

//source[hello.rs]{
extern crate libc;
extern crate yarf;

use libc::{off_t, stat};
use std::io::Write;
use std::os::raw::c_int;
use std::ptr;
use yarf::ReadDirFiller;
use yarf::{FileSystem, FuseFileInfo};

const HELLO_PATH: &str = "/hello";
const HELLO_CONTENT: &str = "Hello, World!\n";

struct HelloFS;

impl FileSystem for HelloFS {
    fn getattr(&self, path: String, stbuf: Option<&mut stat>) -> c_int {
        match path.as_str() {
            "/" => {
                let mut st = stbuf.unwrap();
                st.st_mode = libc::S_IFDIR | 0o755;
                st.st_nlink = 2;
                0
            }
            HELLO_PATH => {
                let mut st = stbuf.unwrap();
                st.st_mode = libc::S_IFREG | 0o444;
                st.st_nlink = 1;
                st.st_size = (HELLO_CONTENT.len() as c_int).into();
                0
            }
            _ => -libc::ENOENT,
        }
    }

    fn open(&self, path: String, _fi: Option<&mut FuseFileInfo>) -> c_int {
        match path.as_str() {
            HELLO_PATH => 0,
            _ => -libc::ENOENT,
        }
    }

    fn read(
        &self,
        path: String,
        buf: &mut [u8],
        _offset: off_t,
        _fi: Option<&mut FuseFileInfo>,
    ) -> c_int {
        match path.as_str() {
            HELLO_PATH => {
                let content_len = HELLO_CONTENT.len();
                let mut wbuf = buf;
                wbuf.write(HELLO_CONTENT.as_bytes()).unwrap();
                content_len as c_int
            }
            _ => -libc::ENOENT,
        }
    }

    fn readdir(
        &self,
        path: String,
        filler: ReadDirFiller,
        _offset: off_t,
        _fi: Option<&mut FuseFileInfo>,
    ) -> c_int {
        match path.as_str() {
            "/" => {
                filler.fill(".", ptr::null(), 0);
                filler.fill("..", ptr::null(), 0);
                filler.fill("hello", ptr::null(), 0);
                0
            }
            _ => -libc::ENOENT,
        }
    }
}

fn main() {
    let fs = Box::new(HelloFS);

    yarf::yarf_main(fs);
}
//}

これにより Rust らしいコードで libfuse のハイレベル API を利用したファイルシステムの実装が可能になりそうです！

=== クレートを crates.io に登録してみる

こうしてある程度使い物になってきたであろうクレートを腐らせてしまうのも気が引けます。
ここでは勇気を出して crates.io @<fn>{crates_io} にクレートを公開してみます。

まず https://crates.io/ にアクセスして GitHub アカウントでログインしてみましょう。
その後 @<img>{syucream1_crates_io_01} のような Account Settings 画面に遷移して、 User Email が設定されていなければ設定しておきましょう。
この画面でさらに、クレートを登録するのに使うトークンを発行できます。
@<img>{syucream1_crates_io_02} のようにトークンを発行して、クレートの登録処理を行う環境で cargo login コマンドを実行してクレデンシャルファイルを準備しておきましょう。

クレートの公開自体は cargo publish することで可能です。
以下は yarf-sys クレートを publish した時の例です。

//cmd{
$ cargo publish
    Updating crates.io index
warning: manifest has no documentation, homepage or repository.
See http://doc.crates.io/manifest.html#package-metadata for more info.
   Packaging yarf-sys v0.0.2 (/path/to/yarf/yarf-sys)
   Verifying yarf-sys v0.0.2 (/path/to/yarf/yarf-sys)
    Updating crates.io index
   Compiling pkg-config v0.3.14
   Compiling libc v0.2.50
   Compiling yarf-sys v0.0.2 (/path/to/yarf/target/package/yarf-sys-0.0.2)
    Finished dev [unoptimized + debuginfo] target(s) in 10.05s
   Uploading yarf-sys v0.0.2 (/path/to/yarf/yarf-sys)
//}

うまくいけば crates.io に自分のクレートのページが生成されるはずです。

無事に crates.io にクレートが公開できれば、後は Cargo.toml に依存関係を記述して利用できるようになります！

//source[Cargo.toml]{
...
[dependencies]
yarf = "0.0.2"
...
//}

//footnote[crates_io][crates.io: https://crates.io/]
//image[syucream1_crates_io_01][crates.io Account Settings][scale=0.8]
//image[syucream1_crates_io_02][crates.io Tokens][scale=0.8]

== 反省点やまとめ

以上、 Rust で ffi で libfuse の バインディングクレートを作るお話でした。
今回のような C, C++ 実装に配慮しつつ恐怖をいだきながら unsafe ブロックで囲む機会はそう多くない気もしています。
しかしながらこれらの使い方や挙動を知っておくことは、自身で新たな Rust バインディングを書きたくなった場合や、既存のクレートのトラブルシュートをする時に有用だと思われます。

こうして Rust のある一面に触れて、強力かつ多機能であることをひしひしと感じられます。
他方、使いこなすのに修練が必要だとも思われます。
安全で高速で美しい Rust のコードを生み出すためにも、みなさんも一緒に精進していきましょう！

余談ですが、今回ネタにした Rust で ffi して libfuse とやり取りするクレートを作成するのは、その性質に沿ったハマりどころがありました。
やはり C のライブラリを ffi で呼ぶ都合、 #[repr(C)] アトリビュートを付与した Rust の構造体が C 側でどう扱われるかは意識する必要があります。
途中、 fuse_operations 構造体を Rust 側で部分的にしかメンバを定義しておらず、 libfuse 側でコールバック関数を呼び出す際に別メンバを参照しに行って結果コールバック関数が未実装と言われるなどのトラブルに見舞われました。
また地味な点ですが、 8 進数リテラルが C++ では prefix が '0' だったのが Rust では '0o' なのに少々悩まされました。
yarf クレートを作るにあたっては Rust の変数のライフタイムと libfuse が管理するポインタの扱いなどには悩まされました。

また、今回のクレートのデバッグには主に rust-lldb を使っていました。
そのほか rustfmt @<fn>{rustfmt} や rust-clippy @<fn>{rust-clippy} にはお世話になりました。
エディタとしては CLion + Rust plugin を使っていました。シンタックスハイライトや補完が思いの外動いて快適です。

//footnote[rustfmt][rustfmt: https://github.com/rust-lang/rustfmt]
//footnote[rust-clippy][rust-clippy: https://github.com/rust-lang/rust-clippy]

