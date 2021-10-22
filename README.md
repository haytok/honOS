# honOS

- [ゼロからのOS自作入門](https://www.amazon.co.jp/%E3%82%BC%E3%83%AD%E3%81%8B%E3%82%89%E3%81%AEOS%E8%87%AA%E4%BD%9C%E5%85%A5%E9%96%80-%E5%86%85%E7%94%B0-%E5%85%AC%E5%A4%AA/dp/4839975868) を参考に自作 OS に取り組む。

# 開発環境の構築方法

- `/home/h-kiwata/edk2` ディレクトリで edk2 の環境をセットアップする。

```bash
source edksetup.sh
```

- `/home/h-kiwata/edk2/Conf/target.txt` のファイルの `ACTIVE_PLATFORM` の箇所を以下のように用途に応じて設定する必要がある。

```bash
 ACTIVE_PLATFORM       = HonoLoaderPkg/HonoLoaderPkg.dsc
```

- `HonoLoaderPkg` は OS 側への`シンボリックリンク`を張る。

- OS 用のディレクトリで edk2 を使用するファイルをビルドする際に、以下のコマンドを実行する。

```bash
build
```

- OS 側のディレクトリで以下のコマンドを実行すると、OS をビルドすることができる。

```bash
make
```

- OS 側のディレクトリで以下のコマンドを実行すると、OS を起動させることができる。

```bash
make run
```

# 画面

## osbook_day30f

![osbook_day30f.gif](https://github.com/dilmnqvovpnmlib/hakiwata/blob/main/content/post/20210901/media/osbook_day30f.gif)

## 参考

- [uchan-nos/mikanos](https://github.com/uchan-nos/mikanos)
