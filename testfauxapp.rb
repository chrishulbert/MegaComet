require 'socket'
clients = %w'Jim Bob Sam John Will Sue Mary Joe'
messages = %w'Hi Bye Hello Gday Goodbye Blah Yada'

manager = TCPSocket.open('localhost',9000)
loop {
	client = clients[rand(clients.length)]
	message = messages[rand(messages.length)]
	command = [2, client, message].pack('CZ*Z*')
	manager.write(command)
	sleep 2	
}
manager.close
