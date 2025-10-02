# This constraint file resides INSIDE the IP package and is evaluated for each instance.
# It "reads" the value of the DIV parameter from the CURRENT instance of the IP
# and creates the corresponding generated clock.

# 1) Read the DIV parameter from the IP instance
set DIV [get_property GENERIC.DIV [current_instance]]
if {$DIV eq ""} {
  # Some Vivado versions use PARAM_VALUE instead of GENERIC
  set DIV [get_property PARAM_VALUE.DIV [current_instance]]
}
if {$DIV eq ""} {
  # Fallback: use the default (must match the default in Verilog)
  set DIV 4
}

# 2) Create a generated clock for the BUFG output of this IP instance
#    Note: we use relative paths via [current_instance]
create_generated_clock -name [format "%s_clk_out" [get_property NAME [current_instance]]] \
  -source [get_pins [current_instance]/u_bufg/I] \
  -divide_by $DIV \
  [get_pins [current_instance]/u_bufg/O]
