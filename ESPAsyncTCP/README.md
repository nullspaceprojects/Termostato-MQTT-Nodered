# ESPAsyncTCP Library (forked copy)

[![GitHub release](https://img.shields.io/github/release/khoih-prog/ESPAsyncTCP.svg)](https://github.com/khoih-prog/ESPAsyncTCP/releases)
[![contributions welcome](https://img.shields.io/badge/contributions-welcome-brightgreen.svg?style=flat)](#Contributing)
[![GitHub issues](https://img.shields.io/github/issues/khoih-prog/ESPAsyncTCP.svg)](http://github.com/khoih-prog/ESPAsyncTCP/issues)


<a href="https://profile-counter.glitch.me/khoih-prog/count.svg" title="Total khoih-prog Visitor count"><img src="https://profile-counter.glitch.me/khoih-prog/count.svg" style="height: 30px;width: 200px;"></a>
<a href="https://profile-counter.glitch.me/khoih-prog-ESPAsyncTCP/count.svg" title="Forked ESPAsyncTCP Visitor count"><img src="https://profile-counter.glitch.me/khoih-prog-ESPAsyncTCP/count.svg" style="height: 30px;width: 200px;"></a>

---
---

## Table of contents

* [Important Notes](#Important-Notes)
* [Why do we need this ESPAsyncTCP library](#why-do-we-need-this-ESPAsyncTCP-library)
  * [Features](#features)
  * [Why Async is better](#why-async-is-better)
  * [Currently supported Boards](#currently-supported-boards)
* [Changelog](changelog.md) 
* [Async TCP Library for ESP8266 Arduino](#Async-TCP-Library-for-ESP8266-Arduino)
* [Documentation](#Documentation)
	* [AsyncClient and AsyncServer](#AsyncClient-and-AsyncServer)
	* [AsyncPrinter](#AsyncPrinter)
	* [AsyncTCPbuffer](#AsyncTCPbuffer)
	* [SyncClient](#SyncClient)
* [Examples](#examples)
  * [1. Client](examples/ClientServer/Client)
  * [2. Server](examples/ClientServer/Server)
  * [3. SyncClient](examples/SyncClient)
* [Libraries and projects that use ESPAsyncTCP](#Libraries-and-projects-that-use-ESPAsyncTCP)
* [Contributions and Thanks](#contributions-and-thanks)
* [Contributing](#contributing)
* [License](#license)
* [Copyright](#copyright)

---
---

### Important Notes

The new ESP8266 cores v3.0.2+ introduces many new breaking features, such as replacing `axtls` with `bearssl`, as well as good ones such as LwIP Ethernet `W5500lwIP`, `W5100lwIP` and `ENC28J60lwIP` libraries.

Because the original [ESPAsyncTCP library](https://github.com/me-no-dev/ESPAsyncTCP) is not well maintained to cope with dynamic environment of new cores, features, etc., this forked [ESPAsyncTCP library](https://github.com/khoih-prog/ESPAsyncTCP) is created as an effort to try `the best` to keep up with the fast changes and avoid compile errors in future cores or dependent libraries.

Hopefully the `bearssl` feature of new ESP8266 cores will be added to this library in the near future to support SSL.

---

### Why do we need this [ESPAsyncTCP library](https://github.com/khoih-prog/ESPAsyncTCP)

#### Features

This library is based on, modified from:

1. [Hristo Gochkov's ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP)

to apply the better and faster **asynchronous** feature of the **powerful** [ESPAsyncTCP Library](https://github.com/me-no-dev/ESPAsyncTCP) with SSL, and will be the base for other more advanced Async libraries for ESP8266, such as ESPAsyncWebServer, AsyncHTTPRequest, AsyncHTTPSRequest, etc.


#### Why Async is better

- Using asynchronous network means that you can handle **more than one connection at the same time**
- **You are called once the request is ready and parsed**
- When you send the response, you are **immediately ready** to handle other connections while the server is taking care of sending the response in the background
- **Speed is OMG**
- **Easy to use API, HTTP Basic and Digest MD5 Authentication (default), ChunkedResponse**
- Easily extensible to handle **any type of content**
- Supports Continue 100
- **Async WebSocket plugin offering different locations without extra servers or ports**
- Async EventSource (Server-Sent Events) plugin to send events to the browser
- URL Rewrite plugin for conditional and permanent url rewrites
- ServeStatic plugin that supports cache, Last-Modified, default index and more
- Simple template processing engine to handle templates


### Currently supported Boards

1. `ESP8266` boards

---
---



## Async TCP Library for ESP8266 Arduino

For ESP32, check [AsyncTCP](https://github.com/me-no-dev/AsyncTCP) or [AsyncTCP_SSL](https://github.com/khoih-prog/AsyncTCP_SSL)

[![Join the chat at https://gitter.im/me-no-dev/ESPAsyncWebServer](https://badges.gitter.im/me-no-dev/ESPAsyncWebServer.svg)](https://gitter.im/me-no-dev/ESPAsyncWebServer?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

This is a fully asynchronous TCP library, aimed at enabling trouble-free, multi-connection network environment for Espressif's ESP8266 MCUs, using WiFi or LwIP Ethernet (W5500lwIP, W5100lwIP and ENC28J60lwIP libraries)

This library is the base for [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)

---

## Documentation

### AsyncClient and AsyncServer

The base classes on which everything else is built. They expose all possible scenarios, but are really raw and require more skills to use.

### AsyncPrinter

This class can be used to send data like any other `Print` interface (`Serial` for example).

The object then can be used outside of the Async callbacks (the loop) and receive asynchronously data using ```onData```. The object can be checked if the underlying `AsyncClient` is connected, or hook to the `onDisconnect` callback.

### AsyncTCPbuffer

This class is really similar to the `AsyncPrinter`, but it differs in the fact that it can buffer some of the incoming data.

### SyncClient

It is exactly what it sounds like. This is a standard, blocking TCP Client, similar to the one included in `ESP8266WiFi`

---
---

### Examples

 1. [Client](examples/ClientServer/Client)
 2. [Server](examples/ClientServer/Server)
 3. [SyncClient](examples/SyncClient)
 

---
---

## Libraries and projects that use ESPAsyncTCP

- [ESP Async Web Server](https://github.com/me-no-dev/ESPAsyncWebServer)
- [Async MQTT client](https://github.com/marvinroger/async-mqtt-client)
- [arduinoWebSockets](https://github.com/Links2004/arduinoWebSockets)
- [ESP8266 Smart Home](https://github.com/baruch/esp8266_smart_home)
- [KBox Firmware](https://github.com/sarfata/kbox-firmware)

---

- [AsyncMQTT_Generic](https://github.com/khoih-prog/AsyncMQTT_Generic)
- [ESPAsync_WiFiManager](https://github.com/khoih-prog/ESPAsync_WiFiManager)
- [AsyncHTTPRequest_Generic](https://github.com/khoih-prog/AsyncHTTPRequest_Generic)
- [AsyncHTTPSRequest_Generic](https://github.com/khoih-prog/AsyncHTTPSRequest_Generic)
- [ESPAsync_WiFiManager_Lite](https://github.com/khoih-prog/ESPAsync_WiFiManager_Lite)
- [AsyncWebServer_Ethernet](https://github.com/khoih-prog/AsyncWebServer_Ethernet)

and many more to come.


---
---

### Issues

Submit issues to: [ESPAsyncTCP issues](https://github.com/khoih-prog/ESPAsyncTCP/issues)

---

## TO DO

1. Search for bug and improvement.
2. Add `bearssl` Async features for new cores
3. Remove `hard-code` if possible

---

## DONE

1. Try to convert to use with new ESP8266 cores v3.0.2+ without `axtls` support
2. Add Table of Contents
3. Modify examples\
4. Add astyle using `allman` style. Restyle the library

---
---

### Contributions and Thanks

Many thanks for everyone for bug reporting, new feature suggesting, testing and contributing to the development of this library.

### Contributions and Thanks

1. Based on and modified from [Hristo Gochkov's ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP). Many thanks to [Hristo Gochkov](https://github.com/me-no-dev) for great [ESPAsyncTCP Library](https://github.com/me-no-dev/ESPAsyncTCP)


<table>
  <tr>
    <td align="center"><a href="https://github.com/me-no-dev"><img src="https://github.com/me-no-dev.png" width="100px;" alt="me-no-dev"/><br /><sub><b>⭐️⭐️ Hristo Gochkov</b></sub></a><br /></td>
  </tr> 
</table>

---

## Contributing

If you want to contribute to this project:

- Report bugs and errors
- Ask for enhancements
- Create issues and pull requests
- Tell other people about this library

---
---

### License

- The library is licensed under [LGPLv3](https://github.com/khoih-prog/ESPAsyncTCP/blob/master/LICENSE)

---

## Copyright

- Copyright (c) 2016- Hristo Gochkov
- Copyright (c) 2022- Khoi Hoang



