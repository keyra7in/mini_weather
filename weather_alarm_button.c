#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>

#define BUF_SIZE 100
#define NAME_SIZE 20
#define ARR_CNT 5

void * send_msg(void * arg);
void * recv_msg(void * arg);
void error_handling(char * msg);
void play_voice();
int is_within_range();

char name[NAME_SIZE] = "[Default]";
char msg[BUF_SIZE];
char set_range[30] = ""; // "HH:MM~HH:MM" 저장용
int is_played = 0;

int main(int argc, char *argv[])
{
    int sock;
    struct sockaddr_in serv_addr;
    pthread_t snd_thread, rcv_thread;
    void * thread_return;

    if(argc != 4) {
        printf("Usage : %s <IP> <port> <name>\n", argv[0]);
        exit(1);
    }

    sprintf(name, "%s", argv[3]);

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");

    sprintf(msg, "[%s:PASSWD]", name);
    write(sock, msg, strlen(msg));

    // 스레드 생성
    pthread_create(&rcv_thread, NULL, recv_msg, (void *)&sock);
    pthread_create(&snd_thread, NULL, send_msg, (void *)&sock);


    pthread_join(snd_thread, &thread_return);
    close(sock);
    return 0;
}

// 현재 시간이 설정된 알람 시간대 안에 있는지 확인하는 함수
int is_within_range() {
    if (strlen(set_range) < 10) return 0;

    time_t timer = time(NULL);
    struct tm *t = localtime(&timer);
    int cur_min = t->tm_hour * 60 + t->tm_min;

    int sh, sm, eh, em;
    if (sscanf(set_range, "%d.%d~%d.%d", &sh, &sm, &eh, &em) == 4) {
        int start_min = sh * 60 + sm;
        int end_min = eh * 60 + em;
        if (cur_min >= start_min && cur_min <= end_min) {
            return 1;
        }
    }
    return 0;
}


void play_voice() {
    printf("\n[ALARM] 시간 조건 충족! 음성 안내를 시작합니다.\n");
    
    system("python3 -c \"\n"
           "import requests, os\n"
           "from gtts import gTTS\n"
           "try:\n"
           "    res = requests.get('http://api.openweathermap.org/data/2.5/forecast?q=Seoul&appid=799a82dcfc4ea4e4109e504c0b189d9e&units=metric&lang=kr', timeout=5).json()\n"
           "    f_list = res['list'][:8]\n"
           "    curr_t = round(f_list[0]['main']['temp'])\n"
           "    max_t = round(max(item['main']['temp_max'] for item in f_list))\n"
           "    min_t = round(min(item['main']['temp_min'] for item in f_list))\n"
           "    will_rain = any('rain' in item for item in f_list)\n"
           "    msg = f'현재 서울 기온은 {curr_t}도입니다. 오늘 최고 기온은 {max_t}도, 최저 기온은 {min_t}도입니다. '\n"
           "    if will_rain:\n"
           "        msg += '예보 중에 비 소식이 있으니 우산을 꼭 챙기세요.'\n"
           "    else:\n"
           "        msg += '오늘은 비 예보가 없어 야외 활동하기 좋겠습니다.'\n"
           "    gTTS(text=msg, lang='ko').save('v.mp3')\n"
           "    os.system('mpg123 v.mp3 > /dev/null 2>&1')\n"
           "    os.remove('v.mp3')\n"
           "except Exception as e: print(f'Error: {e}')\n"
           "\"");
}

void * send_msg(void * arg)
{
    int *sock = (int *)arg;
    int ret;
    fd_set initset, newset;
    struct timeval tv;
    char name_msg[NAME_SIZE + BUF_SIZE + 2];

    FD_ZERO(&initset);
    FD_SET(STDIN_FILENO, &initset);

    while(1) {
        memset(msg, 0, sizeof(msg));
        name_msg[0] = '\0';
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        newset = initset;
        ret = select(STDIN_FILENO + 1, &newset, NULL, NULL, &tv);

        if(FD_ISSET(STDIN_FILENO, &newset)) {
            fgets(msg, BUF_SIZE, stdin);
            if(!strncmp(msg, "quit\n", 5)) {
                *sock = -1;
                return NULL;
            }
            if(msg[0] != '[') {
                strcat(name_msg, "[ALLMSG]");
                strcat(name_msg, msg);
            } else {
                strcpy(name_msg, msg);
            }
            if(write(*sock, name_msg, strlen(name_msg)) <= 0) {
                *sock = -1;
                return NULL;
            }
        }
        if(ret == 0 && *sock == -1) return NULL;
    }
}

void * recv_msg(void * arg)
{
    int * sock = (int *)arg;    
    char name_msg[NAME_SIZE + BUF_SIZE + 1];
    int str_len;

    while(1) {
        memset(name_msg, 0x0, sizeof(name_msg));
        str_len = read(*sock, name_msg, NAME_SIZE + BUF_SIZE);
        if(str_len <= 0) {
            *sock = -1;
            return NULL;
        }
        name_msg[str_len] = 0;
        fputs(name_msg, stdout);

        // 시간대 설정 수신 ([SENDER]08:00~09:00)
        if (strchr(name_msg, '~') != NULL) {
            char *start = strchr(name_msg, ']') + 1;
            strcpy(set_range, start);
            is_played = 0;
            printf("\n[INFO] 알람 설정됨: %s", set_range);
        }

        // 2. 아두이노의 버튼 눌림 메시지 수신 (아두이노 코드의 [HM_CON]GASON 대응)
        if (strstr(name_msg, "LEDON") != NULL) {
            printf("\n[EVENT] 버튼 눌림 감지!");
            if (is_within_range()) {
                printf(" -> 시간 조건 충족! 날씨 예보를 시작합니다.\n");
                play_voice();
            } else {
                printf(" -> 설정된 시간 범위가 아닙니다.\n");
            }
        }
    }
}

void error_handling(char * msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}