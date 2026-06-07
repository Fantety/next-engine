---
title: NexusServer
language_tabs:
  - shell: Shell
  - http: HTTP
  - javascript: JavaScript
  - ruby: Ruby
  - python: Python
  - php: PHP
  - java: Java
  - go: Go
toc_footers: []
includes: []
search: true
code_clipboard: true
highlight_theme: darkula
headingLevel: 2
generator: "@tarslib/widdershins v4.0.30"

---

# NexusServer

Base URLs:

# Authentication

# Auth

<a id="opIdAuth_UpdateAuth"></a>

## PUT Auth_UpdateAuth

PUT /v1/auth

更新认证（非注册！用于对已有用户已有认证方式更新）

> Body 请求参数

```json
{}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|[api.auth.v1.UpdateAuthRequest](#schemaapi.auth.v1.updateauthrequest)| 是 |none|

> 返回示例

> 200 Response

```json
{}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|OK|[api.auth.v1.UpdateAuthReply](#schemaapi.auth.v1.updateauthreply)|

<a id="opIdAuth_CreateAuth"></a>

## POST Auth_CreateAuth

POST /v1/auth

创建认证（非注册！用于对已有用户添加新的认证方式）

> Body 请求参数

```json
{
  "uuid": "string",
  "password": "string",
  "phone": "string",
  "email": "string",
  "twoFactorAuth": "string",
  "threeAuthId": "string",
  "threeAuthType": 0
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|[api.auth.v1.CreateAuthRequest](#schemaapi.auth.v1.createauthrequest)| 是 |none|

> 返回示例

> 200 Response

```json
{}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|OK|[api.auth.v1.CreateAuthReply](#schemaapi.auth.v1.createauthreply)|

<a id="opIdAuth_DeleteAuth"></a>

## DELETE Auth_DeleteAuth

DELETE /v1/auth

删除认证（用于对已有用户已有认证方式删除）

> 返回示例

> 200 Response

```json
{}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|OK|[api.auth.v1.DeleteAuthReply](#schemaapi.auth.v1.deleteauthreply)|

<a id="opIdAuth_ApplyCliAuth"></a>

## POST Auth_ApplyCliAuth

POST /v1/auth/cli/apply

申请CLI授权

> Body 请求参数

```json
{
  "clientId": "string",
  "clientRandom": "string",
  "deviceId": "string",
  "deviceName": "string"
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|[api.auth.v1.ApplyCliAuthRequest](#schemaapi.auth.v1.applycliauthrequest)| 是 |none|

> 返回示例

> 200 Response

```json
{
  "code": 0,
  "message": "string",
  "data": {
    "authUUID": "string"
  }
}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|OK|[api.auth.v1.ApplyCliAuthReply](#schemaapi.auth.v1.applycliauthreply)|

<a id="opIdAuth_CheckCliAuth"></a>

## POST Auth_CheckCliAuth

POST /v1/auth/cli/check

CLI检查授权状态

> Body 请求参数

```json
{
  "clientId": "string",
  "deviceId": "string"
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|[api.auth.v1.CheckCliAuthRequest](#schemaapi.auth.v1.checkcliauthrequest)| 是 |none|

> 返回示例

> 200 Response

```json
{
  "code": 0,
  "message": "string",
  "data": {
    "userId": "string",
    "token": "string",
    "refreshToken": "string"
  }
}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|OK|[api.auth.v1.CheckCliAuthReply](#schemaapi.auth.v1.checkcliauthreply)|

<a id="opIdAuth_HandleCliAuth"></a>

## POST Auth_HandleCliAuth

POST /v1/auth/cli/handle

用户处理授权

> Body 请求参数

```json
{
  "clientId": "string",
  "authUUID": "string",
  "deviceId": "string",
  "accept": true,
  "clientRandom": "string"
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|[api.auth.v1.HandleCliAuthRequest](#schemaapi.auth.v1.handlecliauthrequest)| 是 |none|

> 返回示例

> 200 Response

```json
{
  "code": 0,
  "message": "string"
}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|OK|[api.auth.v1.HandleCliAuthReply](#schemaapi.auth.v1.handlecliauthreply)|

<a id="opIdAuth_Logout"></a>

## POST Auth_Logout

POST /v1/auth/logout

登出（用于对已有用户已有认证方式登出）

> Body 请求参数

```json
{
  "scene": "string",
  "service": "string",
  "all": true,
  "deviceId": "string"
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|[api.auth.v1.LogoutRequest](#schemaapi.auth.v1.logoutrequest)| 是 |none|

> 返回示例

> 200 Response

```json
{
  "code": 0,
  "message": "string"
}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|OK|[api.auth.v1.LogoutReply](#schemaapi.auth.v1.logoutreply)|

<a id="opIdAuth_RegisterAuth"></a>

## POST Auth_RegisterAuth

POST /v1/auth/register

注册认证(新用户注册认证，注册后自动登录)

> Body 请求参数

```json
{
  "nickName": "string",
  "password": "string",
  "phone": "string",
  "email": "string",
  "twoFactorAuth": "string",
  "threeAuthId": "string",
  "threeAuthType": 0,
  "phoneCode": "string",
  "deviceId": "string"
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|[api.auth.v1.RegisterAuthRequest](#schemaapi.auth.v1.registerauthrequest)| 是 |none|

> 返回示例

> 200 Response

```json
{
  "code": 0,
  "message": "string",
  "data": {
    "userId": "string",
    "token": "string",
    "refreshToken": "string"
  }
}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|OK|[api.auth.v1.RegisterAuthReply](#schemaapi.auth.v1.registerauthreply)|

<a id="opIdAuth_UpdateSecurityInfo"></a>

## PUT Auth_UpdateSecurityInfo

PUT /v1/auth/security

更新安全信息

> Body 请求参数

```json
{
  "userId": "string",
  "password": "string",
  "secPhone": "string",
  "secMail": "string",
  "phoneCode": "string",
  "oldPhoneCode": "string",
  "deviceId": "string"
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|[api.auth.v1.UpdateSecurityInfoRequest](#schemaapi.auth.v1.updatesecurityinforequest)| 是 |none|

> 返回示例

> 200 Response

```json
{
  "code": 0,
  "message": "string"
}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|OK|[api.auth.v1.UpdateSecurityInfoReply](#schemaapi.auth.v1.updatesecurityinforeply)|

<a id="opIdAuth_SendPhoneCode"></a>

## POST Auth_SendPhoneCode

POST /v1/auth/send/phone/code

获取手机验证码

> Body 请求参数

```json
{
  "phone": "string"
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|[api.auth.v1.SendPhoneCodeRequest](#schemaapi.auth.v1.sendphonecoderequest)| 是 |none|

> 返回示例

> 200 Response

```json
{
  "code": 0
}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|OK|[api.auth.v1.SendPhoneCodeReply](#schemaapi.auth.v1.sendphonecodereply)|

<a id="opIdAuth_RefreshToken"></a>

## POST Auth_RefreshToken

POST /v1/auth/token/refresh

刷新token（用于允许范围内刷新过期的Token）

> Body 请求参数

```json
{
  "refreshToken": "string",
  "token": "string",
  "scene": "string",
  "service": "string",
  "deviceId": "string"
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|[api.auth.v1.RefreshTokenRequest](#schemaapi.auth.v1.refreshtokenrequest)| 是 |none|

> 返回示例

> 200 Response

```json
{
  "code": 0,
  "message": "string",
  "data": {
    "userId": "string",
    "token": "string",
    "refreshToken": "string"
  }
}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|OK|[api.auth.v1.RefreshTokenReply](#schemaapi.auth.v1.refreshtokenreply)|

<a id="opIdAuth_ValidateAuthByPassword"></a>

## POST Auth_ValidateAuthByPassword

POST /v1/auth/validate/password

目前暂不支持获取认证信息，认证相关均在认证服务中处理
 rpc GetAuth (GetAuthRequest) returns (GetAuthReply);
 验证密码认证（用于对已有用户已有密码认证方式验证）

> Body 请求参数

```json
{
  "phone": "string",
  "password": "string",
  "twoFactorAuth": "string",
  "scene": "string",
  "deviceId": "string"
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|[api.auth.v1.ValidateAuthByPasswordRequest](#schemaapi.auth.v1.validateauthbypasswordrequest)| 是 |none|

> 返回示例

> 200 Response

```json
{
  "code": 0,
  "message": "string",
  "data": {
    "userId": "string",
    "token": "string",
    "refreshToken": "string"
  }
}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|OK|[api.auth.v1.ValidateAuthByPasswordReply](#schemaapi.auth.v1.validateauthbypasswordreply)|

<a id="opIdAuth_ValidateAuthByPhoneCode"></a>

## POST Auth_ValidateAuthByPhoneCode

POST /v1/auth/validate/phone/code

验证手机号验证码认证（用于对已有用户已有手机号验证码认证方式验证）

> Body 请求参数

```json
{
  "userId": "string",
  "phone": "string",
  "code": "string",
  "scene": "string",
  "deviceId": "string"
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|[api.auth.v1.ValidateAuthByPhoneCodeRequest](#schemaapi.auth.v1.validateauthbyphonecoderequest)| 是 |none|

> 返回示例

> 200 Response

```json
{
  "code": 0,
  "message": "string",
  "data": {
    "userId": "string",
    "token": "string",
    "refreshToken": "string"
  }
}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|OK|[api.auth.v1.ValidateAuthByPhoneCodeReply](#schemaapi.auth.v1.validateauthbyphonecodereply)|

# User

<a id="opIdUser_CostUserCredits"></a>

## POST User_CostUserCredits

POST /user/credits/cost

CostUserCredits 消耗用户积分

> Body 请求参数

```json
{
  "userId": "string",
  "credits": "string",
  "title": "string",
  "prod": "string",
  "providerId": "string",
  "remark": "string",
  "sign": "string",
  "signTime": "string"
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|[api.user.v1.CostUserCreditsRequest](#schemaapi.user.v1.costusercreditsrequest)| 是 |none|

> 返回示例

> 200 Response

```json
{
  "code": 0,
  "message": "string",
  "data": {
    "credits": "string"
  }
}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|OK|[api.user.v1.CostUserCreditsReply](#schemaapi.user.v1.costusercreditsreply)|

<a id="opIdUser_FindCreditsHistoryByUserID"></a>

## GET User_FindCreditsHistoryByUserID

GET /user/credits/history

FindCreditsHistoryByUserID 获取用户积分变动历史

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|userId|query|string| 否 |none|
|pageSize|query|integer(int32)| 否 |none|
|page|query|integer(int32)| 否 |none|

> 返回示例

> 200 Response

```json
{
  "code": 0,
  "message": "string",
  "data": {
    "total": "string",
    "histories": [
      {
        "id": "string",
        "userId": "string",
        "credits": "string",
        "title": "string",
        "prod": "string",
        "providerId": "string",
        "remark": "string",
        "time": "string"
      }
    ]
  }
}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|OK|[api.user.v1.FindCreditsHistoryByUserIDReply](#schemaapi.user.v1.findcreditshistorybyuseridreply)|

<a id="opIdUser_BindGiftCard"></a>

## POST User_BindGiftCard

POST /user/giftcard/bind

BindGiftCard 绑定礼品卡

> Body 请求参数

```json
{
  "userId": "string",
  "cardId": "string"
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|[api.user.v1.BindGiftCardRequest](#schemaapi.user.v1.bindgiftcardrequest)| 是 |none|

> 返回示例

> 200 Response

```json
{
  "code": 0,
  "message": "string"
}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|OK|[api.user.v1.BindGiftCardReply](#schemaapi.user.v1.bindgiftcardreply)|

<a id="opIdUser_GetUser"></a>

## GET User_GetUser

GET /user/info

获取用户信息

> 返回示例

> 200 Response

```json
{
  "code": 0,
  "data": {
    "id": "string",
    "nickname": "string",
    "tags": [
      "string"
    ],
    "phone": "string",
    "email": "string",
    "credits": "string",
    "giftCards": {
      "property1": {
        "cardId": "string",
        "quantity": "string",
        "expireAt": "string"
      },
      "property2": {
        "cardId": "string",
        "quantity": "string",
        "expireAt": "string"
      }
    }
  }
}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|OK|[api.user.v1.GetUserReply](#schemaapi.user.v1.getuserreply)|

<a id="opIdUser_UpdateUser"></a>

## PUT User_UpdateUser

PUT /user/info

更新用户信息

> Body 请求参数

```json
{
  "nickname": "string"
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|[api.user.v1.UpdateUserRequest](#schemaapi.user.v1.updateuserrequest)| 是 |none|

> 返回示例

> 200 Response

```json
{
  "code": 0
}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|OK|[api.user.v1.UpdateUserReply](#schemaapi.user.v1.updateuserreply)|

<a id="opIdUser_CreateOrder"></a>

## POST User_CreateOrder

POST /user/order

CreateOrder 创建订单

> Body 请求参数

```json
{
  "planKey": "string",
  "userId": "string",
  "payType": 0,
  "prodId": "string",
  "quantity": "string",
  "provider": "string"
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|[api.user.v1.CreateOrderRequest](#schemaapi.user.v1.createorderrequest)| 是 |none|

> 返回示例

> 200 Response

```json
{
  "code": 0,
  "message": "string",
  "data": {
    "payUrl": "string"
  }
}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|OK|[api.user.v1.CreateOrderReply](#schemaapi.user.v1.createorderreply)|

<a id="opIdUser_GetPublicUsers"></a>

## POST User_GetPublicUsers

POST /user/public/batch

批量获取用户公共信息(业务侧调用)

> Body 请求参数

```json
{
  "ids": [
    "string"
  ]
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|[api.user.v1.GetPublicUsersRequest](#schemaapi.user.v1.getpublicusersrequest)| 是 |none|

> 返回示例

> 200 Response

```json
{
  "code": 0,
  "data": [
    {
      "id": "string",
      "nickname": "string",
      "tags": [
        "string"
      ]
    }
  ]
}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|OK|[api.user.v1.GetPublicUsersReply](#schemaapi.user.v1.getpublicusersreply)|

# 数据模型

<h2 id="tocS_api.auth.v1.ApplyCliAuthReply">api.auth.v1.ApplyCliAuthReply</h2>

<a id="schemaapi.auth.v1.applycliauthreply"></a>
<a id="schema_api.auth.v1.ApplyCliAuthReply"></a>
<a id="tocSapi.auth.v1.applycliauthreply"></a>
<a id="tocsapi.auth.v1.applycliauthreply"></a>

```json
{
  "code": 0,
  "message": "string",
  "data": {
    "authUUID": "string"
  }
}

```

ApplyCliAuthReply 申请CLI授权响应

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|code|integer(int32)|false|none||none|
|message|string|false|none||none|
|data|[api.auth.v1.ApplyCliAuthReply_Data](#schemaapi.auth.v1.applycliauthreply_data)|false|none||none|

<h2 id="tocS_api.user.v1.BindGiftCardReply">api.user.v1.BindGiftCardReply</h2>

<a id="schemaapi.user.v1.bindgiftcardreply"></a>
<a id="schema_api.user.v1.BindGiftCardReply"></a>
<a id="tocSapi.user.v1.bindgiftcardreply"></a>
<a id="tocsapi.user.v1.bindgiftcardreply"></a>

```json
{
  "code": 0,
  "message": "string"
}

```

BindGiftCardReply 绑定礼品卡响应

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|code|integer(int32)|false|none||none|
|message|string|false|none||none|

<h2 id="tocS_api.auth.v1.ApplyCliAuthReply_Data">api.auth.v1.ApplyCliAuthReply_Data</h2>

<a id="schemaapi.auth.v1.applycliauthreply_data"></a>
<a id="schema_api.auth.v1.ApplyCliAuthReply_Data"></a>
<a id="tocSapi.auth.v1.applycliauthreply_data"></a>
<a id="tocsapi.auth.v1.applycliauthreply_data"></a>

```json
{
  "authUUID": "string"
}

```

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|authUUID|string|false|none||none|

<h2 id="tocS_api.user.v1.BindGiftCardRequest">api.user.v1.BindGiftCardRequest</h2>

<a id="schemaapi.user.v1.bindgiftcardrequest"></a>
<a id="schema_api.user.v1.BindGiftCardRequest"></a>
<a id="tocSapi.user.v1.bindgiftcardrequest"></a>
<a id="tocsapi.user.v1.bindgiftcardrequest"></a>

```json
{
  "userId": "string",
  "cardId": "string"
}

```

BindGiftCardRequest 绑定礼品卡请求

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|userId|string|false|none||none|
|cardId|string|false|none||none|

<h2 id="tocS_api.auth.v1.ApplyCliAuthRequest">api.auth.v1.ApplyCliAuthRequest</h2>

<a id="schemaapi.auth.v1.applycliauthrequest"></a>
<a id="schema_api.auth.v1.ApplyCliAuthRequest"></a>
<a id="tocSapi.auth.v1.applycliauthrequest"></a>
<a id="tocsapi.auth.v1.applycliauthrequest"></a>

```json
{
  "clientId": "string",
  "clientRandom": "string",
  "deviceId": "string",
  "deviceName": "string"
}

```

ApplyCliAuthRequest 申请CLI授权请求

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|clientId|string|false|none||none|
|clientRandom|string|false|none||none|
|deviceId|string|false|none||none|
|deviceName|string|false|none||none|

<h2 id="tocS_api.user.v1.CostUserCreditsReply">api.user.v1.CostUserCreditsReply</h2>

<a id="schemaapi.user.v1.costusercreditsreply"></a>
<a id="schema_api.user.v1.CostUserCreditsReply"></a>
<a id="tocSapi.user.v1.costusercreditsreply"></a>
<a id="tocsapi.user.v1.costusercreditsreply"></a>

```json
{
  "code": 0,
  "message": "string",
  "data": {
    "credits": "string"
  }
}

```

CostUserCreditsReply 消耗用户积分响应

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|code|integer(int32)|false|none||none|
|message|string|false|none||none|
|data|[api.user.v1.CostUserCreditsReply_Data](#schemaapi.user.v1.costusercreditsreply_data)|false|none||none|

<h2 id="tocS_api.auth.v1.AuthUser">api.auth.v1.AuthUser</h2>

<a id="schemaapi.auth.v1.authuser"></a>
<a id="schema_api.auth.v1.AuthUser"></a>
<a id="tocSapi.auth.v1.authuser"></a>
<a id="tocsapi.auth.v1.authuser"></a>

```json
{
  "userId": "string",
  "token": "string",
  "refreshToken": "string"
}

```

认证用户数据

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|userId|string|false|none||none|
|token|string|false|none||none|
|refreshToken|string|false|none||none|

<h2 id="tocS_api.user.v1.CostUserCreditsReply_Data">api.user.v1.CostUserCreditsReply_Data</h2>

<a id="schemaapi.user.v1.costusercreditsreply_data"></a>
<a id="schema_api.user.v1.CostUserCreditsReply_Data"></a>
<a id="tocSapi.user.v1.costusercreditsreply_data"></a>
<a id="tocsapi.user.v1.costusercreditsreply_data"></a>

```json
{
  "credits": "string"
}

```

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|credits|string|false|none||更新后积分|

<h2 id="tocS_api.auth.v1.CheckCliAuthReply">api.auth.v1.CheckCliAuthReply</h2>

<a id="schemaapi.auth.v1.checkcliauthreply"></a>
<a id="schema_api.auth.v1.CheckCliAuthReply"></a>
<a id="tocSapi.auth.v1.checkcliauthreply"></a>
<a id="tocsapi.auth.v1.checkcliauthreply"></a>

```json
{
  "code": 0,
  "message": "string",
  "data": {
    "userId": "string",
    "token": "string",
    "refreshToken": "string"
  }
}

```

CheckCliAuthReply CLI检查授权状态响应

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|code|integer(int32)|false|none||none|
|message|string|false|none||none|
|data|[api.auth.v1.AuthUser](#schemaapi.auth.v1.authuser)|false|none||认证用户数据|

<h2 id="tocS_api.user.v1.CostUserCreditsRequest">api.user.v1.CostUserCreditsRequest</h2>

<a id="schemaapi.user.v1.costusercreditsrequest"></a>
<a id="schema_api.user.v1.CostUserCreditsRequest"></a>
<a id="tocSapi.user.v1.costusercreditsrequest"></a>
<a id="tocsapi.user.v1.costusercreditsrequest"></a>

```json
{
  "userId": "string",
  "credits": "string",
  "title": "string",
  "prod": "string",
  "providerId": "string",
  "remark": "string",
  "sign": "string",
  "signTime": "string"
}

```

CostUserCreditsRequest 消耗用户积分请求

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|userId|string|false|none||用户id(必填,对应签名规则中的uid)|
|credits|string|false|none||积分变动，正数为增加，负数为减少(必填)|
|title|string|false|none||积分变动标题，用于显示(必填)|
|prod|string|false|none||产品key（消费方定义，不是产品的id）|
|providerId|string|false|none||服务提供方pid(必填,对应签名规则中的provider)|
|remark|string|false|none||积分变动备注|
|sign|string|false|none||签名(必填) -  签名方式见sign.md|
|signTime|string|false|none||签名时的秒级时间戳(必填)|

<h2 id="tocS_api.auth.v1.CheckCliAuthRequest">api.auth.v1.CheckCliAuthRequest</h2>

<a id="schemaapi.auth.v1.checkcliauthrequest"></a>
<a id="schema_api.auth.v1.CheckCliAuthRequest"></a>
<a id="tocSapi.auth.v1.checkcliauthrequest"></a>
<a id="tocsapi.auth.v1.checkcliauthrequest"></a>

```json
{
  "clientId": "string",
  "deviceId": "string"
}

```

CheckCliAuthRequest CLI检查授权状态请求

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|clientId|string|false|none||none|
|deviceId|string|false|none||none|

<h2 id="tocS_api.user.v1.CreateOrderReply">api.user.v1.CreateOrderReply</h2>

<a id="schemaapi.user.v1.createorderreply"></a>
<a id="schema_api.user.v1.CreateOrderReply"></a>
<a id="tocSapi.user.v1.createorderreply"></a>
<a id="tocsapi.user.v1.createorderreply"></a>

```json
{
  "code": 0,
  "message": "string",
  "data": {
    "payUrl": "string"
  }
}

```

CreateOrderReply 创建订单响应(hubbot部分)

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|code|integer(int32)|false|none||none|
|message|string|false|none||none|
|data|[api.user.v1.CreateOrderReply_Data](#schemaapi.user.v1.createorderreply_data)|false|none||none|

<h2 id="tocS_api.auth.v1.CreateAuthReply">api.auth.v1.CreateAuthReply</h2>

<a id="schemaapi.auth.v1.createauthreply"></a>
<a id="schema_api.auth.v1.CreateAuthReply"></a>
<a id="tocSapi.auth.v1.createauthreply"></a>
<a id="tocsapi.auth.v1.createauthreply"></a>

```json
{}

```

### 属性

*None*

<h2 id="tocS_api.user.v1.CreateOrderReply_Data">api.user.v1.CreateOrderReply_Data</h2>

<a id="schemaapi.user.v1.createorderreply_data"></a>
<a id="schema_api.user.v1.CreateOrderReply_Data"></a>
<a id="tocSapi.user.v1.createorderreply_data"></a>
<a id="tocsapi.user.v1.createorderreply_data"></a>

```json
{
  "payUrl": "string"
}

```

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|payUrl|string|false|none||支付链接|

<h2 id="tocS_api.auth.v1.CreateAuthRequest">api.auth.v1.CreateAuthRequest</h2>

<a id="schemaapi.auth.v1.createauthrequest"></a>
<a id="schema_api.auth.v1.CreateAuthRequest"></a>
<a id="tocSapi.auth.v1.createauthrequest"></a>
<a id="tocsapi.auth.v1.createauthrequest"></a>

```json
{
  "uuid": "string",
  "password": "string",
  "phone": "string",
  "email": "string",
  "twoFactorAuth": "string",
  "threeAuthId": "string",
  "threeAuthType": 0
}

```

创建认证（非注册！用于对已有用户添加新的认证方式）

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|uuid|string|false|none||none|
|password|string|false|none||none|
|phone|string|false|none||none|
|email|string|false|none||none|
|twoFactorAuth|string|false|none||none|
|threeAuthId|string|false|none||none|
|threeAuthType|integer(enum)|false|none||none|

<h2 id="tocS_api.user.v1.CreateOrderRequest">api.user.v1.CreateOrderRequest</h2>

<a id="schemaapi.user.v1.createorderrequest"></a>
<a id="schema_api.user.v1.CreateOrderRequest"></a>
<a id="tocSapi.user.v1.createorderrequest"></a>
<a id="tocsapi.user.v1.createorderrequest"></a>

```json
{
  "planKey": "string",
  "userId": "string",
  "payType": 0,
  "prodId": "string",
  "quantity": "string",
  "provider": "string"
}

```

CreateOrderRequest 创建订单请求(hubbot部分)

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|planKey|string|false|none||none|
|userId|string|false|none||none|
|payType|integer(enum)|false|none||支付方式|
|prodId|string|false|none||产品pid|
|quantity|string|false|none||数量|
|provider|string|false|none||供应商id（预留，暂时留空）|

<h2 id="tocS_api.auth.v1.DeleteAuthReply">api.auth.v1.DeleteAuthReply</h2>

<a id="schemaapi.auth.v1.deleteauthreply"></a>
<a id="schema_api.auth.v1.DeleteAuthReply"></a>
<a id="tocSapi.auth.v1.deleteauthreply"></a>
<a id="tocsapi.auth.v1.deleteauthreply"></a>

```json
{}

```

### 属性

*None*

<h2 id="tocS_api.user.v1.CreditsHistory">api.user.v1.CreditsHistory</h2>

<a id="schemaapi.user.v1.creditshistory"></a>
<a id="schema_api.user.v1.CreditsHistory"></a>
<a id="tocSapi.user.v1.creditshistory"></a>
<a id="tocsapi.user.v1.creditshistory"></a>

```json
{
  "id": "string",
  "userId": "string",
  "credits": "string",
  "title": "string",
  "prod": "string",
  "providerId": "string",
  "remark": "string",
  "time": "string"
}

```

CreditsHistory 积分变动历史

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|id|string|false|none||积分变动历史id|
|userId|string|false|none||用户id|
|credits|string|false|none||积分变动(正数为增加，负数为减少)|
|title|string|false|none||积分变动标题|
|prod|string|false|none||产品key（消费方定义，不是产品的id）|
|providerId|string|false|none||服务提供方pid|
|remark|string|false|none||积分变动备注|
|time|string|false|none||时间(秒级时间戳)|

<h2 id="tocS_api.auth.v1.HandleCliAuthReply">api.auth.v1.HandleCliAuthReply</h2>

<a id="schemaapi.auth.v1.handlecliauthreply"></a>
<a id="schema_api.auth.v1.HandleCliAuthReply"></a>
<a id="tocSapi.auth.v1.handlecliauthreply"></a>
<a id="tocsapi.auth.v1.handlecliauthreply"></a>

```json
{
  "code": 0,
  "message": "string"
}

```

HandleCliAuthReply 用户处理授权响应

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|code|integer(int32)|false|none||none|
|message|string|false|none||none|

<h2 id="tocS_api.user.v1.FindCreditsHistoryByUserIDReply">api.user.v1.FindCreditsHistoryByUserIDReply</h2>

<a id="schemaapi.user.v1.findcreditshistorybyuseridreply"></a>
<a id="schema_api.user.v1.FindCreditsHistoryByUserIDReply"></a>
<a id="tocSapi.user.v1.findcreditshistorybyuseridreply"></a>
<a id="tocsapi.user.v1.findcreditshistorybyuseridreply"></a>

```json
{
  "code": 0,
  "message": "string",
  "data": {
    "total": "string",
    "histories": [
      {
        "id": "string",
        "userId": "string",
        "credits": "string",
        "title": "string",
        "prod": "string",
        "providerId": "string",
        "remark": "string",
        "time": "string"
      }
    ]
  }
}

```

FindCreditsHistoryByUserIDReply 获取用户积分变动历史响应

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|code|integer(int32)|false|none||none|
|message|string|false|none||none|
|data|[api.user.v1.FindCreditsHistoryByUserIDReply_Data](#schemaapi.user.v1.findcreditshistorybyuseridreply_data)|false|none||none|

<h2 id="tocS_api.auth.v1.HandleCliAuthRequest">api.auth.v1.HandleCliAuthRequest</h2>

<a id="schemaapi.auth.v1.handlecliauthrequest"></a>
<a id="schema_api.auth.v1.HandleCliAuthRequest"></a>
<a id="tocSapi.auth.v1.handlecliauthrequest"></a>
<a id="tocsapi.auth.v1.handlecliauthrequest"></a>

```json
{
  "clientId": "string",
  "authUUID": "string",
  "deviceId": "string",
  "accept": true,
  "clientRandom": "string"
}

```

HandleCliAuthRequest 用户处理授权请求

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|clientId|string|false|none||none|
|authUUID|string|false|none||none|
|deviceId|string|false|none||none|
|accept|boolean|false|none||none|
|clientRandom|string|false|none||none|

<h2 id="tocS_api.user.v1.FindCreditsHistoryByUserIDReply_Data">api.user.v1.FindCreditsHistoryByUserIDReply_Data</h2>

<a id="schemaapi.user.v1.findcreditshistorybyuseridreply_data"></a>
<a id="schema_api.user.v1.FindCreditsHistoryByUserIDReply_Data"></a>
<a id="tocSapi.user.v1.findcreditshistorybyuseridreply_data"></a>
<a id="tocsapi.user.v1.findcreditshistorybyuseridreply_data"></a>

```json
{
  "total": "string",
  "histories": [
    {
      "id": "string",
      "userId": "string",
      "credits": "string",
      "title": "string",
      "prod": "string",
      "providerId": "string",
      "remark": "string",
      "time": "string"
    }
  ]
}

```

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|total|string|false|none||none|
|histories|[[api.user.v1.CreditsHistory](#schemaapi.user.v1.creditshistory)]|false|none||[CreditsHistory 积分变动历史]|

<h2 id="tocS_api.auth.v1.LogoutReply">api.auth.v1.LogoutReply</h2>

<a id="schemaapi.auth.v1.logoutreply"></a>
<a id="schema_api.auth.v1.LogoutReply"></a>
<a id="tocSapi.auth.v1.logoutreply"></a>
<a id="tocsapi.auth.v1.logoutreply"></a>

```json
{
  "code": 0,
  "message": "string"
}

```

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|code|integer(int32)|false|none||none|
|message|string|false|none||none|

<h2 id="tocS_api.user.v1.GetPublicUsersReply">api.user.v1.GetPublicUsersReply</h2>

<a id="schemaapi.user.v1.getpublicusersreply"></a>
<a id="schema_api.user.v1.GetPublicUsersReply"></a>
<a id="tocSapi.user.v1.getpublicusersreply"></a>
<a id="tocsapi.user.v1.getpublicusersreply"></a>

```json
{
  "code": 0,
  "data": [
    {
      "id": "string",
      "nickname": "string",
      "tags": [
        "string"
      ]
    }
  ]
}

```

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|code|integer(int32)|false|none||none|
|data|[[api.user.v1.PublicUserInfo](#schemaapi.user.v1.publicuserinfo)]|false|none||[用户公开信息]|

<h2 id="tocS_api.auth.v1.LogoutRequest">api.auth.v1.LogoutRequest</h2>

<a id="schemaapi.auth.v1.logoutrequest"></a>
<a id="schema_api.auth.v1.LogoutRequest"></a>
<a id="tocSapi.auth.v1.logoutrequest"></a>
<a id="tocsapi.auth.v1.logoutrequest"></a>

```json
{
  "scene": "string",
  "service": "string",
  "all": true,
  "deviceId": "string"
}

```

登出(默认仅注销当前设备)

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|scene|string|false|none||none|
|service|string|false|none||none|
|all|boolean|false|none||none|
|deviceId|string|false|none||none|

<h2 id="tocS_api.user.v1.GetPublicUsersRequest">api.user.v1.GetPublicUsersRequest</h2>

<a id="schemaapi.user.v1.getpublicusersrequest"></a>
<a id="schema_api.user.v1.GetPublicUsersRequest"></a>
<a id="tocSapi.user.v1.getpublicusersrequest"></a>
<a id="tocsapi.user.v1.getpublicusersrequest"></a>

```json
{
  "ids": [
    "string"
  ]
}

```

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|ids|[string]|false|none||none|

<h2 id="tocS_api.auth.v1.RefreshTokenReply">api.auth.v1.RefreshTokenReply</h2>

<a id="schemaapi.auth.v1.refreshtokenreply"></a>
<a id="schema_api.auth.v1.RefreshTokenReply"></a>
<a id="tocSapi.auth.v1.refreshtokenreply"></a>
<a id="tocsapi.auth.v1.refreshtokenreply"></a>

```json
{
  "code": 0,
  "message": "string",
  "data": {
    "userId": "string",
    "token": "string",
    "refreshToken": "string"
  }
}

```

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|code|integer(int32)|false|none||none|
|message|string|false|none||none|
|data|[api.auth.v1.AuthUser](#schemaapi.auth.v1.authuser)|false|none||认证用户数据|

<h2 id="tocS_api.user.v1.GetUserReply">api.user.v1.GetUserReply</h2>

<a id="schemaapi.user.v1.getuserreply"></a>
<a id="schema_api.user.v1.GetUserReply"></a>
<a id="tocSapi.user.v1.getuserreply"></a>
<a id="tocsapi.user.v1.getuserreply"></a>

```json
{
  "code": 0,
  "data": {
    "id": "string",
    "nickname": "string",
    "tags": [
      "string"
    ],
    "phone": "string",
    "email": "string",
    "credits": "string",
    "giftCards": {
      "property1": {
        "cardId": "string",
        "quantity": "string",
        "expireAt": "string"
      },
      "property2": {
        "cardId": "string",
        "quantity": "string",
        "expireAt": "string"
      }
    }
  }
}

```

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|code|integer(int32)|false|none||none|
|data|[api.user.v1.UserInfo](#schemaapi.user.v1.userinfo)|false|none||用户完整信息|

<h2 id="tocS_api.auth.v1.RefreshTokenRequest">api.auth.v1.RefreshTokenRequest</h2>

<a id="schemaapi.auth.v1.refreshtokenrequest"></a>
<a id="schema_api.auth.v1.RefreshTokenRequest"></a>
<a id="tocSapi.auth.v1.refreshtokenrequest"></a>
<a id="tocsapi.auth.v1.refreshtokenrequest"></a>

```json
{
  "refreshToken": "string",
  "token": "string",
  "scene": "string",
  "service": "string",
  "deviceId": "string"
}

```

刷新token（用于允许范围内刷新过期的Token）

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|refreshToken|string|false|none||刷新令牌|
|token|string|false|none||认证令牌(旧的认证令牌)|
|scene|string|false|none||场景(http请求时不需要传递，会被覆盖)|
|service|string|false|none||服务(客户端调用时不用传递)|
|deviceId|string|false|none||设备ID|

<h2 id="tocS_api.user.v1.PublicUserInfo">api.user.v1.PublicUserInfo</h2>

<a id="schemaapi.user.v1.publicuserinfo"></a>
<a id="schema_api.user.v1.PublicUserInfo"></a>
<a id="tocSapi.user.v1.publicuserinfo"></a>
<a id="tocsapi.user.v1.publicuserinfo"></a>

```json
{
  "id": "string",
  "nickname": "string",
  "tags": [
    "string"
  ]
}

```

用户公开信息

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|id|string|false|none||none|
|nickname|string|false|none||none|
|tags|[string]|false|none||none|

<h2 id="tocS_api.auth.v1.RegisterAuthReply">api.auth.v1.RegisterAuthReply</h2>

<a id="schemaapi.auth.v1.registerauthreply"></a>
<a id="schema_api.auth.v1.RegisterAuthReply"></a>
<a id="tocSapi.auth.v1.registerauthreply"></a>
<a id="tocsapi.auth.v1.registerauthreply"></a>

```json
{
  "code": 0,
  "message": "string",
  "data": {
    "userId": "string",
    "token": "string",
    "refreshToken": "string"
  }
}

```

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|code|integer(int32)|false|none||none|
|message|string|false|none||none|
|data|[api.auth.v1.AuthUser](#schemaapi.auth.v1.authuser)|false|none||message Data {<br /> 	int64 userId = 1; // 用户ID<br /> 	string nickName = 2; // 昵称<br /> }|

<h2 id="tocS_api.user.v1.UpdateUserReply">api.user.v1.UpdateUserReply</h2>

<a id="schemaapi.user.v1.updateuserreply"></a>
<a id="schema_api.user.v1.UpdateUserReply"></a>
<a id="tocSapi.user.v1.updateuserreply"></a>
<a id="tocsapi.user.v1.updateuserreply"></a>

```json
{
  "code": 0
}

```

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|code|integer(int32)|false|none||none|

<h2 id="tocS_api.auth.v1.RegisterAuthRequest">api.auth.v1.RegisterAuthRequest</h2>

<a id="schemaapi.auth.v1.registerauthrequest"></a>
<a id="schema_api.auth.v1.RegisterAuthRequest"></a>
<a id="tocSapi.auth.v1.registerauthrequest"></a>
<a id="tocsapi.auth.v1.registerauthrequest"></a>

```json
{
  "nickName": "string",
  "password": "string",
  "phone": "string",
  "email": "string",
  "twoFactorAuth": "string",
  "threeAuthId": "string",
  "threeAuthType": 0,
  "phoneCode": "string",
  "deviceId": "string"
}

```

注册认证(新用户注册认证，注册后自动登录一次)

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|nickName|string|false|none||none|
|password|string|false|none||none|
|phone|string|false|none||none|
|email|string|false|none||none|
|twoFactorAuth|string|false|none||none|
|threeAuthId|string|false|none||none|
|threeAuthType|integer(enum)|false|none||none|
|phoneCode|string|false|none||none|
|deviceId|string|false|none||none|

<h2 id="tocS_api.user.v1.UpdateUserRequest">api.user.v1.UpdateUserRequest</h2>

<a id="schemaapi.user.v1.updateuserrequest"></a>
<a id="schema_api.user.v1.UpdateUserRequest"></a>
<a id="tocSapi.user.v1.updateuserrequest"></a>
<a id="tocsapi.user.v1.updateuserrequest"></a>

```json
{
  "nickname": "string"
}

```

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|nickname|string|false|none||user id<br /> int64 id = 1 [(validate.rules).int64 = {gt: 0}];<br /> 昵称|

<h2 id="tocS_api.auth.v1.SendPhoneCodeReply">api.auth.v1.SendPhoneCodeReply</h2>

<a id="schemaapi.auth.v1.sendphonecodereply"></a>
<a id="schema_api.auth.v1.SendPhoneCodeReply"></a>
<a id="tocSapi.auth.v1.sendphonecodereply"></a>
<a id="tocsapi.auth.v1.sendphonecodereply"></a>

```json
{
  "code": 0
}

```

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|code|integer(int32)|false|none||none|

<h2 id="tocS_api.user.v1.UserInfo">api.user.v1.UserInfo</h2>

<a id="schemaapi.user.v1.userinfo"></a>
<a id="schema_api.user.v1.UserInfo"></a>
<a id="tocSapi.user.v1.userinfo"></a>
<a id="tocsapi.user.v1.userinfo"></a>

```json
{
  "id": "string",
  "nickname": "string",
  "tags": [
    "string"
  ],
  "phone": "string",
  "email": "string",
  "credits": "string",
  "giftCards": {
    "property1": {
      "cardId": "string",
      "quantity": "string",
      "expireAt": "string"
    },
    "property2": {
      "cardId": "string",
      "quantity": "string",
      "expireAt": "string"
    }
  }
}

```

用户完整信息

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|id|string|false|none||none|
|nickname|string|false|none||none|
|tags|[string]|false|none||none|
|phone|string|false|none||none|
|email|string|false|none||none|
|credits|string|false|none||none|
|giftCards|object|false|none||none|
|» **additionalProperties**|[api.user.v1.UserInfo_GiftCard](#schemaapi.user.v1.userinfo_giftcard)|false|none||none|

<h2 id="tocS_api.auth.v1.SendPhoneCodeRequest">api.auth.v1.SendPhoneCodeRequest</h2>

<a id="schemaapi.auth.v1.sendphonecoderequest"></a>
<a id="schema_api.auth.v1.SendPhoneCodeRequest"></a>
<a id="tocSapi.auth.v1.sendphonecoderequest"></a>
<a id="tocsapi.auth.v1.sendphonecoderequest"></a>

```json
{
  "phone": "string"
}

```

获取手机验证码

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|phone|string|false|none||none|

<h2 id="tocS_api.user.v1.UserInfo_GiftCard">api.user.v1.UserInfo_GiftCard</h2>

<a id="schemaapi.user.v1.userinfo_giftcard"></a>
<a id="schema_api.user.v1.UserInfo_GiftCard"></a>
<a id="tocSapi.user.v1.userinfo_giftcard"></a>
<a id="tocsapi.user.v1.userinfo_giftcard"></a>

```json
{
  "cardId": "string",
  "quantity": "string",
  "expireAt": "string"
}

```

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|cardId|string|false|none||礼品卡id|
|quantity|string|false|none||剩余资源数量|
|expireAt|string|false|none||过期时间(格式化后时间字符串)|

<h2 id="tocS_api.auth.v1.UpdateAuthReply">api.auth.v1.UpdateAuthReply</h2>

<a id="schemaapi.auth.v1.updateauthreply"></a>
<a id="schema_api.auth.v1.UpdateAuthReply"></a>
<a id="tocSapi.auth.v1.updateauthreply"></a>
<a id="tocsapi.auth.v1.updateauthreply"></a>

```json
{}

```

### 属性

*None*

<h2 id="tocS_api.auth.v1.UpdateAuthRequest">api.auth.v1.UpdateAuthRequest</h2>

<a id="schemaapi.auth.v1.updateauthrequest"></a>
<a id="schema_api.auth.v1.UpdateAuthRequest"></a>
<a id="tocSapi.auth.v1.updateauthrequest"></a>
<a id="tocsapi.auth.v1.updateauthrequest"></a>

```json
{}

```

### 属性

*None*

<h2 id="tocS_api.auth.v1.UpdateSecurityInfoReply">api.auth.v1.UpdateSecurityInfoReply</h2>

<a id="schemaapi.auth.v1.updatesecurityinforeply"></a>
<a id="schema_api.auth.v1.UpdateSecurityInfoReply"></a>
<a id="tocSapi.auth.v1.updatesecurityinforeply"></a>
<a id="tocsapi.auth.v1.updatesecurityinforeply"></a>

```json
{
  "code": 0,
  "message": "string"
}

```

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|code|integer(int32)|false|none||none|
|message|string|false|none||none|

<h2 id="tocS_api.auth.v1.UpdateSecurityInfoRequest">api.auth.v1.UpdateSecurityInfoRequest</h2>

<a id="schemaapi.auth.v1.updatesecurityinforequest"></a>
<a id="schema_api.auth.v1.UpdateSecurityInfoRequest"></a>
<a id="tocSapi.auth.v1.updatesecurityinforequest"></a>
<a id="tocsapi.auth.v1.updatesecurityinforequest"></a>

```json
{
  "userId": "string",
  "password": "string",
  "secPhone": "string",
  "secMail": "string",
  "phoneCode": "string",
  "oldPhoneCode": "string",
  "deviceId": "string"
}

```

更新安全信息

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|userId|string|false|none||none|
|password|string|false|none||新密码(一次只能修改一个字段)|
|secPhone|string|false|none||新手机号带国别码，例如：+8613800000000(一次只能修改一个字段)|
|secMail|string|false|none||新邮箱(一次只能修改一个字段)|
|phoneCode|string|false|none||新手机号验证码|
|oldPhoneCode|string|false|none||旧手机号验证码|
|deviceId|string|false|none||设备ID|

<h2 id="tocS_api.auth.v1.ValidateAuthByPasswordReply">api.auth.v1.ValidateAuthByPasswordReply</h2>

<a id="schemaapi.auth.v1.validateauthbypasswordreply"></a>
<a id="schema_api.auth.v1.ValidateAuthByPasswordReply"></a>
<a id="tocSapi.auth.v1.validateauthbypasswordreply"></a>
<a id="tocsapi.auth.v1.validateauthbypasswordreply"></a>

```json
{
  "code": 0,
  "message": "string",
  "data": {
    "userId": "string",
    "token": "string",
    "refreshToken": "string"
  }
}

```

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|code|integer(int32)|false|none||none|
|message|string|false|none||none|
|data|[api.auth.v1.AuthUser](#schemaapi.auth.v1.authuser)|false|none||message Data {<br /> 	int64 userId = 1; // 用户ID<br /> 	string nickName = 2; // 昵称<br /> 	string token = 3; // 认证令牌<br /> 	string refreshToken = 4; // 刷新令牌<br /> }|

<h2 id="tocS_api.auth.v1.ValidateAuthByPasswordRequest">api.auth.v1.ValidateAuthByPasswordRequest</h2>

<a id="schemaapi.auth.v1.validateauthbypasswordrequest"></a>
<a id="schema_api.auth.v1.ValidateAuthByPasswordRequest"></a>
<a id="tocSapi.auth.v1.validateauthbypasswordrequest"></a>
<a id="tocsapi.auth.v1.validateauthbypasswordrequest"></a>

```json
{
  "phone": "string",
  "password": "string",
  "twoFactorAuth": "string",
  "scene": "string",
  "deviceId": "string"
}

```

验证密码认证（用于对已有用户已有密码认证方式验证）

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|phone|string|false|none||登录手机号带国别码，例如：+8613800000000|
|password|string|false|none||密码|
|twoFactorAuth|string|false|none||二因子认证|
|scene|string|false|none||场景|
|deviceId|string|false|none||设备ID|

<h2 id="tocS_api.auth.v1.ValidateAuthByPhoneCodeReply">api.auth.v1.ValidateAuthByPhoneCodeReply</h2>

<a id="schemaapi.auth.v1.validateauthbyphonecodereply"></a>
<a id="schema_api.auth.v1.ValidateAuthByPhoneCodeReply"></a>
<a id="tocSapi.auth.v1.validateauthbyphonecodereply"></a>
<a id="tocsapi.auth.v1.validateauthbyphonecodereply"></a>

```json
{
  "code": 0,
  "message": "string",
  "data": {
    "userId": "string",
    "token": "string",
    "refreshToken": "string"
  }
}

```

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|code|integer(int32)|false|none||none|
|message|string|false|none||none|
|data|[api.auth.v1.AuthUser](#schemaapi.auth.v1.authuser)|false|none||认证用户数据|

<h2 id="tocS_api.auth.v1.ValidateAuthByPhoneCodeRequest">api.auth.v1.ValidateAuthByPhoneCodeRequest</h2>

<a id="schemaapi.auth.v1.validateauthbyphonecoderequest"></a>
<a id="schema_api.auth.v1.ValidateAuthByPhoneCodeRequest"></a>
<a id="tocSapi.auth.v1.validateauthbyphonecoderequest"></a>
<a id="tocsapi.auth.v1.validateauthbyphonecoderequest"></a>

```json
{
  "userId": "string",
  "phone": "string",
  "code": "string",
  "scene": "string",
  "deviceId": "string"
}

```

验证手机号验证码认证（用于对已有用户已有手机号验证码认证方式验证）

### 属性

|名称|类型|必选|约束|中文名|说明|
|---|---|---|---|---|---|
|userId|string|false|none||用户ID(用于登录后独立验证手机号场景)|
|phone|string|false|none||登录手机号带国别码，例如：+8613800000000|
|code|string|false|none||手机号验证码|
|scene|string|false|none||场景|
|deviceId|string|false|none||设备ID|

