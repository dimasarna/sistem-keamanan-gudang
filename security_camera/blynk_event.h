String ip2Str(IPAddress ip){
  String s="";
  for (int i=0; i<4; i++) {
    s += i  ? "." + String(ip[i]) : String(ip[i]);
  }
  return s;
}

BLYNK_CONNECTED() {
  String url = "http://" + ip2Str(WiFi.localIP());
  Blynk.setProperty(V1, "url", url.c_str());
  Blynk.syncAll();
}
