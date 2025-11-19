# castViewer

Simple Qt6 C++ prototype for screen sharing (Client/Server) over LAN using TCP.

## Structure

- `Server/` - Qt Widgets project that listens on port 45454 and displays incoming JPEG images.
- `Client/` - Qt Widgets project that captures the primary screen, encodes it as JPEG and sends to server.

## How to build & run

1. Open the desired project folder (`Server` or `Client`) in Qt Creator (Qt 6.x).
2. Configure kit (MSVC or MinGW).
3. Build and Run.

## Quick test on single machine

- Run Server.
- Edit `Client/mainwindow.cpp` if needed and set `socket->connectToHost("127.0.0.1", 45454);` (default).
- Run Client. Server should display incoming frames.

## Notes & next steps

- This is a minimal prototype: no framing protocol, no TLS, no authentication.
- Images are sent as raw JPG blob per write(). For robustness add frame headers, length prefix, compression and error handling.
- Next steps: implement framing, ACKs, delta frames, better compression (FFmpeg), remote input, signaling/relay, TLS.

## License
MIT
