import socket

#create socket

def socket_create():
    global host
    global port
    global s

    host = ''
    port = 9999

    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR,1)
    except socket.error as msg:
        print("Socket creation error: " + str(msg))

def socket_bind():
    global host
    global port
    global s

    #Bind socket to port
    try:
        s.bind((host, port))
    except socket.error as msg:
        print("Socket binding error: " + str(msg))

def tcp_server():
    global host
    global port
    global s

    #Call above functions
    socket_create()
    socket_bind()

    s.listen(5)
    print("Server is waiting for client connection")

    while True:
        try:
            connectionSocket, addr = s.accept()
            print("\nConnected to client with IP: " + addr[0] + " | Port: " + str(addr[1]))
            print("Ready to Receive File...")

            #receive file name and type from client
            data = connectionSocket.recv(64) #store bytes from server
            data = data.split(b'\xff',1)[0].rstrip(b'\x00') #remove EOF byte and heading spacers
            fileName = data.decode() #decode and store string

            #Ensure file on client side exists, otherwise will be empty, we want to restart
            if (fileName == ''):
                print("Client attempted to send non-existing file. Reconnecting...")
                connectionSocket.close()
                return("retry")

            #Inform user of file being received
            suffix = fileName.split(".",1)[1]
            print("Receiving ." + suffix + " file " + fileName)

            #create temp file of this type with same name to write into
            try:
                f = open(fileName,'wb') #Mode: write binary
            except:
                print("Error creating temp file to write received bytes into")

            #Receiving file 1024 bytes at a time and writing to temp file f
            l = connectionSocket.recv(1024)
            while (l):
                f.write(l)
                l = connectionSocket.recv(1024) #receives next bytes
            f.close()

            print("Done receiving")

            #Inform client that file was received succesfully
            encodedSTR = (fileName + " has been succesfully received.").encode()
            connectionSocket.send(encodedSTR)
            print("Reply to client has been sent")

            connectionSocket.close()
            print("Socket closed")

            print("Reconnecting to client...")

        except socket.error as err:
            print("Sending/receiving error: " + str(err))
            break

#********************************************************************************

# main function

if __name__ == "__main__":

    #Handle keyboard interrupt gracefully and allow retries on invalid fileName
    while True:
        try:
            LRC = tcp_server()
        except KeyboardInterrupt:
            print("\nGoodbye.")
            raise SystemExit()

        if (LRC != "retry"):
            break

    s.close()
    print("Server is offline")
