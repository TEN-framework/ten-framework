# lws minimal MQTT client

The application connects to a broker at localhost 1883 (unencrypted) or
8883 (tls)

## build

```
 $ cmake . && make
```

## usage

Commandline option|Meaning
---|---
-d <loglevel>|Debug verbosity in decimal, eg, -d15
-s| Use tls and connect to port 8883 instead of 1883

Start mosquitto server locally

```
$ mosquitto
```

Run the example

```
[2020/01/31 10:40:23:7789] U: LWS minimal MQTT client unencrypted [-d<verbosity>][-s]
[2020/01/31 10:40:23:8539] N: lws_mqtt_generate_id: User space provided a client ID 'lwsMqttClient'
[2020/01/31 10:40:23:9893] N: _lws_mqtt_rx_parser: migrated nwsi 0x50febd0 to sid 1 0x5106820
[2020/01/31 10:40:23:9899] U: callback_mqtt: MQTT_CLIENT_ESTABLISHED
[2020/01/31 10:40:23:9967] U: callback_mqtt: WRITEABLE: Subscribing
[2020/01/31 10:40:24:0068] U: callback_mqtt: MQTT_SUBSCRIBED
```

Send something to the test client


```
mosquitto_pub -h 127.0.0.1 -p 1883 -t test/topic0 -m "hello"
```

Observe it received at the test client

```
[2020/01/31 10:40:27:1845] U: callback_mqtt: MQTT_CLIENT_RX
[2020/01/31 10:40:27:1870] N: 
[2020/01/31 10:40:27:1945] N: 0000: 74 65 73 74 2F 74 6F 70 69 63 30                   test/topic0     
[2020/01/31 10:40:27:1952] N: 

```
