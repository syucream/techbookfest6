= ここがクソだよ暗号資産

== はじめに

はじめましての方ははじめまして。
お久しぶりの方はお久しぶりです。るなすたです。

今回は、bitcoin とそこからフォークした monacoin など各種アルトコインの
仕様である、P2SH @<fn>{P2SH-fn} の仕様がいかに酷いか、語りたいと思います。
//footnote[P2SH-fn][最終的な仕様は https://github.com/bitcoin/bips/blob/master/bip-0016.mediawiki にあります。]

今回も、基礎的なところから順に説明していきますので、
頑張ってついてきていただければ幸いです。

== 前提知識

=== 暗号資産におけるトランザクションとは

トランザクション、日本語で言えば取引と訳されるところですが、
暗号資産においても、概ね間違った訳ではありません。

ですが、技術的に見ていくともう少し異なった側面が見えてきます。
まずはそこからお話していきましょう。

ここからは代表して bitcoin について語りますが、
bitcoin とそこからフォークした各種アルトコイン、すべてが対象となる話ですので、
その旨了承していただければと思います。

bitcoin では、資産の価値というものの、所有権（今はまだふわっとしていますが、
後できちんと定義します）を次々と移転していくその経緯をすべて、
データベースとして共有管理しています。

つまり、bitcoin が実際に持っている情報は、資産の価値の移転の記録であり、
その構成要素となる最小単位が、トランザクションと呼ばれるものです。

//image[hoshizuki_tx_graph][資金の流れとトランザクション]

トランザクションには、入力と出力と呼ばれるものがあり、
別のトランザクションを接続するための口、ポートとして機能しています。

あるトランザクションの出力から別のトランザクションの入力に接続する、
この接続によってトランザクションの集合は網目のようにつながっており、
資産の価値の流れを示しています。
これらすべてが、bitcoin の管理する資産の価値の移転情報というわけです。
そして、その 1 単位がトランザクションという位置づけになっています。

また、トランザクションは、複数の入力、複数の出力を持ちます。
複数のトランザクションの出力から資産の価値を束ね、
複数のトランザクションに対して送り出すことができるようになっています。

//image[hoshizuki_tx_inout][トランザクションの入力ポート・出力ポート]

これにより、何人かから集めた資産を束ね、一部を誰かに送金し、残りを自分に送金する、
つまりおつりとして使うことができるわけです。

新規のトランザクションを作成し、他のトランザクションの出力から
新しいトランザクションの入力に接続する。そして、送金先の人が使うための、
もしくは自分が使うための出力を定義する。
これが、bitcoin における送金の仕組みです。

//image[hoshizuki_sending][新規トランザクションを作って既存のトランザクションと接続することが送金の本質]

=== トランザクション出力と資産の価値

トランザクションが連なることで所有権を移転していく資産の価値。
ということは、最後に所有権を持っている人が、その資産の持ち主というわけです。

最後に所有権を持っている、ということはつまり、
まだどこにも接続されていないトランザクション出力のこととなります。

この、「どこにも接続されていないトランザクション出力」のことを、
Unspent Transaction Output、略して UTXO と呼びます。

この UTXO を全部かき集めたものが、その人が持つ資産の残高、となります。

注意したいのは、トランザクション自体に持ち主は存在しない。というところです。
トランザクションはあくまで資産の価値の所有権移転の情報を持っているだけであり、
誰のものでもありません。ただの公開情報でしかありません。
資産の価値はトランザクション自体ではなく、その出力にあるというのがポイントです。

UTXO を使用して別のトランザクションを接続することで、
その資産を使うことになり、新たな UTXO を手に入れるのが、
資産の入手というわけです。

=== トランザクションを接続する仕組み

トランザクションの出力と、別のトランザクションの入力を接続する。
これで所有権の移転が行われるとすると、誰でも接続できては困ります。

そこで、トランザクションの出力は通常ロックされています。

ロックされていると言っても、状態を持つわけではありません
（ブロックチェーンではデータの書き換えができないことに注意）。
トランザクションの出力には、ロックを解除するための条件が書かれています。
これを、scriptPubKey と呼んでいます（具体的な内容については後述します）。

そして、トランザクションの入力には、その条件を満たすデータ列を書きます。
これを scriptSig と呼んでいます（具体的な内容については後述します）。

あるトランザクションの出力に書かれた scriptPubKey、これに対応する
scriptSig を持つ入力を作成することができれば、その UTXO の所有者であることを
証明できる。そういうカラクリを用意します。

今までふわっと所有権と呼んできましたが、UTXO に書かれた scriptPubKey に、
対応する scriptSig を作成できることが、UTXO を所有している、ということであり、
つまり暗号資産を所有している、と呼べるわけです。

