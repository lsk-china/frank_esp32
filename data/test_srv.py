import socket

HOST="0.0.0.0"
PORT=27012
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.bind((HOST, PORT))
sock.listen(9)
while True:
	conn, addr = sock.accept()
	dat = conn.recv(1024)
	with open("test.dat", "wb") as fp:
		fp.write(dat)
	print("Finish")

