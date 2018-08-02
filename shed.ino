#include <StaticThreadController.h>
#include <Thread.h>
#include <ThreadController.h>
#include <DHT.h> // библиотека для датчика температуры и влажности DHT22

#include <OneWire.h>
#include <SoftwareSerial.h>
#include <LiquidCrystal.h>

#define DHTPIN1 30 // 1 DHT22 температуры и влажности
#define DHTPIN2 31 // 2 DHT22 температуры и влажности
#define DHTPIN3 32 // 3 DHT22 температуры и влажности

#define btnNONE   0
#define btnRIGHT  1
#define btnUP     2
#define btnDOWN   3
#define btnLEFT   4
#define btnSELECT 5

// select the pins used on the LCD panel
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// Инициируем датчик
DHT dht1 = DHT(DHTPIN1, DHT22);
DHT dht2 = DHT(DHTPIN2, DHT22);
DHT dht3 = DHT(DHTPIN3, DHT22);
DHT arrayDHT22[] = {dht1,dht2,dht3};
//GSM модуль
// SoftwareSerial mySerial(10, 11); // RX, TX с arduino Mega это не работает, пришлось использовать Serial1

int gerconZamok1 = 23;
int gerconZamok2 = 24;
String currStr = "";
// если эта строка сообщение, то переменная примет значение True
boolean isStringMessage = false;
// датчики температуры и влажности,во вне функций чтобы сохранять старое значение и не обновляться каждый раз
String temp1,temp2,temp3;
int arrayRELAY[] = {47,46,45,44,43,42,41,40};
String arrayR8[] = {"-","-","-","-","-","-","-","-"};
String phone = "";// номер телефона для управления формат 79220000000
int fireA10 = A10;
int fireSOS = 150; // при этом значении дыма вкл SOS
int movPin = 22;

Thread gsmThread = Thread(); // создаём поток управления GSM модулем
Thread zamokThread = Thread(); // создаём поток управления Сигнализацией (гирконы)
Thread LCDThread = Thread(); // создаём поток для обслуживания LCD панели
Thread dtnPress = Thread(); // создаём поток для отслеживания нажатой кнопки

void setup(void) {
  Serial.begin(2400);
  Serial1.begin(2400);
  dht1.begin();
  Serial.println("Goodnight moon!");
  lcd.begin(16, 2);              // start the library
  lcd.setCursor(0,0);
  Serial1.print("AT");
  delay(100);
  Serial1.print("AT+CMGF=1\r");
  delay(500); // задержка на обработку команды
  Serial1.print("AT+IFC=1, 1\r");
  delay(500);
  Serial1.print("AT+CPBS=\"SM\"\r");
  delay(500); // задержка на обработку команды
  Serial1.print("AT+CNMI=1,2,2,1,0\r");
  delay(700);
  sms(String("reset Arduino"),String(phone));

  pinMode(fireA10, INPUT); // init датчик дыма
  pinMode(movPin, INPUT); // init датчик движения
  pinMode(gerconZamok1, INPUT); // init датчик геркон 1 (Zamok)
  pinMode(gerconZamok2, INPUT); // init датчик геркон 2 (Zamok)
  digitalWrite(gerconZamok1, HIGH);
  digitalWrite(gerconZamok2, HIGH);

  for (int i = 0; i < 8; i++) {
    pinMode(arrayRELAY[i], OUTPUT);
  }
  delay(200);
  for (int i = 0; i < 8; i++) {
    digitalWrite(arrayRELAY[i], HIGH);
  }
  gsmThread.onRun(gsm);      // назначаем потоку задачу
  gsmThread.setInterval(1); // задаём интервал срабатывания, мсек

  zamokThread.onRun(zamok);      // назначаем потоку задачу
  zamokThread.setInterval(1000); // задаём интервал срабатывания, мсек

  LCDThread.onRun(LCD);        // назначаем потоку задачу
  LCDThread.setInterval(500);   // задаём интервал срабатывания, мсек
  dtnPress.onRun(read_LCD_buttons);        // назначаем потоку задачу
  dtnPress.setInterval(10);   // задаём интервал срабатывания, мсек
}

void loop() {
  if (gsmThread.shouldRun()) {
    gsmThread.run(); // запускаем поток
  }
  if (zamokThread.shouldRun()) {
    zamokThread.run(); // запускаем поток
  }
  if (LCDThread.shouldRun()) {
    LCDThread.run(); // запускаем поток
  }
  if (dtnPress.shouldRun()) {
    dtnPress.run(); // запускаем поток
  }
}