=== scriptPubKey と scriptSig

ここまでは、トランザクションというもの、送金の仕組み、
UTXO のロックとアンロックという概念について解説しましたが、
その鍵となっている scriptPubKey と scriptSig について、
さらに詳しく掘り下げて解説していきます。

script という単語で勘付いた方も多いと思いますが、実はこの 2 つのデータ列は、
Script @<fn>{Script} と呼ばれる言語で書かれたプログラムとなっています。
//footnote[Script][https://en.bitcoin.it/wiki/Script]

この言語はスタック型の言語となっており、bitcoin にはその機械語を
実行する仮想マシンが実装されています。

bitcoin は、scriptSig と scriptPubKey を連結して 1 つのプログラムとした後、
それを実行します。そして、実行が成功して、スタックトップに TRUE が積まれていた場合、
それをアンロック成功とみなして接続を行います。

ここで実行が失敗した場合や、スタックトップが FALSE だった場合、
bitcoin はその scriptSig は不正なものとみなして、トランザクションごとドロップさせます。

例えば、scriptPubKey に
//list[EQUAL][サンプル scriptPubKey]{
1. OP_PUSH 0x1234
2. OP_EQUAL
//}
と書かれていた場合、この UTXO をアンロックできる scriptSig は
//list[PUSH][サンプル scriptSig]{
1. OP_PUSH 0x1234
//}
となります。

OP_PUSH はスタックに後続の値を積む命令で、OP_EQUAL はスタックトップの 2 つの値を
比較して、一致していれば TRUE、一致していなければ FALSE を積む命令です。

この 2 つの Script プログラムを連結して 1 つにすると
//list[MERGED][サンプル Script]{
1. OP_PUSH 0x1234
2. OP_PUSH 0x1234
3. OP_EQUAL
//}
となり、実行すると無事にスタックトップに TRUE が積まれます。

ここで、scriptSig で PUSH する値が 0x1234 でなかった場合、
OP_EQUAL は FALSE を積むので、接続はできません。

これが、UTXO のロック・アンロックの仕組みとなります。

=== 古典的な Script、P2PK

scriptPubKey と scriptSig には Script 言語で記述できるプログラムならば
どんなものでも書けます。なので、みんな自由に作って構いませんが、
一般的に使われている Script のパターンというものがあります。

というか、そもそもの目的として、「scriptPubKey に対応する scriptSig を
作成できることを持って、所有者である証明とする」というところからして、
暗号的に十分安全なプログラムを作らなければなりません。

まずは古典的なもので、P2PK (pay-to-pubkey) @<fn>{P2PK} があります。
//footnote[P2PK][https://en.bitcoin.it/wiki/Script#Obsolete_pay-to-pubkey_transaction]

//list[P2PK][P2PK]{
scriptSig:
1. OP_PUSH <電子署名>
scriptPubKey:
1. OP_PUSH <公開鍵>
2. OP_CHECKSIG
//}

OP_CHECKSIG は、スタックの先頭に積んである 2 つの値を、
公開鍵と電子署名として扱い、ECDSA（楕円曲線 DSA）の署名検証を行う命令です。
バリエーションとして、署名検証に失敗したら即実行を中断する OP_CHECKSIGVERIFY を
使用する場合もあります。

scriptSig、scriptPubKey の名前の由来もわかっていただけたのではないでしょうか。

=== scriptPubKey と UTXO set

bitcoin のノードは、新規のトランザクションを通知されたとき、
そのトランザクションが正式なものか検証を行います。

二重使用といって、同じ出力を 2 つのトランザクションの入力として使うことは禁止されているため、
UTXO set と呼ばれる、UTXO をリストアップしたデータベースを持っています。
このデータベースの肥大化を防ぐため、UTXO の一部である scriptPubKey は極力小さくしたい。
という要望があります。

今回はこの要望を満たそうと試みているところが根幹にあります。

=== scriptPubKey を小さくする P2PKH

ECDSA の公開鍵はそれなりに大きいです。

bitcoin の使用する ECDSA は、secp256k1 という楕円曲線を使用しているため、
公開鍵は compressed 形式というフォーマットで書いても 33Byte になります。

そこで、以下の Script が考案されました。
現在でも一般的に使われている P2PKH (pay-to-pubkey-hash) @<fn>{P2PKH} です。
//footnote[P2PKH][https://en.bitcoin.it/wiki/Script#Standard_Transaction_to_Bitcoin_address]

