# EX-word Memo App

CASIO製電子辞書 **EX-word DATAPLUS 5/6/7** の実機上で動作する、シンプルなメモ帳アプリです。

内蔵ストレージ／SDカードにテキストメモを作成・閲覧・編集・削除できます。編集画面ではローマ字入力による半角カタカナ入力にも対応しています。

## スクリーンショット

<img width="4080" height="2288" alt="Image" src="***REMOVED***" />

## 主な機能

- メモの一覧表示（内蔵ドライブ / SDカード切り替え可）
- 新規メモの作成
- 既存メモの閲覧・編集（複数行対応の簡易テキストエディタ）
- メモの削除
- ローマ字入力による半角カタカナ入力（英字入力との切り替え可）

メモは各ドライブのルートにある `MEMO` フォルダ（`\drv0\MEMO\` または `\crd0\MEMO\`）に、1メモ1テキストファイルとして保存されます。

## 操作方法

### 一覧画面

| キー | 動作 |
|---|---|
| UP / DOWN | カーソル移動 |
| ENTER | 選択したメモを開いて編集 |
| RIGHT | 新規メモを作成 |
| BACKSPACE | 選択したメモを削除（確認あり） |
| LEFT | 内蔵ドライブ／SDカードを切り替え |
| H | ヘルプ表示 |
| V | バージョン情報表示 |
| BACK / POWER | アプリ終了 |

### 編集画面

| キー | 動作 |
|---|---|
| A〜Z | 文字入力（[EN]モード時は英大文字、[Kana]モード時はローマ字入力） |
| SHIFT | [EN]（英字）⇔ [Kana]（半角カタカナ）の入力モード切り替え |
| SYMBOLキー | スペース入力 |
| ENTER | 改行 |
| BACKSPACE | 1文字削除（ローマ字入力中はまず入力中のローマ字を1文字削除） |
| F1（FUNC1） | 保存して一覧に戻る |
| POWER | 保存して一覧に戻る（データ消失防止） |
| BACK | 確認の上、編集を破棄してキャンセル |

### かな入力（半角カタカナ）について

- SHIFTキーで [EN]（英字入力）と [Kana]（かな入力）を切り替えられます。
- [Kana]モードでは、ローマ字を1〜3文字打つと対応する半角カタカナに変換されます。
  例: `KA`→`ｶ`、`SHI`/`SI`→`ｼ`、`KYO`→`ｷｮ`、`GA`→`ｶﾞ`、`XTU`→`ｯ`
- 促音（っ）は子音を連続して打つと自動的に挿入されます（例: `GAKKO`→`ｶﾞｯｺ`）。
- 撥音（ん）は基本的に `NN` で入力します。単独の `N` の後に母音以外の文字が続いた場合は自動的に `ﾝ` として確定します（例: `RINGO`→`ﾘﾝｺﾞ`）。
- 対応するのは**半角カタカナのみ**です。ひらがな・全角文字・漢字は非対応です。

## ビルド方法

このプロジェクトは CASIO EX-word 実機向けの SH4a クロスコンパイル環境（[devkitPro / devkitSH4](https://github.com/MaxSignal/buildscripts)）が必要です。

```bash
git clone <このリポジトリ>
cd exword-memo-app

# devkitPro一式をリポジトリと同じ階層に展開する
curl -sSfL https://github.com/MaxSignal/buildscripts/releases/download/Linux/devkitPro.tar.gz | tar -zxf -

source env-file   # DEVKITPRO / DEVKITSH4 / PATH を設定
make
```

`env-file` はリポジトリ直下に `devkitPro/` フォルダがある前提になっています。別の場所に展開した場合は、`DEVKITPRO` と `DEVKITSH4` を実際のパスに合わせて自分で設定してください。

ビルドが成功すると `build/` 以下にEX-word実機へインストール可能なファイル一式が生成されます。

## 技術的な補足

- 半角カタカナのビットマップフォント（`src/libct/fonts/jpn-font.c`）は Noto Sans CJK JP から 8x8ピクセルへダウンサンプルして生成しています。
- 複数行テキストエディタ（`src/libct/ui/dialog/memo_editor.c`）は本アプリのために新規実装したものです。

## ライセンス・クレジット

本プロジェクトは [Gnuboy EX](https://github.com/brijohn/gnuboy-ex)（GPL-2.0）をベースにしたファイルマネージャーの改造から始まっており、その規定に従い本リポジトリも [GPL-2.0](COPYING) でライセンスされています。

また、ビルドの雛形として [brain-hackers/exword-template](https://github.com/brain-hackers/exword-template) を利用しています。

半角カタカナのフォントグリフは [Noto Sans CJK JP](https://fonts.google.com/noto/specimen/Noto+Sans+JP)（SIL Open Font License）から生成しました。
