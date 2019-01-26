/*
  FishinoWeatherStation.ino
  Emanuele  Signoretta

  July 2017

  This sketch allows you to collect values from different sensors:
  -MQ7 : for the CO concentration in the air;
  -DHT 11/22 : for the humidity;
  -DS18b20 : for the temperature;
  -BMP180 : for the pressure (measured in mb or Hpa);
  -ML8511 : for the UV intensity;
  -Wind 01 or Wind 02 : for the wind speed;
  -Rain-mod : to detect rain;
  -AS3935 : to detect lightnings on a maximum distance of 40 km.

  Then the sketch posts these data on a web page, and:
  -every 5 minutes it saves the data (including day and time of the measuerement) on a micro-sd datalog file ;
  -every 60 minutes it sends you a push notification by the Pushetta web app;
  -if the system detects a stroke, it will immediatly save all the data and send you a push notification.

*/

//include libraries
//includo le librerie

#include<MQ7.h>
#include <DHT.h>
#include <DHT_U.h>
#include <Every.h>
#include <Fishetta.h>
#include <Fishino.h>
#include <FishinoRTC.h>
#include <FishinoAS3935.h>
#include <FishinoDallasTemperature.h>
#include <FishinoWebServer.h>
#include <ML8511.h>
#include <OneWire.h>
#include <SD.h>
#include <SFE_BMP180.h>
#include <SPI.h>
#include <Wind.h>
#include <Wire.h>
#include <FishinoFlash.h>


//Default language for indexHandler, datalog and push notifications. Use EN for English and DE for German.
//Lingua predefinita per l' indexHandler, il datalog e le notifiche push. Cambiare il valore in EN per l'inglese e in DE per il tedesco.
#define IT
#define AS3935_ADDRESS  0x03 //AS3935 default i2c address. Indirizzo i2c predefinito del sensore AS3935
#define IRQ_PIN     3 //AS3935 IRQ pin.
#define CS_PIN      0 //AS3935 CS pin. NOT USED! NON USATO!
#define MQ7_VRL_PIN  A1 //MQ7 pin
#define wind_pin  9 //Anemometer pin
#define rainpin  7 //Rainmod pin
#define UVOUT  A0 //ML8511 pin
#define REF_3V3  A5 //Reference pin for ML8511. Reference pin per il sensore ML8511
#define DHTPIN 5 //DHT pin
#define ONE_WIRE_BUS 8 //DS18B20 pin
#define statusled_green 17 //Green status led pin. Pin del led di stato verde
#define statusled_red 16 //Red status led pin. Pin del led di stato rosso
#define DHTTYPE DHT22 //DHT sensor type, change it in case of differnt sensor type. Modello di sensore DHT, cambiare in funzione del modello usato.
//#define ALTITUDE 650.0 //Location altitude, set it before uploading the sketch. Altitudine della località, impostarla prima di caricare lo sketch.




//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
// CONFIGURATION DATA    -- ADAPT TO YOUR NETWORK !!!
// DATI DI CONFIGURAZIONE -- ADATTARE ALLA PROPRIA RETE WiFi !!!
#ifndef __MY_NETWORK_H

// OPERATION MODE :
// NORMAL (STATION) -- NEEDS AN ACCESS POINT/ROUTER
// STANDALONE (AP)  -- BUILDS THE WIFI INFRASTRUCTURE ON FISHINO
// COMMENT OR UNCOMMENT FOLLOWING #define DEPENDING ON MODE YOU WANT
// MODO DI OPERAZIONE :
// NORMAL (STATION) -- HA BISOGNO DI UNA RETE WIFI ESISTENTE A CUI CONNETTERSI
// STANDALONE (AP)  -- REALIZZA UNA RETE WIFI SUL FISHINO
// COMMENTARE O DE-COMMENTARE LA #define SEGUENTE A SECONDA DELLA MODALITÀ RICHIESTA
// #define STANDALONE_MODE

// here pur SSID of your network
// inserire qui lo SSID della rete WiFi
#define MY_SSID  "TIM-Signoretta"

// here put PASSWORD of your network. Use "" if none
// inserire qui la PASSWORD della rete WiFi -- Usare "" se la rete non ￨ protetta
#define MY_PASS "Signoretta63649700"

// here put required IP address of your Fishino
// comment out this line if you want AUTO IP (dhcp)
// NOTES :
//    for STATION_MODE if you use auto IP you must find it somehow !
//    for AP_MODE you MUST choose a free address range
// here put required IP address (and maybe gateway and netmask!) of your Fishino
// comment out this lines if you want AUTO IP (dhcp)
// NOTE : if you use auto IP you must find it somehow !
// inserire qui l'IP desiderato ed eventualmente gateway e netmask per il fishino
// commentare le linee sotto se si vuole l'IP automatico
// nota : se si utilizza l'IP automatico, occorre un metodo per trovarlo !
#define IPADDR  192, 168,   1, 132
#define GATEWAY 192, 168,   1, 1
#define NETMASK 255, 255, 255, 0

#endif

const char * APIKEY  = "49a718b15815a7adef0be97e09654fc0b2a4f62b" ; // Put here your Pushetta API key. Inserire qui la propria chiave API di Pushetta.
const char * CHANNEL  = "Stazione meteo"; // Put here your Pushetta channel name. Inserire qui il nome del proprio canale Pushetta.
//
//
// NOTE : for prototype green version owners, set SD_CS to 3 !!!
// NOTA : per i possessori del prototipo verde di Fishino, impostare SD_CS a 3 !!!
#ifdef SDCS
const int SD_CS = SDCS;
#else
const int SD_CS = 4;
#endif


//
// END OF CONFIGURATION DATA
// FINE DATI DI CONFIGURAZIONE
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

// define ip address if required
// NOTE : if your network is not of type 255.255.255.0 or your gateway is not xx.xx.xx.1
// you should set also both netmask and gateway
#ifdef IPADDR
IPAddress ip(IPADDR);
#ifdef GATEWAY
IPAddress gw(GATEWAY);
#else
IPAddress gw(ip[0], ip[1], ip[2], 1);
#endif
#ifdef NETMASK
IPAddress nm(NETMASK);
#else
IPAddress nm(255, 255, 255, 0);
#endif
#endif

// the web server object. L'oggetto web server
FishinoWebServer web(80);

// sd card stuffs. Gli oggetti della scheda sd
Sd2Card card;
SdVolume volume;
SdFile root;
SdFile file;
SdFile datalog;

