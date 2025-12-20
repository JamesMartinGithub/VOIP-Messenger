# VOIP-Messenger
(2025) Peer-to-peer VOIP and messaging app using C++, Winsock, WASAPI, XAudio2, and QT

## Setup
One party needs to select 'server' and port forward to bypass NAT for non-LAN connections

## Technologies/Techniques Used:
- Winsock API used to establish TCP connection for text messaging and determine endpoints for P2P UDP audio transmission
- WASAPI and Xaudio2 APIs for audio input/output
- C++ multithreading and file reading/writing
- QT for GUI with signal/slot thread synchronisation
- Visual studio debugging using stack traces, watches to analyse memory, and code step-through
- CMake for build automation

## Features:
- Text messaging
- Voice calling
- Message history saving/loading supporting multiple friends
- Audio input device selection

## Gallery:
<img src="https://github.com/user-attachments/assets/29c48534-47f7-47a5-bc34-266bb743aa1d" width="800" height="603" style="display:none">

## Acknowledgement
MPMCQueue by Erik Rigtorp is used
- https://github.com/rigtorp/MPMCQueue
