# 組み込みマニュアルの点検と直し方 (v0.22 の候補)

> **このドキュメントの位置づけ**
> 利用者の依頼 (2026-11-08)「マニュアル記載の整合性を再度チェックしてください」。3 つの観点
> (日本語と英語の食い違い / 今のコードとの食い違い / ページどうしの食い違い) で、組み立てたあとの
> 全 120 ページと正本 (`app/manual/manual-before.txt`、`app/` の `@manual` の注釈、`app/manual/AI.md`)、
> 利用者向けの文書 (`docs/page-tool.md` ほか) を読み、指摘をコードで確かめた。
> 英語版が関数のページの一部だけなのは決めたこと (v0.8.0) なので、点検の対象にしない。
> 2026-11-08 に利用者が、§1〜§3 をすべて直すこと、§4 の C1〜C5 をすべて直すことを決めた。

マニュアルの中のリンクと見出しへのリンクは、機械で照らして壊れているものが無かった
(`tests/manual.sh` も見ている)。日本語と英語の 46 組は、早見表と画面の名前を除いて中身が合っていた。

---

## 1. 高 — 書いてあるとおりにすると、期待と違う結果になる

| # | ページ | 何が違うか | 直し方 |
|---|---|---|---|
| H1 | Wiki/Function/redirect | 概要の書式が `&include([[ページ名]], timeout=…)` になっている。写すと転送ではなく埋め込みになる (`wiki_redirect.inc:52`) | `&redirect(` |
| H2 | Wiki/Function/datepages (ja・en) | 例 `&datepages([[予定]]], future, 20, 2012-07-01)` は `]` が 1 つ多い。数字だけの引数は総数の上限 (`limit`) で、説明の「20 件ずつ」にならない (`wiki_datepages.inc:96・128`、`wiki_pages.inc:40`) | `&datepages([[予定]], future, each=20, 2012-07-01)` |
| H3 | Administration (サイトの高度な設定。ja・en) | 例で `DEFAULT_PAGE_TYPE` を「デフォルトのページ種別」として text にしているが、新規作成の画面の既定は `NEWPAGE_DEFAULT_TYPE`。例のとおりにしても新規作成の既定は変わらない (`manual-before.txt:634-645`) | 例を `NEWPAGE_DEFAULT_TYPE` に替える |
| H4 | Wiki/Function/pages | `name=` は子ページの名前ではなく**ページ名の全体**と比べる (`storage_file.inc:509`)。`&pages([[日記]], name=2026-*)` は 1 件も出ない | 「ページ名の全体と比べる。子ページで絞るなら `name=日記/2026-*` か `name=*/2026-*`」と書き、例を足す |
| H5 | Wiki/Function/pages (ja) | 「高度な例」の `更新日時#desc` は効かない (並べ替えの項目は `mtime` などの英語の名前)。英語版は `mtime#desc` で正しい (`wiki_pages.inc:290`) | `mtime#desc` |

## 2. 中 — 情報が欠ける・古い・ページの中で食い違う