// Define the variables. Definisco le variabili.
bool rain = false;
bool temp = false;
bool uv_sens = false;
bool hum_sens  = false;
bool pressure_sens = false;
double uvIntensity = 0 ;
double ppm = 0;
double speed = 0;
double temperatura = 0;
double hum = 0 ;
double Pressure = 0;
double P,T;
uint32_t  getUtcFromNtp(void);
uint32_t  rtcEpochTime;
uint32_t epochNtp, epochTz;
uint32_t rtcEpochNow;
int startup_activity = 1;


DHT dht(DHTPIN, DHTTYPE); //Create the dht object. Creo l'oggetto dht
Fishetta push(CHANNEL, APIKEY);// Create the push object. Creo l'oggetto push
FishinoAS3935 AS3935(AS3935_ADDRESS, CS_PIN, IRQ_PIN);//Create the AS3935 object. Creo l'oggetto AS3935
OneWire oneWire(ONE_WIRE_BUS);//Create the oneWire object. Creo l'oggetto oneWire
DallasTemperature sensors(&oneWire);//Create the sensors object. Creo l'oggetto sensors
ML8511 uv(UVOUT, REF_3V3); //Create the uv object. Creo l'oggetto uv
MQ7  mq7(MQ7_VRL_PIN, 3.3); //Create the mq7 object. Creo l'oggetto mq7
SFE_BMP180 pressure; //Create the pressure object. Creo l'oggetto pressure
Wind wind(wind_pin);// Create the wind object. Creo l'oggetto wind




// interrupt trigger global var
//variabile globale del triggere dell'interrupt
volatile int8_t AS3935IrqTriggered = 0;

// This is irq handler for AS3935 interrupts, has to return void and take no arguments. Always make code in interrupt handlers fast and short
// Questo è l'handler per gli interrupts dell'AS3935, deve essere di tipo void e non ha bisogno di argomenti. Fare sempre corto e veloce il codice nell'handler degli interrupts
void AS3935_ISR() {
	// digitalWrite(25, HIGH);
	AS3935IrqTriggered = 1;
}


//For DEBUG: prints the AS3935 register's value on the serial monitor. Per il DEBUG: stampa sul monitor seriale i valori dei registri dell'AS3935
void printAS3935Registers() {
	int noiseFloor = AS3935.getNoiseFloor();
	int spikeRejection = AS3935.getSpikeRejection();
	int watchdogThreshold = AS3935.getWatchdogThreshold();
	Serial.print("Noise floor is: ");
	Serial.println(noiseFloor, DEC);
	Serial.print("Spike rejection is: ");
	Serial.println(spikeRejection, DEC);
	Serial.print("Watchdog threshold is: ");
	Serial.println(watchdogThreshold, DEC);
}




void connectToAp(void) {

	Serial << F("Ram libera: ") << Fishino.freeRam() << "\r\n";

	// reset and test wifi module. Resetta e testa il modulo wifi
	Serial << F("Resettando il fishino...");

	while (!Fishino.reset()) {
			Serial << ".";
			delay(500);
		}

	Serial << F("OK\r\n");

	// set PHY mode 11G. Setta il PHY mode a 11G
	Fishino.setPhyMode(PHY_MODE_11G);


	Fishino.setMode(STATION_MODE);

	// NOTE : INSERT HERE YOUR WIFI CONNECTION PARAMETERS !!!!!!
	Serial << F("Connessione all' AP...");

	while (!Fishino.begin(F(MY_SSID), F(MY_PASS))) {
			Serial << ".";
			delay(500);
		}

	Serial << F("OK\r\n");


	// setup IP or start DHCP server
#ifdef IPADDR
	Fishino.config(ip, gw, nm);
#else
	Fishino.staStartDHCP();
#endif

	// wait for connection completion
	Serial << F("Aspettando l'IP...");

	while (Fishino.status() != STATION_GOT_IP) {
			Serial << ".";
			delay(500);
		}

	Serial << F("OK\r\n");

	// print current IP address
	Serial << F("Indirizzo IP :") << Fishino.localIP() << "\r\n";
}


//
//  Get UTC time
//
uint32_t wifiGetUtc() {
	int i;
	uint32_t epoch2k;
	uint32_t retval = 0;


	for (i = 0; i < 10; i++) {

			uint32_t secsSince1900 = Fishino.ntpEpoch();
			// Unix time starts on Jan 1 1970. In seconds, that's 2208988800
			// il tempo Unix inizia dal primo Gennaio 1970. In secondi, sono 2208988800
			// const unsigned long seventyYears = 2208988800UL;

			while (secsSince1900 <= 1893459661) {
					secsSince1900 = Fishino.ntpEpoch();
				}

			const uint32_t secondsTo2k = 3155673600UL;

			// subtract seventy years
			// sottrae dal tempo NTP la base Unix
			epoch2k = secsSince1900 - secondsTo2k;

			Serial << F(" \n Secondi trascorsi dal 1 Gennaio 1900 = ") << secsSince1900 << "\n";

			// print Unix time:
			// stampa il tempo Unix
			Serial << F("Tempo Unix = ") << epoch2k << "\n";

			// now convert NTP time into everyday time
			// ora convertiamo il tempo NTP in formato leggibile
			// print the hour, minute and second
			// stampa ora, minuti e secondi

			// UTC is the time at Greenwich Meridian (GMT)
			// Tempo UTC (ora al meridiano di Greenwich, GMT)
			Serial << F("Ora UTC: ");

			// print the hour (86400 equals secs per day)
			// stampa l'ora (contando 86400 secondi al giorno
			Serial << ((epoch2k  % 86400L) / 3600);

			Serial.print(':');

			if (((epoch2k % 3600) / 60) < 10) {
					// In the first 10 minutes of each hour, we'll want a leading '0'
					// nei primi 10 minuti di ogni ora vogiamo uno zero iniziale
					Serial << '0';
				}

			// print the minute (3600 equals secs per minute)
			// stampa i minuti (contando 3600 secondi per minuto)
			Serial << ((epoch2k  % 3600) / 60);

			Serial << ':';

			if ((epoch2k % 60) < 10) {
					// In the first 10 seconds of each minute, we'll want a leading '0'
					// nei primi 10 secondi di ogni minuto vogliamo lo zero iniziale
					Serial << '0';
				}

			// print the second
			// stampa i secondi
			Serial << epoch2k % 60 << "\n";

			retval = epoch2k;


			if (retval > 0)
				break;
		}

	return retval;
}

//For DEBUG: prints date and time on the serial monitor. Per il DEBUG: stampa data e ora sul monitor seriale
void showDate(const char* txt, const DateTime &dt)