int g1 = 0;// 0 - закрыто, 1 - открыто
int g2 = 0;// 0 - закрыто, 1 - открыто
int timeZamokDefaul = 60;//60 sec
int timeZamok = timeZamokDefaul;//60 sec
boolean openg1 = false;
boolean openg2 = false;
boolean block = true;
boolean selectBlock = false;

boolean zamokSmsTimer = true;
int zamokSmsTimerCount = timeZamokDefaul;// через сколько секунд будет повторно отправляться sms о сигнализации

void zamok() {
  g1 = digitalRead(gerconZamok1);
  g2 = digitalRead(gerconZamok2);
  if (block) {
    if (g1 == 1 && openg1 == false) {
      openg1 = true;
    }
    if (g2 == 1 && openg2 == false) {
      openg2 = true;
    }
    if (openg1 || openg2) {
      if (timeZamok == 0) {
        //sms сделать отправку в 60 sec
        if (zamokSmsTimerCount == 0) {
          zamokSmsTimer = true;
          zamokSmsTimerCount = timeZamokDefaul;
        } else if (!zamokSmsTimer) {
          zamokSmsTimerCount--;
        }
        if (zamokSmsTimer) {
          if (openg1 && openg2) {
            Serial.println("zamok: SOS KALITKA!!!");
            sms(String("zamok: SOS KALITKA!!!\rzamok: SOS BOPOTA!!!"),String(phone));
          } else if (openg1 && !openg2) {
            Serial.println("zamok: SOS KALITKA!!!");
            sms(String("zamok: SOS KALITKA!!!"),String(phone));
          } else if (!openg1 && openg2) {
            Serial.println("zamok: SOS BOPOTA!!!");
            sms(String("zamok: SOS BOPOTA!!!"),String(phone));
          } 
          zamokSmsTimer = false;
        }
      } else {
        timeZamok--;
        Serial.println("sms zamok: SOS!!! ->"+String(timeZamok));
      }
    }
    motionSensor();
    fire();
  }
  if (!block && selectBlock) {
    //запускаем таймер и после ставим на block
    if (timeZamok == 0) {
      block = true;
      openg1 = false;
      openg2 = false;
      timeZamok = timeZamokDefaul;
      zamokSmsTimerCount = timeZamokDefaul;

      selectBlock = false;
      Serial.println("zamok: on");
    } else {
      timeZamok--;
      Serial.println("zamok: on -> "+String(timeZamok));
    }
  }
  if (block && selectBlock) {
    //запускаем таймер и после снимаем block
    block = false;
    timeZamok = timeZamokDefaul;
    selectBlock = false;
    Serial.println("zamok: off");
  }
  Serial.println(String(g1)+" - "+String(g2));
}

