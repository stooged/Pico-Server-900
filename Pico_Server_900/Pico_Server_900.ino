#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <LEAmDNS.h>
#include "Adafruit_TinyUSB.h"
#include "exfathax.h"
#include "Loader.h"

                    // enable internal goldhen.h [ true / false ]
#define INTHEN true // goldhen is placed in the app partition to free up space on the storage for other payloads.
                    // with this enabled you do not upload goldhen to the board, set this to false if you wish to upload goldhen.


                      // enable autohen [ true / false ]
#define AUTOHEN false // this will load goldhen instead of the normal index/payload selection page, use this if you only want hen and no other payloads.
                      // INTHEN must be set to true for this to work.


                    // enable fan threshold [ true / false ]
#define FANMOD true // this will include a function to set the consoles fan ramp up temperature in °C
                    // this will not work if usb control is disabled.


#if FANMOD
#include "fan.h"
#endif


#if INTHEN
#include "goldhen.h"
#endif


DNSServer dnsServer;
WebServer webServer;
Adafruit_USBD_MSC msc;
Adafruit_USBD_Device dev;
#define FILESYS LittleFS 
boolean hasEnabled = false;
boolean hasStarted = false;
int ftemp = 70;
long enTime = 0;
File upFile;
String firmwareVer = "1.00";


//-------------------DEFAULT SETTINGS------------------//

                       // use config.ini [ true / false ]
#define USECONFIG true // this will allow you to change these settings below via the admin webpage.
                       // if you want to permanently use the values below then set this to false.

// access point
String AP_SSID = "PS4_WEB_AP";
String AP_PASS = "password";
IPAddress Server_IP(10,1,1,1);
IPAddress Subnet_Mask(255,255,255,0);

// wifi
boolean connectWifi = false;   // enabling this option will disable the access point
String WIFI_SSID = "Home_WIFI"; 
String WIFI_PASS = "password";
String WIFI_HOSTNAME = "ps4.local";

//server port
int WEB_PORT = 80;

//exfathax Wait(milliseconds)
int USB_WAIT = 10000;

//-----------------------------------------------------//
#include "pages.h"



String split(String str, String from, String to)
{
  String tmpstr = str;
  tmpstr.toLowerCase();
  from.toLowerCase();
  to.toLowerCase();
  int pos1 = tmpstr.indexOf(from);
  int pos2 = tmpstr.indexOf(to, pos1 + from.length());   
  String retval = str.substring(pos1 + from.length() , pos2);
  return retval;
}


bool instr(String str, String search)
{
int result = str.indexOf(search);
if (result == -1)
{
  return false;
}
return true;
}


String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+" B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+" KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+" MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+" GB";
  }
}


String urlencode(String str)
{
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
      }
      yield();
    }
    encodedString.replace("%2E",".");
    return encodedString;
}


void disableUSB()
{
   webServer.send(200, "text/plain", "ok");
   enTime = 0;
   hasEnabled = false;
   dev.detach();
}


void enableUSB()
{
   webServer.send(200, "text/plain", "ok");
   dev.attach();
   enTime = millis();
   hasEnabled = true;
}


void sendwebmsg(String htmMsg)
{
    String tmphtm = "<!DOCTYPE html><html><head><style>body { background-color: #1451AE;color: #ffffff;font-size: 14px; font-weight: bold; margin: 0 0 0 0.0; padding: 0.4em 0.4em 0.4em 0.6em;}</style></head><center><br><br><br><br><br><br>" + htmMsg + "</center></html>";
    webServer.setContentLength(tmphtm.length());
    webServer.send(200, "text/html", tmphtm);
}

String getContentType(String filename){
  if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  else if(filename.endsWith(".bin")) return "application/octet-stream";
  else if(filename.endsWith(".manifest")) return "text/cache-manifest";
  return "text/plain";
}


