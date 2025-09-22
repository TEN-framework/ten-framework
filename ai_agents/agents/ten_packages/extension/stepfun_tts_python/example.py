from pathlib import Path
from openai import OpenAI
 
speech_file_path = Path(__file__).parent / "step-tts.mp3"
 
client = OpenAI(
  api_key="STEP_API_KEY",
  base_url="https://api.stepfun.com/v1"
)
response = client.audio.speech.create(
  model="step-tts-mini",
  voice="cixingnansheng",
  input="智能阶跃，十倍每个人的可能.",
  extra_body={
    "volume":1.0 ,# volume 在拓展参数里
    "voice_label":{
      "language": "粤语",  # 可选：语言
      "emotion": "高兴",   # 可选：情感
      "style": "慢速"      # 可选：说话语速
    }
  }
)
response.stream_to_file(speech_file_path)
 
