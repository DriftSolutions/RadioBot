//@AUTOHEADER@BEGIN@
/**********************************************************************\
|                          ShoutIRC RadioBot                           |
|           Copyright 2004-2020 Drift Solutions / Indy Sams            |
|        More information available at https://www.shoutirc.com        |
|                                                                      |
|                    This file is part of RadioBot.                    |
|                                                                      |
|   RadioBot is free software: you can redistribute it and/or modify   |
| it under the terms of the GNU General Public License as published by |
|  the Free Software Foundation, either version 3 of the License, or   |
|                 (at your option) any later version.                  |
|                                                                      |
|     RadioBot is distributed in the hope that it will be useful,      |
|    but WITHOUT ANY WARRANTY; without even the implied warranty of    |
|     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the     |
|             GNU General Public License for more details.             |
|                                                                      |
|  You should have received a copy of the GNU General Public License   |
|  along with RadioBot. If not, see <https://www.gnu.org/licenses/>.   |
\**********************************************************************/
//@AUTOHEADER@END@

#include "welcome.h"

struct WEATHER_INFO {
	char City[128];
//	char Region[128];
	//bool isCelsius;
	//char DistanceUnits[32];
	char PressureUnits[64];
	char SpeedUnits[32];
	char Humidity[8];

	//int WindChill;
	char WindDirection[128];
	int WindSpeed;
	char WindString[128];

	char Condition[128];
	int Temp;
};

void DoWeather(WEB_QUEUE * Scan) {

	string url = "http://api.openweathermap.org/data/2.5/weather?zip=";
	char * city = api->curl->escape(Scan->data, strlen(Scan->data));
	url += city;
	api->curl->free(city);
	url += "&units=";
	if (Scan->getCelcius) {
		url += "metric";
	} else {
		url += "imperial";
	}
	url += "&mode=xml&APPID=";
	city = api->curl->escape(wel_config.WeatherKey, strlen(wel_config.WeatherKey));
	url += city;
	api->curl->free(city);
	printf("URL: %s\n", url.c_str());

	char buf[2048], buf2[128], fn1[MAX_PATH];
	WEATHER_INFO wi;
	memset(&wi, 0, sizeof(wi));
	bool part1=false;

	char md5buf[64];
	memset(md5buf, 0, sizeof(md5buf));
	hashdata("md5", (unsigned char *)Scan->data, strlen(Scan->data), md5buf, sizeof(md5buf));
	sprintf(fn1, "./tmp/weather.%s.1.tmp", md5buf);

	char * file1 = get_cached_download(url.c_str(), fn1, buf, sizeof(buf));
	if (file1 != NULL) {
		printf("Content: %s\n", file1);
		TiXmlDocument doc;// = new TiXmlDocument();
		doc.Parse(file1);
		TiXmlElement * root = doc.FirstChildElement("current");
		if (root) {
			//e = root->FirstChildElement("display_location");
			TiXmlElement * e = root->FirstChildElement("temperature");
			if (e) {
				wi.Temp = atoi_safe(e->Attribute("value"));
			}
			e = root->FirstChildElement("city");
			if (e) {
				strcpy_safe(wi.City, e->Attribute("name"), sizeof(wi.City));
				strtrim(wi.City, " ,");
				if (strlen(wi.City)) {
					part1 = true;
				}
			}
			e = root->FirstChildElement("weather");
			if (e) {
				strcpy_safe(wi.Condition, e->Attribute("value"), sizeof(wi.Condition));
			}
			e = root->FirstChildElement("wind");
			if (e) {
				TiXmlElement * f = e->FirstChildElement("speed");
				if (f) {
					wi.WindSpeed = atoi_safe(f->Attribute("value"));
					strcpy_safe(wi.WindString, f->Attribute("name"), sizeof(wi.WindString));
					strcpy_safe(wi.SpeedUnits, f->Attribute("unit"), sizeof(wi.SpeedUnits));
				}
				f = e->FirstChildElement("direction");
				if (f) {
					strcpy_safe(wi.WindDirection, f->Attribute("code"), sizeof(wi.WindDirection));
				}
			}
			e = root->FirstChildElement("humidity");
			if (e) {
				strcpy_safe(wi.Humidity, e->Attribute("value"), sizeof(wi.Humidity));
				strcat_safe(wi.Humidity, e->Attribute("unit"), sizeof(wi.Humidity));
			}
			e = root->FirstChildElement("pressure");
			if (e) {
				strcpy_safe(wi.PressureUnits, e->Attribute("value"), sizeof(wi.PressureUnits));
				strcat_safe(wi.PressureUnits, " ", sizeof(wi.PressureUnits));
				strcat_safe(wi.PressureUnits, e->Attribute("unit"), sizeof(wi.PressureUnits));
			}
		} else {
			sprintf(buf, _("Sorry %s, there was an error getting your weather information (no current element)"), Scan->pres->Nick);
			Scan->pres->std_reply(Scan->pres, buf);
		}
	} else {
		sprintf(buf, _("Sorry %s, there was an error getting your weather information (%s)"), buf2);
		Scan->pres->std_reply(Scan->pres, buf);
	}
	//delete doc;

	if (part1) {
		sprintf(buf, _("Current conditions for %s"), wi.City);
		Scan->send_func(Scan->pres, buf);
		sprintf(buf, _("%s, %d%s"), wi.Condition, wi.Temp, Scan->getCelcius ? "C":"F");
		Scan->send_func(Scan->pres, buf);
		//sprintf(buf, _("Wind: %s"), wi.WindString);
		sprintf(buf, _("Wind: %s at %d %s"), wi.WindString, wi.WindSpeed, wi.SpeedUnits);
		Scan->send_func(Scan->pres, buf);
		sprintf(buf, _("Relative Humidity: %s, Barometric Pressure: %s"), wi.Humidity, wi.PressureUnits);
		Scan->send_func(Scan->pres, buf);
		//sprintf(buf, _("Visibility: %s %s"), wi.Sunrise, Scan->getCelcius ? "km":"mi");
		//Scan->send_func(Scan->pres, buf);

		stringstream sstr;
		char * nick = api->db->MPrintf("%q", Scan->pres->User ? Scan->pres->User->Nick:Scan->pres->Nick);
		city = api->db->MPrintf("%q", Scan->data);
		sstr << "REPLACE INTO Weather (Nick, City, IsCelcius, TimeStamp) VALUES ('" << nick << "', '" << city << "', " << Scan->getCelcius << ", " << time(NULL) << ");";
		api->db->Free(city);
		api->db->Free(nick);
		api->db->Query(sstr.str().c_str(), NULL, NULL, NULL);
	} else {
		remove(fn1);
	}

	zfreenn(file1);
}