bool loadFromFileSys(String path) {
 path = webServer.urlDecode(path);
 //Serial.println(path);
 if (path.equals("/connecttest.txt"))
 {
  webServer.setContentLength(22);
  webServer.send(200, "text/plain", "Microsoft Connect Test");
  return true;
 }
 if (path.equals("/config.ini"))
 {
  return false;
 }
  if (path.endsWith("/")) {
    path += "index.html";
  }
  
  if (instr(path,"/update/ps4/"))
  {
    String Region = split(path,"/update/ps4/list/","/");
    handleConsoleUpdate(Region);
    return true;
  }
  if (instr(path,"/document/") && instr(path,"/ps4/"))
  {
     webServer.sendHeader("Location","http://" + WIFI_HOSTNAME + "/index.html");
     webServer.send(302, "text/html", "");
     return true;
  }

  if (path.endsWith("usbon") && webServer.method() == HTTP_POST)
  {
     enableUSB();
     webServer.send(200, "text/html", "ok");
     return true;
  }

  if (path.endsWith("usboff") && webServer.method() == HTTP_POST)
  {
     disableUSB();
     webServer.send(200, "text/html", "ok");
     return true;
  }


  String dataType = getContentType(path);
  bool isGzip = false;

  File dataFile;
  dataFile = FILESYS.open(path + ".gz", "r");
  if (!dataFile) {
    dataFile = FILESYS.open(path, "r");
  }
  else
  {
    isGzip = true;
  }
  
  if (!dataFile) {
     if (path.endsWith("index.html") || path.endsWith("index.htm"))
     {
        webServer.sendHeader("Content-Encoding", "gzip");
        webServer.send(200, "text/html", index_gz, sizeof(index_gz));
        return true;
     }
     if (path.endsWith("payloads.html"))
     {
        #if INTHEN && AUTOHEN
          webServer.sendHeader("Content-Encoding", "gzip");
          webServer.send(200, "text/html", autohen_gz, sizeof(autohen_gz));
        #else
          handlePayloads();
        #endif
        return true;
     }
     if (path.endsWith("loader.html"))
     {
        webServer.sendHeader("Content-Encoding", "gzip");
        webServer.send(200, "text/html", loader_gz, sizeof(loader_gz));
        return true;
     }
     if (path.endsWith("style.css"))
     {
        webServer.sendHeader("Content-Encoding", "gzip");
        webServer.send(200, "text/css", style_gz, sizeof(style_gz));
        return true;
     }
#if INTHEN
     if (path.endsWith("goldhen.bin"))
     {
        webServer.sendHeader("Content-Encoding", "gzip");
        webServer.send(200, "application/octet-stream", goldhen_gz, sizeof(goldhen_gz));
        return true;
     }
#endif
    return false;
  }
  if (webServer.hasArg("download")) {
    dataType = "application/octet-stream";
    String dlFile = path;
    if (dlFile.startsWith("/"))
    {
     dlFile = dlFile.substring(1);
    }
    webServer.sendHeader("Content-Disposition", "attachment; filename=\"" + dlFile + "\"");
    webServer.sendHeader("Content-Transfer-Encoding", "binary");
  }
  else
  {
    if (isGzip && path.endsWith(".bin"))
    {
      webServer.sendHeader("Content-Encoding", "gzip");
    }
  }
  if (webServer.streamFile(dataFile, dataType) != dataFile.size()) {
    //Serial.println("Sent less data than expected!");
  }
  dataFile.close();
  return true;
}


