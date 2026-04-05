# Unix/Linux Programming Study
### 名著『Unix/Linuxプログラミング理論と実践』学習備忘録

This repository contains my personal exercises and implementations from the book *Understanding Unix/Linux Programming*.
このリポジトリは、名著『Unix/Linuxプログラミング理論と実践』を読み解きながら、実際に手を動かして実装したコードの記録です。

---

##  Key Improvements / 工夫したポイント

- **Automated Build Environment**: Configured `Makefile` to automatically compile files by simply placing them in a new directory and running `make "filename"`.
- **効率的なビルド環境**: 新しいディレクトリを作成して Makefile を配置するだけで、`make "ファイル名"` で即座にコンパイルできる環境を構築しました。

- **Buffered I/O Library**: Placed a common library (utmplib) in the common/ directory to improve I/O performance by reducing system calls. (Planning to add more headers sequentially.)
- **バッファリング版ライブラリ**: システムコールの回数を減らし、パフォーマンスを向上させる共通ライブラリ（`utmplib`）をcommonディレクトリに配置しています。（順次ヘッダ追加予定）

##  Directory Structure / 構成

- `who/`: User login information tool (who1, who2, and buffered who3).
- `more/`: Terminal pager implementation.
- `cp/`: File copy utility.
- `common/`: Shared headers and libraries used across projects.

---

##  About Me / 自己紹介

**From Chef to Systems Engineer: A Journey of Lifelong Learning**
**板前からシステムエンジニアへ：生涯学習の旅**

- 🇯🇵 **Infrastructure Engineer** based in Japan, originally from Akita.🇯🇵 
  日本を拠点にする現役のインフラ系エンジニア（秋田県出身）。
- **Late Bloomer**: Touched a PC for the first time at age 42.
  42歳で生まれて初めてPCに触れる。
- **Career Pivot**: Successfully transitioned from a professional Chef to the IT industry at age 47.
  47歳で25年の板前修業に区切りをつけ、IT業界へ完全転身。
- **Current Focus**: Deep diving into Linux Kernel internals and computer architecture.
  現在はLinuxカーネルの内部構造やコンピュータアーキテクチャを深く学習中。
- **New Chapter**: Coding for the world from my new 2LDK home office.
  心機一転、2LDKの新居から世界へ向けてコードを発信しています。

"It's never too late to start." / 「始めるのに遅すぎることはない」
