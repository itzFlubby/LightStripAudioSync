import sys
import asyncio
import websockets

ssl_context = None
if len(sys.argv) == 3:
    import ssl

    ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ssl_context.load_cert_chain(sys.argv[1], sys.argv[2])

BUFFER_SIZE = 1024

HOST = "127.0.0.1"
TCP_PORT = 3334
WS_PORT = 3335
WSS_PORT = 3336

ws_clients = set()


async def ws_pipe_tcp():
    while True:
        try:
            print(f"[INFO] Starting data pipe from {HOST}:{TCP_PORT}...")
            tcp_reader, _ = await asyncio.open_connection(HOST, TCP_PORT)
            print(f"[++++] Pipe connected!")

            while True:
                data = await tcp_reader.read(BUFFER_SIZE)

                if data[1] == 0x03:
                    print("[XXXX] Exiting...")
                    return

                payload = data[3 : (len(data) - 1)]

                dead_ws_clients = set()
                for ws_client in ws_clients:
                    try:
                        await ws_client.send(payload)
                    except:
                        dead_ws_clients.add(ws_client)

                ws_clients.difference_update(dead_ws_clients)
        except Exception as e:
            print(f"[CRIT] An error occured: {e}!")

        await asyncio.sleep(5)


async def ws_handler(websocket):
    ws_clients.add(websocket)
    print(
        f"[++++] Registered WebSocket client:\n\tIP: {websocket.remote_address[0]}, Port: {websocket.remote_address[1]}"
    )
    try:
        await websocket.wait_closed()
    finally:
        ws_clients.remove(websocket)
        print(
            f"[----] Unregistered WebSocket client:\n\tIP: {websocket.remote_address[0]}, Port: {websocket.remote_address[1]}"
        )


async def main():
    asyncio.create_task(ws_pipe_tcp())

    await websockets.serve(ws_handler, "0.0.0.0", WS_PORT)
    print(f"[++++] Started WebSocket on port {WS_PORT}!")

    if ssl_context:
        await websockets.serve(ws_handler, "0.0.0.0", WSS_PORT, ssl=ssl_context)
        print(f"[++++] Started WebSocket (TLS) on port {WSS_PORT}!")

    # Run forever
    await asyncio.Future()


if __name__ == "__main__":
    asyncio.run(main())
