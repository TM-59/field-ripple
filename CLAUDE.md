# field-ripple

C11 + libm の sine-Gordon 場の実験。`src/field.c` が物理計算の正典。
端末版・CSV/SVG・GIF は維持し、ブラウザー版も同じ C を Emscripten で実行する。
数値グラフを装飾画像で代用しない。単位は c=1 の無次元量として明示する。

## 検査

- `make test`: 物理・GIF・ブラウザー用 C アダプター・物語の層(`test_line`)の検査と、`story_bench --check`
  (bench の行を今のコードで出し直し `docs/narrative/` にバイト一致で在ることを確かめる)。
- `make story`: `docs/narrative/{play-order,ablation,tension}.json` を書き出す。`make story-check` は全行の再生成と照合。
  行はバイト一致で比べるので、生成・確認は Dockerfile の Debian 13(gcc 14)で行う(CI も `docker build .`)。
- `make bench`: 解析解との比較、一定バイアスの加速、減速の実測。
- `make web`: Emscripten が必要。`dist/` 全体で 10MB 以下。
- `make smoke`: ローカル限定の Playwright 実入力・原画読込・画像保存。
- `make plots`: 既存の CSV/SVG/GIF を再生成。
- CI は PR の軽量ゲート `docker build .`(Dockerfile の中で `make test` と `make plots`)だけ。ブラウザー撮影・Web ビルドはローカル。
- 保存・乱数は使わない。URL のサブパスで動作し、外部通信を追加しない。

## 独自原画の例外 (2026-10-24、Issue #8)

`assets/observatory.jpg` は Codex built-in image_gen で生成した独自デザイン。
第三者の画像・音声・フォント、既存作品の意匠は持ち込まない。
原画は入口と観測画面の背景に実描画し、物理グラフは C の配列から描く。
生成指示・採否は `docs/art-direction.md`、実画面は `docs/images/` に置く。
観測室(自由観測)には人物・物語を足さない。物語は下の「The Flood Line」に分ける。

## The Flood Line(物語の層、2026-11-03、Issue #7。指示者決定によるシナリオ重厚化)

- `src/line.c` は `src/field.c` を変えずに上に載る遊びの層(振り子の鎖 = sine-Gordon の力学模型、一様なトルク = ε)。
  場面は固定の進行点(夜の始まり・各区間の終わり)で開き、物語が変えるのは電池の残量・次の印・速度計の揺れ・濡れた電池の漏れ・
  失った turn の代償・ドラムのトルクだけ。台詞は `web/app.mjs`、ビート表は `docs/narrative/beats.json`。
- モーターは 2π t(線の最も遅い振動の 1 周期)かけて回り始め・止まる。急なトルクは線全体を鳴らし続け、解の外で turn が壊れるため。
- bench の自動操作は `tests/line_policy.h`(計器だけを読む人に近い操作)、種は `sub_seed`(XOR・足し算を使わない)。

## 記録

README は英語主・日本語補足。public repo の commit/PR に AI 署名を付けない。
提出・デプロイはこのスプリントの範囲外。生成物 `dist/`・依存・キャッシュは追跡しない。
