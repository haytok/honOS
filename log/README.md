# 概要

- 自作 OS をするのログを残す。

## 開発環境のセットアップ

- この Ansible の箇所は飛ばした。ここを飛ばしても MikanOS は起動したので、問題ないと思われる。

```bash
TASK [check whether qemu-system-gui exists] *********************************************************************************************************************************
fatal: [localhost]: FAILED! => {"changed": true, "cmd": ["dpkg-query", "--show", "qemu-system-gui"], "delta": "0:00:00.034209", "end": "2021-08-14 00:53:29.920960", "msg": "non-zero return code", "rc": 1, "start": "2021-08-14 00:53:29.886751", "stderr": "dpkg-query: qemu-system-gui に一致するパッケージが見つかりません", "stderr_lines": ["dpkg-query: qemu-system-gui に一致するパッケージが見つかりません"], "stdout": "", "stdout_lines": []}
...ignoring
```

### 参考

- [mikanos-build](https://github.com/uchan-nos/mikanos-build)
