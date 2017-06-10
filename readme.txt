// Appairer un nouveau device
bluetoothctl
	power on
	scan on
	agent on
	pair 98:D3:35:00:A2:33
	trust 98:D3:35:00:A2:33

// créer le port série adhoc
sudo hcitool scan 
  98:D3:35:00:A2:33
sudo rfcomm bind /dev/rfcomm0 98:D3:35:00:A2:33 1
ls -l /dev/rfcomm0

added in /etc/rc.local
	sudo rfcomm bind /dev/rfcomm0 98:D3:35:00:A2:33 1

/etc/bluetooth/rfcomm.conf:

rfcomm0 {
  # Automatically bind the device at startup
  bind yes;
  # Bluetooth address of the device
  device 98:D3:35:00:A2:33;
  # RFCOMM channel for the connection
  channel 1;
  # Description of the connection
  comment "This is HC05's serial port.";
}

// Lire et écrire sur le port série en ligne de commande
echo 1 > /dev/rfcomm0
cat -v < /dev/rfcomm0
