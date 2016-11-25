#!/bin/bash

ip addr add 10.2.0.1/24 dev tap0
ip link set tap0 up
