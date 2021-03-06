= ブロックチェーン詐欺を見抜く

== はじめに

一昨年くらいから、ブロックチェーンが企業でも流行りだして、
いろんなネタが出てきています。しかし、どうみても詐欺じみた企画も
たくさん見えてきたので、そろそろまとめておきます。

この記事は、@lunatic_star のブログ @<fn>{blog} に載せた記事の編集版です。
//footnote[blog][https://hoshizuki.hateblo.jp/]

== パブリックチェーンの仕組み

パブリックチェーンは、簡単にまとめると
「ハッシュレート（計算能力）で見たとき、ネットワーク全体の大多数のノードは信頼できる」
という仮定で動いています。

その「信頼できるノード」をかき集める手段として
「マイニングに参加したら報酬が得られる」
というアドバンテージを与えます。

そうすることにより「報酬をエサに充分に多数のマイナーが集まる」ことを目論み、
最初の仮定を満たそうというのが、基本的な仕組みです。

== パブリックチェーン企画の詐欺

さて、ここで新たなパブリックチェーンを立ち上げる企画が出てきたとします。
その企画はどこまで提案していますか？

「充分に多数のマイナーが集まるほどの『報酬』」をきちんと提示できていますか？

これがないと、極少量の計算資源でブロックチェーンを改ざんできてしまいます。
これでは「改ざんできない」というブロックチェーンのメリットを享受できません。

ちなみに、新規に立ち上げた独自のトークンは、報酬とは呼べません。
だって、現金に換金できないんですから。

将来的に上場を目指す？じゃぁ、上場するまでは何を報酬にしますか？
上場するまでは改ざんが容易なブロックチェーンシステムです。
そんなものに誰が信頼して値段をつけますか？値段がつかないトークンが上場できますか？
そこまできちんと説明できなければ、その企画は詐欺も同然です。

別に報酬に限らなくてもいいですが、信頼できる計算資源を充分な量かき集める手法を
提示できない限り、そのパブリックチェーンに改ざん耐性なんてありません。

改ざん耐性のないブロックチェーンを使うメリットなんて一切ないですね。

再度問います。その企画は、充分な計算資源の補給まで保証してくれますか？
それには、相当な予算をつぎ込む必要がありますが、そこまで保証してくれますか？

== プライベートチェーンの仕組み

逆に、ノード・マイナーを信頼できる機材に限定してしまうのが、
プライベートチェーンです。

とはいえ、今時、PC を乗っ取る手口なんていくらでもありますよね。
OS への攻撃、アプリケーションへの攻撃、マルウェアの侵入、ソーシャルハック、etc...
信頼したノードがいつまでも信頼できると仮定するのは少々無理があります。

つまり、完全に信頼できる機材なんて、この世に存在しません。ということは、ここでも
「信頼したノードのうち、大多数が犯罪者に乗っ取られていない」ことが仮定されます。

== プライベートチェーン企画の詐欺

実は、プライベートチェーンでできることは、基本的にすべて RDB で実現できます。

まず、複数台構成による単一障害点の排除は、
普通に MySQL か何かを複数台立ち上げてください。
マイナーノードにのみ、書き込み権限をつければ、
プライベートチェーンとほぼ同等の構成になります。

次に、マイナー（ノード）の大多数が乗っ取られていない仮定は、RDB の方が実は制約が緩いです。

RDB からの応答が食い違ったら多数決を取りましょう。これで、過半数が乗っ取られなければ
応答は信頼できます。一方、プライベートチェーンの場合、実は過半数を乗っ取らなくても
改ざん攻撃は成功する可能性があります。

51% 攻撃の名前だけ広まった結果「ブロックチェーンは 51% 乗っ取られると改ざんされる」と
理解されていることが多いですが、51% というのはあくまで「改ざんの成功率が 100% になる」条件であります。
改ざんが可能という話であれば、10% でも 20% でも数パーセントの確率で改ざんに成功します。
それが、承認からの経過時間によって信頼度が高まる、という話でしかありません。

ブロックチェーンでは、悪意のあるマイナーが多ければ多いほど（過半数に届かなくとも）改ざん成功率は高まります。
また、承認からの経過時間が短ければ短いほど改ざん成功率が高まります。
そのため、実運用にはその反対の条件を満たす必要があるわけです。
一方、RDB には承認からの経過時間など関係ありません。承認後、即信頼できます。
プライベートチェーンは経過時間によって信頼度が上がる仕組みなので、待ち時間があることに比べると、
かなり不便ではないでしょうか。

最後にトランザクション、取引履歴の管理ですが、
ブロックチェーンでは改ざん不能、つまり後からの変更ができないため、
Script と呼ばれる仮想マシンを使った大掛かりな仕組みを用意しています。
RDB だったら過去のトランザクションデータの一部を変更できるので、
そんな仕組みを用意しなくても、もっと簡単な仕組みで取引履歴を管理できます。

ついでに言えば、ブロックチェーンと RDB では、承認できるスループットが
数十倍から数百倍違うと言われています。

それでも、プライベートチェーンを使いますか？
RDB でよくない？

== サイドチェーンの仕組み

サイドチェーンは、マージドマイニングという仕組みを使うことで
「サイドチェーンのブロックを1つ掘り当てると、メインチェーンの報酬も同時にもらえる」
というものになっています。

つまり、例えば bitcoin のサイドチェーンを新規に立ち上げた場合、
bitcoin のマイナーがついでに掘ってくれることを期待できます。

また、サイドチェーンを掘ると bitcoin ももらえるので、
bitcoin 前述の「報酬」をきちんとクリアできることがわかります。

なので、マージドマイニングのサイドチェーンは、
パブリックチェーンと比べれば比較的現実的な解決策と言えます。

ただし。現行のメインチェーンのマイナーがサイドチェーンのマージドマイニングに
移行してくれることが条件です。そのための広報活動などを地道に行う覚悟は必要です。

結局、マイナー集めをしなきゃいけないことに変わりはないですが、
報酬がある分、比較的、現実的ではあるのではないでしょうか。

それともう 1 つ注意点。
マージドマイニングには、namecoin で採用されている
「coinbase 領域にサイドチェーンの情報を書く」という方式と、
RootStock で採用されている「OP_RETURN 領域にサイドチェーンの情報を書く」
という 2 種類の方式しかありません。

つまり、1 つのメインチェーンに対して、一度にマイニングできるサイドチェーンは 2 本まで。なのです。
サイドチェーンが乱立すると、サイドチェーン同士でマイナーの取り合いが発生します。
これはちょっと厄介ですね。頑張ってくださいとしか。

== Ethereum のトークン

Ethereum には、新規に、独自のトークンを作成して売買する機能があります。
これも、Ethereum による信頼性担保があるため、他の選択肢と比べれば、
かなり現実的な解決策と言えるでしょう。

== まとめ

 1. パブリックチェーンを新規で立ち上げる企画はほぼ全部詐欺です
 2. プライベートチェーンは、RDB の劣化版なので、素直に RDB を使いましょう
 3. サイドチェーンは条件付きですが現実的です。条件次第でギリギリ許容範囲でしょう。
 4. Ethrerum のトークンもなんとか現実的です。これは許容できるのではないでしょうか。

頑張って極少数の詐欺じゃないものを見つけ出してください。

以上となります。