int motionCount = 0;
boolean motionTimer = false;
void motionSensor() {
  int val = digitalRead(movPin);
  if (motionCount == 60) {// def 60 sec
    releControl(6,"-");
    motionTimer = false;
    motionCount = 0;
  } else if(motionTimer) {
    motionCount++;
    // Serial.println("motionCount++ "+String(motionCount));  
  }
  if (val == 1 && !motionTimer) {
    releControl(6,"+");
    motionTimer = true;
  }
}
void gsm() {
  if (!Serial1.available()) {
    return;
  }
  char currSymb = Serial1.read();    
  if ('\r' == currSymb) {
      if (isStringMessage) {
        Serial.println("SMS send");
        Serial.println("currStr"+currStr);
          //если текущая строка - SMS-сообщение,
          //отреагируем на него соответствующим образом
          if (!currStr.compareTo("/1 on")) {
            releControl(0,"+");
          } else if (!currStr.compareTo("/1 off")) {
            releControl(0,"-");
          }
          if (!currStr.compareTo("/2 on")) {
            releControl(1,"+");
          } else if (!currStr.compareTo("/2 off")) {
            releControl(1,"-");
          } 
          if (!currStr.compareTo("/3 on")) {
            releControl(2,"+");
          } else if (!currStr.compareTo("/3 off")) {
            releControl(2,"-");
          } 
          if (!currStr.compareTo("/4 on")) {
            releControl(3,"+");
          } else if (!currStr.compareTo("/4 off")) {
            releControl(3,"-");
          } 
          if (!currStr.compareTo("/5 on")) {
            releControl(4,"+");
          } else if (!currStr.compareTo("/5 off")) {
            releControl(4,"-");
          } 
          if (!currStr.compareTo("/6 on")) {
            releControl(5,"+");
          } else if (!currStr.compareTo("/6 off")) {
            releControl(5,"-");
          } 
          if (!currStr.compareTo("/7 on")) {
            releControl(6,"+");
          } else if (!currStr.compareTo("/7 off")) {
            releControl(6,"-");
          } 
          if (!currStr.compareTo("/8 on")) {
            releControl(7,"+");
          } else if (!currStr.compareTo("/8 off")) {
            releControl(7,"-");
          } 
         
          if (!currStr.compareTo("/s")) {
            String t1,t2,t3;
            temperatur(t1,t2,t3);
            
            String zamok1 = "off";
            String zamok2 = "off";
            if (g1 == 1) {
              zamok1 = "on";
            }
            if (g2 == 1) {
              zamok2 = "on";
            }
            sms(String("1 "+arrayR8[0]+" 2 "+arrayR8[1]+" 3 "+arrayR8[2]+" 4 "+arrayR8[3]+'\r'+
                       "5 "+arrayR8[4]+" 6 "+arrayR8[5]+" 7 "+arrayR8[6]+" 8 "+arrayR8[7]+'\r'+
                       "t1: "+t1+" t2: "+t2+" t3: "+t3+'\r'+
                       sensorFire()+'\r'+
                       "zamok 1: "+zamok1+'\r'+
                       "zamok 2: "+zamok2),String(phone));
          }
          if (!currStr.compareTo("/off")) {
            for (int i = 0; i < 8; i++) {
              releControl(i,"-");
            }
          }
          if (!currStr.compareTo("/help")) {
            sms(String("/1 on/off /2 on/off /3 on/off /4 on/off\r/5 on/off /6 on/off /7 on/off /8 on/off\r/s - statistika\r/off - all off\r/z on/off\r/help-help!!"),String(phone));
          }
          if (!currStr.compareTo("/z on")) {
            block = true;
            openg1 = false;
            openg2 = false;
            timeZamok = timeZamokDefaul;
            zamokSmsTimerCount = timeZamokDefaul;
          }
          if (!currStr.compareTo("/z off")) {
            block = false;
            timeZamok = timeZamokDefaul;
          }
          isStringMessage = false;
      } else {
          if (currStr.startsWith("+CMT")) {
            if (currStr.substring(8,19) == phone) {
              isStringMessage = true;
            }
          }
      }
      currStr = "";
  } else if ('\n' != currSymb) {
      currStr += String(currSymb);
  }
}

void sms(String text, String phone) {
  Serial.println("SMS send started"); //AT+CMGF=1
  Serial1.println("AT+CMGF=1");
  delay(300);
  Serial1.println("AT+CMGS=\"+" + phone + "\"");
  delay(1000);
  Serial1.print(text);
  delay(500);
  Serial1.print((char)26);
  delay(700);
  Serial.println("SMS send finish");
}


boolean fireTimer = true;
int fireCount = 60;

void fire() {
  if (fireCount == 0) {
    fireTimer = true;
    fireCount = 60;
  } else if (!fireTimer) {
    fireCount--;
  }
  int analogSensor = analogRead(fireA10);
  Serial.println(sensorFire());
  // Проверяем, достигнуто ли пороговое значение
  if (fireTimer && analogSensor > fireSOS){//150 дым-пожар примерно in fireSOS
    sms(String(sensorFire()+'\r'+"FIRE !!!! SOS !!!!"),String(phone));
    Serial.print(sensorFire()+'\r'+"FIRE !!!! SOS !!!!");
    fireTimer = false;
  }
}

String sensorFire() {
  return "sensorFire: "+String(analogRead(fireA10));
}
void releControl(int index,String vol) {
  if (vol == "+") {
    arrayR8[index] = "+";
    digitalWrite(arrayRELAY[index], LOW);
  } else {
    arrayR8[index] = "-";
    digitalWrite(arrayRELAY[index], HIGH);
  }
}
void temperatur(String & temp1, String & temp2, String & temp3) {
  String arraTemp[] = {"","",""};
  for (int i = 0; i < 3; i++) {
    float h = arrayDHT22[i].readHumidity();//Считываем влажность
    float t = arrayDHT22[i].readTemperature();// Считываем температуру
    // Проверка удачно прошло ли считывание.
    if (isnan(h) || isnan(t)) {
      arraTemp[i] = "0%0C";
    }
    arraTemp[i] = String(int(h))+"%"+""+String(int(t))+"C";
  }
  temp1 = arraTemp[0];
  temp2 = arraTemp[1];
  temp3 = arraTemp[2];
}

