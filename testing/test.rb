kilo = 1024
mega = 1024*1024
giga = 1024*1024*1024
def is_time(line)
	line[4,1]=='/' && line[7,1]=='/'
end
def p(bit)
	a = bit.to_i
	a*=1024 if bit.include?('K')
	a*=1024*1024 if bit.include?('M')
	a*=1024*1024*1024 if bit.include?('G')
	a
end

# Get enough stats for an hour
cmd = 'top -stats command,rsize,rprvt,rshrd,faults,cpu -l 360 -s 10 > stats.txt' # 1 hour
cmd = 'top -stats command,rsize,rprvt,rshrd,faults,cpu -l 36 -s 10 > stats.txt' # 6 mins
cmd = 'top -stats command,rsize,rprvt,rshrd,faults,cpu -l 10 > stats.txt' # 10 sec
system cmd

samples=0
sample_time=''

mc_rsize = mc_rprvt = mc_rshrd = mc_faults = mc_cpu = 0
mm_rsize = mm_rprvt = mm_rshrd = mm_faults = mm_cpu = 0
cpu_user = cpu_sys = cpu_idl = 0

# when processing, skip the first entry since it'll have invalid cpu %
puts 'Time,Workers rsize (Megs),Workers rprvt,Workers rshrd,Workers faults,Workers cpu,' +
	'Manager rsize,Manager rprvt,Manager rshrd,Manager faults,Manager cpu,Total cpu user,Total cpu sys,Total cpu idle'
File.open("stats.txt").each { |line|
	if is_time(line) # end of a sample
		samples += 1
		
		if samples >= 3 # skip the first 2 samples before we start outputting
			puts sample_time + ',' +
				(mc_rsize/mega).to_s + ',' + (mc_rprvt/mega).to_s + ',' + (mc_rshrd/mega).to_s + ',' + mc_faults.to_s + ',' + mc_cpu.to_s + ',' +
				(mm_rsize/mega).to_s + ',' + (mm_rprvt/mega).to_s + ',' + (mm_rshrd/mega).to_s + ',' + mm_faults.to_s + ',' + mm_cpu.to_s + ',' +
				cpu_user.to_s + ',' + cpu_sys.to_s + ',' + cpu_idl.to_s
		end

		# initialise for this next sample
		sample_time = line.strip
		mc_rsize = mc_rprvt = mc_rshrd = mc_faults = mc_cpu = 0
		mm_rsize = mm_rprvt = mm_rshrd = mm_faults = mm_cpu = 0
		cpu_user = cpu_sys = cpu_idl = 0
	end
	if line.start_with?('CPU usage') # Eg CPU usage: 15.38% user, 46.15% sys, 38.46% idle 
		bits = line.split
		cpu_user = bits[2].to_f.round
		cpu_sys = bits[4].to_f.round
		cpu_idl = bits[6].to_f.round
	end
	if line.start_with?('megacomet') # Worker
		bits = line.split
		mc_rsize += p(bits[1])
		mc_rprvt += p(bits[2])
		mc_rshrd += p(bits[3])
		mc_faults += p(bits[4])
		mc_cpu += p(bits[5])
	end
	if line.start_with?('megamanager') # Manager
		bits = line.split
		mm_rsize += p(bits[1])
		mm_rprvt += p(bits[2])
		mm_rshrd += p(bits[3])
		mm_faults += p(bits[4])
		mm_cpu += p(bits[5])
	end
}
