/*******************************************************************************
* Websocket-Arduino, a websocket implementation for Arduino
* Copyright 2014 NaAl (h20@alocreative.com)
* Based on previous implementations by
* Copyright 2011 Per Ejeklint
* and
* Copyright 2010 Ben Swanson
* and
* Copyright 2010 Randall Brewer
* and
* Copyright 2010 Oliver Smith

* Some code and concept based off of Webduino library
* Copyright 2009 Ben Combee, Ran Talbott

* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:

* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.

* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
* 
* -------------
* Now based off
* http://www.whatwg.org/specs/web-socket-protocol/
* 
* - OLD -
* Currently based off of "The Web Socket protocol" draft (v 75):
* http://tools.ietf.org/html/draft-hixie-thewebsocketprotocol-75
*******************************************************************************/

#include "SparkWebSocketServer.h"

#include "Base64.h"

#include "tropicssl/sha1.h"

struct WS_User_Func_Lookup_Table_t
{
    int (*pUserFunc)(String userArg);
    char userFuncKey[USER_FUNC_KEY_LENGTH];
    char userFuncArg[USER_FUNC_ARG_LENGTH];
    int userFuncRet;
    bool userFuncSchedule;
} WS_User_Func_Lookup_Table[USER_FUNC_MAX_COUNT];

SparkWebSocketServer::SparkWebSocketServer(TCPServer &tcpServer) {
  for(uint8_t i=0;i<MAX_CLIENTS;i++) {
    clients[i]=NULL;
  }
  server=&tcpServer;
  previousMillis = 0;
}

bool SparkWebSocketServer::handshake(TCPClient &client) {
  uint8_t pos=0;
  bool found=false;
  for(pos=0;pos<MAX_CLIENTS;pos++) {
    if(clients[pos] != NULL && &client == clients[pos]) {
      found=true;
      break;
    }
  }
  if(!found) {
    for(pos=0;pos<MAX_CLIENTS;pos++) {
      if(clients[pos]==NULL) {
        break;
      }
    }
  }
  if(pos>=MAX_CLIENTS) {
    return false;
  }
  // If there is an empty spot
  if (pos < MAX_CLIENTS) {
    // Check request and look for websocket handshake
    if (analyzeRequest(BUFFER_LENGTH, client)) {
      clients[pos]=&client;
      return true;
    } else {
      // Might just need to break until out of socket_client loop.


      return false;
    }
  } else {

    disconnectClient(client);
    return false;
  }
}

void SparkWebSocketServer::disconnectClient(TCPClient &client) {

  // Should send 0x8700 to server to tell it I'm quitting here.
  client.write((uint8_t) 0x87);
  client.write((uint8_t) 0x00);

  client.flush();
  delay(10);
  client.stop();
  for(uint8_t i=0;i<MAX_CLIENTS;i++) {
    if(clients[i]!=NULL && &client == clients[i]) {
      TCPClient *tmp=clients[i];
      delete tmp;
      clients[i]=NULL;
      break;
    }
  }
}

void SparkWebSocketServer::getData(String &data, TCPClient &client) {
  handleStream(data, client);
}


void SparkWebSocketServer::handleStream(String &data, TCPClient &client) {
  int length;
  uint8_t mask[4];

  if (client.connected()) {
    length=timedRead(client);
    if (!client.connected() || length==-1) {
      return ;
    }

    length = timedRead(client) & 127;
    if (!client.connected()) {
      return ;
    }

    if (length == 126) {
      length = timedRead(client) << 8;
      if (!client.connected()) {
        return ;
      }

      length |= timedRead(client);
      if (!client.connected()) {
        return ;
      }

    } else if (length == 127) {

      return;
    }

    // get the mask
    mask[0] = timedRead(client);
    if (!client.connected()) {
      return ;
    }

    mask[1] = timedRead(client);
    if (!client.connected()) {

      return ;
    }

    mask[2] = timedRead(client);
    if (!client.connected()) {
      return ;
    }

    mask[3] = timedRead(client);
    if (!client.connected()) {
      return ;
    }

    for (int i=0; i<length; ++i) {
      data += (char) (timedRead(client) ^ mask[i % 4]);
      if (!client.connected()) {
        return ;
      }
    }
  }
}

int SparkWebSocketServer::timedRead(TCPClient &client) {
  uint8_t test=0;
  while (test<20 && !client.available() && client.connected()) {
    delay(1);
    test++;
  }
  if(client.connected()) {
    return client.read();
  }
  return -1;
}

void SparkWebSocketServer::sendEncodedData(char *str, TCPClient &client) {
  int size = strlen(str);
  if(!client) {
    return;
  }
  // string type
  client.write(0x81);

  // NOTE: no support for > 16-bit sized messages
  if (size > 125) {
    client.write(126);
    client.write((uint8_t) (size >> 8));
    client.write((uint8_t) (size && 0xFF));
  } else {
    client.write((uint8_t) size);
  }

  for (int i=0; i<size; ++i) {
    client.write(str[i]);
  }

}

