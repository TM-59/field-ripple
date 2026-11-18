# 要件(field-ripple)

今のコードとテストから起こした要件。基準の数値は README の Tests 節と各テストの出力が出どころ。
対応は名前の宣言で、要件を満たすことの意味的な証明ではない。`make check` の trace は、
下の ID を名前に持つ CTest の testcase(`tests/ctest/CMakeLists.txt`)だけを集める(M61)。

### REQ-kink-physics
sine-Gordon のキンク(`src/field.c`)が解析解に従う: 静止エネルギー 8、`x = vt`、エネルギー `8γ` を 1e-3 で保存、
v = 0, 0.6, 0.9 で最大勾配 `2γ`(ローレンツ収縮)、小さな波が `ω² = k² + 1`、押しが `p/E` に一致。
対応: `test_field`。

### REQ-browser-adapter
ブラウザー用アダプター(`src/observer.c`)が端末版の解法と一致し、標本と入力の範囲を検査し、押す向き・有限領域での停止・リセットが正しい。
対応: `test_observer`。

### REQ-flood-line
物語「The Flood Line」(`src/line.c`)の場面の順・物語の数値・重い場面の静けさ・両方の選択を検査し、bench の代表 8 行が `docs/narrative/` とバイト一致する。
対応: `test_line`・`story_bench --check`。

### REQ-collisions
キンク–反キンクとブリーザー(`src/pair.c`)が Perring–Skyrme と一致し、位相のずれ 5 % 以内、振動数 1 % 以内、エネルギーと運動量を保ち、格子で 2 次収束する。
対応: `test_pair`。