| # | ページ | 何が違うか | 直し方 |
|---|---|---|---|
| M1 | SiteMenu (クローン。ja・en) | 「題名とタグは本文に書いてあるので一緒に写る」は Markdown のページだけ。wiki・text・file のタグは写らない (`clone.inc` はタグを写さない) | 写るもの・写らないものを種別ごとに書く |
| M2 | Comparison (ja・en) | 「Markdown記法では書けないもの」に、無い関数 `&subpage`、「検索結果の埋め込み|&search」(&search は検索のフォーム)、「新着・更新の一覧|&mtime」(&mtime は更新日時) | `&pages &childpages &listpages` / 「検索のフォーム」 / `&recent &feed` |
| M3 | Comparison (ja・en) | 「節の終わりを明示」の「同上」が、行を足した結果、別の行を指している | 中身を書く (Markdown の見出しは範囲を持たない) |
| M4 | Wiki/Function/index、Markdown/Extra (ja・en) | 「''ツール''の''目次''」というメニューは無い (ページメニューの「目次」。英語は Summary) | 「''ページメニュー''の''目次''」 |
| M5 | Wiki/Function/time (ja・en) | 例「作成された日は … `&mtime;`」。mtime は更新日時 | `&btime;` |
| M6 | Wiki/Function/childpages | 「エイリアス」の節が「&&listpages は」になっている | 「&&childpages は」 |
| M7 | Wiki/Function/datepages (ja・en) | reverse は既定で付いているのに、パラメータの欄は「順序を逆にします」。日付の引数は最後に書かないと効かない。&pages のパラメータ (show=toc など) が使えることが書いていない | 3 つとも書く |
| M8 | Wiki/Function/calendar | 古い版で保存したきりの日のページは目次が出る (UPGRADE.md にはある)。weeks の既定 (behind=1・ahead=3) が無い。「...」が出るのは狭い画面だけ (サイドバーは日付の下線)。「要約」と「目次」の呼び名の揺れ (画面・&pages・CHANGELOG・手順書は「目次」) | 書き足し、「目次」に揃える |
| M9 | 早見表 (Markdown・Wiki。ja・en)、Wiki/Function/meta | 短いタイトルが無い (`short_title:`、`&title(short)`)。&meta の概要と早見表に `short_title` が無い | 足す |
| M10 | Administration (コマンドラインの道具。ja・en) | 0.17.0 から配っている `page` が無い | 1 行足す (内部用の道具は載せない) |
| M11 | PageMenu (履歴。ja・en) | ロック中に「この版に戻す」を使える人を「管理者とロックした本人」としている。実際は「ロック可能」の権限を持つ人 (本人に限るのは「…だけがアンロックできる」でロックしたとき)。同じページの「ロック」の節とも食い違う | 「ロック」の節に合わせる |
| M12 | Administration/Settings (設定項目の表。ja・en) | `PAGE_FILE_IMAGE_RESIZE_MAX_SIZE` の既定が 1048576 (実際は 524288)。`WIKI_ICON_IMAGE_WIDTH/HEIGHT` はどこからも読まれていない (上流から表にだけ残る)。クローンの上限 `CLONE_MAX_*` 5 つが無い | 直す・消す・足す |
| M13 | Wiki/Function/pages | fields に type・size・content_type・pageview、sort に pageview、show に date が使えるのに書いていない (例は size と type を使っている) | 足す |
| M14 | Wiki/Function/meta | 出せる項目 (作成者・最終更新者・種別・添付の大きさなど) の多くが書いていない | まとめて書く |
| M15 | AI.md (と SKILL.md)、docs/page-tool.md | 終了コード 2 に `invalid_site` が無い。page-tool.md の「ロック中に書ける人」が PageMenu と違う。画面の名前「ユーザー権限」(画面は「ユーザーの管理」)。短いタイトルが書いていない | 直す (AI.md を直したら SKILL.md に写す) |
| M16 | 英語の Wiki 早見表 | 「そのまま表示 (""…"")」の行が無い。行に関数のページへのリンクが無く、英語の「Functions」のページから訳してある 21 個の関数のページへ進めない | 行を足し、訳してある関数の行だけリンクにする |

## 3. 低 — 体裁・細部・古くなった記述

- 英語の画面の名前のずれ: Access analysis → Access analyzer、Load tone → Load a tone、Tag → Tags、
  Meta information → Meta info、''Table of contents'' in ''Tools'' → Summary、pre の Admin tools > Appearance →
  Appearance theme preferences。色調の識別子の説明 (大文字も使える)。英語の早見表の全角の「，」、
  二重の空白、綴り (seperator)。
- 早見表の treepages・link の書式が日本語と英語で違う。popularpages の早見表の `allpages` は効かない、
  概要に `[[ページ名]]` が無い。blogpages の早見表の「件数」(数字だけは総数の上限) と概要の `each=`。
- Comparison の「部分編集の単位」に折りたたみ、「子ページのツリー」に withsystem が無い。
- 説明を空にした添付の HTML のリンクの文字 (リンクのページと HTML ファイルのページの食い違い)。
- PageMenu の「この版に戻す」で短いタイトルも戻る。エクスポートの `$title$` はページ名の最後の部分、
  prefix= / suffix= が書いていない。&pages(show=contents) は &more より前の抜粋だけ・入れ子は出さない。
  &feed の対象はリンクの文字、num=。&search はマニュアルを含めずに探す・書き方はサイトの検索と同じ。
  &pre・&code の言語名、&redirect・&math の数字だけの引数、&math の dpi の範囲。
- 用語の揺れ: 短いタイトルが出る「マス」と「セル」、「メニューバー」と「サイドバー」、メタ情報の呼び名 (英語)。
  Markdown/Extra の「ツール」と「ページメニュー」。
- 文が途中で切れている: Wiki/Function/plain。
- 古くなった記述: &insert の「IE では効果がありません」、&calendar の `extension=openssl.so`、
  &mtime・&time・エクスポートの php.net の strftime へのリンク (NextForm は同じ書式を自前で受ける)、
  マニュアルに残る「今までどおり」「これまでどおり」。

## 4. マニュアルの外で見つけたこと (直すかは利用者が決める)

