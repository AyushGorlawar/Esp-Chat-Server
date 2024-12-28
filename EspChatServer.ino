#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer_Generic.h>
#include <ArduinoJson.h>

const char* ssid = "ChatBox";
const char* password = "1234567890";

AsyncWebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

struct ChatClient {
    uint8_t id;
    String username;
    bool isActive;
};

#define MAX_CLIENTS 10
ChatClient chatClients[MAX_CLIENTS];

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>Dark Chat</title>
    <style>
        :root {
            --bg-primary: #1a1a1a;
            --bg-secondary: #2d2d2d;
            --text-primary: #ffffff;
            --text-secondary: #b0b0b0;
            --accent: #4CAF50;
            --accent-hover: #45a049;
        }
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        body {
            font-family: Arial, sans-serif;
            background-color: var(--bg-primary);
            color: var(--text-primary);
            height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
        }
        .chat-container {
            width: 100%;
            max-width: 600px;
            height: 100vh;
            display: flex;
            flex-direction: column;
            background-color: var(--bg-secondary);
        }
        .chat-header {
            padding: 20px;
            background-color: var(--bg-primary);
            text-align: center;
            font-size: 1.2em;
            font-weight: bold;
        }
        .chat-messages {
            flex-grow: 1;
            padding: 20px;
            overflow-y: auto;
            display: flex;
            flex-direction: column;
            gap: 10px;
        }
        .message {
            max-width: 80%;
            padding: 10px 15px;
            border-radius: 15px;
            background-color: var(--bg-primary);
            word-wrap: break-word;
        }
        .message.sent {
            align-self: flex-end;
            background-color: var(--accent);
        }
        .message.received {
            align-self: flex-start;
        }
        .chat-input-container {
            padding: 20px;
            background-color: var(--bg-primary);
        }
        .chat-input {
            display: flex;
            gap: 10px;
        }
        .chat-input input {
            flex-grow: 1;
            padding: 10px 15px;
            border: none;
            border-radius: 25px;
            background-color: var(--bg-secondary);
            color: var(--text-primary);
            font-size: 1em;
            outline: none;
        }
        .chat-input button {
            padding: 10px 20px;
            border: none;
            border-radius: 25px;
            background-color: var(--accent);
            color: white;
            font-size: 1em;
            cursor: pointer;
            transition: background-color 0.3s;
        }
        .chat-input button:hover {
            background-color: var(--accent-hover);
        }
        .username-modal {
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: rgba(0,0,0,0.8);
            display: flex;
            align-items: center;
            justify-content: center;
            z-index: 1000;
        }
        .username-form {
            background: var(--bg-secondary);
            padding: 20px;
            border-radius: 12px;
            width: 90%;
            max-width: 300px;
        }
        .username-form h2 {
            margin-bottom: 15px;
            color: var(--text-primary);
            text-align: center;
        }
        .message-meta {
            font-size: 0.8em;
            color: var(--text-secondary);
            margin-bottom: 4px;
        }
        .message.sent .message-meta {
            text-align: right;
        }
        .message.received .message-meta {
            text-align: left;
        }
        @media (max-width: 600px) {
            .chat-container {
                height: 100vh;
                max-width: none;
            }
            .message {
                max-width: 90%;
            }
        }
    </style>
