# BGM(M63 #34、親 TM-59/project-context#598)

方針(project-context 20-stacks.md「ゲームの BGM」): 基調はバラード。戦闘の役と物語は別の曲で、場面の切り替えで替える。歌は文字を読まない場面だけ。

## 鳴らす経路

ブラウザー版(C の物理を Emscripten で動かす頁)だけに Web Audio の再生を足した。物理は C のまま、JS は画面から曲を選ぶだけ。
端末版・CSV/SVG/GIF は無音のまま。別の頁は無い(衝突とブリーザーの画面は同じ頁の中なので基調の曲)。
曲は最初の操作の後に `bgm/list.json` と `bgm/<曲>.ogg` を読む(初期ロードに入れない)。読めない・復号できない曲は無音のまま遊びを止めない。

## 場面 → 曲

| 画面 | 曲 | 強さ |
|---|---|---|
| 入口・自由観測(Free observation)・衝突とブリーザー | observatory(基調のバラード) | 1 |
| The Flood Line の夜を走らせる間(区間) | line(戦闘の役、pulse) | 1(夜の一時停止は 0.4) |
| 夜の場面(hut・stones・bell) | story | 0.7 |
| 重い場面(spill・ford・cut) | story | 0.4 |
| 夜明けの場面(結末、dawn) | dawn | 1 |

戦う相手・主戦は無い。turn を送って印に止める区間を戦闘の役にした。自由観測は押すまで止まった状態が普通なので一時停止で下げない。場面は選択を押すまで止まる(時間で流れない)。判定は `web/bgm.mjs` の `cue()`(画面の mode・場面・重さ・夜の一時停止だけを読む)。