| # | 場所 | 何が起きるか |
|---|---|---|
| C1 | `wiki_analyze.inc:29` (&popularpages) | 「マニュアルを除く」(`is_ignore_system_page`) を、読まれない側の配列に入れている。&pages や &recent と違い、マニュアルのページも候補に入る (閲覧数が記録されていれば一覧に出る。出るかは確かめていない) |
| C2 | `handler/wiki/wiki_markdown.inc:74` | Wiki 記法から Markdown への変換の確認画面で、&search の札が「検索結果の埋め込み」(M2 と同じ誤り) |
| C3 | `tool/markdown_change_check:251` | 出力の文が「ツールの「目次」」(M4 と同じ誤り) |
| C4 | `tool/analyze_weblog_reader:4` | 時間帯を Asia/Tokyo と決め打ちしている (サイトの時間帯を見ない) |
| C5 | `README.md` | 見出し「現在の状態 — v0.2.0」が古い |

## 5. 直し方 (案)

- §1〜§3 はマニュアル (と利用者向けの文書) の文だけを直す。働きは変えない。日本語と英語の両方にある
  ものは両方を直す。ブランチ `docs/manual-review`、まとまりごとに 1 コミット。
- AI.md を直したら `deploy/scripts/sync-skill.sh` で SKILL.md に写す (`tests/manual.sh` が食い違いを止める)。
- 確かめ: `tests/manual.sh` (全ページが警告なしで描け、中のリンクが切れていない)、ゴールデンマスター、
  スモーク。直した例 (H2・H4・H5 など) は複製サイトで実際に書いて、説明どおりに出ることを見る。
- §4 は別のブランチ (`fix/manual-review-code`) で直す。C1 は、先にマニュアルのページが一覧に混ざる
  ことを複製で確かめてから直す。

## 6. 直した結果 (2026-11-08)

- §1〜§3 をすべて直した (ブランチ `docs/manual-review`、まとまりごとに 1 コミット)。点検で見られなかった
  色調・アクセス解析・HTML ファイルのページも読み、画面の文言・道具の使い方・隔離の働きがコードと合って
  いることを確かめた。
- 直した例を複製サイトで書いて確かめた: `&pages(…, name=親/2026-*)` は子ページが出て `name=2026-*` は
  0 件、`mtime#desc` は新しい順、`&datepages(…, future, each=2, …)` は 2 件ずつで「次」のリンクが出る。
- `tests/manual.sh` (全ページが警告なしで描け、中のリンクが切れていない) が通る。英語の早見表に足した
  リンクも含む。
- **作業中に踏んだこと**: マニュアルは PHP のコメント (`/** … */`) の中に書いてあるので、例に `*/` を
  書くとコメントが閉じて PHP が壊れる (`name=&*/2026-&*` で一度 Parse error にした)。直したあとは、
  変えたファイルを `php -l` で確かめ、足した行に `*/` が無いことも見る。
- AI 向けのページには、短いタイトルの書き方を 1 項目だけ足した。v0.21 では「AI に勧める書き方では
  ない」として足さなかったが、利用者が今回すべて直すと決めたので、「利用者や依頼に指定があるときだけ
  書く」と添えた。

## 7. §4 の結果 (2026-11-08、ブランチ `fix/manual-review-code`)

| # | どうしたか |
|---|---|
| C1 | 組み込みのマニュアルは保存先に実体が無いので、もともと &popularpages の候補に入らない。混ざるのは、0.6.0 までに焼き付けたマニュアルのページが保存先に残るサイトだけだった。それを複製の上で作って一覧に出ることを確かめ (`tests/short-title.sh`)、除く指定をページを探す条件に移した (`withsystem` で含める。&pages と同じ) |
| C2・C3 | 変換の確認画面の札を「検索のフォーム」、確かめの道具の文を「ページメニューの「目次」」にした。コードのコメントと開発者向けの文書の「ツールの目次」は、コードの中の呼び名 (`$TOOLS`) なのでそのまま |
| C4 | **直す必要が無かった。** 読み手 (`analyze_weblog_reader`) はログの時刻を時差込みで読み、世界共通の時刻 (`gmmktime`) にして書き手に渡す。日ごとに区切るのは書き手 (`analyze_weblog_writer`) で、サイトを読み込んでからサイトの時間帯 (`nf_date()`) で区切っている。読み手の `Asia/Tokyo` は結果に効いていない。点検の指摘 (と、それをもとにした利用者への説明) が誤りだったので、コードは変えていない |
| C5 | 見出しだけでなく中身も古かった (v0.1 の説明のまま、これからの表に済んだ版)。版の番号を書かない見出しにし、版ごとの主な変更の表と、v0.1 の説明を「出発点」として残した。予定は project-overview のロードマップを指す |
