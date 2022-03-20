
import paho.mqtt.client as mqtt
host = 'broker.emqx.io'
port = 1883

client = mqtt.Client(protocol=mqtt.MQTTv311)
client.connect(host, port=port, keepalive=60)

client.publish("/CAN/getlogs","START")