{
	Serial.print(txt);
	Serial.print(' ');
	Serial.print(dt.year(), DEC);
	Serial.print('/');
	Serial.print(dt.month(), DEC);
	Serial.print('/');
	Serial.print(dt.day(), DEC);
	Serial.print(' ');


	// if hours are less than 10 it will print a 0 first and then the value

	if (dt.hour() < 10) {
			Serial.print("0");
			Serial.print(dt.hour(), DEC);
		}

	else
		Serial.print(dt.hour(), DEC);

	Serial.print(':');

	//if minutes are less than 10 it will print a 0 first and then the value
	if (dt.minute() < 10) {
			Serial.print("0");
			Serial.print(dt.minute(), DEC);
		}

	else
		Serial.print(dt.minute(), DEC);

	Serial.print(':');

	//if seconds are less than 10 it will print a 0 first and then the value
	if (dt.second() < 10) {
			Serial.print("0");
			Serial.print(dt.second(), DEC);
		}

	else
		Serial.print(dt.second(), DEC);

	Serial.println();
}


// sends a file to client. Invia file al client
void sendFile(FishinoWebServer& web, const char* filename) {
	Serial << F("Ram libera: ") << Fishino.freeRam() << "\r\n";

	if (!filename) {
			web.sendErrorCode(404);
			web << F("Could not parse URL");
		}

	else {
			FishinoWebServer::MimeType mimeType = web.getMimeTypeFromFilename(filename);
			web.sendErrorCode(200);
			web.sendContentType(mimeType);

			// ask to cache image types
			// controlla se alcuni tipi di immagini sono memorizzati nella cache

			if (
				mimeType == FishinoWebServer::MIMETYPE_GIF ||
				mimeType == FishinoWebServer::MIMETYPE_JPG ||
				mimeType == FishinoWebServer::MIMETYPE_PNG ||
				mimeType == FishinoWebServer::MIMETYPE_ICO
			)
				web << F("Cache-Control:public, max-age=900\r\n");

			web.endHeaders();

			if (file.open(&root, filename, O_READ)) {
					Serial << F("Lettura del file: ");
					Serial.println(filename);
					web.sendFile(file);
					file.close();
				}

			else {
					web << F("Impossibile trovare il file: ") << filename << "\n";
				}
		}
}

// handle file requests. Gestisce le richieste dei file
bool fileHandler(FishinoWebServer& web) {
	String filename = web.getFileFromPath(web.getPath());

	sendFile(web, filename.c_str());
	return true;
}

//handle IT language handler. Gestisce l'handler dell'italiano
bool itHandler(FishinoWebServer& web) {
	sendFile(web, "INDEX_IT.HTM");
	return true;
}

//handle EN language handler. Gestisce l'handler dell'inglese
bool enHandler(FishinoWebServer& web) {
	sendFile(web, "INDEX_EN.HTM");
	return true;
}

//handle DE language handler. Gestisce l'handler del tedesco
bool deHandler(FishinoWebServer& web) {
	sendFile(web, "INDEX_DE.HTM");
	return true;
}

// handle index requests. Gestisce le richieste del file index
bool indexHandler(FishinoWebServer& web) {
#if defined(EN)
	sendFile(web, "INDEX_EN.HTM");
#elif defined(DE)
	sendFile(web, "INDEX_DE.HTM");
#elif defined(IT)
	sendFile(web, "INDEX_IT.HTM");
#else
#error "lingua non supportata"
#endif
	return true;
}

// handle status requests
////ITALIAN  STATUS HANDLER: creates a .json with real time data. HANDLER DI STATO IN ITALIANO: crea un file .json con i dati in tempo reale
bool statusHandler(FishinoWebServer& web) {
	DateTime dt = RTC.now();

	web.sendErrorCode(200);
	web.sendContentType(F("application/json"));

	web.endHeaders();

	FishinoClient& client = web.getClient();
	client.println("{");

	client.print(" \"umidita\": \"");

	if (hum_sens == false) {
			client.println("sensore scollegato.\",");
		}

	else
		if (hum_sens == true) {
				client.print(hum);
				client.println(" %\",");
			}

	client.print("\"temperatura\": \"");

	if (temp == false)
		client.println("sensore scollegato.\",");
	else
		if (temp == true) {
				client.print(temperatura);
				client.println(" C\",");
			}

	client.print("\"pressione\": \"");

	if (pressure_sens == false)
		client.println("sensore scollegato.\",");
	else {
			// client.print(Pressure);
			client.print(P);
			client.println(" mb\",");
		}

	client.print("\"vento\": \"");

	client.print(speed);

	client.println(" Km/h\",");
	client.print("\"co\": \"");
	client.print(ppm, 2);
	client.println(" ppm\",");

	client.print("\"uv\": \"");

	if (uv_sens == false)
		client.println("sensore scollegato.\",");
	else
		if (uv_sens == true) {
				client.print(uvIntensity);
				client.println(" mW/cm^2\",");
			}

	client.print("\"pioggia\": \"");

	if (rain == true)
		client.println(" Si\",");

	else
		if (rain == false)
			client.println(" No\",");


	client.print("\"fulmini\": \"");

	if (AS3935IrqTriggered) {

			// reset the flag
			AS3935IrqTriggered = 0;

			int irqSource = AS3935.getInterruptSource();
			// here we go into loop checking if interrupt has been triggered, which kind of defeats
			// returned value is bitmap field, bit 0 - noise level too high, bit 2 - disturber detected, and finally bit 3 - lightning!


			if (irqSource & 0b0001)
				///Serial.println("Noise level too high, try adjusting noise floor");
				client.print("Livello del rumore troppo alto, prova a settare la soglia del rumore!");
			else
				if (irqSource & 0b0100)
					client.print("Rilevato disturbo");
				else
					if (irqSource & 0b1000) {
							// need to find how far that lightning stroke, function returns approximate distance in kilometers,
							// where value 1 represents storm in detector's near victinity, and 63 - very distant, out of range stroke
							// everything in between is just distance in kilometers
							// è necessario scoprire quanto lontano è scoccato il fulmine, la funzione restitusce la distanza approsimativa in chilometri,
							// dove il valore 1 significa che il fulmine è molto vicino al rilevatore, e 63 molto lontano, fuori portata
							// tutto ciò che è nel mezzo è solo la distanza in chilometri
							int strokeDistance = AS3935.getLightningDistanceKm();

							if (strokeDistance == 1)
								client.print("Tempesta sopra la testa, attento!");

							else
								if (strokeDistance == 63)

									client.print("Rilevato fumine fuori portata.");

								else
									if (strokeDistance < 63 && strokeDistance > 1) {
											client.print("Rilevato fulmine a ");
											client.print(strokeDistance, DEC);
											client.print(" km di distanza.");

										}

						}

		}

	else
		client.print(" nessun rilevamento.");

	client.print(" \",\n \"dataora\": \"");

	client.print(dt.day(), DEC);

	client.print("/");

	client.print(dt.month(), DEC);

	client.print("/");

	client.print(dt.year(), DEC);

	client.print(" ");

	if (dt.hour() < 10) {
			client.print("0");
			client.print(dt.hour(), DEC);
		}

	else
		client.print(dt.hour(), DEC);

	client.print(":");

	if (dt.minute() < 10) {
			client.print("0");
			client.print(dt.minute(), DEC);
		}

	else
		client.print(dt.minute(), DEC);

	client.print(":");

	if (dt.second() < 10) {
			client.print("0");
			client.print(dt.second(), DEC);
		}

	else
		client.print(dt.second(), DEC);

	client.println("\"");

	client.print("}\n");

	client.flush();

	client.stop();

	return true;
}

