model string required
需要使用的模型名称，当前支持 step-tts-mini 和 step-tts-vivid。

input string required
要生成的文本，最大长度为 1000 个字符

voice string required
生成时使用的音色信息，支持官方音色和开发者自生成音色。

response_format string optional
返回的音频格式，支持 wav,mp3,flac,opus. 默认为 mp3 格式

speed float optional
语速，取值范围为 0.5~2，默认值 1.0。0.5 表示 0.5 倍速。

volume float optional
音频，取值范围为 0.1~2.0，默认值 1.0。0.1 表示缩小至 10% 音量；2.0 表示扩大至 200% 音量。

voice_label object optional
音色标签，使用自定义音色时需要传入。language、emotion 和 style 三个自动同时只能有一个字段有值，暂不支持多个组合。

language string optional
语言，支持粤语、四川话、日语 三个选项。
emotion string optional
情感，支持 高兴、非常高兴、生气、非常生气、悲伤、撒娇 六个选项；
style string optional
说话语速，支持 慢速、极慢、快速、极快 四个选项。
sample_rate integer optional
采样率，支持 8000、16000、22050、24000 四个选项。默认值为 24000。采样率越高，音质越好，但文件体积也会更大。