# Cookie Stamp Maker 
GR-PEACHで型抜きクッキーの型を作るためのプログラムです。（開発中）
GR-LYCHEEの開発環境については、[GR-LYCHEE用オフライン開発環境の手順](https://developer.mbed.org/users/dkato/notebook/offline-development-lychee-langja/)を参照ください。

## 概要
カメラ画像をグレースケールに変換して明度を基に3次元のデータを作ります。
データはSTLファイル形式でUSB、または、SDに保存するサンプルです。  
USBとSDが両方挿入されている場合は、先に検出した方のデバイスに接続します。  
オンボードのボタン ``USER_BUTTON0`` を押すと処理を開始します。