////ENGLISH  STATUS HANDLER: creates a .json with real time data. HANDLER DI STATO IN INGLESE: crea un file .json con i dati in tempo reale
bool statusHandler_en(FishinoWebServer& web) {
	DateTime dt = RTC.now();

	web.sendErrorCode(200);
	web.sendContentType(F("application/json"));

	web.endHeaders();

	FishinoClient& client = web.getClient();
	client.println("{");

	client.print(" \"humidity\": \"");

	if (hum_sens == false) {
			client.println("sensor disconnected.\",");
		}

	else
		if (hum_sens == true) {
				client.print(hum);
				client.println(" %\",");
			}

	client.print("\"temperature\": \"");

	if (temp == false)
		client.println("sensor disconnected.\",");
	else
		if (temp == true) {
				client.print(temperatura);
				client.println(" C\",");
			}

	client.print("\"pressure\": \"");

	if (pressure_sens == false)
		client.println("sensor disconnected.\",");
	else {
			client.print(P);
			client.println(" mb\",");
		}

	client.print("\"wind\": \"");

	client.print(speed);
	client.println(" Km/h\",");

	client.print("\"co\": \"");
	client.print(ppm, 2);
	client.println(" ppm\",");

	client.print("\"uv\": \"");

	if (uv_sens == false)
		client.println("sensor disconnected.\",");
	else
		if (uv_sens == true) {
				client.print(uvIntensity);
				client.println(" mW/cm^2\",");
			}

	client.print("\"rain\": \"");

	if (rain == true)
		client.println(" yes\",");

	else
		if (rain == false)
			client.println(" no\",");


	client.print("\"strokes\": \" ");

	if (AS3935IrqTriggered) {

			// reset the flag
			AS3935IrqTriggered = 0;

			int irqSource = AS3935.getInterruptSource();
			// here we go into loop checking if interrupt has been triggered, which kind of defeats
			// returned value is bitmap field, bit 0 - noise level too high, bit 2 - disturber detected, and finally bit 3 - lightning!


			if (irqSource & 0b0001)
				client.print("Noise level too high, try adjusting noise floor");
			else
				if (irqSource & 0b0100)
					client.print("Disturber detected");
				else
					if (irqSource & 0b1000) {
							// need to find how far that lightning stroke, function returns approximate distance in kilometers,
							// where value 1 represents storm in detector's near victinity, and 63 - very distant, out of range stroke
							// everything in between is just distance in kilometers
							// è necessario scoprire quanto lontano è scoccato il fulmine, la funzione restitusce la distanza approsimativa in chilometri,
							// dove il valore 1 significa che il fulmine è molto vicino al rilevatore, e 63 molto lontano, fuori portata
							// tutto ciò che è nel mezzo è solo la distanza in chilometri
							int strokeDistance = AS3935.getLightningDistanceKm();

							if (strokeDistance == 1)
								client.print("Storm overhead, watch out!");

							else
								if (strokeDistance == 63)
									client.print("Out of range lightning detected.");

								else
									if (strokeDistance < 63 && strokeDistance > 1) {
											client.print("Lightning detected ");
											client.print(strokeDistance, DEC);
											client.print(" kilometers away.");

										}

						}

		}

	else
		client.print(" no detection.");

	client.print(" \"  , \n\"datetime\": \"");

	client.print(dt.day(), DEC);

	client.print("/");

	client.print(dt.month(), DEC);

	client.print("/");

	client.print(dt.year(), DEC);

	client.print(" ");

	if (dt.hour() < 10) {
			client.print("0");
			client.print(dt.hour(), DEC);
		}

	else
		client.print(dt.hour(), DEC);

	client.print(":");

	if (dt.minute() < 10) {
			client.print("0");
			client.print(dt.minute(), DEC);
		}

	else
		client.print(dt.minute(), DEC);

	client.print(":");

	if (dt.second() < 10) {
			client.print("0");
			client.print(dt.second(), DEC);
		}

	else
		client.print(dt.second(), DEC);

	client.println("\"");

	client.print("}\n");

	client.flush();

	client.stop();

	return true;
}

