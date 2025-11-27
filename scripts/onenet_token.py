import time
import base64
import hmac
import hashlib
from urllib.parse import quote

def token(res, key, exp_time_seconds=31536000):
    """
    生成 OneNET Token
    :param res: 资源 (products/{pid}/devices/{device_name})
    :param key: 产品的 AccessKey 或设备的 AccessKey (Base64 编码前)
    :param exp_time_seconds: 过期时间，默认 1 年
    :return: token 字符串
    """
    try:
        # 1. 计算过期时间戳
        et = str(int(time.time()) + exp_time_seconds)
        
        # 2. 解码 Key (OneNET 控制台提供的 Key 是 Base64 编码的)
        key_bytes = base64.b64decode(key)
        
        # 3. 构造签名原串
        # 格式: et + '\n' + method + '\n' + res + '\n' + version
        # method 固定为 md5, sha1, sha256 等，这里用 sha1
        # version 固定为 2018-10-31
        sign_method = 'sha1'
        version = '2018-10-31'
        to_sign = (et + '\n' + sign_method + '\n' + res + '\n' + version).encode('utf-8')
        
        # 4. 计算签名
        sign = base64.b64encode(hmac.new(key_bytes, to_sign, hashlib.sha1).digest()).decode('utf-8')
        
        # 5. 拼接 Token
        # res 和 sign 需要 URL Encode
        token = 'version={}&res={}&et={}&method={}&sign={}'.format(
            version, quote(res, safe=''), et, sign_method, quote(sign, safe='')
        )
        return token
        
    except Exception as e:
        return str(e)

if __name__ == '__main__':
    print("=== OneNET Token 生成工具 ===")
    pid = input("请输入产品 ID (Product ID): ").strip()
    device_name = input("请输入设备名称 (Device Name): ").strip()
    access_key = input("请输入设备密钥 (Device Access Key): ").strip()
    
    res = "products/{}/devices/{}".format(pid, device_name)
    
    print("\n正在生成 Token...")
    t = token(res, access_key)
    print("\n生成的 Token (请复制到代码 ONENET_TOKEN 宏定义中):")
    print("-" * 60)
    print(t)
    print("-" * 60)
