# field-ripple

C11 + libm の sine-Gordon 場の実験。`src/field.c` が物理計算の正典。
端末版・CSV/SVG・GIF は維持し、ブラウザー版も同じ C を Emscripten で実行する。
数値グラフを装飾画像で代用しない。単位は c=1 の無次元量として明示する。

## 検査

- `make test`: 物理・GIF・ブラウザー用 C アダプターの検査。
- `make bench`: 解析解との比較、一定バイアスの加速、減速の実測。
- `make web`: Emscripten が必要。`dist/` 全体で 10MB 以下。
- `make smoke`: ローカル限定の Playwright 実入力・原画読込・画像保存。
- `make plots`: 既存の CSV/SVG/GIF を再生成。
- Docker を使わない。CI は PR の C 単体テストだけ。
- 保存・乱数は使わない。URL のサブパスで動作し、外部通信を追加しない。

## 独自原画の例外 (2026-10-24、Issue #8)

`assets/observatory.jpg` は Codex built-in image_gen で生成した独自デザイン。
第三者の画像・音声・フォント、既存作品の意匠は持ち込まない。
原画は入口と観測画面の背景に実描画し、物理グラフは C の配列から描く。
生成指示・採否は `docs/art-direction.md`、実画面は `docs/images/` に置く。
人物のない物理実験なので、人物・敵・物語は追加しない。

## 記録

README は英語主・日本語補足。public repo の commit/PR に AI 署名を付けない。
提出・デプロイはこのスプリントの範囲外。生成物 `dist/`・依存・キャッシュは追跡しない。
