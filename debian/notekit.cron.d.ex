#
# Regular cron jobs for the notekit package
#
0 4	* * *	root	[ -x /usr/bin/notekit_maintenance ] && /usr/bin/notekit_maintenance