//list[P2PKH][P2PKH]{
scriptSig:
1. OP_PUSH <電子署名>
2. OP_PUSH <公開鍵>
scriptPubKey:
1. OP_DUP
2. OP_HASH160
3. OP_PUSH <公開鍵のハッシュ>
4. OP_EQUALVERIRFY
5. OP_CHECKSIG
//}

OP_HASH160 は、スタックトップの SHA-256 ハッシュを計算してから、
RIPEMD-160 ハッシュを計算する命令です。
OP_EQUALVERIFY はスタックトップの 2 要素を比較して、不一致ならば
実行を即時失敗させる命令です。

scriptPubKey の命令数自体は増えていますが、いずれも 1Byte の命令ですので、
公開鍵のハッシュが 160bit = 20Byte と考えれば、25Byte とそれなりに縮小できました。

また、この形式にはもう 1 つメリットがあります。

scriptPubKey に書くのが公開鍵ではなく、公開鍵のハッシュ値で済む、ということです。
つまり、誰かに送金するとき、相手の公開鍵ではなく公開鍵のハッシュを教えてもらえば済みます。
この手続きでやり取りするデータ量が小さくなるというものです。

そこで、この公開鍵のハッシュを Base58 エンコードしたものでやり取りすれば便利だね？というわけで、
それをコインアドレスと呼んでいます。未だによく使われていますね。
1BvBMSEYstWetqTFn5Au4m4GFg7xJaNVN2 こんな形式のやつです。

=== scriptPubKey のおばけ、マルチシグ

マルチシグという言葉はご存じでしょうか？

scriptPubKey に複数の公開鍵を書いておいて、
それに対応する電子署名が何個か以上あれば検証成功とする機能です。

例えば、以下の例を見てみましょう。

//list[MSIG][MULTI-SIG]{
scriptSig:
1. OP_PUSH 0
2. OP_PUSH <電子署名１>
3. OP_PUSH <電子署名２>
scriptPubKey:
1. OP_PUSH 2
2. OP_PUSH <公開鍵１>
3. OP_PUSH <公開鍵２>
4. OP_PUSH <公開鍵３>
5. OP_PUSH 3
6. OP_CHECKMULTISIG
//}

scriptPubKey には 3 つの公開鍵を書いておき、
そのうち 2 つの電子署名があれば接続していいよ、というスクリプトです。
OP_CHECKMULTISIG はそんな感じでうまく動く命令です。

scriptSig はちょっと注意が必要で、OP_CHECKMULTISIG の実装にバグがあるので、
先頭にダミーのデータを PUSH する必要があります。
これは bitcoin 当初にあったバグで、未だに引きずっています。酷い……

さて、ここで思い出してください。UTXO set の肥大化を抑えたいので、
scriptPubKey は小さくしたい。ところがどうでしょう。
この scriptPubKey は公開鍵 1 つで 33Byte ある公開鍵のおばけです。

これをなんとかしたい、というわけで今回のお話が始まります。

===[column] OP_CHECKMULTISIG のバグ

OP_CHECKMULTISIG は、スタックが以下の状態になっていることを想定して動作します。

//table[MSIG_STACK][OP_CHECKMULTISIG 実行直前のスタック]{
公開鍵の個数（今回は３）
公開鍵３
公開鍵２
公開鍵１
要求する電子署名の個数（今回は２）
電子署名２
電子署名１
ダミーの値
//}

スタックの一番底にダミーの値が PUSH されていないと正常に動作しません。
つまり、OP_CHECKMULTISIG は 1 つ余分にデータを POP してしまうわけです。

これが、bitcoin 当初からあったバグで、未だに互換性のため仕様となっています。

===[/column]

== 本編に入る前に

ここまでの説明が、bitcoin の元実装となります。

この頃の Script は、不正な動作を防ぐためにあえてチューリング完全に
ならないように設計されており、与えられた命令を順番に実行するだけの、
本当に素直な、仮想マシンでしかありませんでした。

本当に、普通の、ただのスタック型仮想マシンだったんです……

== scriptPubKey を縮小する試み

さて、マルチシグでは scriptPubKey が肥大化しがちという問題がありました。

ここから P2SH (pay-to-script-hash) が採用されるまでの流れを追っていきたいと思います。

=== エントリーナンバー１．BIP-12 - OP_EVAL を導入しよう！

2011/10/18、BIP (Bitcoin Improvement Proposals) という bitcoin の改善提案の
No.12 @<fn>{BIP-12} として、OP_EVAL の導入が提案されました。
//footnote[BIP-12][https://github.com/bitcoin/bips/blob/master/bip-0012.mediawiki]

新たに redeem script という概念を導入します。

