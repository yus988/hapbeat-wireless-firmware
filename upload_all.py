# PlatformIO CLI で実行すること！pwshではエラーが出る。

import subprocess
import serial.tools.list_ports
import os
import time

# VSCodeのUIで選択中の環境を取得（デフォルトは "DuoWL_V3-ESPNOW"）
# TARGET_ENV = os.getenv('PIOENV', 'DuoWL_V3-ESPNOW_JUDO0806')
# TARGET_ENV = os.getenv('PIOENV', 'DuoWL_V2-ESPNOW_JUDO0806')
# TARGET_ENV = os.getenv('PIOENV', 'BandWL_V3_GEN_ESPNOW')
# TARGET_ENV = os.getenv('PIOENV', 'DuoWL_V3_DEMO_ESPNOW')

TARGET_ENV = os.getenv("PIOENV", "DuoWL_V3_NECKDEAFLYMPICESPNOW")
# TARGET_ENV = os.getenv("PIOENV", "DuoWL_V2_NECKDEAFLYMPICESPNOW")

# === Upload mode switch ===
# 下の２行のうち、必要な方のコメントを外してください。
PROGRAM_ONLY = False  # True: プログラムのみ書き換え（音声データ保持）
# PROGRAM_ONLY = True

# 除外したいポートデバイスを指定（例: ["COM3", "COM5"]）
# EXCLUDED_PORTS = []
EXCLUDED_PORTS = ["COM7", "COM35"]


# TARGET_ENV = os.getenv('PIOENV', 'DuoWL_V3_GEN_ESPNOW')
# TARGET_ENV = os.getenv('PIOENV', 'DuoWL_V2_GEN_ESPNOW')


# ESP32のポートを検出する関数
def find_esp32_ports():
    ports = list(serial.tools.list_ports.comports())
    esp32_ports = []

    for port in ports:
        if port.device in EXCLUDED_PORTS:
            continue
        if "USB" in port.description:
            esp32_ports.append(port.device)
        # 以下の条件を有効化すると CP210/CH910 系USBシリアルも対象に含められます。
        # elif "CP210" in port.description or "CH910" in port.description:
        #     esp32_ports.append(port.device)

    return esp32_ports


# 各デバイスにプログラムとファイルシステムを順次アップロードする関数
def upload_program_and_fs():
    devices = find_esp32_ports()

    if not devices:
        print("ESP32デバイスが見つかりませんでした。")
        return

    print(f"{len(devices)}台のESP32デバイスが見つかりました。")

    for device in devices:
        print(
            f"\n{device} に {TARGET_ENV} 環境でアップロードを開始します... (mode: {'program-only' if PROGRAM_ONLY else 'full'})"
        )

        if not PROGRAM_ONLY:
            # 0️⃣ フラッシュの消去
            try:
                print(f"{device} のフラッシュを消去しています...")
                subprocess.run(
                    [
                        "pio",
                        "run",
                        "-e",
                        TARGET_ENV,
                        "-t",
                        "erase",
                        "--upload-port",
                        device,
                    ],
                    check=True,
                )
                print(f"{device} のフラッシュ消去が完了しました。")
            except subprocess.CalledProcessError:
                print(f"{device} のフラッシュ消去に失敗しました。")
                continue  # 消去が失敗した場合、次のデバイスへ

            time.sleep(2)  # デバイスのリセット待ち
        else:
            print("フラッシュ消去をスキップします（program-only モード）。")

        # 1️⃣ ファームウェア（プログラム）のアップロード
        try:
            subprocess.run(
                [
                    "pio",
                    "run",
                    "-e",
                    TARGET_ENV,
                    "-t",
                    "upload",
                    "--upload-port",
                    device,
                ],
                check=True,
            )
            print(f"{device} へのファームウェアアップロードが完了しました。")
        except subprocess.CalledProcessError:
            print(f"{device} へのファームウェアアップロードに失敗しました。")
            continue  # ファームウェアが失敗した場合、次のデバイスへ

        time.sleep(2)  # デバイスのリセット待ち

        if not PROGRAM_ONLY:
            # 2️⃣ ファイルシステム（LittleFS/SPIFFS）のアップロード
            try:
                subprocess.run(
                    [
                        "pio",
                        "run",
                        "-e",
                        TARGET_ENV,
                        "-t",
                        "uploadfs",
                        "--upload-port",
                        device,
                    ],
                    check=True,
                )
                print(f"{device} へのファイルシステムアップロードが完了しました。")
            except subprocess.CalledProcessError:
                print(f"{device} へのファイルシステムアップロードに失敗しました。")
        else:
            print(
                "ファイルシステムアップロードをスキップします（program-only モード）。"
            )

        time.sleep(2)  # 次のデバイスへの切り替え待機


if __name__ == "__main__":
    upload_program_and_fs()
