def is_time(line)
	line[4,1]=='/' && line[7,1]=='/'
end

# Get enough stats for an hour
cmd = 'top -stats command,rsize,rprvt,rshrd,faults,cpu -l 360 -s 10 > stats.txt'
cmd = 'top -stats command,rsize,rprvt,rshrd,faults,cpu -l 3 > stats.txt'
system cmd

samples=0
sample_time=''

mc_rsize = mc_rprvt = mc_rshrd = mc_faults = mc_cpu = 0
mm_rsize = mm_rprvt = mm_rshrd = mm_faults = mm_cpu = 0

# when processing, skip the first entry since it'll have invalid cpu %
puts 'Time,Process,rsize,rprvt,rshrd,faults,cpu'
File.open("stats.txt").each { |line|
	if is_time(line) # end of a sample
		samples += 1
		
		if samples >= 3 # skip the first 2 samples before we start outputting
			puts sample_time + ',workers,' + mc_rsize + ',' + mc_rprvt + ',' + mc_rshrd + ',' + mc_faults + ',' + mc_cpu
			puts sample_time + ',manager,' + mm_rsize + ',' + mm_rprvt + ',' + mm_rshrd + ',' + mm_faults + ',' + mm_cpu
		end

		# initialise for this next sample
		sample_time = line
		mc_rsize = mc_rprvt = mc_rshrd = mc_faults = mc_cpu = 0
		mm_rsize = mm_rprvt = mm_rshrd = mm_faults = mm_cpu = 0
	end
	if line.start_with?('megacomet') # Worker
	end
	if line.start_with?('megamanager') # Manager
	end
}
