require 'socket'
#clients = %w'Jim Bob Sam John Will Sue Mary Joe'
clients = ['Sue']
messages = %w'Hi Bye Hello Gday Goodbye Blah Yada'

manager = TCPSocket.open('localhost',9000)
count = 0
loop {
	#client = clients[rand(clients.length)]
	client = '%07d' % (rand*1000000)
	message = messages[rand(messages.length)]
	command = [2, client, message].pack('CZ*Z*')
	manager.write(command)
	sleep 0.1

	count += 1
	puts('%d messages sent' % count) if (count%100 == 0)
}
manager.close
