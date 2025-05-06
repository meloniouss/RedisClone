import socket
import threading
import time

SERVER_HOST = "127.0.0.1"
SERVER_PORT = 6379
NUM_CLIENTS = 3


def client_task(id):
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((SERVER_HOST, SERVER_PORT))
        for i in range(3):
            message = "PING"
            sent = sock.send(message.encode())

            # Log how many bytes were sent to check if the message was fully sent
            if sent != 4:
                print(f"[Client {id}] Warning: only sent {sent} bytes")

            # Get the current time and format it
            current_time = time.strftime("%H:%M:%S", time.localtime())
            print(f"[{current_time}] [Client {id}] Sent: {message}")

            response = sock.recv(1024).decode()

            # Log the response with timestamp
            current_time = time.strftime("%H:%M:%S", time.localtime())
            print(f"[{current_time}] [Client {id}] Received: {response}")

            time.sleep(1)

        sock.close()
        current_time = time.strftime("%H:%M:%S", time.localtime())
        print(f"[{current_time}] [Client {id}] Disconnected")
    except Exception as e:
        current_time = time.strftime("%H:%M:%S", time.localtime())
        print(f"[{current_time}] [Client {id}] Error: {e}")


threads = []

for i in range(NUM_CLIENTS):
    t = threading.Thread(target=client_task, args=(i,))
    threads.append(t)
    t.start()

for t in threads:
    t.join()

print("All clients finished.")
