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
gap
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

# Blender reads the class attributes that describe an add-on's operators and
# panels, and calls its unregister function when the add-on is turned off
_.bl_category
_.bl_context
_.bl_label
_.bl_options
_.bl_region_type
_.bl_space_type
unregister

# The add-on sets these Blender properties, which Blender reads
_.display_type
_.hide_select
_.show_in_front
_.use_bone_envelopes
_.use_vertex_groups

# unittest calls a test module's tearDownModule once its tests have run
tearDownModule
