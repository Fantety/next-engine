# ServerSign / VerifyServerSign 签名说明

本文档说明 `pkg/sign` 中服务器签名与验签的当前规则，供内部服务和外部服务提供商对接时参考。

## 1. 签名算法

- 算法：`HMAC-SHA256`
- 输出格式：`hex` 小写十六进制字符串
- 字符编码：`UTF-8`
- 签名版本：当前固定为 `v1`

签名原文 `payload` 的拼接格式如下：

```text
v1
{service}
{uid}
{provider}
{sign_time}
{body}
```

注意：

- 各字段之间使用 `\n` 连接，不要多加空格。
- `uid` 和 `provider` 必须以十进制整数字符串形式参与签名。
- `sign_time` 必须以 Unix 秒级时间戳的十进制字符串参与签名。
- `body` 必须是“标准 JSON 字符串”。
- `body == ""` 时，服务端会按 `{}` 参与签名。
- `body` 末尾如果带有一个换行符 `\n`，服务端会在签名前移除这个尾部换行。

最终签名：

```text
sign = hex( HMAC_SHA256(secret, payload) )
```

## 2. 字段说明

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `service` | `string` | 是 | 服务名称，例如 `admin`、`prod` |
| `uid` | `int64` | 是 | 用户 ID |
| `provider` | `int64` | 是 | 服务提供商 ID |
| `body` | `string` | 否 | 参数主体的标准 JSON 字符串 |
| `sign_time` | `int64` | 是 | 调用方签名时的 Unix 秒级时间戳 |
| `secret` | `string` | 是 | 共享签名密钥，仅通信双方持有 |
| `sign` | `string` | 是 | 最终签名字符串，小写 hex |

## 3. `body` 规则

当前实现中，`body` 不再是对象类型，而是“已经准备好的 JSON 字符串”。

这意味着：

- 服务端不会替你重新排序 JSON 键。
- 服务端不会替你压缩 JSON 空格。
- 服务端不会替你修正数字或字符串类型。
- 服务端只会做两件事：
  - 如果 `body == ""`，按 `{}` 处理
  - 去掉结尾单个 `\n`

因此，外部调用方必须自己保证最终传入的 `body` 字符串稳定且一致。

推荐约定：

- 使用紧凑 JSON，不带多余空格和换行
- JSON 对象键顺序固定
- 避免不同语言生成风格不一致

推荐格式示例：

```json
{"card_id":"VIP-2026-ABC","meta":{"from":"ops","scene":"gift"},"score":1}
```

不推荐：

```json
{
  "score": 1,
  "card_id": "VIP-2026-ABC",
  "meta": {
    "scene": "gift",
    "from": "ops"
  }
}
```

虽然它们语义上可能相同，但字符串不同，签名结果也会不同。

## 4. 数值类型建议

为避免跨语言数值精度差异，推荐遵循以下约定：

- `uid` 和 `provider` 使用 `int64`
- `sign_time` 使用 Unix 秒级 `int64`
- `body` 内部的金额、积分、数量等字段，推荐使用整数
- 高精度小数建议转字符串
- 禁止使用 `NaN`、`Infinity`、`-Infinity`

例如以下两种 `body` 字符串签名通常不同：

```json
{"score":1}
```

```json
{"score":"1"}
```

## 5. 验签规则

验签时，使用收到的：

- `service`
- `uid`
- `provider`
- `body`
- `sign_time`

按同样规则重新生成签名，再与对方传来的 `sign` 比较。

服务端当前实现：

- 支持大小写不敏感的 hex 签名输入
- 使用 `hmac.Equal` 进行常量时间比较
- 要求签名时间在容忍窗口内

当前时间窗口：

- `TimeUnixToleranceSec = 30`

即：

- 如果 `sign_time` 早于当前时间超过 30 秒，验签失败
- `sign_time < 0`，验签失败

## 6. 推荐对接流程

推荐外部服务按以下步骤进行签名：

1. 先构造业务对象
2. 将对象序列化为稳定的、紧凑的 JSON 字符串
3. 记录当前 Unix 秒级时间戳 `sign_time`
4. 拼接 `payload`
5. 使用共享密钥做 `HMAC-SHA256`
6. 输出小写 hex 字符串

## 7. Go 参考实现

