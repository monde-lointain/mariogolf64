# Names vulture counts as unused that are used where it cannot see. It reads
# this file as code; nothing runs it.
#
# oracle.py reads the fields of its trace's TypedDicts by string key, and
# names TraceRecord in a cast's string.
birth
calls_crossed
conflicts
copy_preferences
death
hard_conflicts
live_length
n_copy_sugg
n_refs
n_sugg
phys_reg
preferences
priority
qty
stage
TraceRecord

# gdb calls a breakpoint's stop method when it is hit, in oracle_gdb.py
_.stop
