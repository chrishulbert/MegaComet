require 'socket'
workerCount = 8
sockets = []
(0...workerCount).each { |workerNo|
	puts '%d connecting' % workerNo
	socket = TCPSocket.open('localhost',9000) 
	command = [1, workerNo].pack('CC')
	socket.write(command)
	sockets.push(socket)
}
puts 'All connected, now reading from them all'
loop {
	sockets.each_with_index {|socket, workerNo|
		begin
			mesg, sender_addrinfo = socket.recv_nonblock(1024)
			if mesg.length>0
				puts 'Worker %d received %s' % [workerNo, mesg]
			end
		rescue IO::WaitReadable
			# nothing incoming
		end
	}
	sleep 0.1 # be nice to the OS
}
