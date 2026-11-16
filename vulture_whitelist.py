# Names vulture counts as unused that are used where it cannot see. It reads
# this file as code; nothing runs it.
#
# Blender reads the class attributes that describe an add-on's operators and
# panels, and calls its register and unregister functions when the add-on
# is turned on and off
_.bl_category
_.bl_context
_.bl_label
_.bl_options
_.bl_region_type
_.bl_space_type
register
unregister

# The add-on sets these Blender properties, which Blender reads
_.display_type
_.hide_select
_.show_in_front
_.use_bone_envelopes
_.use_vertex_groups

# unittest calls a test module's tearDownModule once its tests have run
tearDownModule