</head>
<body>
    <div id="username-modal" class="username-modal">
        <div class="username-form">
            <h2>Enter your username</h2>
            <div class="chat-input">
                <input type="text" id="usernameInput" placeholder="Your name..." autocomplete="off">
                <button onclick="setUsername()">Join</button>
            </div>
        </div>
    </div>

    <div class="chat-container">
        <div class="chat-header">
            Dark Chat Room
        </div>
        <div class="chat-messages" id="chatMessages">
            <div class="message received">
                <div class="message-meta">System</div>
                <div class="message-content">
                    Welcome to the Dark Chat! 🌙
                </div>
            </div>
        </div>
        <div class="chat-input-container">
            <div class="chat-input">
                <input type="text" id="messageInput" placeholder="Type a message..." autocomplete="off">
                <button onclick="sendMessage()">Send</button>
            </div>
        </div>
    </div>

    <script>
        let ws;
        let username = '';
        let reconnectAttempts = 0;
        const maxReconnectAttempts = 5;
        
        function setUsername() {
            const input = document.getElementById('usernameInput');
            const name = input.value.trim();
            
            if (name) {
                username = name;
                document.getElementById('username-modal').style.display = 'none';
                connectWebSocket();
            }
        }

        function connectWebSocket() {
            ws = new WebSocket('ws://' + window.location.hostname + ':81');
            
            ws.onopen = function() {
                console.log('Connected to WebSocket');
                reconnectAttempts = 0;
                ws.send(JSON.stringify({
                    type: 'setUsername',
                    username: username
                }));
                document.getElementById('messageInput').focus();
            };
            
            ws.onmessage = function(event) {
                try {
                    const data = JSON.parse(event.data);
                    if (data.username && data.message) {
                        addMessage(data.message, data.username === username ? 'sent' : 'received', data.username);
                    }
                } catch (e) {
                    console.error('Error parsing message:', e);
                }
            };
            
            ws.onclose = function() {
                console.log('WebSocket closed');
                if (reconnectAttempts < maxReconnectAttempts) {
                    reconnectAttempts++;
                    setTimeout(connectWebSocket, 1000 * reconnectAttempts);
                }
            };
        }

        function sendMessage() {
            const input = document.getElementById('messageInput');
            const message = input.value.trim();
            
            if (message && ws && ws.readyState === WebSocket.OPEN) {
                console.log('Sending message:', message);
                ws.send(JSON.stringify({
                    type: 'message',
                    message: message
                }));
                input.value = '';
                input.focus();
            }
        }

        function addMessage(message, type, msgUsername) {
            const chatMessages = document.getElementById('chatMessages');
            const messageDiv = document.createElement('div');
            messageDiv.className = `message ${type}`;
            
            messageDiv.innerHTML = `
                <div class="message-meta">${escapeHtml(msgUsername)}</div>
                <div class="message-content">${escapeHtml(message)}</div>
            `;
            
            chatMessages.appendChild(messageDiv);
            chatMessages.scrollTop = chatMessages.scrollHeight;
        }

        function escapeHtml(unsafe) {
            return unsafe
                .replace(/&/g, "&amp;")
                .replace(/</g, "&lt;")
                .replace(/>/g, "&gt;")
                .replace(/"/g, "&quot;")
                .replace(/'/g, "&#039;");
        }

        document.getElementById('messageInput').addEventListener('keypress', function(e) {
            if (e.key === 'Enter') {
                sendMessage();
            }
        });

        document.getElementById('usernameInput').addEventListener('keypress', function(e) {
            if (e.key === 'Enter') {
                setUsername();
            }
        });
    </script>
</body>
</html>
)rawliteral";

void handleWebSocketMessage(uint8_t num, uint8_t* payload) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        Serial.println("Failed to parse JSON");
        return;
    }

    const char* type = doc["type"];
    
    if (strcmp(type, "setUsername") == 0) {
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (chatClients[i].id == num || (!chatClients[i].isActive && chatClients[i].id == 0)) {
                chatClients[i].id = num;
                chatClients[i].username = doc["username"].as<String>();
                chatClients[i].isActive = true;
                break;
            }
        }
    }
    else if (strcmp(type, "message") == 0) {
        String username;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (chatClients[i].id == num && chatClients[i].isActive) {
                username = chatClients[i].username;
                StaticJsonDocument<512> response;
                response["username"] = username;
                response["message"] = doc["message"];
                
                String responseString;
                serializeJson(response, responseString);
                webSocket.broadcastTXT(responseString);
                break;
            }
        }
    }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (chatClients[i].id == num) {
                    chatClients[i].id = 0;
                    chatClients[i].username = "";
                    chatClients[i].isActive = false;
                    break;
                }
            }
            break;
        case WStype_CONNECTED:
            break;
        case WStype_TEXT:
            handleWebSocketMessage(num, payload);
            break;
    }
}

void setup() {
    Serial.begin(115200);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        chatClients[i].id = 0;
        chatClients[i].username = "";
        chatClients[i].isActive = false;
    }
    
    WiFi.softAP(ssid, password);
    
    Serial.println();
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html);
    });

    server.begin();
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
}

void loop() {
    webSocket.loop();
}
