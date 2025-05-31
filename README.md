# 🚀 Redis Clone in C++

A Redis clone written from scratch in C++ for the sake of understanding concepts like networking, serialization, and replication. This project implements the foundational components of Redis, including RESP parsing, a key-value store, RDB state loading, and master-replica sync.

## ⚙️ Features

- 🧠 **In-memory key-value store**  
  Supports standard commands like `GET`, `SET`, `REPLCONF`, `PSYNC`, `PING`, `KEYS`, etc.

- 🔌 **RESP (Redis Serialization Protocol)**  
  Fully implements RESP parsing and formatting, including edge cases.

- 📂 **RDB-based persistence**  
  Supports loading Redis RDB files on startup to restore server state.

- 🔁 **Master-replica replication**  
  Implements replication, syncing data from a master instance to replicas over the network.

- 📡 **Concurrent request handling**  
  Handles multiple client connections simultaneously using Winsock.
  
## 🚧 Not Implemented (By Choice)

Some Redis features like `MULTI`, `EXEC`, `WATCH`, AOF logging, and snapshot creation (serializing the entire in-memory dataset) were intentionally left out to stay focused on core behavior.

## 🏁 Status

This project is complete in its current form. I’m not actively adding features, but the existing implementation is solid.