```go
package main

import (
	"crypto/hmac"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"strconv"
	"strings"
	"time"
)

func compactJSON(v any) (string, error) {
	data, err := json.Marshal(v)
	if err != nil {
		return "", err
	}
	return string(data), nil
}

func sign(secret, service string, uid, provider int64, body string, signTime int64) string {
	if body == "" {
		body = "{}"
	}
	body = strings.TrimSuffix(body, "\n")
	payload := strings.Join([]string{
		"v1",
		service,
		strconv.FormatInt(uid, 10),
		strconv.FormatInt(provider, 10),
		strconv.FormatInt(signTime, 10),
		body,
	}, "\n")

	mac := hmac.New(sha256.New, []byte(secret))
	mac.Write([]byte(payload))
	return hex.EncodeToString(mac.Sum(nil))
}

func main() {
	body, err := compactJSON(map[string]any{
		"card_id": "VIP-2026-ABC",
		"meta": map[string]any{
			"from":  "ops",
			"scene": "gift",
		},
		"score": 1,
	})
	if err != nil {
		panic(err)
	}

	signText := sign("your-secret", "admin", 1001, 2002, body, time.Now().Unix())
	fmt.Println(signText)
}
```

## 8. Python 参考实现

```python
import hashlib
import hmac
import json
import time


def compact_json(body_obj: dict | None) -> str:
    if body_obj is None:
        return "{}"
    return json.dumps(
        body_obj,
        ensure_ascii=False,
        separators=(",", ":"),
    )


def sign(secret: str, service: str, uid: int, provider: int, body: str, sign_time: int) -> str:
    if body == "":
        body = "{}"
    body = body.rstrip("\n")
    payload = "\n".join([
        "v1",
        service,
        str(uid),
        str(provider),
        str(sign_time),
        body,
    ])
    return hmac.new(
        secret.encode("utf-8"),
        payload.encode("utf-8"),
        hashlib.sha256,
    ).hexdigest()


if __name__ == "__main__":
    body = compact_json({
        "card_id": "VIP-2026-ABC",
        "meta": {"from": "ops", "scene": "gift"},
        "score": 1,
    })
    print(sign("your-secret", "admin", 1001, 2002, body, int(time.time())))
```

## 9. Node.js 参考实现

```javascript
const crypto = require("crypto");

function compactJSON(bodyObj) {
  if (bodyObj == null) {
    return "{}";
  }
  return JSON.stringify(bodyObj);
}

function sign(secret, service, uid, provider, body, signTime) {
  if (body === "") {
    body = "{}";
  }
  body = body.replace(/\n$/, "");
  const payload = [
    "v1",
    service,
    String(uid),
    String(provider),
    String(signTime),
    body,
  ].join("\n");

  return crypto
    .createHmac("sha256", Buffer.from(secret, "utf8"))
    .update(Buffer.from(payload, "utf8"))
    .digest("hex");
}

const body = compactJSON({
  card_id: "VIP-2026-ABC",
  meta: { from: "ops", scene: "gift" },
  score: 1,
});

console.log(sign("your-secret", "admin", 1001, 2002, body, Math.floor(Date.now() / 1000)));
```

## 10. Rust 参考实现

依赖：

```toml
[dependencies]
hmac = "0.12"
sha2 = "0.10"
hex = "0.4"
serde_json = "1"
```

示例：

```rust
use hmac::{Hmac, Mac};
use serde_json::json;
use sha2::Sha256;
use std::time::{SystemTime, UNIX_EPOCH};

type HmacSha256 = Hmac<Sha256>;

fn compact_json(value: &serde_json::Value) -> String {
    serde_json::to_string(value).unwrap()
}

fn sign(secret: &str, service: &str, uid: i64, provider: i64, body: &str, sign_time: i64) -> String {
    let body = if body.is_empty() { "{}" } else { body };
    let body = body.trim_end_matches('\n');

    let payload = [
        "v1".to_string(),
        service.to_string(),
        uid.to_string(),
        provider.to_string(),
        sign_time.to_string(),
        body.to_string(),
    ]
    .join("\n");

    let mut mac = HmacSha256::new_from_slice(secret.as_bytes()).unwrap();
    mac.update(payload.as_bytes());
    hex::encode(mac.finalize().into_bytes())
}

fn main() {
    let body = compact_json(&json!({
        "card_id": "VIP-2026-ABC",
        "meta": {
            "from": "ops",
            "scene": "gift"
        },
        "score": 1
    }));

    let sign_time = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_secs() as i64;

    let sign_text = sign("your-secret", "admin", 1001, 2002, &body, sign_time);
    println!("{}", sign_text);
}
```

## 11. 对接建议

- 外部服务商落地前，请先用固定测试数据与服务端做一次联调
- 请确保 `body` 最终传给签名函数的是“完全确定的 JSON 字符串”
- 如果不同语言生成的 JSON 键顺序不稳定，请先在各自语言里固定序列化规则
- 若联调失败，优先逐项比对：
  - `service`
  - `uid`
  - `provider`
  - `sign_time`
  - `body`
  - `payload`
- 生产环境建议同时做签名与验签，不要只做单向签名