//list[BIP12][BIP-12]{
redeem script:
1. OP_PUSH 2
2. OP_PUSH <公開鍵１>
3. OP_PUSH <公開鍵２>
4. OP_PUSH <公開鍵３>
5. OP_PUSH 3
6. OP_CHECKMULTISIG
scriptSig:
1. OP_PUSH 0
2. OP_PUSH <電子署名１>
3. OP_PUSH <電子署名２>
4. OP_PUSH <redeem script>
scriptPubKey:
1. OP_DUP
2. OP_HASH160
3. OP_PUSH <redeem script のハッシュ値>
4. OP_EQUALVERIFY
5. OP_EVAL
//}

おわかりでしょうか？redeem script と呼ばれるものをあらかじめ構築しておき、
scriptSig の最後で、redeem script の機械語をデータとして PUSH しています。

記事の都合上、まとめて並べて書いてありますが、仮想マシンが実行するのは
scriptSig と scriptPubKey だけです。

scriptPubKey ではまずその redeem script をコピーしておいてから、
そのハッシュ値を求め、scriptPubKey に記載されているハッシュ値と比較します。
つまり、scriptPubKey で規定した redeem script であることを確認するわけですね。
OP_EQUALVERIFY なので、一致しない場合はそこで実行失敗扱いになります。

最後に、OP_EVAL で先ほどコピーした redeem script を実行します。
スタックには scriptSig で先に PUSH してある電子署名が載っているので、
OP_CHECKMULTISIG は問題なく成功します。

これで、マルチシグの公開鍵がいくつに増えても、scriptPubKey は 24Byte で固定されます。
格段に縮小されましたね。scriptSig がその分肥大化しますが、こちらは大した問題ではありません。

=== エントリーナンバー２．BIP-16 - 仮想マシンを魔改造しよう！

2012/01/03、かなりトリッキーな手法が提案されました @<fn>{BIP-16}。
//footnote[BIP-16][https://github.com/bitcoin/bips/blob/master/bip-0016.mediawiki]

まずは提案されたプログラムを見てみましょう。

//list[BIP16][BIP-16]{
redeem script:
1. OP_PUSH 2
2. OP_PUSH <公開鍵１>
3. OP_PUSH <公開鍵２>
4. OP_PUSH <公開鍵３>
5. OP_PUSH 3
6. OP_CHECKMULTISIG
scriptSig:
1. OP_PUSH 0
2. OP_PUSH <電子署名１>
3. OP_PUSH <電子署名２>
4. OP_PUSH <redeem script>
scriptPubKey:
1. OP_HASH160
2. OP_PUSH <redeem script のハッシュ値>
3. OP_EQUAL
//}

redeem script と scriptSig は BIP-12 と同じですが、scriptPubKey がシンプルになっています。
なぜ、これで正常に動作するのでしょうか？

BIP-16 はかなりアグレッシブな提案で、なんと仮想マシンの処理系に手を加えることを提案しています。

scriptPubKey が、OP_HASH160 - OP_PUSH (20Byte) - OP_EQUAL の３つの組み合わせから成り立つ場合に限り、
最後まで実行しきった後、scriptSig 実行直後の状態を復元し、スタックトップをプログラムとみなして
実行を継続する。という提案です。

つまり、まずは scriptSig を実行します。すると電子署名と redeem script がスタックに乗ります。
次に scriptPubKey を実行します。スタックトップの redeem script のハッシュ値を計算して、
scriptPubKey に記載されているハッシュ値と一致することを確認します。
これで redeem script が正しいものであると確認できます。

そして、scriptPubKey が規定のフォーマットに沿っているため、追加の処理をします。
scriptSig 実行直後の状態、つまり電子署名と redeem script がスタックに乗っている状態を復元して、
スタックトップにある redeem script をプログラムとして実行します。
そこで公開鍵がスタックに積まれ、OP_CHECKMULTISIG が実行される、と。

な、なんだそれは……アクロバティック過ぎる……

おまえ、それ本気で言ってんの？

=== エントリーナンバー３．BIP-17 - いっそ専用の命令を追加しよう！

2012/01/18、さらに別の提案がなされます @<fn>{BIP-17}。
//footnote[BIP-17][https://github.com/bitcoin/bips/blob/master/bip-0017.mediawiki]

ここまでくればもう何でもアリなんでしょうか。

とりあえず提案内容を見てみましょう。

//list[BIP17][BIP-17]{
scriptSig:
1. OP_PUSH 0
2. OP_PUSH <電子署名１>
3. OP_PUSH <電子署名２>
4. OP_CODESEPARATOR
 - ここから下が redeem script
