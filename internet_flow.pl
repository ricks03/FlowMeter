# Category = Weather

#@ Monitors flow meter data from a web interface

# Retrieves flow page 
#
# Rick Steeves
# misterhouse@corwyn.net
# Last Updated:
# 210121 - Creation


####################
## Reads flow meter(s) configured on an Arduino and reporting the data on 
## a web server interface. 
## Data returned by the website currently tab-separated and formatted: 
##     Rate (L/mi): 0.00 0.00 V(L/last minute): 0.00 0.00 V(L/last hour): 0.00 0.00 V(L/total): 8.81 6.18
##
## Based on weather_monitor.pl, and my internet_usgs.pl and RickButton.pl code (which are in turn based on 
## other's shoulders)
## Using: 
##   Arduino Mega 2560 (for enough memory)
##   an Arduino Ethernet shield (W5100 Ethernet controller detected)
##   1-30L-min-1-2MPa-Hall-Effect-Flowmeter-Control-Water-Flow-Sensor-For-Arduino
##
## Uses values from MH.ini to determine IP address of flow meter
## mh.ini: flow_mail    (instead using mail_home)
##             email address to send alerts to
## mh.ini: flow_URL
##             IP address of web server
## mh.ini: flow_notify
##             When warnings occor (not implemented)
#################### 

my $flow_URL; 		# The URL to build the request to the flow
my %flow_Notify; 	# The River level to send notification

my $f_flow_list = "$config_parms{data_dir}/web/flow.txt";
my $f_flow_html = "$config_parms{data_dir}/web/flow.html";

# USsed to track data stored in %Weather with state and state_now
$FlowTotal    = new Weather_Item 'FlowTotal';
$FlowRate     = new Weather_Item 'FlowRate';
# noloop=start

# the URL of the flow meter
$flow_URL = "http://$config_parms{flow_URL}"; 

# noloop=stop

#$p_flow_list = new Process_Item("get_url -ua $flow_URL $f_flow_html");
$p_flow_list = new Process_Item("get_url $flow_URL $f_flow_html");

#############################################

$v_flow_list  = new Voice_Cmd('[Get,Read,Show] flow total');
$v_flow_list -> set_info('This is the total for the flow meter.'); 

$state = said $v_flow_list;

#Check to see if there's anything in the file before speaking
if ($state eq 'Read') {
  if (file_size $f_flow_list) { speak text => "app=usgs $f_flow_list"; } else { speak text => 'app=usgs No flow data!';}
} elsif ($state eq 'Show') {
    if (file_size $f_flow_list) { respond text => $f_flow_list, time => 240, font => 'Times 25 bold', geometry => '+0+0', width => 36, height => 12; }
    else { respond text => 'No flow data!', time => 240, font => 'Times 25 bold', geometry => '+0+0', width => 36, height => 12; }
} elsif ($state eq 'Get') {
    # Do this only if the file has not already been updated in the last hour
#     if (-s $f_flow_html > 10 and
#         time_date_stamp(3, $f_flow_html) eq time_date_stamp(3)) {
#         print_log "flow data is current";
# 		# fixed 100219 looks like it spoke, then set things to read, and then read it again. 
#         start $p_flow_list 'do_nothing';  # Fire the process with no-op, so we can still run the parsing code for debug
#     }
#     else {
        if (&net_connect_check) {
            print_log "Retrieving flow data from the network ...";
            # Use start instead of run so we can detect when it is done
            start $p_flow_list;
        }
        else { speak "app=usgs Sorry, flow data requires a network connection"; }
#     }            
    &respond_wait;  # Tell web browser to wait for respond
    print "Getting the current flow...\n";
}

my @flow_Values = ();
if (done_now $p_flow_list) {
    my $text = file_read $f_flow_html;

    #To clean up the CGI flow Site
    # clean up the top explanation
    $text =~ s/#.+#//s;
    # print "Text1 = $text\n";
    #To clean up the table headers
    #$text =~ s/^.+?(flow)/$1/s;

    # Split out the values
    my @flow_List = split(/\n/,$text);
    my $flow_Value = "";
    my $flow_Message = "";
    
    foreach $flow_Value (@flow_List) {
		@flow_Values = split("\t", $flow_Value);
		# $flow_Level{"@flow_Values[1]"} = @flow_Values[7];
        # This just skips over the first couple of lines as there's not enough data in it. 
        # Faster and easier than regex
        # note that means that the flow_Values values get reset.
        unless ($flow_Values[10]) { next; }  # Skip over any results that don't have the data
		print "Result: " . $flow_Values[9] . " " . $flow_Values[10] . " " . $flow_Values[11] . "\n";
        # Only report running if it's running
        if ($flow_Values[1] > 0 ) { 
          $flow_Message .= "Currently running at a rate of $flow_Values[1] liters per minute.\n";
        }
        # Only report flow volume if there is some. 
        if ( $flow_Values[7] > 0 ) { 
          $flow_Message .= "Flow volume in the last hour is " . $flow_Values[7] . " liters.\n"; 
        }
        $flow_Message .= "Total volume is " . sprintf("%.0f", $flow_Values[10]) . " liters.\n"; 
        $Weather{FlowHour}   =  $flow_Values[7];
        $Weather{FlowRate}   =  $flow_Values[1]; 
        $Weather{FlowTotal}  =  $flow_Values[10]; 
        print "Flow Hour is $Weather{FlowHour} liters, Total:  $Weather{FlowTotal} liters.\n";        
    } 
    
     # Write out an updated file if we have new data
    if ($flow_Message) {
#    		print_log ("Sending notification that rivers are up!");
# 		# Notify friends that they need to go if it's warm enough!
#  		if (($Weather{TempOutdoor} >= $config_parms{flow_Temp}) && ($config_parms{flow_Friends})) {
# 			print_log ( "Notifying friends who need to know!");
#     			&net_mail_send(
# 				subject => "Woohoo! Rivers are up",
# 		   		to      => $config_parms{flow_Friends},
#                 		text    => $flow_Message);
# 		} else { # Email me anyway even if it's too cold
# 			$flow_Message .= "Current temp is " . $Weather{TempOutdoor} . " degrees.\n";
#     		&net_mail_send(
# 				subject => "Woohoo! Rivers are up",
#                  		text    => $flow_Message,
# 		 		to      => $config_parms{flow_Mail});
#     	}
# 
     	file_write($f_flow_list, $flow_Message);
#     	set $v_flow_list 'Read';
#        &setFlow;
 	}
}

