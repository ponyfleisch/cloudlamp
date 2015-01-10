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

TCPClient client;

bool hasClient = false;

SparkWebSocketServer::SparkWebSocketServer(TCPServer &tcpServer) {
  server=&tcpServer;
  previousMillis = 0;
}

bool SparkWebSocketServer::handshake(TCPClient &client) {
  analyzeRequest(BUFFER_LENGTH, client);
}

void SparkWebSocketServer::disconnectClient(TCPClient &client) {
  // Should send 0x8700 to server to tell it I'm quitting here.
  client.write((uint8_t) 0x88);
  client.write((uint8_t) 0x00);

  client.flush();
  delay(10);
  client.stop();

  hasClient = false;
}

void SparkWebSocketServer::getData(String &data, TCPClient &client) {
  handleStream(data, client);
}


void SparkWebSocketServer::handleStream(String &data, TCPClient &client) {
  int length;
  uint8_t mask[4];

  if (client.connected()) {
    length=timedRead(client);

    if(length == 136){


      disconnectClient(client);
      return ;
    }

    if (!client.connected() || length==-1) {
      hasClient = false;
      return ;
    }

    length = timedRead(client);


    length = length  & 127;

    if (!client.connected()) {
      hasClient = false;
      return ;
    }

    if (length == 126) {
      length = timedRead(client) << 8;
      if (!client.connected()) {
        hasClient = false;
        return ;
      }

      length |= timedRead(client);
      if (!client.connected()) {
        hasClient = false;
        return ;
      }

    } else if (length == 127) {

      return;
    }

    // get the mask
    mask[0] = timedRead(client);
    if (!client.connected()) {
      hasClient = false;
      return ;
    }

    mask[1] = timedRead(client);
    if (!client.connected()) {
      hasClient = false;
      return ;
    }

    mask[2] = timedRead(client);
    if (!client.connected()) {
      hasClient = false;
      return ;
    }

    mask[3] = timedRead(client);
    if (!client.connected()) {
      hasClient = false;
      return ;
    }

    for (int i=0; i<length; ++i) {
      data += (char) (timedRead(client) ^ mask[i % 4]);
      if (!client.connected()) {
        hasClient = false;
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
    uint8_t in = client.read();


    return in;
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
    previousMillis = currentMillis;
  }

  if(!client){
    client = server->available();
  }

  if(hasClient == false && client && client.connected() ) {
    handshake(client);
    hasClient = true;
  }

  if(client){
    if(!client.connected()){
      disconnectClient(client);
    }else{
      if(client.available() > 0) {
        String input, result;
        getData(input, client);
        (*cBack)(input, result);
        if(result.length() > 0) sendData(result, client);
      }else{
        if(beat) sendData("HB", client);
      }
    }
  }

  return;

}

bool SparkWebSocketServer::analyzeRequest(int bufferLength, TCPClient &client) {
  // Use String library to do some sort of read() magic here.
  String temp;

  int bite;
  bool foundupgrade = false;
  String newkey, origin, host;


  // TODO: More robust string extraction
  while ((bite = client.read()) != -1) {

    temp += (char)bite;

    if ((char)bite == '\n') {
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

      base64_encode(b64Result, result, 20);

      client.print("HTTP/1.1 101 Web Socket Protocol Handshake\r\n");
      client.print("Upgrade: websocket\r\n");
      client.print("Connection: Upgrade\r\n");
      client.print("Sec-WebSocket-Accept: ");
      client.print(b64Result);
      client.print(CRLF);
      client.print(CRLF);

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