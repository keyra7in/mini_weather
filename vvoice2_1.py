import requests
from gtts import gTTS
import os
import time

# --- 설정 ---
API_KEY = "799a82dcfc4ea4e4109e504c0b189d9e"
CITY = "Seoul"

def get_weather_text():
    # 5일/3시간 예보 API 주소로 변경 (forecast)
    url = f"http://api.openweathermap.org/data/2.5/forecast?q={CITY}&appid={API_KEY}&units=metric&lang=kr"

    try:
        res = requests.get(url)
        data = res.json()

        if res.status_code != 200:
            return f"서버 에러 발생: {data.get('message', '알 수 없는 이유')}"

        # 1. 향후 24시간(3시간 간격 * 8개) 데이터 가져오기
        forecast_list = data['list'][:8]

        # 2. 최고/최저 기온 분석
        temps = [item['main']['temp'] for item in forecast_list]
        max_temp = round(max(temps))
        min_temp = round(min(temps))

        # 3. 비 예보 확인 (8개 데이터 중 하나라도 비 소식이 있는지)
        will_rain = False
        rain_desc = ""
        for item in forecast_list:
            if "비" in item['weather'][0]['description']:
                will_rain = True
                rain_desc = item['weather'][0]['description']
                break

        # 4. 음성 문장 조합
        current_desc = forecast_list[0]['weather'][0]['description']
        current_temp = round(forecast_list[0]['main']['temp'])

        text = f"현재 {CITY}는 {current_desc}이며 기온은 {current_temp}도입니다. "
        text += f"오늘 최고 기온은 {max_temp}도, 최저 기온은 {min_temp}도입니다. "

        if will_rain:
            text += f"예보 중에 {rain_desc} 소식이 있으니 우산을 꼭 챙기세요."
        else:
            text += "오늘은 비 예보가 없어 야외 활동하기 좋겠습니다."

        return text

    except Exception as e:
        print(f"에러 상세 내용: {e}")
        return "날씨 예보 데이터를 분석하는 도중 오류가 발생했습니다."

def play_voice(text):
    filename = "weather_voice.mp3"
    try:
        tts = gTTS(text=text, lang='ko')
        tts.save(filename)
        os.system(f"mpg123 {filename}")
    finally:
        if os.path.exists(filename):
            os.remove(filename)

if __name__ == "__main__":
    weather_msg = get_weather_text()
    print(f"🔊 분석된 내용: {weather_msg}")
    play_voice(weather_msg)
