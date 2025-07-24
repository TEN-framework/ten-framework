def encrypt(key: str) -> str:
    step = int(len(key) / 5)
    if step > 0:
        if step > 5:
            step = 5
        prefix = key[:step]
        suffix = key[-step:]

        return f"{prefix}...{suffix}"
    else:
        return key

def adjust_volume_pcm_s16le(pcm_data: bytearray, volume_factor: float) -> bytearray:
    """
    调整 PCM 数据的音量。

    参数:
    pcm_data (bytearray): 输入的 PCM 数据。
    volume_factor (float): 音量因子。1.0 表示不变，0.5 表示减半音量，1.5 表示增加50%的音量。

    返回:
    bytearray: 调整音量后的 PCM 数据。
    """

    import numpy as np

    # 将字节数据转换为 numpy 数组
    pcm_array = np.frombuffer(pcm_data, dtype=np.int16)

    # 确保音频数据是浮点型，以避免溢出
    pcm_array = pcm_array.astype(np.float32)

    # 调整音量
    pcm_array *= volume_factor

    # 确保数据在适当的范围内
    pcm_array = np.clip(pcm_array, -32768, 32767)

    # 转换回原始数据类型
    pcm_array = pcm_array.astype(np.int16)

    # 将 numpy 数组转换回字节数据
    adjusted_pcm_data = pcm_array.tobytes()

    return bytearray(adjusted_pcm_data)
