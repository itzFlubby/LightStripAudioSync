import asyncio
import websockets

BUFFER_SIZE = 1024

HOST = "127.0.0.1"
TCP_PORT = 3334
WS_PORT = 3335

ws_clients = set()


async def ws_pipe_tcp():
    while True:
        try:
            print(f"[INFO] Starting data pipe from {HOST}:{TCP_PORT}...")
            tcp_reader, _ = await asyncio.open_connection(HOST, TCP_PORT)

            while True:
                data = await tcp_reader.read(BUFFER_SIZE)
                if not data:
                    pass

                dead_ws_clients = set()
                for ws_client in ws_clients:
                    try:
                        await ws_client.send(data[3 : (len(data) - 1)])
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
    pipe = asyncio.create_task(ws_pipe_tcp())

    async with websockets.serve(ws_handler, HOST, WS_PORT) as server:
        await asyncio.Future()  # run forever


if __name__ == "__main__":
    asyncio.run(main())
