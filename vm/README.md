# Tutorial

Primeiramente, para configurar a imagem, rodar o script setup.sh. Depois, para iniciar a maquina virtual:

sudo ./roda.sh

A máquina virutal vai ser iniciada e o script up.sh automaticamente vai ser
executado para configurar a rede.

Quando a máquina bootar, é normal que trave no momento de configurar a rede
(eth0). Isso porque o RedHat está tentando usar DHCP para configurar a
interface, mas nenhum servidor de DHCP está rodando.
É só esperar um pouco, vai dar um erro mas a inicialização vai ocorrer
normalmente. Depois do boot, logar com:

user: root
password: Tr@b@lh0-3

E configurar a rede da maneira certa (IP estatico):

vi /etc/sysconfig/network-scripts/ifcfg-eth0

DEVICE=eth0
ONBOOT=yes
BOOTPROTO=none
IPADDR=10.2.0.100
NETMASK=255.255.255.0