int step = 0;
int lcd_key = 0;
int relCursor = 0;
boolean btnNONEPRESS = true;// Это нужно для того чтобы считывалась нажатая кнопка после отжатия только один раз
// LCD
void LCD() {
  lcd.setCursor(0,0);            // move to the begining of the second line

  if (btnNONEPRESS) {// если true то кнопка отжата и можно считать показания какую кнопку нажали
    switch (lcd_key) {             // depending on which button was pushed, we perform an action
      case btnRIGHT:{
        clearLine();
        relCursor++;
        if (relCursor > 7) relCursor = 0;
        break;
      }
      case btnLEFT:{
        clearLine();
        relCursor--;
        if (relCursor < 0) relCursor = 7;
        break;
      }
      case btnUP:{
        clearLine();
        step++;
        if (step > 2) step = 0;
        break;
      }
      case btnDOWN:{
        clearLine();
        step--;
        if (step < 0) step = 2;
        break;
      }
      case btnSELECT:{
        clearLine();
        //zamok
        if (step == 0) {
          selectBlock = true;
        }
        //rele
        if (step == 1) {
          if (arrayR8[relCursor] == "-") {
            releControl(relCursor,"+");
          } else {
            releControl(relCursor,"-");
          }
        }
        break;
      }
    }
    lcd_key = btnNONE; // 0 для предотвращения бесконечного считывания кнопки, обнуляем её
    renderLCD();
  }
  
}
void renderLCD() {
  if (step == 0) {
    if ((block == true && openg1 == false ) && (block == true && openg2 == false )) {
      clearLine();
      lcd.print("ZAMOK: on");
    } else if ((block == true && openg1 == true ) || (block == true && openg2 == true )) {
      if (timeZamok != 0) {
        clearLine();
        lcd.print("ZAMOK: "+String(timeZamok));
      } else {
        clearLine();
        lcd.print("ZAMOK: SOS");
      }
    } else if (!block && selectBlock) {
      if (timeZamok != 0) {
        clearLine();
        lcd.print("ZAMOK: "+String(timeZamok));
      }
    } else if (!block) {
      clearLine();
      lcd.print("ZAMOK: off");
    }
  }
  if (step == 1) {
    //rele status
    String lsdLine;
    for (int i = 0; i < 8; i++) {
      String newLine = String(i+1)+" "+arrayR8[i]+" ";
      if(relCursor == i) {
        newLine = String(i+1)+"["+arrayR8[i]+"]";
      }
      lsdLine += newLine;
    }
    lcd.print(lsdLine.substring(0,16));
    lcd.setCursor(0,1);
    lcd.print(lsdLine.substring(16));
  }
  if (step == 2) {
    // градусники
    String newTemp1,newTemp2,newTemp3;
    temperatur(newTemp1,newTemp2,newTemp3);
    if (temp1 != newTemp1 || temp2 != newTemp2 || temp3 != newTemp3) {
      temp1 = newTemp1; temp2 = newTemp2; temp3 = newTemp3;
    }
    // Serial.println(temp1+"|"+temp2+"|"+temp3);
    lcd.print("|"+temp1+"||"+temp2+"|");
    lcd.setCursor(0,1);
    lcd.print("    |"+temp3+"|");
  }
}
void clearLine() {
  lcd.setCursor(0, 0);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 0);
}

int adc_key_in  = 0;
// определяем нажатую кнопку и возвращаем название
void read_LCD_buttons() {
  adc_key_in = analogRead(A0);
  if (adc_key_in < 100)   {
    if(lcd_key == btnNONE) {
      lcd_key = btnRIGHT; // 1
      btnNONEPRESS = false;
    }
  } else if (adc_key_in < 200)  {
    if(lcd_key == btnNONE) {
      lcd_key = btnUP; // 2
      btnNONEPRESS = false;
    }
  } else if (adc_key_in < 400)  {
    if(lcd_key == btnNONE) {
      lcd_key = btnDOWN; // 3
      btnNONEPRESS = false;
    }
  } else if (adc_key_in < 600)  {
    if(lcd_key == btnNONE) {
      lcd_key = btnLEFT; // 4
      btnNONEPRESS = false;
    }
  } else if (adc_key_in < 800)  {
    if(lcd_key == btnNONE) {
      lcd_key = btnSELECT; // 5
      btnNONEPRESS = false;
    }
  } else if (adc_key_in > 1000) {
    // lcd_key = btnNONE; // 0
    btnNONEPRESS = true;
  }
}