void SparkWebSocketServer::sendEncodedData(String str, TCPClient &client) {
  int size = str.length() + 1;
  char cstr[size];

  str.toCharArray(cstr, size);

  sendEncodedData(cstr, client);
}

void SparkWebSocketServer::sendData(const char *str, TCPClient &client) {
  if (client && client.connected()) {
    sendEncodedData(str, client);
  }

}

void SparkWebSocketServer::sendData(String str, TCPClient &client){
  if (client && client.connected()) {
    sendEncodedData(str, client);
  }

}


void SparkWebSocketServer::doIt() {
  //handle new clients

  unsigned long currentMillis = millis();
  bool beat=currentMillis - previousMillis > HB_INTERVAL;
  if(beat) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
  }

  TCPClient client = server->available();

  if(client && client.connected() ) {
    Serial.println("connected!");
    Serial.flush();
    handshake(client);
  }

  for(uint8_t i=0;i<MAX_CLIENTS;i++) {
    TCPClient *myClient=clients[i];

    return;

    for(uint8_t j=0; j < 20; j++){
      if(myClient) myClient->connected();
    }

    if(myClient!=NULL && !(myClient->connected())) {
      Serial.print("Disconnect: ");
      Serial.print(previousMillis);
      Serial.print(" ");
      Serial.println(i);
      Serial.flush();
      disconnectClient(*myClient);
    } else if(myClient!=NULL) {
      Serial.print("Get Data: ");
      Serial.println(i);
      Serial.flush();

      if(myClient->available() > 0){
        String input;
        int bite;

        while ((bite = myClient->read()) != -1) {
          input += (char) bite;
        }

        Serial.print("Data length: ");
        Serial.println(input.length());
        Serial.flush();

        if(input.length()>0) {
          Serial.print("Got data: ");
          Serial.println(input);
          Serial.flush();

          String result;
          (*cBack)(input, result);
          sendData(result, *myClient);
        }
      } else {
        Serial.print("Beat: ");
        Serial.println(beat);
        Serial.flush();

        if(beat) {
          if(myClient!=NULL && myClient->connected()) {
            Serial.print("Heartbeat: ");
            Serial.println(i);
            Serial.flush();

            sendData("HB", *myClient);
          }
        }
      }
    }
  }
}

bool SparkWebSocketServer::analyzeRequest(int bufferLength, TCPClient &client) {
  // Use String library to do some sort of read() magic here.
  String temp;

  Serial.println("Analyzing");

  int bite;
  bool foundupgrade = false;
  String newkey, origin, host;


  // TODO: More robust string extraction
  while ((bite = client.read()) != -1) {

    temp += (char)bite;

    if ((char)bite == '\n') {
      Serial.print("IN: ");
      Serial.println(temp);
      Serial.flush();

      // TODO: Should ignore case when comparing and allow 0-n whitespace after ':'. See the spec:
      // http://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html
    if (!foundupgrade && temp.startsWith("Upgrade: websocket")) {
        foundupgrade = true;
      } else if (temp.startsWith("Origin: ")) {
        origin = temp.substring(8,temp.length() - 2); // Don't save last CR+LF
      } else if (temp.startsWith("Host: ")) {
        host = temp.substring(6,temp.length() - 2); // Don't save last CR+LF
      } else if (temp.startsWith("Sec-WebSocket-Key: ")) {
        newkey = temp.substring(19,temp.length() - 2); // Don't save last CR+LF

        Serial.print("Set Newkey: ");
        Serial.println(newkey);
        Serial.flush();
      }
      temp = "";
    }


    if (!client.available()) {
      delay(20);
    }
  }

  if (!client.connected()) {
    return false;
  }

  temp += 0; // Terminate string

  // Assert that we have all headers that are needed. If so, go ahead and
  // send response headers.
  if (foundupgrade == true) {

    Serial.println("Yo, we're done here.");
    Serial.flush();

    Serial.print("Newkey: ");
    Serial.println(newkey);
    Serial.flush();

    if (newkey.length() > 0) {

      // add the magic string
      newkey += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

      uint8_t hash[100];
      char result[21];
      char b64Result[30];

      //char hexstring[41];
      unsigned char a[newkey.length()+1];
      a[newkey.length()]=0;
      memcpy(a,newkey.c_str(),newkey.length());
      sha1(a,newkey.length(),hash); // 10 is the length of the string
      //toHexString(hash, hexstring);


      for (uint8_t i=0; i<20; ++i) {
        result[i] = (char)hash[i];
      }
      result[20] = '\0';

      Serial.print("Result: ");
      Serial.println(result);
      Serial.flush();

      base64_encode(b64Result, result, 20);

      client.print("HTTP/1.1 101 Web Socket Protocol Handshake\r\n");
      client.print("Upgrade: websocket\r\n");
      client.print("Connection: Upgrade\r\n");
      client.print("Sec-WebSocket-Accept: ");
      client.print(b64Result);
      client.print(CRLF);
      client.print(CRLF);

      Serial.print("b64 result: ");
      Serial.println(b64Result);
      Serial.flush();

      return true;
    } else {
      // something went horribly wrong
      return false;
    }
  } else {
    // Nope, failed handshake. Disconnect

    return false;
  }
}