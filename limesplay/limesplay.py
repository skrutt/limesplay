# coding: utf-8

from os.path import exists
import serial, psutil
from time import sleep
import datetime
#for ips
import socket
import fcntl
import struct

def get_ip_address(ifname):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    return socket.inet_ntoa(fcntl.ioctl(
        s.fileno(),
        0x8915,  # SIOCGIFADDR
        struct.pack('256s', ifname[:15])
    )[20:24])

#get_ip_address('eth0')  # '192.168.0.110'

def get_ip(ifname):
        try:
                ip = get_ip_address(ifname);
        except IOError: #No ip on that interface!
                ip = 'no_ip'
        return ip


def main():
    ser=initSer()   #get serialport
    #Do main stuff
    count = 0
    while ser:
        #Send start byte here
        ser.write(chr(1))
        ser.write("Cpu" + bar(psutil.cpu_percent(),12) )
        ser.write(str(datetime.datetime.now())[11:16])
        #flip page
        if ((count % 15) >= 10):
                ip_str = get_ip('wlan1')
		if(ip_str == 'no_ip'):	#try to get ip for different interfaces
			ip_str = get_ip('eth0')
		if(ip_str == 'no_ip'):
			ip_str = get_ip('eth1')
		if(ip_str == 'no_ip'):
			ip_str = get_ip('eth2')
		if(ip_str == 'no_ip'):
			ip_str = get_ip('eth3')
		if(ip_str == 'no_ip'):
			ip_str = get_ip('usb0')
                ser.write("IP: " + ip_str + '             ')
	else:
	        ser.write("Ram" + bar(ram_percent(),12))
       		ser.write(str(datetime.datetime.now())[5:10])
        sleep(0.3) 
        count += 1
        if count > 255:
            count = 0    
    print 'Init serial returned false'

def ram_free():
    free = psutil.cached_phymem() + psutil.avail_phymem() + psutil.phymem_buffers()
    return free
    
def ram_total():
    return psutil.TOTAL_PHYMEM
    
def ram_percent():
    ram = 1.0 - float(ram_free()) / ram_total()
    return ram * 100
    
def bar(percent, length = 10):
    bar = '['
    for x in range(length - 2):
        if percent > 100.0 * x / (length - 2):
            bar += chr(255)
        else:
            bar += ' '
    return bar + ']'
def scandinavianSupport(text):
    textlist = list(text)   #make a list 
    text=''
    for c in range(len(textlist)):
        if textlist[c]=='å':
            text+=chr(134)
        elif textlist[c]=='Å':
            text+=chr(143)
        elif textlist[c]=='ä':
            text+=chr(225) # new
        elif textlist[c]=='Ä':     #change letters to right machinecode
            text+=chr(142)
        elif textlist[c]=='ö':
            text+=chr(239)  # new
        elif textlist[c]=='Ö':
            text+=chr(153)
        else:
            text+=repr(textlist[c])[2]
    return text #return new string
  
def initSer(baud=19200,timeout=0):
    #find and open serialport
    serpath=''  #init variable
    for num in range(10):            #search ports 0-10
        serpath = '/dev/ttyUSB{}'.format(num)
        if exists(serpath):    #run program with found port
            break
        else:
            print '{} not found'.format(serpath)
            if num == 9:
                print 'Serialtty not found.'
                print 'Failed opening serialport.'
                return False
    
    print 'Trying to open', serpath
    ser=serial.Serial(serpath,baud,timeout=timeout)
    if ser.isOpen():
        
        print 'Success, Stty open at', serpath
        return ser
    print 'Failed opening serialport. ???'
    return False
    
main()   