void handleNotFound() {
  if (loadFromFileSys(webServer.uri())) {
    return;
  }
  String message = "\n\n";
  message += "URI: ";
  message += webServer.uri();
  message += "\nMethod: ";
  message += (webServer.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += webServer.args();
  message += "\n";
  for (uint8_t i = 0; i < webServer.args(); i++) {
    message += " NAME:" + webServer.argName(i) + "\n VALUE:" + webServer.arg(i) + "\n";
  }
  webServer.send(404, "text/plain", "Not Found");
  //Serial.print(message);
}

void handleFileUpload() {
  if (webServer.uri() != "/upload.html") {
    webServer.send(500, "text/plain", "Internal Server Error");
    return;
  }
  HTTPUpload& upload = webServer.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    if (filename.equals("/config.ini"))
    {return;}
    upFile = FILESYS.open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (upFile) {
      upFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (upFile) {
      upFile.close();
    }
  }
}



void handleFormat()
{
  //Serial.print("Formatting Filesystem");
  FILESYS.end();
  FILESYS.format();
  FILESYS.begin();
#if USECONFIG
  writeConfig();
#endif  
  webServer.sendHeader("Location","/fileman.html");
  webServer.send(302, "text/html", "");
}


void handleDelete(){
  if(!webServer.hasArg("file")) 
  {
    webServer.sendHeader("Location","/fileman.html");
    webServer.send(302, "text/html", "");
    return;
  }
 String path = webServer.arg("file");
 if (FILESYS.exists("/" + path) && path != "/" && !path.equals("config.ini")) {
    FILESYS.remove("/" + path);
 }
   webServer.sendHeader("Location","/fileman.html");
   webServer.send(302, "text/html", "");
}


void handleFileMan() {
  Dir dir = FILESYS.openDir("/");
  String output = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>File Manager</title><style type=\"text/css\">a:link {color: #ffffff; text-decoration: none;} a:visited {color: #ffffff; text-decoration: none;} a:hover {color: #ffffff; text-decoration: underline;} a:active {color: #ffffff; text-decoration: underline;} table {font-family: arial, sans-serif; border-collapse: collapse; width: 100%;} td, th {border: 1px solid #dddddd; text-align: left; padding: 8px;} button {display: inline-block; padding: 1px; margin-right: 6px; vertical-align: top; float:left;} body {background-color: #1451AE;color: #ffffff; font-size: 14px; padding: 0.4em 0.4em 0.4em 0.6em; margin: 0 0 0 0.0;}</style><script>function statusDel(fname) {var answer = confirm(\"Are you sure you want to delete \" + fname + \" ?\");if (answer) {return true;} else { return false; }}</script></head><body><br><table id=filetable></table><script>var filelist = ["; 
  int fileCount = 0;
  while(dir.next()){
    File entry = dir.openFile("r");
    String fname = String(entry.name());
    if (fname.startsWith("/")){fname = fname.substring(1);}
    if (fname.length() > 0 && !fname.equals("config.ini"))
    {
      fileCount++;
      fname.replace("|","%7C");fname.replace("\"","%22");
      output += "\"" + fname + "|" + formatBytes(entry.size()) + "\",";
    }
    entry.close();
  }
  if (fileCount == 0)
  {
      output += "];</script><center>No files found<br>You can upload files using the <a href=\"/upload.html\" target=\"mframe\"><u>File Uploader</u></a> page.</center></p></body></html>";
  }
  else
  {
      output += "];var output = \"\";filelist.forEach(function(entry) {var splF = entry.split(\"|\"); output += \"<tr>\";output += \"<td><a href=\\\"\" +  splF[0] + \"\\\">\" + splF[0] + \"</a></td>\"; output += \"<td>\" + splF[1] + \"</td>\";output += \"<td><a href=\\\"/\" + splF[0] + \"\\\" download><button type=\\\"submit\\\">Download</button></a></td>\";output += \"<td><form action=\\\"/delete\\\" method=\\\"post\\\"><button type=\\\"submit\\\" name=\\\"file\\\" value=\\\"\" + splF[0] + \"\\\" onClick=\\\"return statusDel('\" + splF[0] + \"');\\\">Delete</button></form></td>\";output += \"</tr>\";}); document.getElementById(\"filetable\").innerHTML = output;</script></body></html>";
  }
  webServer.setContentLength(output.length());
  webServer.send(200, "text/html", output);
}



void handlePayloads() {
  Dir dir = FILESYS.openDir("/");
  String output = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>Pico Server</title><script>function setpayload(payload,title,waittime){ sessionStorage.setItem('payload', payload); sessionStorage.setItem('title', title); sessionStorage.setItem('waittime', waittime);  window.open('loader.html', '_self');}</script><style>.btn {transition-duration: 0.4s; box-shadow: 0 8px 16px 0 rgba(0,0,0,0.2), 0 6px 20px 0 rgba(0,0,0,0.19); background-color: DodgerBlue; border: none; color: white; padding: 12px 16px; font-size: 16px; cursor: pointer; font-weight: bold;} .btn:hover { background-color: RoyalBlue;} .slct{transition-duration: 0.4s;box-shadow: 0 8px 16px 0 rgba(0,0,0,0.2), 0 6px 20px 0 rgba(0,0,0,0.19);text-align: center;-webkit-appearance: none;background-color: DodgerBlue;border: none;color: white;padding: 9px 1px;font-size: 16px;cursor: pointer;font-weight: bold;}.slct:hover {background-color: RoyalBlue;} body { background-color: #1451AE; color: #ffffff; font-size: 14px; font-weight: bold; margin: 0 0 0 0.0; overflow-y:hidden; text-shadow: 3px 2px DodgerBlue;} .main { padding: 0px 0px; position: absolute; top: 0; right: 0; bottom: 0; left: 0; overflow-y:hidden;} msg {color: #ffffff; font-weight: normal; text-shadow: none;} a {color: #ffffff; font-weight: bold;}</style></head><body><center><h1>9.00 Payloads</h1>";
  int cntr = 0;
  int payloadCount = 0;
  if (USB_WAIT < 5000){USB_WAIT = 5000;} // correct unrealistic timing values
  if (USB_WAIT > 25000){USB_WAIT = 25000;}

#if INTHEN
  payloadCount++;
  cntr++;
  output +=  "<a onclick=\"setpayload('goldhen.bin','" + String(INTHEN_NAME) + "','" + String(USB_WAIT) + "')\"><button class=\"btn\">" + String(INTHEN_NAME) + "</button></a>&nbsp;";
#endif

  while(dir.next()){
    File entry = dir.openFile("r");
    String fname = String(entry.name());
    if (fname.startsWith("/")){fname = fname.substring(1);}
    if (fname.length() > 0)
    {
    if (fname.endsWith(".gz")) {
        fname = fname.substring(0, fname.length() - 3);
    }
    if (fname.endsWith(".bin"))
    {
      payloadCount++;
      String fnamev = fname;

      fnamev.replace(".bin","");
      output +=  "<a onclick=\"setpayload('" + urlencode(fname) + "','" + fnamev + "','" + String(USB_WAIT) + "')\"><button class=\"btn\">" + fnamev + "</button></a>&nbsp;";
      cntr++;
      if (cntr == 3)
      {
        cntr = 0;
        output +=  "<p></p>";
      }
    }
    }
    entry.close();
  }


#if FANMOD
  payloadCount++;
  output +=  "<br><p><a onclick='setfantemp()'><button class='btn'>Set Fan Threshold</button></a><select id='temp' class='slct'></select></p><script>function setfantemp(){var e = document.getElementById('temp');var temp = e.value;var xhr = new XMLHttpRequest();xhr.open('POST', 'setftemp?temp=' + temp, true);xhr.onload = function(e) {if (this.status == 200) {sessionStorage.setItem('payload', 'fant.bin'); sessionStorage.setItem('title', 'Fan Temp ' + temp + ' &deg;C'); localStorage.setItem('temp', temp); sessionStorage.setItem('waittime', '10000');  window.open('loader.html', '_self');}};xhr.send();}var stmp = localStorage.getItem('temp');if (!stmp){stmp = 70;}for(var i=55; i<=85; i=i+5){var s = document.getElementById('temp');var o = document.createElement('option');s.options.add(o);o.text = i + String.fromCharCode(32,176,67);o.value = i;if (i == stmp){o.selected = true;}}</script>";
#endif

  if (payloadCount == 0)
  {
      output += "<msg>No .bin payloads found<br>You need to upload the payloads to the Pico board using a pc/laptop connect to <b>" + AP_SSID + "</b> and navigate to <a href=http://" + Server_IP.toString() + "/admin.html>http://" + Server_IP.toString() + "/admin.html</a> and upload the .bin payloads using the <b>File Uploader</b></msg></center></body></html>";
  }
  output += "</center></body></html>";
  webServer.setContentLength(output.length());
  webServer.send(200, "text/html", output);
}


#if USECONFIG
void handleConfig()
{
  if(webServer.hasArg("ap_ssid") && webServer.hasArg("ap_pass") && webServer.hasArg("web_ip") && webServer.hasArg("web_port") && webServer.hasArg("subnet") && webServer.hasArg("wifi_ssid") && webServer.hasArg("wifi_pass") && webServer.hasArg("wifi_host") && webServer.hasArg("usbwait")) 
  {
    AP_SSID = webServer.arg("ap_ssid");
    if (!webServer.arg("ap_pass").equals("********"))
    {
      AP_PASS = webServer.arg("ap_pass");
    }
    WIFI_SSID = webServer.arg("wifi_ssid");
    if (!webServer.arg("wifi_pass").equals("********"))
    {
      WIFI_PASS = webServer.arg("wifi_pass");
    }
    String tmpip = webServer.arg("web_ip");
    String tmpwport = webServer.arg("web_port");
    String tmpsubn = webServer.arg("subnet");
    String WIFI_HOSTNAME = webServer.arg("wifi_host");

    String tmpcw = "false";

    if (webServer.hasArg("usewifi"))
    {
      String tmpcval = webServer.arg("usewifi");
      if (tmpcval.equals("true"))
      {
        tmpcw = "true";
      }
    }

    int USB_WAIT = webServer.arg("usbwait").toInt();
    File iniFile = FILESYS.open("/config.ini", "w");
    if (iniFile) {
    iniFile.print("\r\nAP_SSID=" + AP_SSID + "\r\nAP_PASS=" + AP_PASS + "\r\nWEBSERVER_IP=" + tmpip + "\r\nWEBSERVER_PORT=" + tmpwport + "\r\nSUBNET_MASK=" + tmpsubn + "\r\nWIFI_SSID=" + WIFI_SSID + "\r\nWIFI_PASS=" + WIFI_PASS + "\r\nWIFI_HOST=" + WIFI_HOSTNAME + "\r\nCONWIFI=" + tmpcw + "\r\nUSBWAIT=" + String(USB_WAIT) + "\r\n");
    iniFile.close();
    }
    String htmStr = "<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"8; url=/info.html\"><style type=\"text/css\">#loader {  z-index: 1;   width: 50px;   height: 50px;   margin: 0 0 0 0;   border: 6px solid #f3f3f3;   border-radius: 50%;   border-top: 6px solid #3498db;   width: 50px;   height: 50px;   -webkit-animation: spin 2s linear infinite;   animation: spin 2s linear infinite; } @-webkit-keyframes spin {  0%  {  -webkit-transform: rotate(0deg);  }  100% {  -webkit-transform: rotate(360deg); }}@keyframes spin {  0% { transform: rotate(0deg); }  100% { transform: rotate(360deg); }} body { background-color: #1451AE; color: #ffffff; font-size: 20px; font-weight: bold; margin: 0 0 0 0.0; padding: 0.4em 0.4em 0.4em 0.6em;}   #msgfmt { font-size: 16px; font-weight: normal;}#status { font-size: 16px;  font-weight: normal;}</style></head><center><br><br><br><br><br><p id=\"status\"><div id='loader'></div><br>Config saved<br>Rebooting</p></center></html>";
    webServer.setContentLength(htmStr.length());
    webServer.send(200, "text/html", htmStr);
    delay(1000);
    rp2040.reboot();
  }
  else
  {
   webServer.sendHeader("Location","/config.html");
   webServer.send(302, "text/html", "");
  }
}


void handleConfigHtml()
{
  String tmpCw = "";
  String tmpAp = "";
  if (connectWifi)
  {
    tmpCw = "checked";
  }
  else
  {
    tmpAp = "checked";
  }
  String htmStr = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>Config Editor</title><style type=\"text/css\">body {background-color: #1451AE; color: #ffffff; font-size: 14px;font-weight: bold; margin: 0 0 0 0.0; padding: 0.4em 0.4em 0.4em 0.6em;} input[type=\"submit\"]:hover {background: #ffffff;color: green;}input[type=\"submit\"]:active { outline-color: green; color: green; background: #ffffff; }table {font-family: arial, sans-serif;border-collapse: collapse;}td {border: 1px solid #dddddd;text-align: left;padding: 8px;}th {border: 1px solid #dddddd; background-color:gray;text-align: center;padding: 8px;} input[type=radio] {appearance: none;background-color: #fff;width: 15px;height: 15px;border: 2px solid #ccc;border-radius: 2px;display: inline-grid;place-content: center;}input[type=radio]::before {content: \"\";width: 10px;height: 10px;transform: scale(0);transform-origin: bottom left;background-color: #fff;clip-path: polygon(13% 50%, 34% 66%, 81% 2%, 100% 18%, 39% 100%, 0 71%);}input[type=radio]:checked::before {transform: scale(1);}input[type=radio]:checked{background-color:#00D651;border:2px solid #00D651;} </style></head><body><form action=\"/config.html\" method=\"post\"><center><table><tr><th colspan=\"2\"><center>Access Point</center></th></tr><tr><td>AP SSID:</td><td><input name=\"ap_ssid\" value=\"" + AP_SSID + "\"></td></tr><tr><td>AP PASSWORD:</td><td><input name=\"ap_pass\" value=\"********\"></td></tr><tr><td>AP IP:</td><td><input name=\"web_ip\" value=\"" + Server_IP.toString() + "\"></td></tr><tr><td>SUBNET MASK:</td><td><input name=\"subnet\" value=\"" + Subnet_Mask.toString() + "\"></td></tr><tr><td>USE AP:</td><td><input type=\"radio\" name=\"usewifi\" value=\"false\" " + tmpAp +"></td></tr><tr><th colspan=\"2\"><center>Web Server</center></th></tr><tr><td>WEBSERVER PORT:</td><td><input name=\"web_port\" value=\"" + String(WEB_PORT) + "\"></td></tr><tr><th colspan=\"2\"><center>Wifi Connection</center></th></tr><tr><td>WIFI SSID:</td><td><input name=\"wifi_ssid\" value=\"" + WIFI_SSID + "\"></td></tr><tr><td>WIFI PASSWORD:</td><td><input name=\"wifi_pass\" value=\"********\"></td></tr><tr><td>WIFI HOSTNAME:</td><td><input name=\"wifi_host\" value=\"" + WIFI_HOSTNAME + "\"></td></tr><tr><td>USE WIFI:</td><td><input type=\"radio\" name=\"usewifi\" value=\"true\" " + tmpCw + "></tr><tr><th colspan=\"2\"><center>Auto USB Wait</center></th></tr><tr><td>WAIT TIME(ms):</td><td><input name=\"usbwait\" value=\"" + String(USB_WAIT) + "\"></td></tr></table><br><input id=\"savecfg\" type=\"submit\" value=\"Save Config\"></center></form></body></html>";
  webServer.setContentLength(htmStr.length());
  webServer.send(200, "text/html", htmStr);
}
#endif


#if FANMOD
void handleSetTemp()
{
    if (webServer.hasArg("temp"))
    {
      ftemp = webServer.arg("temp").toInt();
      webServer.send(200, "text/plain", "ok");
    }
    else
    {
      webServer.send(404, "text/plain", "Not Found");
    }
}

void handleFanbin()
{
   if (ftemp < 55 || ftemp > 85){ftemp = 70;}
   uint8_t *fant = (uint8_t *) malloc(sizeof(uint8_t)*sizeof(fan)); 
   memcpy_P(fant, fan, sizeof(fan));
   fant[250] = ftemp; fant[368] = ftemp;
   webServer.send(200, "application/octet-stream", fant, sizeof(fan));
   free(fant);
}
#endif

void handleReboot()
{
  webServer.sendHeader("Content-Encoding", "gzip");
  webServer.send(200, "text/html", rebooting_gz, sizeof(rebooting_gz));
  delay(1000);
  rp2040.reboot();
}


void handleUploadHtml()
{
  webServer.sendHeader("Content-Encoding", "gzip");
  webServer.send(200, "text/html", upload_gz, sizeof(upload_gz));
}


void handleFormatHtml()
{
  webServer.sendHeader("Content-Encoding", "gzip");
  webServer.send(200, "text/html", format_gz, sizeof(format_gz));
}


void handleAdminHtml()
{
  webServer.sendHeader("Content-Encoding", "gzip");
  webServer.send(200, "text/html", admin_gz, sizeof(admin_gz));
}


void handleConsoleUpdate(String rgn)
{
  String Version = "05.050.000";
  String sVersion = "05.050.000";
  String lblVersion = "5.05";
  String imgSize = "0";
  String imgPath = "";
  String xmlStr = "<?xml version=\"1.0\" ?><update_data_list><region id=\"" + rgn + "\"><force_update><system level0_system_ex_version=\"0\" level0_system_version=\"" + Version + "\" level1_system_ex_version=\"0\" level1_system_version=\"" + Version + "\"/></force_update><system_pup ex_version=\"0\" label=\"" + lblVersion + "\" sdk_version=\"" + sVersion + "\" version=\"" + Version + "\"><update_data update_type=\"full\"><image size=\"" + imgSize + "\">" + imgPath + "</image></update_data></system_pup><recovery_pup type=\"default\"><system_pup ex_version=\"0\" label=\"" + lblVersion + "\" sdk_version=\"" + sVersion + "\" version=\"" + Version + "\"/><image size=\"" + imgSize + "\">" + imgPath + "</image></recovery_pup></region></update_data_list>";
  webServer.setContentLength(xmlStr.length());
  webServer.send(200, "text/xml", xmlStr);
}


void handleRebootHtml()
{
  webServer.sendHeader("Content-Encoding", "gzip");
  webServer.send(200, "text/html", reboot_gz, sizeof(reboot_gz));
}


void handleInfo()
{
  FSInfo fs_info;
  FILESYS.info(fs_info);
  String output = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>System Information</title><style type=\"text/css\">body { background-color: #1451AE;color: #ffffff;font-size: 14px;font-weight: bold; margin: 0 0 0 0.0; padding: 0.4em 0.4em 0.4em 0.6em;}</style></head>";
  output += "<hr>###### Software ######<br><br>";
  output += "Firmware version " + firmwareVer + "<br><hr>";
  output += "###### MCU ######<br><br>";
  output += "Chip Id: " + String(rp2040.getChipID()) + "<br>";
  output += "CPU frequency: " + String(rp2040.f_cpu()) + " Hz<br>";
  output += "MCU temp: " + String(analogReadTemp()) + " &deg;C<br>"; 
  output += "Total Heap: " + formatBytes(rp2040.getTotalHeap()) + "<br>";
  output += "Used Heap: " + formatBytes(rp2040.getUsedHeap()) + "<br>";
  output += "Free Heap: " + formatBytes(rp2040.getFreeHeap()) + "<br><hr>";
  output += "###### File system (LittleFS) ######<br><br>";
  output += "Total space: " + formatBytes(fs_info.totalBytes) + "<br>";
  output += "Used space: " + formatBytes(fs_info.usedBytes) + "<br>";
  output += "Block size: " + String(fs_info.blockSize) + "<br>";
  output += "Page size: " + String(fs_info.pageSize) + "<br>";
  output += "Maximum open files: " + String(fs_info.maxOpenFiles) + "<br>";
  output += "Maximum path length: " +  String(fs_info.maxPathLength) + "<br><hr>";
  output += "</html>";
  webServer.setContentLength(output.length());
  webServer.send(200, "text/html", output);
}


#if USECONFIG
void writeConfig()
{
  File iniFile = FILESYS.open("/config.ini", "w");
  if (iniFile) {
  String tmpcw = "false";
  if (connectWifi){tmpcw = "true";}
  iniFile.print("\r\nAP_SSID=" + AP_SSID + "\r\nAP_PASS=" + AP_PASS + "\r\nWEBSERVER_IP=" + Server_IP.toString() + "\r\nWEBSERVER_PORT=" + String(WEB_PORT) + "\r\nSUBNET_MASK=" + Subnet_Mask.toString() + "\r\nWIFI_SSID=" + WIFI_SSID + "\r\nWIFI_PASS=" + WIFI_PASS + "\r\nWIFI_HOST=" + WIFI_HOSTNAME + "\r\nCONWIFI=" + tmpcw + "\r\nUSBWAIT=" + String(USB_WAIT) + "\r\n");
  iniFile.close();
  }
}
#endif


int32_t msc_read_callback (uint32_t lba, void* buffer, uint32_t bufsize)
{
  if (lba > 4){lba = 4;}
  memcpy(buffer, exfathax[lba], bufsize);
  return bufsize;
}


void startFileSystem()
{
  if (FILESYS.begin()) {
#if USECONFIG     
  if (FILESYS.exists("/config.ini")) {
  File iniFile = FILESYS.open("/config.ini", "r");
  if (iniFile) {
  String iniData;
    while (iniFile.available()) {
      char chnk = iniFile.read();
      iniData += chnk;
    }
   iniFile.close();
   
   if(instr(iniData,"AP_SSID="))
   {
   AP_SSID = split(iniData,"AP_SSID=","\r\n");
   AP_SSID.trim();
   }
   
   if(instr(iniData,"AP_PASS="))
   {
   AP_PASS = split(iniData,"AP_PASS=","\r\n");
   AP_PASS.trim();
   }
   
   if(instr(iniData,"WEBSERVER_PORT="))
   {
   String strWprt = split(iniData,"WEBSERVER_PORT=","\r\n");
   strWprt.trim();
   WEB_PORT = strWprt.toInt();
   }
   
   if(instr(iniData,"WEBSERVER_IP="))
   {
    String strwIp = split(iniData,"WEBSERVER_IP=","\r\n");
    strwIp.trim();
    Server_IP.fromString(strwIp);
   }

   if(instr(iniData,"SUBNET_MASK="))
   {
    String strsIp = split(iniData,"SUBNET_MASK=","\r\n");
    strsIp.trim();
    Subnet_Mask.fromString(strsIp);
   }

   if(instr(iniData,"WIFI_SSID="))
   {
   WIFI_SSID = split(iniData,"WIFI_SSID=","\r\n");
   WIFI_SSID.trim();
   }
   
   if(instr(iniData,"WIFI_PASS="))
   {
   WIFI_PASS = split(iniData,"WIFI_PASS=","\r\n");
   WIFI_PASS.trim();
   }
   
   if(instr(iniData,"WIFI_HOST="))
   {
   WIFI_HOSTNAME = split(iniData,"WIFI_HOST=","\r\n");
   WIFI_HOSTNAME.trim();
   }
   
   if(instr(iniData,"CONWIFI="))
   {
    String strcw = split(iniData,"CONWIFI=","\r\n");
    strcw.trim();
    if (strcw.equals("true"))
    {
      connectWifi = true;
    }
    else
    {
      connectWifi = false;
    }
   }
   if(instr(iniData,"USBWAIT="))
   {
    String strusw = split(iniData,"USBWAIT=","\r\n");
    strusw.trim();
    USB_WAIT = strusw.toInt();
   }
    }
  }
  else
  {
   writeConfig(); 
  }
#endif
  }
  hasStarted = true;
}


void loadAP()
{
   WiFi.softAPConfig(Server_IP, Server_IP, Subnet_Mask);
   WiFi.softAP(AP_SSID, AP_PASS);
   dnsServer.setTTL(30);
   dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
   dnsServer.start(53, "*", Server_IP);
}


void loadSTA()
{
   WiFi.setHostname(WIFI_HOSTNAME.c_str());
   WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
   if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      loadAP();
   } else {
      IPAddress LAN_IP = WiFi.localIP(); 
      if (LAN_IP)
      {
         String mdnsHost = WIFI_HOSTNAME;
         mdnsHost.replace(".local","");
         MDNS.begin(mdnsHost, LAN_IP);
         dnsServer.setTTL(30);
         dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
         dnsServer.start(53, "*", LAN_IP);
      }
   }
}


void setup() {

  msc.setID("PS4", "PICO Server", "1.0");
  msc.setCapacity(8192, 512);
  msc.setReadWriteCallback(msc_read_callback, 0, 0);
  msc.setUnitReady(true);
  msc.begin();
  dev.detach();
  
  startFileSystem();

  webServer.onNotFound(handleNotFound);
  webServer.on("/upload.html", HTTP_POST, []() {
  webServer.sendHeader("Location","/fileman.html");
  webServer.send(302, "text/html", "");
  }, handleFileUpload);
  webServer.on("/upload.html", HTTP_GET, handleUploadHtml);
  webServer.on("/format.html", HTTP_GET, handleFormatHtml);
  webServer.on("/format.html", HTTP_POST, handleFormat);
  webServer.on("/fileman.html", HTTP_GET, handleFileMan);
  webServer.on("/info.html", HTTP_GET, handleInfo);
  webServer.on("/delete", HTTP_POST, handleDelete);
#if USECONFIG
  webServer.on("/config.html", HTTP_GET, handleConfigHtml);
  webServer.on("/config.html", HTTP_POST, handleConfig);
#endif
  webServer.on("/admin.html", HTTP_GET, handleAdminHtml);
  webServer.on("/reboot.html", HTTP_GET, handleRebootHtml);
  webServer.on("/reboot.html", HTTP_POST, handleReboot);
#if FANMOD
  webServer.on("/fant.bin", HTTP_GET, handleFanbin);
  webServer.on("/setftemp", HTTP_POST, handleSetTemp);
#endif  
  webServer.begin(WEB_PORT);
}


void loop() {
  if (hasEnabled && millis() >= (enTime + 15000))
  {
       enTime = 0;
       hasEnabled = false;
       dev.detach();
  } 
  webServer.handleClient();
}


void setup1() {
  while (!hasStarted)
  {
    delay(1000);
  }
  if (connectWifi && WIFI_SSID.length() > 0 && WIFI_PASS.length() > 0)
  {
    loadSTA();
  }
  else
  {
    loadAP();
  }
}


void loop1() {
  dnsServer.processNextRequest();
  MDNS.update();
}
