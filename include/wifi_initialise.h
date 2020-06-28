#ifndef _WIFI_INITIALISE_H
#define _WIFI_INITIALISE_H

class Wifi_initialise {
public:
    char ROUTER_SSID[50];
    char ROUTER_PASSWD[50];

    Wifi_initialise(char* ROUTER_SSID, char* ROUTER_PASSWD);
    ~Wifi_initialise(void);

    virtual void wifi_initialise() = 0;
    void wifi_deinit();
    void wifi_start();
    void wifi_stop();
    void wifi_restore();
};

#endif