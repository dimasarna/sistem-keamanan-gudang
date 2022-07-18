const char* token = "5593489325:AAHkoog4XIb2hLRbuLtPXWIjIORKMjrfuw4";
const char* channel = "@pusat_notifikasi";
String message = "Terdapat kebakaran!!";

void vSetupTelegramBot() {
  bot.setClock("WIB-7");
  bot.setUpdateTime(500);
  bot.setTelegramToken(token);

  _PL();
  _PL("Test Telegram connection... ");
  bot.begin() ? _PL("OK") : _PL("NOK");
}

void vSendTelegram() {
  if (xDB1.alarm == 1 || xDB2.alarm == 1)
    bot.sendToChannel(channel, message, true);
}
