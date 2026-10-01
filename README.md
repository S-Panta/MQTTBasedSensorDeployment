## Adaptive Sampling For Environmental Monitoring Network using MQTT as network protocol
This repository contains hardware and software design for deploying the environmental monitoring wireless sensor nodes using MQTT as the communication protocol. Under MQTT pub/sub architecture, there are mainly three components: 
- Publisher: The client that publish data to the server on topics. Here, topic has same idea to URL for web services.
- Broker: The central server that routes the message from Publisher to Subscriber.
- Subscriber: The device that listens to the message from the publish by subscribing to the topic that these publishers publish the message.
A single device can be both publishing to one topic and subscribing to another topic