$v_flow_setFlow = new Voice_Cmd "Update flow data";
$v_flow_setFlow-> set_info('Updates the Flow Sensor variables');
$v_flow_setFlow -> set_authority('anyone');

# Done as a voice command so the update interval can be set as a trigger, instead
# of hard-coded
if (said $v_flow_setFlow || $Startup || $Reload) {
	#&setFlow;
    set $v_flow_list 'Get';
}

sub setFlow {
	# updates the %Weather hash for known values from the flow sensor.
	# this way these items can be graphed as the flow values used by weather_rrd_update.pl
	# This is being set on read, so I probably don't need it here. Esp. since the [7] value doesn't make it here. 
    # Get the flow states
    #if (defined $Weather{Flow} and $Weather{Flow} ne 'unknown') {
	#   $Weather{Flow} = sprintf("%3.2f",$flow_Values[7]); 
    #}
    # print "C:Flow Total is $Weather{FlowTotal} liters set from value: $flow_Values[10] value.\n";
    #set $v_flow_list 'Get';
}

# Query about recent flow
$v_what_flow = new  Voice_Cmd('How much flow have we had in the last ' .
                              '[hour,2 hours,6 hours,12 hours,day,2 days,3 days,4 days,5 days,6 days,week,2 weeks,3 weeks,month,' .
                              '2 months,3 months,4 months,6 months]');
$v_what_flow-> set_info('Flow measured by our flow meter and logged by mh');
if (my $period = said $v_what_flow) {
    undef $temp;
    my $days;
    # Get last record from the day in question
    my ($number, $unit) = $period =~ /(\d*) ?(\S+)/;
    $number = 1 unless $number;

    if ($unit =~ /hour/) {
        $days = $number / 24;
    }
    elsif ($unit =~ /day/) {
        $days = $number;
    }
    elsif ($unit =~ /week/) {
        $days = $number * 7;
    }
    elsif ($unit =~ /month/) {
        $days = $number * 30;
    }
    else {
        print "\n\nError in internet_flow.pl code. period=$period\n";
    }
    my $flow = flow_since($Time - $days * 3600 * 24);
    if ($flow == -1) {
        $temp .= "Sorry, no flow data has been collected";
    }
    else {
        if ($flow) {
            $temp  .= "We have had $flow liters of flow ";
        }
        else {
            $temp .= "No flow has ocurred ";
        }
        if ($period eq 'day') {
            $temp .= 'in the last 24 hours';
        }
        else {
            $temp .= "in the last $period";
        }
    }
    respond $temp;
}

# report the start of the first flow each day
# while flowing, report hourly totals and remainder when rain stops

my $firstflow = 0;
$firstflow = 0 if $New_Day;

$isflowing_timer = new Timer;
if (expired $isflowing_timer) {
    $Weather{IsFlowing} = 0 ;
    my $minutes = int(60 - minutes_remaining $flow_report_timer);
    my $flow = flow_since($Time - 60 * $minutes) if $minutes;
    if ($Save{mode} ne 'mute') { speak "$flow liters flowed in the past $minutes minutes" if $flow; }
    set $flow_report_timer 0;
}

$flow_report_timer = new Timer;
if (expired $flow_report_timer and $Weather{IsFlowing}) {
    my $flow = flow_since($Time - 60 * 60);
    if ($Save{mode} ne 'mute') { speak "$flow liters flowed in the past hour" if $flow; }
    set $flow_report_timer 60 * 60 if $Weather{IsFlowing};
}

my $flowtotal_prev = 0;
my $flow_file = "$config_parms{data_dir}/flow.dbm";
if (my $flow = state_now $FlowTotal) {
    $Weather{FlowRecent} = 0;
    $Weather{FlowRecent} = $flow - $flowtotal_prev if $flow > $flowtotal_prev;
#   print "db r=$flow p=$flowtotal_prev w=$Weather{FlowRecent} f=$firstflow\n";
    if ($Weather{FlowRecent} > 0) {
        respond "Notice, AC pumps are running" unless $firstflow;
        dbm_write $flow_file, $Time, $Weather{FlowRecent};
#       logit "$config_parms{data_dir}/flow.dbm", "$Time_Now $Weather{FlowRecent}"
        set $isflowing_timer 20 * 60;
        set $flow_report_timer 60 * 60 unless active $flow_report_timer;
        $firstflow++;
        $Weather{IsFlowing}++;
    }
    $flowtotal_prev = $flow;
}

sub flow_since {
    my $time = shift;
    my %flow_dbm = read_dbm $flow_file;

    return -1 unless keys %flow_dbm;
    my $amount = 0;
    foreach my $event (reverse sort keys %flow_dbm) {
        #print "db e=$event a=$amount r=$flow_dbm{$event}\n";
        if ($event > $time) {
            $amount += $flow_dbm{$event};
        }
        else {
            last;
        }
    }
    eval "untie %flow_dbm";
    $amount = round $amount, 1;  # Round to nearest 1/10
    return $amount;
}
###########################################################
# Release Notes:
# 210121 v. 1.0 release
