import socket
from time import sleep

#create socket
def socket_create():
    global host
    global port
    global s

    #VM uses enp0s8 interface, do sudo dhclient enp0s8 to request new DHCP addr if none
    host = "192.168.56.4" #Server address
    port = 9999

    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR,1)
    except socket.error as msg:
        print("Socket creation error: " + str(msg))
        raise SystemExit() #exit as server is not up

#connect socket
def socket_connect():
    global host
    global port
    global s

    #Connect to server
    try:
        s.connect((host,port))
        print("Connected to the server IP: " + host + " | Port: " + str(port))
    except socket.error as msg:
        print("Socket connection error: " + str(msg))
        print("Please ensure server is running and host address is correct")
        raise SystemExit() #exit as server is not up

#main tcp_client code
def tcp_client():
    global host
    global port
    global s

    while True:
        #call above functions to create and connect socket
        socket_create()
        socket_connect()

        fileName = input("Enter name of file to send: ")
        if fileName == 'quit':
            return()

        else:
            
            #read binary of specified file 
            try:
                f = open(fileName,'rb') #mode: read binary
            except:
                print("Error reading file, ensure it exists in FileTransfer dir")
                return("retry")

            try:
                print("Sending...")

                #identify length of file name
                length = len(fileName)
                #calculate number of spacing bytes needed
                size = 64 - length

                #send file name and type to server to generate temp file of that name and type
                fileName = fileName.encode()
                for x in range(size):
                    fileName += b'\x00'
                
                #Send fileName to server
                s.send(fileName)

                #Attempt to read the provided file
                try:
                    l = f.read(1024)
                except:
                    print("File was not found in FileTransfer dir")
                    return("retry")

                #send 1024 bytes of file at a time
                while (l):
                    s.send(l) #send 1024 bytes to server
                    l = f.read(1024) #reads next bytes from file
                f.close()

                #Confirmation
                print("Sent!")

                #Notify server that entire file has been sent by shutting down socket
                s.shutdown(socket.SHUT_WR)

                #receive the reply from server
                print("Reply from server: " + str(s.recv(1024).decode()))

                #close the socket
                s.close()
                print("Socket closed")

            except socket.error as err:
                print("Sending/receiving error: " + str(err))
                raise SystemExit() #exit as server has crashed
                
   
#********************************************************************************

# main function

if __name__ == "__main__":
    
    #Handle keyboard interrupt gracefully and allow retries on invalid fileName
    while True:
        try:
            LRC = tcp_client()
            s.close()
            sleep(0.5)
        except KeyboardInterrupt:
            print("\nGoodbye.")
            raise SystemExit()

        if (LRC != "retry"):
            break