////GERMAN  STATUS HANDLER: creates a .json with real time data. HANDLER DI STATO IN TEDESCO: crea un file .json con i dati in tempo reale
bool statusHandler_de(FishinoWebServer& web) {
	DateTime dt = RTC.now();

	web.sendErrorCode(200);
	web.sendContentType(F("application/json"));

	web.endHeaders();

	FishinoClient& client = web.getClient();
	client.println("{");

	client.print(" \"feuchtigkeit\": \"");

	if (hum_sens == false) {
			client.println("sensor ausgesteckt.\",");
		}

	else
		if (hum_sens == true) {
				client.print(hum);
				client.println(" %\",");
			}

	client.print("\"temperatur\": \"");

	if (temp == false)
		client.println("sensor ausgesteckt.\",");
	else
		if (temp == true) {
				client.print(temperatura);
				client.println(" C\",");
			}

	client.print("\"luftdruck\": \"");

	if (pressure_sens == false)
		client.println("sensor ausgesteckt.\",");
	else {
			client.print(P);
			client.println(" mb\",");
		}

	client.print("\"wind\": \"");

	client.print(speed);
	client.println(" Km/h\",");

	client.print("\"co\": \"");

	client.print(ppm, 2);
	client.println(" ppm\",");
	client.print("\"uv\": \"");

	if (uv_sens == false)
		client.println("sensor ausgesteckt.\",");
	else
		if (uv_sens == true) {
				client.print(uvIntensity);
				client.println(" mW/cm^2\",");
			}

	client.print("\"regen\": \"");

	if (rain == true)
		client.println(" ein\",");

	else
		if (rain == false)
			client.println(" keine\",");


	client.print("\"blitz\": \" ");

	if (AS3935IrqTriggered) {

			// reset the flag
			AS3935IrqTriggered = 0;

			int irqSource = AS3935.getInterruptSource();
			// here we go into loop checking if interrupt has been triggered, which kind of defeats
			// returned value is bitmap field, bit 0 - noise level too high, bit 2 - disturber detected, and finally bit 3 - lightning!


			if (irqSource & 0b0001)
				client.print("Lautstärke der Geräusche zu hoch, versuche die Lautstärke neu einzustellen!");
			else
				if (irqSource & 0b0100)
					client.print("Störung aufgehoben");
				else
					if (irqSource & 0b1000) {
							// need to find how far that lightning stroke, function returns approximate distance in kilometers,
							// where value 1 represents storm in detector's near victinity, and 63 - very distant, out of range stroke
							// everything in between is just distance in kilometers
							// è necessario scoprire quanto lontano è scoccato il fulmine, la funzione restitusce la distanza approsimativa in chilometri,
							// dove il valore 1 significa che il fulmine è molto vicino al rilevatore, e 63 molto lontano, fuori portata
							// tutto ciò che è nel mezzo è solo la distanza in chilometri
							int strokeDistance = AS3935.getLightningDistanceKm();

							if (strokeDistance == 1)
								client.print("Achtung, Blitz über dem Kopf");

							else
								if (strokeDistance == 63)
									client.print("Blitz auserhalb der Reichweite erkannt");

								else
									if (strokeDistance < 63 && strokeDistance > 1) {
											/*Serial.print("Lightning detected ");
											  Serial.print(strokeDistance, DEC);
											  Serial.print (" kilometers away.  ");*/
											client.print("Blitz in ");
											client.print(strokeDistance, DEC);
											client.print(" Entfernung erkannt");

										}

						}

		}

	else
		client.print(" keine abgabe");

	client.print(" \" , \n \"datumstunde\": \"");

	client.print(dt.day(), DEC);

	client.print("/");

	client.print(dt.month(), DEC);

	client.print("/");

	client.print(dt.year(), DEC);

	client.print(" ");

	if (dt.hour() < 10) {
			client.print("0");
			client.print(dt.hour(), DEC);
		}

	else
		client.print(dt.hour(), DEC);

	client.print(":");

	if (dt.minute() < 10) {
			client.print("0");
			client.print(dt.minute(), DEC);
		}

	else
		client.print(dt.minute(), DEC);

	client.print(":");

	if (dt.second() < 10) {
			client.print("0");
			client.print(dt.second(), DEC);
		}

	else
		client.print(dt.second(), DEC);

	client.println("\"");

	client.print("}\n");

	client.flush();

	client.stop();

	return true;
}



void setupRtcfromNTP(void) {
	// RTC Init, if required

	if (!RTC.isrunning()) {
			Serial.print("RTC non operativo!");
		}

	// WiFi Init

	if ((epochNtp = wifiGetUtc()) == 0) {
			Serial.println("NTP non disponibile");
		}

	else {

			RTC.adjust(epochNtp);

			// Check New RTC settings
			// Controlla le nuove impostazioni del RTC

			Serial.print("Epoch dal server NTP  :");
			Serial.println(epochNtp);

			showDate("Rtc impostato :", RTC.now());
		}
}


bool saveData(String data) {
	if (!file.isOpen()) // check if file is open. Contolla se il file è aperto
		datalog.open(&root, "datalog.txt", O_CREAT | O_WRITE | O_APPEND); //If file is not open, it will be opened. Se il file non è aperto, verrà aperto.

	if (datalog.isOpen()) {
			// if datalog is open, it will save data with an end line. Se il datalog è aperto salva i dati con un fine linea.
			if (datalog.write(data.c_str())) {
					datalog.write(" \n");
					Serial.println("Dati salvati");
				}

			else
				Serial.println("Errore nella scrittura sul file ");

			datalog.sync();

			datalog.close();

			return true;
		}

	else {
			Serial.println("Errore nell'apertura del file datalog.txt");
			return false;
		}

}

void getSensorsData(void) {
	ppm = mq7.getPPM();
	uvIntensity = uv.getuvIntensity();
	speed = wind.getKmhSpeed();
	sensors.requestTemperatures();
	temperatura = sensors.getTempCByIndex(0);
	hum = dht.readHumidity();

	char status;
	//double T, p0, a;

 status = pressure.startTemperature();
  if (status != 0)
  {
    // Wait for the measurement to complete:
    delay(status);

    // Retrieve the completed temperature measurement:
    // Note that the measurement is stored in the variable T.
    // Function returns 1 if successful, 0 if failure.

    status = pressure.getTemperature(T);
    if (status != 0)
    {
      // Start a pressure measurement:
      // The parameter is the oversampling setting, from 0 to 3 (highest res, longest wait).
      // If request is successful, the number of ms to wait is returned.
      // If request is unsuccessful, 0 is returned.

      status = pressure.startPressure(3);
      if (status != 0)
      {
        // Wait for the measurement to complete:
        delay(status);

        // Retrieve the completed pressure measurement:
        // Note that the measurement is stored in the variable P.
        // Note also that the function requires the previous temperature measurement (T).
        // (If temperature is stable, you can do one temperature measurement for a number of pressure measurements.)
        // Function returns 1 if successful, 0 if failure.

        status = pressure.getPressure(P,T);
      }
      
      }

  }
	if (uvIntensity < 0.00)
		uv_sens = false;
	else
		uv_sens = true;


	if (temperatura == -127.00)
		temp = false;

	else
		temp = true;

	if (P == 0.00)
		pressure_sens = false;
	else
		pressure_sens = true;

	if (digitalRead(rainpin) == HIGH)
		rain = false;
	else
		if (digitalRead(rainpin) == LOW)
			rain = true;

	if (isnan(hum))
		hum_sens = false;
	else
		hum_sens = true;


}

