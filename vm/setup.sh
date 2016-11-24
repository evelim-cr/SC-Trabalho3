#!/bin/bash

# Cria uma imagem que referencia a imagem base.
# Assim modificações na maquina virtual são feitas no arquivo
# hda.qcow2 e a imagem base fica intacta (read-only).
qemu-img create -f qcow2 -b RH72-sem-horacio.qcow2 hda.qcow2
