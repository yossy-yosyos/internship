#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <wiringPi.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_PINS 100  // 最大ピン数

// ピンとその初期状態を保持する構造体
typedef struct {
    int pin;
    int state;  // 1: HIGH, 0: LOW
} PinConfig;

PinConfig pin_configs[MAX_PINS];
int num_pins = 0;  // 読み込んだピンの数

// GPIOピンの制御
void control_gpio(int pin, const char *command) {
    if (strcmp(command, "ON") == 0) {
        digitalWrite(pin, HIGH);
        printf("GPIO PIN %d is ON\n", pin);
    } else if (strcmp(command, "OFF") == 0) {
        digitalWrite(pin, LOW);
        printf("GPIO PIN %d is OFF\n", pin);
    }
}

// ピン番号が有効か確認する関数
int is_valid_pin(int pin) {
    for (int i = 0; i < num_pins; i++) {
        if (pin_configs[i].pin == pin) {
            return 1;  // ピンが有効
        }
    }
    return 0;  // ピンが無効
}

// config.txt からピンと初期状態を読み込む関数
void read_config(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open config file");
        exit(EXIT_FAILURE);
    }

    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), file)) {
        int pin;
        char state[10];

        // ファイルからピンと初期状態を読み取る
        if (sscanf(line, "PIN %d %s", &pin, state) == 2) {
            pin_configs[num_pins].pin = pin;
            pin_configs[num_pins].state = (strcmp(state, "HIGH") == 0) ? 1 : 0;
            num_pins++;
        }
    }
    fclose(file);

    // ピンをセットアップ
    for (int i = 0; i < num_pins; i++) {
        pinMode(pin_configs[i].pin, OUTPUT);
        digitalWrite(pin_configs[i].pin, pin_configs[i].state);
        printf("PIN %d initialized to %s\n", pin_configs[i].pin, 
            pin_configs[i].state == 1 ? "HIGH" : "LOW");
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <config_file>\n", argv[0]);
        return -1;
    }

    // WiringPiのセットアップ
    if (wiringPiSetupGpio() == -1) {
        printf("WiringPi setup failed\n");
        return -1;
    }

    // config.txt の読み込み
    read_config(argv[1]);

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};
    char response[BUFFER_SIZE] = {0};

    // ソケットの作成
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // アドレスとポートの設定
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // ソケットにアドレスとポートをバインド
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // クライアントからの接続を待つ
    if (listen(server_fd, 1) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Waiting for a connection...\n");

    while (1) {
        // 接続を受け付ける
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("accept failed");
            close(server_fd);
            exit(EXIT_FAILURE);
        }

        // データを受信
        read(new_socket, buffer, BUFFER_SIZE);
        printf("Received: %s\n", buffer);

        // PIN番号とコマンドを分割して取得
        int pin;
        char command[10];
        sscanf(buffer, "PIN %d %s", &pin, command);
        printf("Parsed PIN: %d, Command: %s\n", pin, command);  // デバッグ用出力

        // ピンが有効かどうか確認
        if (is_valid_pin(pin)) {
            control_gpio(pin, command);  // GPIOピンの制御

            // 成功時の応答
            snprintf(response, sizeof(response), "ACK: PIN %d = %s\n", pin, command);
        } else {
            // エラーメッセージを作成
            snprintf(response, sizeof(response), "NACK: Invalid PIN or Command\n");
        }

        // クライアントに応答を送信
        send(new_socket, response, strlen(response), 0);
        printf("Response sent: %s\n", response);

        // ソケットを閉じる
        close(new_socket);
    }

    return 0;
}