void setup(void) {
	//uint32_t tim;
	Serial.begin(115200); //Start the serial communication with 115220 baud rate. Inizia la comunicazione seriale con 115200 baud al secondo

	//while (!Serial) // wait for serial port to connect. Needed for Leonardo only
		//;  // attende l'apertura della porta seriale. Necessario solo per le boards Leonardo

	pinMode(rainpin, INPUT);

	pinMode(statusled_green, OUTPUT);

	pinMode(statusled_red, OUTPUT);

	digitalWrite(statusled_red, HIGH);

	digitalWrite(statusled_green, LOW);

	wind.begin();

	uv.begin();

	dht.begin();

	sensors.begin();

	sensors.setWaitForConversion(false);

	sensors.setResolution(12);

	sensors.requestTemperatures();
	
	SPI.begin();
	
		if (pressure.begin())
		Serial.println("BMP180 inizializzato correttamente");
	else
		// Oops, something went wrong, this is usually a connection problem,
		// see the comments at the top of this sketch for the proper connections.
		// Oops, qualcosa è andato storto, di solito è un problema di collegamento,
		// guarda i commenti in cima allo sketch per i collegamenti corretti.

		Serial.println("Inizializzazione del BMP180 fallita");

	Serial.println("Starting");

	Wire.begin();

	Wire.setClock(400000);

	AS3935.reset();

	// and run calibration
	// if lightning detector can not tune tank circuit to required tolerance,
	// calibration function will return false
	// avvia la calibrazione
	// se il rilevatore di fulmini non riesce a calibrare il circuito fino alla tolleranza desiderata,
	// la funzione di calibrazione restituirà il valore falso
	if (!AS3935.calibrate())
		Serial.println("Tuning out of range, check your wiring, your sensor and make sure physics laws have not changed!");

	// first let's turn on disturber indication and print some register values from AS3935
	// tell AS3935 we are indoors, for outdoors use setOutdoors() function
	// prima di tutto attiviamo l'indicatore del disturbo e stampiamo alcuni valori del registro dell'AS3935
	// comunichiamo al sensore che siamo all'interno, per l'uso esterno usare la funzione setOutdoors()
	AS3935.setIndoors();


	AS3935.setNoiseFloor(1);

	AS3935.setSpikeRejection(2);

	AS3935.setWatchdogThreshold(1);

	// turn on indication of distrubers, once you have AS3935 all tuned, you can turn those off with disableDisturbers()
	// Attiviamo l'indicatore dei disturbi, una volta che hai calibrato l'AS3935 puoi disabilitarlo con disableDisturbers()
	AS3935.enableDisturbers();

	//AS3935.disableDisturbers();
	printAS3935Registers();

	AS3935IrqTriggered = 0;

	// Using interrupts means you do not have to check for pin being set continously, chip does that for you and
	// notifies your code
	// demo is written and tested on ChipKit MAX32, irq pin is connected to max32 pin 2, that corresponds to interrupt 1
	// look up what pins can be used as interrupts on your specific board and how pins map to int numbers

	// ChipKit Max32 - irq connected to pin 2
	attachInterrupt(digitalPinToInterrupt(IRQ_PIN), AS3935_ISR, FALLING);

	// uncomment line below and comment out line above for Arduino Mega 2560, irq still connected to pin 2
	// attachInterrupt(0,AS3935Irq,RISING);

	connectToAp();

	setupRtcfromNTP();

	// Set the SDcard CS pin as an output
	pinMode(SD_CS, OUTPUT);

	// Turn off the SD card! (wait for configuration)
	digitalWrite(SD_CS, HIGH);

	// initialize the SD card.

	Serial << F("Inizializzando la micro SD...\n");

	// Pass over the speed and Chip select for the SD card
	bool hasFilesystem = false;

	if (!card.init(SD_CS))
		Serial << F("Apertura dell'sd fallita!\n");

	// initialize a FAT volume.
	//inizializza un volume FAT
	else
		if (!volume.init(&card))
			Serial << F("Inizializzazione del volume fallita!\n");
		else
			if (!root.openRoot(&volume))
				Serial << F("Apertura della root fallita!\n");
			else
				hasFilesystem = true;

	// if no filesystem present, just signal it and stop
	// se non è presente il file syste viene segnalato e si stoppa
	if (!hasFilesystem) {
			Serial << F("Filesystem non presente. -- Stop.\n");

			while (1)
				;
		}

	;

	// setup accepted headers and handlers
	web.addHeader(F("Content-Length"));

	web
	.addHandler(F("/")        , FishinoWebServer::GET , &indexHandler)
	.addHandler(F("/it")        , FishinoWebServer::GET , &itHandler)
	.addHandler(F("/en")        , FishinoWebServer::GET , &enHandler)
	.addHandler(F("/de")        , FishinoWebServer::GET , &deHandler)
	.addHandler(F("/data.js")   , FishinoWebServer::GET , &statusHandler)
	.addHandler(F("/data_en.js")   , FishinoWebServer::GET , &statusHandler_en)
	.addHandler(F("/data_de.js")   , FishinoWebServer::GET , &statusHandler_de)
	.addHandler(F("/" "*")      , FishinoWebServer::GET , &fileHandler)
	;

	Serial << F("Inizializzazione avvenuta!\n");

	// Start the web server.
	Serial << F("Avviando il web server...\n");

	web.begin();

	Serial << F("Pronto ad accettare le richieste HTTP.\n");

	digitalWrite(statusled_green, HIGH);

	digitalWrite(statusled_red, LOW);





}

