import socket
import threading
import sys
import time

START_PORT = 5000
NUM_PORTS = 3

def handle_client(conn, addr, port):
    print(f"[+] Connection on TCP port {port} from {addr}")
    try:
        while True:
            data = conn.recv(1024)
            if not data:
                break
            print(f"[{port} TCP] Received: {data!r}")
            reply = f"ACK from TCP port {port}: got {len(data)} bytes\n".encode()
            conn.sendall(reply)
    except Exception as e:
        print(f"[{port} TCP] Error: {e}")
    finally:
        conn.close()

def listen_on_port(port):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("0.0.0.0", port))
    s.listen(1)
    print(f"Listening on TCP port {port}")
    while True:
        conn, addr = s.accept()
        threading.Thread(target=handle_client, args=(conn, addr, port), daemon=True).start()

def listen_on_port_udp(port):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("0.0.0.0", port))
    print(f"Listening on UDP port {port}")
    try:
        while True:
            data, addr = s.recvfrom(1024)
            print(f"[{port} UDP] Received from {addr}: {data!r}")
            reply = f"ACK from UDP port {port}: got {len(data)} bytes\n".encode()
            s.sendto(reply, addr)
    except Exception as e:
        print(f"[{port} UDP] Error: {e}")
    finally:
        s.close()

if __name__ == "__main__":
    threads = []
    for i in range(NUM_PORTS):
        port = START_PORT + i
        t_tcp = threading.Thread(target=listen_on_port, args=(port,), daemon=True)
        t_tcp.start()
        threads.append(t_tcp)
        
        t_udp = threading.Thread(target=listen_on_port_udp, args=(port,), daemon=True)
        t_udp.start()
        threads.append(t_udp)
        
    print("Servers running. Press Ctrl+C to stop.")
    try:
        threading.Event().wait()
    except KeyboardInterrupt:
        sys.exit(0)