- 切り替えは 2.5 秒の交差フェード。一時停止は 0.4 倍。
- **全部器楽**: どの場面も文字か計器を読むので、歌入りは作らない。
- 消音: 見出しの ♪(44px)と `M`。保存はしない(この repo は保存機能を持たない)。消音は出口(master)でも切るので、交差フェードで消えていく前の曲も同時に黙る(TM-59/project-context#668 の穴を最初から作らない)。

## 曲(採用・門の値)

| 場面(曲) | 鳴る画面 | 門 | 採用 | 通過 | ループ | LUFS / 山 | 継ぎ目の音色 | 落ち込み / 静かな窓 | 大きさ |
|---|---|---|---|---|---|---|---|---|---|
| observatory | 基調(入口・自由観測・衝突とブリーザーの画面) | soft | observatory-c1-s916 | 12/12 | 21.818 秒 | -15.9 / -2.2 dBFS | 0.44 | 0.0 秒 / 0.0 | 480,881 B |
| line | 戦闘の役(The Flood Line の区間で turn を送る間) | pulse 54.1 BPM(計画 108) | line-c0-s911 | 10/12 | 35.662 秒 | -16.0 / -3.9 dBFS | 0.47 | 0.0 秒 / 0.0 | 845,449 B |
| story | 物語(The Flood Line の場面。重い場面と一時停止は 0.4 倍) | soft | story-c0-s916 | 9/12 | 25.714 秒 | -16.0 / -1.8 dBFS | 0.49 | 1.5 秒 / 0.098 | 518,557 B |
| dawn | 結末(夜の終わりの場面) | soft | dawn-c0-s916 | 11/12 | 16.0 秒 | -16.0 / -1.2 dBFS | 0.54 | 0.0 秒 / 0.0 | 374,270 B |

計 2,219,157 B(4 曲)。通過 42/48。全候補の値は `docs/bgm/gate.md`、生の値は `docs/bgm/report.json`。

## 作り方

1. 計画: `docs/bgm/plan.json`(場面 × caption 2 × seed 6、seed 911〜916。他の作品と同じ caption・seed を使わない)。caption は音の特徴だけで、曲名・作者・作品名を書かない。
2. 生成: ACE-Step 1.5 turbo(MIT、steps 8)を Colab の L4 で 1 回の確保(計算枠 587.62 → 587.5、端末からの 1 回 297.0 秒、48 本)。
   `cd ~/dev/local-llm-ops && .venv/bin/python -m tools.colab bgm --repo <この repo> --out <dir> --backend colab --gpu L4`
   Colab が使えない時の落ちる先は `scripts/bgm_gen.py`(手元の CPU、local-llm-ops の music venv)。
3. 門と書き出し: `~/.local/share/local-llm-ops/genvenv/music/bin/python scripts/bgm_bake.py --takes <dir>`(ffmpeg が要る)。
   門は spot-gap #33・td-games #38 と同じ値で緩めていない: soft(バラード・物語・結末)は local-llm-ops `tools/genmodels/mood.py` の soft の門、pulse(戦闘の役)は PULSE_GATE + テンポ ±5%、全部に -16 LUFS ±1.5・山 -1 dBFS・継ぎ目の音色の跳び 2 倍以下・12 dB の落ち込み 2 秒以下、pulse は静かな窓の割合 0.10 以下、soft のループは 16 秒以上。
   この repo の scripts/ は Ruff ALL・mypy strict の対象なので、spot-gap の `scripts/bgm_bake.py` を同じ門のまま書き直した(soundfile・matplotlib を使わず ffmpeg で読み書き・作図)。門の判定が元の script と一致することは current-swimmer #34 で 48 本を照合して確かめた(同じ script)。
4. 出所と利用条件: 曲は ACE-Step 1.5(MIT、出力に第三者の権利の主張なし)で手元の計画から生成した独自の音源。第三者の音源・歌詞・旋律は使っていない。外部の生成 AI・API key は使っていない。

## 波形とスペクトル(自分で見た所見)

画像は 2 周つないだ物で、真ん中が継ぎ目(上が波形、下が対数の周波数のスペクトル)。

- `observatory`: 音数の少ない旋律と和音が途切れずに続き、頭の 3 秒だけ高い帯が厚い。継ぎ目で段差は見えない(音色の跳び 0.44)。
- `line`: 拍が等間隔に続く。継ぎ目の直前の約 1.5 秒だけ打楽器と低い帯が抜ける(1 周 35.7 秒に 1 回のつなぎの形。音量は中央値から 12 dB までは落ちず、落ち込み 0 秒・静かな窓 0)。
- `story`: 句の間が静かで、句ごとに膨らむ。最も長い静けさは 1.5 秒(門 2 秒の内)、静かな窓の割合 0.098。
- `dawn`: ループの頭に強い 1 打(山 -1.2 dBFS、門 -1 dBFS の内)があり、その後は細かい音が続く。1 周 16.0 秒ごとにこの 1 打が戻る。

人の試聴は門にしない(客観の門を通した物だけを採る)。

## 試験

- `node scripts/bgm-smoke.mjs`(ローカル専用、port `FR_BGM_PORT` 既定 22081、記録 `docs/bgm/smoke.json`):
- nothing fetched before the first press
- observatory plays(出口 -21.3 dBFS)
- story plays(出口 -25.9 dBFS)
- mute during a crossfade silences the fading track too(出口 -120 dBFS)
- line plays(出口 -17.8 dBFS)
- pause ducks(出口 -17.8 → -36.9 dBFS)
- ♪ mutes and restores(出口 -120 → -26.4 dBFS)
- cues: heavy scenes story 0.4x, dawn, entry and observatory (paused or not) observatory
- dist bytes
- dist 2,608,745 B(10 MB の門の内。BGM 4 曲 2,219,157 B を含む)。
- 既存の `make test`(物理・GIF・物語・衝突、`story_bench --check` 8 行一致)と `node scripts/smoke.mjs`(19 項目、夜を実入力で走らせる)は変わらず通る。
- 遊べる門(`bin/playability-gate.py`、main と branch を同じ時間帯 2026-11-27 08:44): main 7/7・branch 7/7(②タップで進行 ○ click,pointerdown,pointerup → ○ click,pointerdown,pointerup)。