5. OP_PUSH 2
6. OP_PUSH <公開鍵１>
7. OP_PUSH <公開鍵２>
8. OP_PUSH <公開鍵３>
9. OP_PUSH 3
10. OP_CHECKMULTISIG
scriptPubKey:
1. OP_PUSH <redeem script のハッシュ値>
2. OP_CHECKHASHVERIFY
3. OP_DROP
//}

新しく出てきた OP_CODESEPARATOR は、プログラムの区切りを示す命令で、
特に何もしません。NOP です。

提案された OP_CHECKHASHVERIFY (CHV) は、scriptSig の OP_CODESEPARATOR 以降のハッシュ値を求めて、
それをスタックトップと比較して、一致しない場合は実行失敗とする命令です。
OP_CHECKHASHVERIFY に割り当てる命令コードは、元々 OP_NOP2 として何もしないことになっているので、
古い実装でも問題なく受け付けることができるように、スタックトップはそのまま残します。

まずは scriptSig を実行します。普通に電子署名と公開鍵をスタックに積んで OP_CHECKMULTISIG を
実行します。そして次に scriptPubKey で redeem script のハッシュ値をスタックに積んでから、
例の OP_CHECKHASHVERIFY 命令を実行します。scriptSig の OP_CODESEPARATOR 以降、
つまり 5-10 の機械語データのハッシュ値を求めて、スタックトップと比較検証します。
そして、スタックトップに残っているハッシュ値を捨てて完了です。

おぅ……もうこれはこの用途にしか使えない、完全に専用の命令ですな……

== さて、勝敗やいかに！？

この３つの提案のうち、実際に P2SH として採用されたのは、
エントリーナンバー２番、仮想マシンに魔改造を加える、でした！

えっ……？

……えっっ！？！？

なぜ、この選択肢が選ばれたのか、理由はそれぞれの BIP に書かれています。

いずれの変更も、bitcoin プログラムのバージョンによって受け入れの可否が異なる場合があります。
その場合、意図せずブロックチェーンが分岐してしまう恐れがありました。

そのため、どの BIP にも採択の条件を記載してあります。
それは、PoW な bitcoin ではハッシュレートこそパワーである！というわけで、
ハッシュレート比で 50% 以上の賛同があれば採択とする。というわけです。

具体的には、マイニングをしているユーザに、
「マイニング成功したブロックに唯一存在する、coinbase トランザクション @<fn>{coinbase_tx} の、
coinbase 領域 @<fn>{coinbase_area} に“OP_EVAL”“/P2SH/”“p2sh/CHV”の文字列を記載してもらう」
ことを依頼しました。
//footnote[coinbase_tx][マイニング報酬となる、発掘者に報酬を送金する、入力の存在しないトランザクションです。]
//footnote[coinbase_area][上述の通り、入力トランザクションが存在しないので、scriptSig 領域は何を書いても OK な領域なので、こう呼ばれます。]

そして、一定期間中に掘られたブロックの coinbase トランザクションの統計を取り、
50% を超えた BIP-16 が採択された、というわけです。

要するに、株主総会のようにハッシュレートに応じた投票権を持つわけですが、
bitcoin の仕組み上、それは事実上の秘密選挙となるわけです。

このやり方は bitcoin や他のアルトコインでもよく行われている手法ですが、
参加者全員による議論は行われないため、議事録が残らないという欠点があります。
参加者のそれぞれが別の場所で個別に議論している痕跡などは残っていますが、
言い換えればその程度の情報しか残っていないことになります。

つまり、なぜ BIP-16 が選ばれたのか、それはマイナーたちとコミュニティそれぞれの考えが
あることで、一概には言えません。たまたま BIP-16 推しのユーザが
大きなハッシュレートを持っていたのかもしれません。

ただ OP_EVAL 案。これは比較的わかりやすくて、ループが作れるようになるため、
チューリング完全な言語になってしまう。そうすると静的解析が難しくなるため、
否定的な意見があったそうです。

bitcoin コミュニティはチューリング完全に親を殺されたのでしょうか。

== そんなわけで

そんなこんなで、BIP-12 と BIP-17 は取り下げ扱いとなり、
今日に至るまで、BIP-16 が P2SH の標準として認められることとなりました。

心から思う。本当に、本当にこんな魔改造で良いの？？良かったの！？と。

そしてこの記事を書く前に、P2SH の仕様策定の経緯を調べて驚きました。
bitcoin の仕様は議論で決まるのではないのだと。
確かに、PoW 型ブロックチェーンは 50% 以上のハッシュレートの持ち主を信用するシステムなので、
それにそった意見のそろえ方だとは思いますが、意志決定の経緯や理由が残らないというのは、
後から追うときに非常にツラいものだと思いました。