// ciclo infinito
void loop(void) {
	DateTime dt = RTC.now();
	String message;
	String lightning;

	getSensorsData();

	web.process();


	//Startup activity: it will be happen just once. Attività iniziale: avverrà solo una volta



	if (Fishino.status() == STATION_GOT_IP) {
			// Check if Fishino is joined to a Wifi network. If it has ang IP address:
			//Controlla se il Fishino è connesso ad una rete wifi. Se ha un indirizzo IP:
			digitalWrite(statusled_green, HIGH); //Green status led will be turned on. Il led di stato verde verrà acceso.
			digitalWrite(statusled_red, LOW); //Red status led wil be turned off. Il led di stato rosso verrà spento.
		}

	else {
			digitalWrite(statusled_green, LOW);//Green status led will be turned off. Il led di stato verde verrà spento.
			digitalWrite(statusled_red, HIGH);//Red status led wil be turned on. Il led di stato rosso verrà acceso.
		}

	///////////////////////////////////////////////FULMINI


#if defined(EN)
	message = FString("Date & time: ");

	message += String(dt->day, DEC);
	message += FString("/");
	message += String(dt->month, DEC);
	message += FString("/");
	message += String(2000 + dt->year, DEC);
	message += FString(" ");

	if (dt->hour < 10) {
			message += FString("0");
			message += String(dt->hour, DEC);
		}

	else
		message += String(dt->hour, DEC);

	message += FString(":");

	if (dt->minute < 10) {
			message += FString("0");
			message += String(dt->minute, DEC);
		}

	else
		message += String(dt->minute, DEC);

	message += FString(":");

	if (dt->second < 10) {
			message += FString("0");
			message += String(dt->second, DEC);
		}

	else
		message += String(dt->second, DEC);

	message += FString(". Temperature: ");

	if (temp == false)
		message += FString("sensor disconnected.");
	else
		if (temp == true) {
				message += String(temperatura, 3);
				message += FString(" C.");
			}

	message += FString(" Humidity: ");

	if (hum_sens == false)
		message += FString("sensor disconnected. ");
	else
		if (hum_sens == true) {
				message += String(hum, 3);
				message += FString("% .");
			}

	message += FString("Wind speed: ");

	message += String(speed, 4);
	message += FString(" Km/h. UV rays' intensity: ");

	message += String(uvIntensity, 4);
	message += FString(" mW/cm^2.");


	message += FString(" CO concentration: ");

	message += String(ppm, 2);
	message += FString(" ppm.");

	message += FString(" Rain detection: ");

	if (rain == true)
		message += FString("yes.");
	else
		if (rain == false)
			message += FString("no.");

	message += FString(" Air pressure: ");

	if (pressure_sens == false)
		message += FString(" sensor disconnected.");
	else
		if (pressure_sens == true) {
				message += String(P, 4);
				message += FString(" mb.");
			}

	message += FString(" Stroke detection: ");

	//   message +=  lightning.c_str();

	// first step is to find out what caused interrupt. Il primo passo è scoprire cosa ha causato l'interrupt
	// as soon as we read interrupt cause register, irq pin goes low. Non appena legge dal registro cosa ha causato l'i
	// the whole purpose of interrupts, but in real life you could put your chip to sleep
	// and lower power consumption or do other nifty things

	if (AS3935IrqTriggered) {

			// reset the flag
			AS3935IrqTriggered = 0;

			int irqSource = AS3935.getInterruptSource();
			// here we go into loop checking if interrupt has been triggered, which kind of defeats
			// returned value is bitmap field, bit 0 - noise level too high, bit 2 - disturber detected, and finally bit 3 - lightning!


			if (irqSource & 0b0001) {
					Serial.println("Noise level too high, try adjusting noise floor");
					message += FString("Noise level too high, try adjusting noise floor");
				}

			else
				if (irqSource & 0b0100) {
						message += FString("Disturber detected");
						Serial.println("Disturber detected");
					}

				else
					if (irqSource & 0b1000) {
							// need to find how far that lightning stroke, function returns approximate distance in kilometers,
							// where value 1 represents storm in detector's near victinity, and 63 - very distant, out of range stroke
							// everything in between is just distance in kilometers
							// è necessario scoprire quanto lontano è scoccato il fulmine, la funzione restitusce la distanza approsimativa in chilometri,
							// dove il valore 1 significa che il fulmine è molto vicino al rilevatore, e 63 molto lontano, fuori portata
							// tutto ciò che è nel mezzo è solo la distanza in chilometri
							int strokeDistance = AS3935.getLightningDistanceKm();

							if (strokeDistance == 1) {
									message += FString("Storm overhead, watch out!");
									Serial.print("Storm overhead, watch out! ");
								}

							else
								if (strokeDistance == 63) {
										Serial.print("Out of range lightning detected.  ");

										message += FString("Out of range lightning detected.");
									}

								else
									if (strokeDistance < 63 && strokeDistance > 1) {
											Serial.print("Lightning detected ");
											Serial.print(strokeDistance, DEC);
											Serial.print(" kilometers away.  ");

											message += FString("Lightning detected ");
											message += String(strokeDistance, DEC);
											message += FString(" kilometers away.");

										}

						}

		}

	else
		message += FString(" no detection.");

#elif defined(DE)
	message = FString("Datum und Stunde: ");

	message += String(dt->day, DEC);

	message += FString("/");

	message += String(dt->month, DEC);

	message += FString("/");

	message += String(2000 + dt->year, DEC);

	message += FString(" ");

	if (dt->hour < 10) {
			message += FString("0");
			message += String(dt->hour, DEC);
		}

	else
		message += String(dt->hour, DEC);

	message += FString(":");

	if (dt->minute < 10) {
			message += FString("0");
			message += String(dt->minute, DEC);
		}

	else
		message += String(dt->minute, DEC);

	message += FString(":");

	if (dt->second < 10) {
			message += FString("0");
			message += String(dt->second, DEC);
		}

	else
		message += String(dt->second, DEC);

	message += FString(". Temperatur: ");

	if (temp == false)
		message += FString("sensor ausgesteckt.");
	else
		if (temp == true) {
				message += String(temperatura, 3);
				message += FString(" C.");
			}

	message += FString(" Feuchtigkeit: ");

	if (hum_sens == false)
		message += FString("sensor ausgesteckt. ");
	else
		if (hum_sens == true) {
				message += String(hum, 3);
				message += FString("%. ");
			}

	message += FString("Windgeschwindigkeit: ");

	message += String(speed, 4);
	message += FString("(Km/h. UV-Intensität: ");

	message += String(uvIntensity, 4);
	message += FString(" mW/cm^2.");


	message += FString(" CO Luftgehalt: ");

	message += String(ppm, 2);
	message += FString(" ppm.");
	message += FString(" Regen erkannt: ");

	if (rain == true)
		message += FString("eine.");
	else
		if (rain == false)
			message += FString("keine.");

	message += FString(" Luftdruck: ");

	if (pressure_sens == false)
		message += FString(" sensor ausgesteckt.");
	else
		if (pressure_sens == true) {
				message += String(P, 4);
				message += FString(" mb.");
			}

	message += FString(" Blitzerkennung: ");



	if (AS3935IrqTriggered) {

			// reset the flag
			AS3935IrqTriggered = 0;

			int irqSource = AS3935.getInterruptSource();
			// here we go into loop checking if interrupt has been triggered, which kind of defeats
			// returned value is bitmap field, bit 0 - noise level too high, bit 2 - disturber detected, and finally bit 3 - lightning!


			if (irqSource & 0b0001) {
					Serial.println("Lautstärke der Geräusche zu hoch, versuche die Lautstärke neu einzustellen!");
					message += FString("Lautstärke der Geräusche zu hoch, versuche die Lautstärke neu einzustellen!");
				}

			else
				if (irqSource & 0b0100) {
						message += FString("Störung aufgehoben");
						Serial.println("Störung aufgehoben");
					}

				else
					if (irqSource & 0b1000) {
							// need to find how far that lightning stroke, function returns approximate distance in kilometers,
							// where value 1 represents storm in detector's near victinity, and 63 - very distant, out of range stroke
							// everything in between is just distance in kilometers
							// è necessario scoprire quanto lontano è scoccato il fulmine, la funzione restitusce la distanza approsimativa in chilometri,
							// dove il valore 1 significa che il fulmine è molto vicino al rilevatore, e 63 molto lontano, fuori portata
							// tutto ciò che è nel mezzo è solo la distanza in chilometri
							int strokeDistance = AS3935.getLightningDistanceKm();

							if (strokeDistance == 1) {
									message +=  FString("Achtung, Blitz über dem Kopf");
									Serial.print("Achtung, Blitz über dem Kopf");
								}

							else
								if (strokeDistance == 63) {
										Serial.print("Blitz auserhalb der Reichweite erkannt");

										message += FString("blitz auserhalb der Reichweite erkannt");
									}

								else
									if (strokeDistance < 63 && strokeDistance > 1) {
											Serial.print("Blitz in  ");
											Serial.print(strokeDistance, DEC);
											Serial.print(" entfernung erkannt.  ");
											message += FString("Blitz in ");
											message += String(strokeDistance, DEC);
											message += FString(" entfernung erkannt");

										}

						}

		}

	else
		message += FString(" keine abgabe");

#elif defined(IT)
	message = FString("Data e ora: ");

	message += String(dt.day(), DEC);

	message += FString("/");

	message += String(dt.month(), DEC);

	message += FString("/");

	message += String(dt.year(), DEC);

	message += FString(" ");

	if (dt.hour() < 10) {
			message += FString("0");
			message += String(dt.hour(), DEC);
		}

	else
		message += String(dt.hour(), DEC);

	message += FString(":");

	if (dt.minute() < 10) {
			message += FString("0");
			message += String(dt.minute(), DEC);
		}

	else
		message += String(dt.minute(), DEC);

	message += FString(":");

	if (dt.second() < 10) {
			message += FString("0");
			message += String(dt.second(), DEC);
		}

	else
		message += String(dt.second(), DEC);

	message += FString(". Temperatura: ");

	if (temp == false)
		message += FString("sensore scollegato.");
	else
		if (temp == true) {
				message += String(temperatura, 3);
				message += FString(" C.");
			}

	message += FString(" Umidita': ");

	if (hum_sens == false)
		message += FString("sensore scollegato. ");
	else
		if (hum_sens == true) {
				message += String(hum, 3);
				message += FString(" %. ");
			}

	message += FString("Velocita' del vento: ");

	message += String(speed, 4);
	message += FString(" Km/h. Radiazioni UV: ");

	message += String(uvIntensity, 4);
	message += FString(" mW/cm^2.");


	message += FString(" CO nell'aria: ");

	message += String(ppm, 3);
	message += FString(" ppm.");
	message += FString(" Rilevamento precipitazioni: ");

	if (rain == true)
		message += FString("presenti.");
	else
		if (rain == false)
			message += FString("assenti.");

	message += FString(" Pressione atmosferica: ");

	if (pressure_sens == false)
		message += FString(" sensore scollegato.");
	else
		if (pressure_sens == true) {
				message += String(P, 4);
				message += FString(" mb.");
			}

	message += FString(" Rilevamento fulmini: ");

	if (AS3935IrqTriggered) {

			// reset the flag
			AS3935IrqTriggered = 0;

			int irqSource = AS3935.getInterruptSource();
			// here we go into loop checking if interrupt has been triggered, which kind of defeats
			// returned value is bitmap field, bit 0 - noise level too high, bit 2 - disturber detected, and finally bit 3 - lightning!


			if (irqSource & 0b0001) {
					Serial.println("Livello del rumore troppo alto, prova a settare la soglia del rumore!");
					message += FString("Livello del rumore troppo alto, prova a settare la soglia del rumore!");
				}

			else
				if (irqSource & 0b0100) {
						Serial.println("Rilevato disturbo");
						message += FString("Rilevato disturbo");
					}

				else
					if (irqSource & 0b1000) {
							// need to find how far that lightning stroke, function returns approximate distance in kilometers,
							// where value 1 represents storm in detector's near victinity, and 63 - very distant, out of range stroke
							// everything in between is just distance in kilometers
							// è necessario scoprire quanto lontano è scoccato il fulmine, la funzione restitusce la distanza approsimativa in chilometri,
							// dove il valore 1 significa che il fulmine è molto vicino al rilevatore, e 63 molto lontano, fuori portata
							// tutto ciò che è nel mezzo è solo la distanza in chilometri
							int strokeDistance = AS3935.getLightningDistanceKm();

							if (strokeDistance == 1) {
									Serial.print("Tempesta sopra la testa, attento!");
									message += FString("Tempesta sopra la testa, attento!");
								}

							else
								if (strokeDistance == 63) {
										Serial.print("Out of range lightning detected.");

										message += FString("Out of range lightning detected.");
									}

								else
									if (strokeDistance < 63 && strokeDistance > 1) {
											Serial.print("Rilevato fulmine a  ");
											Serial.print(strokeDistance, DEC);
											Serial.print(" km di distanza. ");
											message += FString("Rilevato fulmine a ");
											message += String(strokeDistance, DEC);
											message += FString(" km di distanza.");


										}

							saveData(message.c_str());

							if (push.sendPushNotification(message.c_str()))
								Serial.println("Notifica push inviata.");
						}

		}

	else
		message += FString(" nessun rilevamento.");

#else
#error "lingua non supportata"
#endif
	EVERY(300000) {

		saveData(message.c_str());

	}

	EVERY(3600000) {

		if (push.sendPushNotification(message.c_str()))
			Serial.println("Notifica push inviata.");


	}

	if (startup_activity == 1) {
			//If startup_activity is 1, do stuffs between the brackets. Se la variabile startup_activity è 1, esegui ciò che è nelle parentesi


			if (push.sendPushNotification(message.c_str()))
				Serial.println("Notifica push inviata.");

			saveData(message.c_str());


			startup_activity = 2; //Set startup_activity to 2 , so this activity won't be repeated. Imposta il valore di startup_activity a 2, così non verà ripetuta
		}

}
